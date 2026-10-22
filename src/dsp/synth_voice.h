// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

namespace zrythm::dsp
{

/**
 * @brief Base class for a single polyphonic synthesizer voice.
 *
 * The base class owns the note bookkeeping shared by all voices (current
 * note/channel, age for voice stealing, sample rate); subclasses implement
 * how notes and pitch bend affect their DSP and how audio is rendered.
 *
 * Lifecycle: the voice manager calls note_on() to start a note, note_off()
 * to begin its release (envelope voices stay active until their tail decays,
 * then deactivate themselves), and cut() to silence the voice immediately
 * (voice stealing, panic).
 *
 * All note/render methods are called on the audio thread from
 * PolyVoiceManager and must be real-time safe.
 *
 * @see PolyVoiceManager
 */
class SynthVoice
{
public:
  /** 14-bit pitch bend center position (no bend). */
  static constexpr int kPitchBendCenter = 0x2000;

  SynthVoice () = default;
  SynthVoice (const SynthVoice &) = delete;
  SynthVoice &operator= (const SynthVoice &) = delete;
  virtual ~SynthVoice () = default;

  /** Whether the voice is currently playing a note. */
  bool is_active () const noexcept { return active_; }

  /** Currently playing MIDI note number, or -1 if inactive. */
  int current_note () const noexcept { return current_note_; }

  /** MIDI channel of the currently playing note (0-15). */
  int current_channel () const noexcept { return current_channel_; }

  /**
   * @brief Sequence number of the current note-on (higher = newer).
   *
   * Used by the voice manager to steal the oldest voice.
   */
  std::uint32_t note_sequence () const noexcept { return note_sequence_; }

  double sample_rate () const noexcept { return sample_rate_; }

  /**
   * @brief Sets the sample rate (called during processing preparation).
   *
   * Not RT-safe; called before rendering starts.
   */
  virtual void set_sample_rate (double sample_rate)
  {
    sample_rate_ = sample_rate;
  }

  /**
   * @brief Starts a note on this voice.
   *
   * The base class records the note bookkeeping; overrides must call the
   * base implementation first, then update their DSP.
   *
   * @param channel MIDI channel (0-15).
   * @param pitch MIDI note number (0-127).
   * @param velocity Normalized velocity (0-1).
   * @param note_sequence Monotonic sequence number from the voice manager.
   */
  virtual void note_on (
    int           channel,
    int           pitch,
    float         velocity,
    std::uint32_t note_sequence) noexcept [[clang::nonblocking]]
  {
    active_ = true;
    current_note_ = pitch;
    current_channel_ = channel;
    note_sequence_ = note_sequence;
  }

  /**
   * @brief Begins the release phase of the current note.
   *
   * The default deactivates the voice immediately. Voices with an envelope
   * override this to start the release and deactivate themselves (via
   * deactivate()) once their output has decayed to silence.
   */
  virtual void note_off () noexcept [[clang::nonblocking]] { deactivate (); }

  /**
   * @brief Immediately silences and frees the voice.
   *
   * Used for voice stealing and all-notes-off (panic). The base class
   * deactivates the voice; overrides may also reset DSP state.
   */
  virtual void cut () noexcept [[clang::nonblocking]] { deactivate (); }

  /**
   * @brief Handles a pitch bend change.
   *
   * @param value_0_to_16383 14-bit bend value (kPitchBendCenter = center).
   */
  virtual void pitch_bend (int value_0_to_16383) noexcept [[clang::nonblocking]]
  {
  }

  /**
   * @brief Renders the voice into @p output (adding to existing contents).
   *
   * Only called while the voice is active.
   *
   * @param output Target buffer.
   * @param start_sample First sample index to render.
   * @param num_samples Number of samples to render.
   */
  virtual void render (
    juce::AudioBuffer<float> &output,
    int                       start_sample,
    int                       num_samples) noexcept [[clang::nonblocking]] = 0;

protected:
  /**
   * @brief Deactivates the voice, making it available for new notes.
   *
   * Voices with a release tail call this once the tail has decayed to
   * silence.
   */
  void deactivate () noexcept [[clang::nonblocking]]
  {
    active_ = false;
    current_note_ = -1;
  }

private:
  bool          active_{};
  int           current_note_{ -1 };
  int           current_channel_{};
  std::uint32_t note_sequence_{};
  double        sample_rate_{ 44100.0 };
};

} // namespace zrythm::dsp
