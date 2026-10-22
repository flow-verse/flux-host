// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace zrythm_test_plugins
{

/** Minimal deterministic sine synth shared by the Test Synth fixtures. */
class SineSynth
{
public:
  static constexpr size_t kNumVoices = 8;

  void set_sample_rate (double sample_rate) { sample_rate_ = sample_rate; }

  void note_on (int16_t pitch, double velocity)
  {
    auto &voice = voices_[next_voice_];
    next_voice_ = (next_voice_ + 1) % voices_.size ();
    voice.pitch = pitch;
    voice.velocity = velocity;
    voice.phase = 0.0;
    voice.active = true;
  }

  void note_off (int16_t pitch)
  {
    for (auto &voice : voices_)
      {
        if (voice.active && voice.pitch == pitch)
          voice.active = false;
      }
  }

  /** Adds the synth output into the output buffers. */
  void process (float * left, float * right, uint32_t num_samples, double gain)
  {
    for (auto &voice : voices_)
      {
        if (!voice.active)
          continue;

        const double freq =
          440.0 * std::exp2 (static_cast<double> (voice.pitch - 69) / 12.0);
        const double phase_inc = 2.0 * std::numbers::pi * freq / sample_rate_;
        const double amp = 0.25 * gain * voice.velocity;
        for (uint32_t n = 0; n < num_samples; ++n)
          {
            const auto s = static_cast<float> (std::sin (voice.phase) * amp);
            left[n] += s;
            right[n] += s;
            voice.phase += phase_inc;
            if (voice.phase >= 2.0 * std::numbers::pi)
              voice.phase -= 2.0 * std::numbers::pi;
          }
      }
  }

private:
  struct Voice
  {
    int16_t pitch = 0;
    double  velocity = 0.0;
    double  phase = 0.0;
    bool    active = false;
  };

  std::array<Voice, kNumVoices> voices_{};
  size_t                        next_voice_ = 0;
  double                        sample_rate_ = 44100.0;
};

} // namespace zrythm_test_plugins
