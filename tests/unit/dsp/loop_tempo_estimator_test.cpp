// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <cmath>
#include <numbers>
#include <random>

#include "dsp/loop_tempo_estimator.h"

#include <gtest/gtest.h>

namespace zrythm::dsp
{

class LoopTempoEstimatorTest : public ::testing::Test
{
protected:
  static constexpr auto SAMPLE_RATE = units::sample_rate (44100);

  /**
   * Generates a mono drum loop (4-on-the-floor kick with offbeat hats) at the
   * given BPM.
   */
  static utils::audio::AudioBuffer make_drum_loop (double bpm, int num_bars)
  {
    const auto sr = SAMPLE_RATE.in (units::sample_rate);
    const auto samples_per_beat = static_cast<int> (sr * 60.0 / bpm);
    const auto total_samples = samples_per_beat * 4 * num_bars;

    utils::audio::AudioBuffer buf (1, total_samples);
    buf.clear ();
    auto * samples = buf.getWritePointer (0);

    std::mt19937                          rng (42);
    std::uniform_real_distribution<float> noise_dist (-1.f, 1.f);

    const auto add_kick = [&] (int start) {
      const auto len = static_cast<int> (sr * 0.15);
      double     phase = 0.;
      for (int i = 0; i < len && start + i < total_samples; ++i)
        {
          const auto t = static_cast<double> (i) / sr;
          // pitch envelope sweeping down from ~150Hz to ~50Hz
          const auto freq = 50. + 100. * std::exp (-t * 30.);
          phase += 2. * std::numbers::pi * freq / sr;
          samples[start + i] +=
            static_cast<float> (std::sin (phase) * std::exp (-t * 25.));
        }
    };
    const auto add_hat = [&] (int start) {
      const auto len = static_cast<int> (sr * 0.03);
      for (int i = 0; i < len && start + i < total_samples; ++i)
        {
          const auto t = static_cast<double> (i) / sr;
          samples[start + i] +=
            noise_dist (rng) * 0.3f * static_cast<float> (std::exp (-t * 100.));
        }
    };
    const auto add_snare = [&] (int start) {
      const auto len = static_cast<int> (sr * 0.15);
      for (int i = 0; i < len && start + i < total_samples; ++i)
        {
          const auto t = static_cast<double> (i) / sr;
          samples[start + i] +=
            noise_dist (rng) * 0.6f * static_cast<float> (std::exp (-t * 40.));
        }
    };

    for (int beat = 0; beat < 4 * num_bars; ++beat)
      {
        if (beat % 4 == 1 || beat % 4 == 3)
          add_snare (beat * samples_per_beat);
        else
          add_kick (beat * samples_per_beat);
      }
    // 16th-note hats to anchor the tatum grid
    for (int step = 0; step < 16 * num_bars; ++step)
      {
        add_hat (step * samples_per_beat / 4);
      }

    return buf;
  }
};

TEST_F (LoopTempoEstimatorTest, EstimatesTempoOfDrumLoop)
{
  const auto buf = make_drum_loop (120., 8);
  const auto bpm = estimate_loop_bpm (buf, SAMPLE_RATE);
  ASSERT_TRUE (bpm.has_value ());
  EXPECT_NEAR (bpm->in (units::bpm), 120., 1.0);
}

TEST_F (LoopTempoEstimatorTest, ReturnsNulloptForSilence)
{
  utils::audio::AudioBuffer buf (1, 44100 * 10);
  buf.clear ();
  EXPECT_EQ (estimate_loop_bpm (buf, SAMPLE_RATE), std::nullopt);
}

TEST_F (LoopTempoEstimatorTest, ReturnsNulloptForEmptyBuffer)
{
  const utils::audio::AudioBuffer buf (0, 0);
  EXPECT_EQ (estimate_loop_bpm (buf, SAMPLE_RATE), std::nullopt);
}

TEST_F (LoopTempoEstimatorTest, ReturnsNulloptForLongAudio)
{
  // audio longer than a minute is not analyzed
  utils::audio::AudioBuffer buf (1, 44100 * 65);
  buf.clear ();
  EXPECT_EQ (estimate_loop_bpm (buf, SAMPLE_RATE), std::nullopt);
}

} // namespace zrythm::dsp
