// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <algorithm>

#include "dsp/loop_tempo_estimator.h"
#include "utils/float_ranges.h"

#include <LoopTempoEstimator/LoopTempoEstimator.h>

namespace zrythm::dsp
{

namespace
{
/**
 * @brief Feeds mono-mixed samples from a JUCE buffer to the
 * loop-tempo-estimator library.
 */
class AudioBufferLteReader final : public LTE::LteAudioReader
{
public:
  AudioBufferLteReader (
    const utils::audio::AudioBuffer &buf,
    units::sample_rate_t             sample_rate)
      : buf_ (buf), sample_rate_ (sample_rate.in (units::sample_rate))
  {
  }

  double GetSampleRate () const override { return sample_rate_; }

  long long GetNumSamples () const override { return buf_.getNumSamples (); }

  void
  ReadFloats (float * buffer, long long where, size_t numFrames) const override
  {
    const auto num_channels = buf_.getNumChannels ();
    const auto frames_available = std::clamp<long long> (
      buf_.getNumSamples () - where, 0, static_cast<long long> (numFrames));

    std::fill_n (buffer, numFrames, 0.f);
    for (int ch = 0; ch < num_channels; ++ch)
      {
        const auto * src = buf_.getReadPointer (ch, static_cast<int> (where));
        for (long long i = 0; i < frames_available; ++i)
          {
            buffer[i] += src[i];
          }
      }
    if (num_channels > 1)
      {
        utils::float_ranges::mul_k2 (
          { buffer, numFrames }, 1.f / static_cast<float> (num_channels));
      }
  }

private:
  const utils::audio::AudioBuffer &buf_;
  double                           sample_rate_;
};
} // namespace

std::optional<units::bpm_t>
estimate_loop_bpm (
  const utils::audio::AudioBuffer &buf,
  units::sample_rate_t             sample_rate)
{
  if (
    buf.getNumSamples () == 0 || sample_rate <= units::sample_rate (0)
    || !utils::audio::buffer_has_audio (buf, 0, buf.getNumSamples ()))
    {
      return std::nullopt;
    }

  const AudioBufferLteReader reader{ buf, sample_rate };
  const auto                 bpm =
    LTE::GetBpm (reader, LTE::FalsePositiveTolerance::Strict, [] (double) { });
  if (!bpm.has_value ())
    {
      return std::nullopt;
    }
  return units::bpm (*bpm);
}

} // namespace zrythm::dsp
