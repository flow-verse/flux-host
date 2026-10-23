// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <fmt/format.h>

#include "dsp/tempo_map.h"

namespace flux::engine
{

/**
 * Conversions between the agent API's musical units and engine ticks.
 *
 * Bars are 1-based and may be fractional: bar 1.0 is the start of the song,
 * bar 2.5 is half-way through the second bar. A "beat" is the time
 * signature's beat unit (a quarter note in 4/4, an eighth note in 6/8).
 */

inline double
ticks_per_bar_at (const zrythm::dsp::TempoMap &tempo_map, double ticks)
{
  const auto ts = tempo_map.time_signature_at_tick (
    zrythm::units::ticks (static_cast<int64_t> (std::floor (ticks))));
  return static_cast<double> (ts.ticks_per_bar ().in (zrythm::units::ticks));
}

inline double
ticks_per_beat_at (const zrythm::dsp::TempoMap &tempo_map, double ticks)
{
  const auto ts = tempo_map.time_signature_at_tick (
    zrythm::units::ticks (static_cast<int64_t> (std::floor (ticks))));
  return static_cast<double> (ts.ticks_per_beat ().in (zrythm::units::ticks));
}

/**
 * @brief Rounds @p v to @p decimals places for JSON output.
 *
 * Goes through text so the result is the double closest to the decimal
 * value (the build uses -ffast-math, under which x / 100 is not exact).
 */
inline double
round_decimals (double v, int decimals)
{
  return std::strtod (fmt::format ("{:.{}f}", v, decimals).c_str (), nullptr);
}

/** @pre bar >= 1. */
inline double
bar_to_ticks (const zrythm::dsp::TempoMap &tempo_map, double bar)
{
  const auto whole = static_cast<int> (std::floor (bar));
  const auto frac = bar - whole;
  const double start =
    tempo_map.musical_position_to_tick ({ .bar = whole, .beat = 1, .sixteenth = 1, .tick = 0 })
      .asDouble ();
  return start + (frac * ticks_per_bar_at (tempo_map, start));
}

inline double
ticks_to_bar (const zrythm::dsp::TempoMap &tempo_map, double ticks)
{
  const auto pos = tempo_map.tick_to_musical_position (
    zrythm::units::ticks (static_cast<int64_t> (std::floor (ticks))));
  const double bar_start =
    tempo_map
      .musical_position_to_tick ({ .bar = pos.bar, .beat = 1, .sixteenth = 1, .tick = 0 })
      .asDouble ();
  return pos.bar + ((ticks - bar_start) / ticks_per_bar_at (tempo_map, bar_start));
}

} // namespace flux::engine
