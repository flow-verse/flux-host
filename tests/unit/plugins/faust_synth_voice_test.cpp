// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <cmath>
#include <memory>
#include <ranges>

#include "plugins/faust/faust_base.h"
#include "plugins/faust/faust_controls.h"
#include "plugins/faust/faust_synth_voice.h"

#include <gtest/gtest.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace zrythm::plugins::faust
{

namespace
{

/**
 * @brief Minimal stub of a Faust instrument dsp.
 *
 * Exposes the polyphonic-carrier controls (freq/gain/gate) and produces a
 * constant-amplitude signal across all output channels. The amplitude is
 * mutable so tests can simulate silence vs. sustained output.
 */
class StubDsp : public dsp
{
public:
  FAUSTFLOAT freq_zone{};
  FAUSTFLOAT gain_zone{};
  FAUSTFLOAT gate_zone{};

  /// Amplitude written to every sample of every channel on each compute()
  /// call. Tests mutate this to drive the envelope follower.
  float amplitude = 0.f;

  static constexpr int kSampleRate = 48000;
  static constexpr int kNumOutputs = 2;

  int getNumInputs () override { return 0; }
  int getNumOutputs () override { return kNumOutputs; }
  int getSampleRate () override { return kSampleRate; }

  void buildUserInterface (UI * ui) override
  {
    // Labels match the Faust polyphonic-carrier convention so the voice finds
    // the voice_zones_ entries.
    ui->addHorizontalSlider (
      "freq", &freq_zone, /*init*/ 0.f, /*min*/ 0.f, /*max*/ 20000.f, /*step*/
      1.f);
    ui->addHorizontalSlider ("gain", &gain_zone, 0.f, 0.f, 1.f, 0.001f);
    ui->addHorizontalSlider ("gate", &gate_zone, 0.f, 0.f, 1.f, 1.f);
  }

  void  init (int sr) override { instanceInit (sr); }
  void  instanceInit (int sr) override { /* no per-instance constants */ }
  void  instanceConstants (int sr) override { /* no per-instance constants */ }
  void  instanceResetUserInterface () override { }
  void  instanceClear () override { }
  dsp * clone () override { return new StubDsp (*this); }
  void  metadata (Meta * m) override { m->declare ("name", "Stub"); }

  void compute (int count, FAUSTFLOAT ** inputs, FAUSTFLOAT ** outputs) override
  {
    for (const auto ch : std::views::iota (0, kNumOutputs))
      {
        auto * out = outputs[ch];
        for (int n = 0; n < count; ++n)
          out[n] = amplitude;
      }
  }
};

class FaustSynthVoiceTest : public ::testing::Test
{
protected:
  static constexpr int kSampleRate = StubDsp::kSampleRate;
  static constexpr int kMaxBlock = 256;
  /// Matches the FaustPlugin defaults (see FaustPlugin::setup_faust).
  static constexpr float kReleaseSeconds = 2.f;
  static constexpr float kSilenceThresholdDb = -80.f;

  void SetUp () override
  {
    auto stub_owned = std::make_unique<StubDsp> ();
    stub_ = stub_owned.get ();
    voice_ = std::make_unique<FaustSynthVoice> (std::move (stub_owned));
    voice_->prepare (
      kSampleRate, kMaxBlock, kReleaseSeconds, kSilenceThresholdDb);

    output_.setSize (StubDsp::kNumOutputs, kMaxBlock);
    output_.clear ();
  }

  void render_block () { voice_->render (output_, 0, kMaxBlock); }

  /// Number of blocks equivalent to @p seconds at the test's sample rate and
  /// block size.
  static int blocks_for_seconds (float seconds)
  {
    return static_cast<int> (std::ceil (seconds * kSampleRate / kMaxBlock));
  }

  StubDsp *                        stub_{};
  std::unique_ptr<FaustSynthVoice> voice_;
  juce::AudioBuffer<float>         output_;
};

// Voice-controlled zones (freq/gain/gate) receive the expected values on
// note_on, mapping velocity into the gain control's declared range.
TEST_F (FaustSynthVoiceTest, NoteOnSetsFreqGainGate)
{
  voice_->note_on (
    /*channel*/ 0, /*pitch*/ 60, /*velocity*/ 0.5f, /*sequence*/ 1);

  EXPECT_FLOAT_EQ (
    stub_->freq_zone,
    static_cast<FAUSTFLOAT> (juce::MidiMessage::getMidiNoteInHertz (60)));
  // gain = min + velocity * (max - min) = 0 + 0.5 * (1 - 0) = 0.5
  EXPECT_FLOAT_EQ (stub_->gain_zone, 0.5f);
  EXPECT_FLOAT_EQ (stub_->gate_zone, 1.f);
  EXPECT_TRUE (voice_->is_active ());
}

// note_off() drops the gate but leaves the voice active so its release tail
// can render out. The envelope follower decides when to free the voice.
TEST_F (FaustSynthVoiceTest, NoteOffReleasesGateButKeepsVoiceActive)
{
  stub_->amplitude = 0.5f;
  voice_->note_on (0, 60, 0.8f, 1);
  render_block (); // voice produces output, envelope rises
  ASSERT_TRUE (voice_->is_active ());

  voice_->note_off ();
  EXPECT_FLOAT_EQ (stub_->gate_zone, 0.f);
  EXPECT_TRUE (voice_->is_active ())
    << "Voice should remain active for release tail after note_off";
}

// When the dsp produces silence, the voice deactivates within a block of the
// envelope decaying below the silence threshold after note_off.
TEST_F (FaustSynthVoiceTest, VoiceDeactivatesAfterSilencePostNoteOff)
{
  // Drive the envelope up first with loud output.
  stub_->amplitude = 1.f;
  voice_->note_on (0, 60, 1.f, 1);
  render_block ();
  ASSERT_TRUE (voice_->is_active ());

  // Then go silent and release the note.
  stub_->amplitude = 0.f;
  voice_->note_off ();

  // With the RMS follower at zero output, the envelope decays exponentially
  // toward 0; it must cross the silence threshold within the configured
  // release_seconds (plus a small margin for the half-block RMS quantization).
  const int max_blocks = blocks_for_seconds (kReleaseSeconds * 1.5f);
  int       blocks = 0;
  while (voice_->is_active () && blocks < max_blocks)
    {
      render_block ();
      ++blocks;
    }
  EXPECT_FALSE (voice_->is_active ())
    << "Voice was not freed after " << blocks << " blocks of silence";
}

// When the dsp keeps producing loud output after note_off, the voice stays
// active for far longer than the release_seconds — regression test for the
// peak-hold bug where a transient lifts the envelope for the entire tail.
TEST_F (FaustSynthVoiceTest, VoiceStaysActiveWhileLoud)
{
  stub_->amplitude = 1.f;
  voice_->note_on (0, 60, 1.f, 1);
  voice_->note_off ();

  // Render 3x the release time. With sustained loud output the RMS follower
  // never falls below the silence threshold, so the voice must stay alive.
  const int blocks = blocks_for_seconds (kReleaseSeconds * 3.f);
  for (int i = 0; i < blocks; ++i)
    render_block ();

  EXPECT_TRUE (voice_->is_active ())
    << "Voice was incorrectly freed after " << blocks
    << " blocks of loud output";
}

} // namespace

} // namespace zrythm::plugins::faust
