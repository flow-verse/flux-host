// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <optional>

#include "utils/audio.h"
#include "utils/units.h"

namespace zrythm::dsp
{

/**
 * @brief Estimates the tempo of the given audio if it appears to be a music
 * loop.
 *
 * Wrapper around the loop-tempo-estimator library. The audio is mixed down to
 * mono for analysis. Audio longer than roughly a minute is skipped (it is
 * unlikely to be a loop and costly to analyze).
 *
 * @param buf Audio frames (any channel count).
 * @param sample_rate Sample rate of @p buf.
 *
 * @return The estimated BPM, or std::nullopt if the audio is unlikely to be a
 * loop.
 */
std::optional<units::bpm_t>
estimate_loop_bpm (
  const utils::audio::AudioBuffer &buf,
  units::sample_rate_t             sample_rate);

} // namespace zrythm::dsp
