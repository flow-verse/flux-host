// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <concepts>
#include <vector>

#include "dsp/tick_types.h"

namespace zrythm::structure::arrangement
{

/**
 * @brief One iteration of the loop-expanded clip content.
 *
 * Loopable clips play their content in a specific order:
 * - First, the "intro" from clip_start to loop_end (plays once).
 * - Then, the loop region from loop_start to loop_end (repeats).
 *
 * Each LoopSegment represents one contiguous chunk of source content
 * mapped to an absolute position within the clip.
 */
struct LoopSegment
{
  /** Start of this segment's range in source-content space. */
  dsp::ContentTick virt_start;
  /** End of this segment's range in source-content space. */
  dsp::ContentTick virt_end;
  /** Start of this segment in clip-absolute space (0 = clip start). */
  dsp::ContentTick abs_start;
  /** End of this segment in clip-absolute space. */
  dsp::ContentTick abs_end;
};

/**
 * @brief Iterates over loop-expanded segments of a clip.
 *
 * Calls @p callback for each segment, in playback order:
 * 1. Intro leg: virtual [clip_start, loop_end) → absolute [0, loop_end -
 * clip_start)
 * 2. Loop legs: virtual [loop_start, loop_end) → absolute shifts by loop_length
 * each iteration
 *
 * All segments are clamped to end_tick. For non-looped clips, pass end_tick
 * equal to clip_ticks so only the intro leg (without wrapping) is produced.
 *
 * @param clip_start  Where in the source the clip begins playing.
 * @param loop_start  Start of the loop region in source content.
 * @param loop_end    End of the loop region in source content.
 * @param end_tick    Maximum absolute tick (e.g. clip_ticks, or
 * display_end_tick during drag for extended loop preview).
 * @param callback    Called with a LoopSegment for each iteration.
 */
template <std::invocable<const LoopSegment &> Func>
void
for_each_loop_segment (
  dsp::ContentTick clip_start,
  dsp::ContentTick loop_start,
  dsp::ContentTick loop_end,
  dsp::ContentTick end_tick,
  Func           &&callback)
{
  const auto loop_length =
    max (dsp::ContentTick{ units::ticks (0.0) }, loop_end - loop_start);

  auto virt_start = clip_start;
  auto virt_end = loop_end;
  auto abs_start = dsp::ContentTick{ units::ticks (0.0) };
  auto abs_end = loop_end - clip_start;

  // Clamp first leg to end_tick.
  if (abs_end > end_tick)
    {
      const auto diff = abs_end - end_tick;
      virt_end = virt_end - diff;
      abs_end = abs_end - diff;
    }

  while (abs_start < end_tick)
    {
      if (abs_end <= abs_start)
        break;

      callback (LoopSegment{ virt_start, virt_end, abs_start, abs_end });

      const auto current_len = abs_end - abs_start;
      if (current_len <= dsp::ContentTick{})
        break;

      // Subsequent legs use the loop region.
      virt_start = loop_start;
      virt_end = loop_end;
      abs_start = abs_start + current_len;
      abs_end = abs_end + loop_length;

      if (abs_end > end_tick)
        {
          const auto diff = abs_end - end_tick;
          virt_end = virt_end - diff;
          abs_end = abs_end - diff;
        }
    }
}

/**
 * @brief Timeline delta-tick positions of each loop wrap point.
 *
 * Iterates loop boundaries uniformly in content space (each iteration covers
 * the same content ticks), then converts each boundary to a timeline delta
 * via @p warp_lookup. During tempo ramps the warp is non-linear, producing
 * non-uniformly spaced deltas.
 *
 * @param warp_lookup     Callable mapping ContentTick → TimelineTick
 *                        (e.g. a lambda wrapping dsp::warp_lookup).
 * @param clip_start_ct   Where in the source the clip begins playing.
 * @param loop_start_ct   Start of the loop region in source content.
 * @param loop_end_ct     End of the loop region in source content.
 * @param max_delta_ticks Upper bound (exclusive) for returned deltas.
 * @param max_iterations  Safety limit on iteration count.
 */
template <typename WarpLookup>
std::vector<dsp::TimelineTick>
compute_loop_boundary_deltas (
  WarpLookup      &&warp_lookup,
  dsp::ContentTick  clip_start_ct,
  dsp::ContentTick  loop_start_ct,
  dsp::ContentTick  loop_end_ct,
  dsp::TimelineTick max_delta_ticks,
  int               max_iterations = 1000)
{
  std::vector<dsp::TimelineTick> result;

  const auto loop_length =
    max (dsp::ContentTick{ units::ticks (0.0) }, loop_end_ct - loop_start_ct);
  if (loop_length <= dsp::ContentTick{})
    return result;

  auto cumulative = loop_end_ct - clip_start_ct;

  for (int i = 0; i < max_iterations; ++i)
    {
      const auto delta = warp_lookup (cumulative);
      if (delta >= max_delta_ticks)
        break;
      result.push_back (delta);
      cumulative = cumulative + loop_length;
    }

  return result;
}

} // namespace zrythm::structure::arrangement
