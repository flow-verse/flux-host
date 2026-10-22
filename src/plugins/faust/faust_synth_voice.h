// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/synth_voice.h"
#include "plugins/faust/faust_base.h"
#include "plugins/faust/faust_controls.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace zrythm::plugins::faust
{

/**
 * @brief A dsp::SynthVoice wrapping one Faust dsp instance.
 *
 * Follows the Faust polyphonic convention: voice-managed controls named
 * "freq" (Hz), "gain" (velocity-mapped) and "gate" are set on note on/off,
 * plus an optional control with [midi:pitchwheel] metadata driven by pitch
 * bend.
 *
 * The wrapped dsp must have zero audio inputs; render() passes nullptr for
 * inputs to dsp::compute(). This invariant is asserted in prepare().
 */
class FaustSynthVoice : public zrythm::dsp::SynthVoice
{
public:
  explicit FaustSynthVoice (std::unique_ptr<dsp> dsp);

  /** Returns the dsp's controls in enumeration order (same for all voices). */
  const std::vector<FaustControl> &controls () const { return controls_; }

  /** Sets a shared (non voice-managed) control value by control index. */
  void set_control_value (size_t control_index, FAUSTFLOAT value) noexcept
    [[clang::nonblocking]];

  /**
   * @brief Initializes the dsp and render buffers for processing.
   *
   * @param release_seconds Time for the release-tail envelope follower to
   *   decay from unity to the silence threshold. Covers reverb tails and
   *   long release envelopes; override via the `zrythm_release_seconds`
   *   metadata on the dsp.
   * @param silence_threshold_db RMS level (dBFS) below which the voice is
   *   considered silent after note-off. Override via the
   *   `zrythm_silence_threshold_db` metadata on the dsp.
   *
   * Not RT-safe; called during processing preparation.
   */
  void prepare (
    int   sample_rate,
    int   max_block_length,
    float release_seconds,
    float silence_threshold_db);

  /**
   * @brief Returns whether the control is voice-managed (freq/gain/gate, or
   * a [midi:pitchwheel] control) and should not be exposed as a plugin
   * parameter.
   */
  static bool is_voice_managed_control (const FaustControl &control);

  // ============================================================================
  // dsp::SynthVoice interface
  // ============================================================================

  void note_on (
    int           channel,
    int           pitch,
    float         velocity,
    std::uint32_t note_sequence) noexcept [[clang::nonblocking]] override;

  void note_off () noexcept [[clang::nonblocking]] override;

  void cut () noexcept [[clang::nonblocking]] override;

  void
  pitch_bend (int value_0_to_16383) noexcept [[clang::nonblocking]] override;

  void render (
    juce::AudioBuffer<float> &output,
    int                       start_sample,
    int num_samples) noexcept [[clang::nonblocking]] override;

private:
  /** Pointers to the voice-managed control zones. */
  struct VoiceZones
  {
    FaustControl * freq{};
    FaustControl * gain{};
    FaustControl * gate{};
    FaustControl * pitchwheel{};
  };

private:
  std::unique_ptr<dsp>      dsp_;
  std::vector<FaustControl> controls_;

  VoiceZones voice_zones_;

  /** Per-voice render buffer (channels = dsp outputs). */
  juce::AudioBuffer<float>  voice_buffer_;
  std::vector<FAUSTFLOAT *> voice_out_ptrs_;

  /** Release-tail detection: the envelope follower decays exponentially
   * after note-off; when it drops below `silence_threshold_` the voice is
   * freed. Both are computed in prepare() from the plugin's metadata-driven
   * configuration. */
  float silence_threshold_{ 1e-4f }; // -80 dBFS amplitude
  float release_seconds_{ 2.f };
  float envelope_{};
  bool  gate_released_{};
};

} // namespace zrythm::plugins::faust
