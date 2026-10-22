// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <cmath>
#include <ranges>

#include "plugins/faust/faust_synth_voice.h"

namespace zrythm::plugins::faust
{

FaustSynthVoice::FaustSynthVoice (std::unique_ptr<dsp> dsp)
    : dsp_ (std::move (dsp))
{
  FaustControlEnumerator enumerator;
  dsp_->buildUserInterface (&enumerator);
  controls_ = enumerator.controls ();

  for (auto &control : controls_)
    {
      if (control.label == u8"freq")
        voice_zones_.freq = &control;
      else if (control.label == u8"gain")
        voice_zones_.gain = &control;
      else if (control.label == u8"gate")
        voice_zones_.gate = &control;
      else if (control.midi == u8"pitchwheel")
        voice_zones_.pitchwheel = &control;
    }
}

bool
FaustSynthVoice::is_voice_managed_control (const FaustControl &control)
{
  return control.label == u8"freq" || control.label == u8"gain"
         || control.label == u8"gate"
         // pitchwheel is a MIDI control, not a host parameter: the
         // PolyVoiceManager drives it from incoming pitch-bend messages.
         || control.midi == u8"pitchwheel";
}

void
FaustSynthVoice::set_control_value (size_t control_index, FAUSTFLOAT value) noexcept
{
  if (control_index < controls_.size ())
    {
      *controls_[control_index].zone = value;
    }
}

void
FaustSynthVoice::prepare (
  int   sample_rate,
  int   max_block_length,
  float release_seconds,
  float silence_threshold_db)
{
  // Instrument DSPs must have zero audio inputs: render() drives compute()
  // with nullptr for inputs (the voice is a generator, not a filter).
  assert (dsp_->getNumInputs () == 0);

  set_sample_rate (static_cast<double> (sample_rate));
  dsp_->instanceInit (sample_rate);

  release_seconds_ = release_seconds;
  silence_threshold_ = std::pow (10.f, silence_threshold_db / 20.f);

  const int num_voice_outs = dsp_->getNumOutputs ();
  voice_buffer_.setSize (num_voice_outs, max_block_length);
  voice_out_ptrs_.resize (num_voice_outs);
  for (const auto ch : std::views::iota (0, num_voice_outs))
    voice_out_ptrs_[ch] = voice_buffer_.getWritePointer (ch);
}

void
FaustSynthVoice::note_on (
  int           channel,
  int           pitch,
  float         velocity,
  std::uint32_t note_sequence) noexcept
{
  zrythm::dsp::SynthVoice::note_on (channel, pitch, velocity, note_sequence);

  gate_released_ = false;
  envelope_ = 0.f;

  if (voice_zones_.freq != nullptr)
    {
      *voice_zones_.freq->zone =
        static_cast<FAUSTFLOAT> (juce::MidiMessage::getMidiNoteInHertz (pitch));
    }
  if (voice_zones_.gain != nullptr)
    {
      *voice_zones_.gain->zone =
        voice_zones_.gain->min
        + velocity * (voice_zones_.gain->max - voice_zones_.gain->min);
    }
  if (voice_zones_.gate != nullptr)
    {
      *voice_zones_.gate->zone = 1.f;
    }
}

void
FaustSynthVoice::note_off () noexcept
{
  // Let the envelope release naturally; render() deactivates the voice once
  // the tail decays to silence
  if (voice_zones_.gate != nullptr)
    {
      *voice_zones_.gate->zone = 0.f;
    }
  gate_released_ = true;
}

void
FaustSynthVoice::cut () noexcept
{
  if (voice_zones_.gate != nullptr)
    {
      *voice_zones_.gate->zone = 0.f;
    }
  gate_released_ = false;
  envelope_ = 0.f;
  deactivate ();
}

void
FaustSynthVoice::pitch_bend (int value_0_to_16383) noexcept
{
  if (voice_zones_.pitchwheel == nullptr)
    return;

  const auto &control = *voice_zones_.pitchwheel;
  const auto  normalized = static_cast<float> (value_0_to_16383) / 16383.f;
  *control.zone = control.min + normalized * (control.max - control.min);
}

void
FaustSynthVoice::render (
  juce::AudioBuffer<float> &outputBuffer,
  int                       startSample,
  int                       numSamples) noexcept
{
  if (numSamples == 0)
    return;

  const int num_voice_outs = dsp_->getNumOutputs ();
  if (num_voice_outs == 0)
    return;

  voice_buffer_.clear (0, numSamples);

  dsp_->compute (numSamples, nullptr, voice_out_ptrs_.data ());

  // compute() writes through cached raw pointers, bypassing JUCE's isClear
  // tracking - mark the buffer dirty so addFrom()/getReadPointer() below
  // operate on the actual samples
  voice_buffer_.setNotClear ();

  // Sum the voice into the output buffer.
  for (const auto ch : std::views::iota (0, outputBuffer.getNumChannels ()))
    {
      const int voice_ch = std::min (ch, num_voice_outs - 1);
      outputBuffer.addFrom (
        ch, startSample, voice_buffer_, voice_ch, 0, numSamples);
    }

  // RMS across all voice channels: a single-sample transient in a 256-sample
  // block contributes 1/256 of its energy, so RMS naturally smooths transients
  // (a -20 dBFS peak becomes -44 dBFS RMS in that block) without lifting the
  // envelope for the entire release tail.
  float sum_sq = 0.f;
  for (const auto ch : std::views::iota (0, num_voice_outs))
    {
      const auto * src = voice_buffer_.getReadPointer (ch);
      for (int n = 0; n < numSamples; ++n)
        sum_sq += src[n] * src[n];
    }
  const float block_rms =
    std::sqrt (sum_sq / static_cast<float> (num_voice_outs * numSamples));

  // Peak-hold with exponential decay: instant attack, decay from unity to
  // `silence_threshold_` over `release_seconds_`. The coefficient is
  // block-size-independent: at sample rate SR, block size B,
  // `coeff^(SR/B * release_seconds)` == silence_threshold_.
  const float release_coeff = std::exp (
    std::log (silence_threshold_) * static_cast<float> (numSamples)
    / (release_seconds_ * static_cast<float> (sample_rate ())));
  envelope_ = std::max (block_rms, envelope_ * release_coeff);

  // Detect silence after note-off to free the voice
  if (gate_released_ && envelope_ < silence_threshold_)
    {
      gate_released_ = false;
      envelope_ = 0.f;
      deactivate ();
    }
}

} // namespace zrythm::plugins::faust
