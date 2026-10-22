// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <vector>

#include "dsp/content_time_warp.h"
#include "dsp/tick_types.h"
#include "structure/arrangement/loop_segment_iterator.h"

#include <gtest/gtest.h>

namespace zrythm::structure::arrangement
{

namespace
{

using Tick = dsp::ContentTick;

auto
mk_tick (double v) -> Tick
{
  return Tick{ units::ticks (v) };
}

/// Runs the iterator and returns all segments as a simple vector.
std::vector<LoopSegment>
collect_segments (Tick clip_start, Tick loop_start, Tick loop_end, Tick end_tick)
{
  std::vector<LoopSegment> result;
  for_each_loop_segment (
    clip_start, loop_start, loop_end, end_tick,
    [&] (const LoopSegment &seg) { result.push_back (seg); });
  return result;
}

} // namespace

// ---------------------------------------------------------------------------
// Non-looped: end_tick == clip length (intro only, no repetitions)
// ---------------------------------------------------------------------------

TEST (LoopSegmentIteratorTest, NonLoopedProducesSingleIntroSegment)
{
  // clip_start=0, loop=[0,100), clip length=100
  auto segs =
    collect_segments (mk_tick (0), mk_tick (0), mk_tick (100), mk_tick (100));
  ASSERT_EQ (segs.size (), 1u);
  EXPECT_DOUBLE_EQ (segs[0].virt_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_end.asDouble (), 100.0);
}

TEST (LoopSegmentIteratorTest, EndTickWithinIntroClamps)
{
  // end_tick=30 < loop_end=100 → intro segment clamped to 30
  auto segs =
    collect_segments (mk_tick (0), mk_tick (50), mk_tick (100), mk_tick (30));
  ASSERT_EQ (segs.size (), 1u);
  EXPECT_DOUBLE_EQ (segs[0].virt_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].virt_end.asDouble (), 30.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_end.asDouble (), 30.0);
}

// ---------------------------------------------------------------------------
// Looped: end_tick > intro length (intro + loop repetitions)
// ---------------------------------------------------------------------------

TEST (LoopSegmentIteratorTest, LoopedOneIteration)
{
  // clip_start=0, loop=[50,100), clip length=150
  // Intro: [0,100) → abs [0,100)
  // Loop 1: [50,100) → abs [100,150)
  auto segs =
    collect_segments (mk_tick (0), mk_tick (50), mk_tick (100), mk_tick (150));
  ASSERT_EQ (segs.size (), 2u);

  EXPECT_DOUBLE_EQ (segs[0].virt_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_end.asDouble (), 100.0);

  EXPECT_DOUBLE_EQ (segs[1].virt_start.asDouble (), 50.0);
  EXPECT_DOUBLE_EQ (segs[1].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_start.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_end.asDouble (), 150.0);
}

TEST (LoopSegmentIteratorTest, LoopedMultipleIterations)
{
  // clip_start=0, loop=[50,100), clip length=250
  // Intro: abs [0,100)
  // Loop 1: abs [100,150)
  // Loop 2: abs [150,200)
  // Loop 3: abs [200,250)
  auto segs =
    collect_segments (mk_tick (0), mk_tick (50), mk_tick (100), mk_tick (250));
  ASSERT_EQ (segs.size (), 4u);

  EXPECT_DOUBLE_EQ (segs[0].abs_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_end.asDouble (), 100.0);

  EXPECT_DOUBLE_EQ (segs[1].abs_start.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_end.asDouble (), 150.0);

  EXPECT_DOUBLE_EQ (segs[2].abs_start.asDouble (), 150.0);
  EXPECT_DOUBLE_EQ (segs[2].abs_end.asDouble (), 200.0);

  EXPECT_DOUBLE_EQ (segs[3].abs_start.asDouble (), 200.0);
  EXPECT_DOUBLE_EQ (segs[3].abs_end.asDouble (), 250.0);

  // Virtual range of all loop legs is [50,100)
  for (size_t i = 1; i < segs.size (); ++i)
    {
      EXPECT_DOUBLE_EQ (segs[i].virt_start.asDouble (), 50.0);
      EXPECT_DOUBLE_EQ (segs[i].virt_end.asDouble (), 100.0);
    }
}

TEST (LoopSegmentIteratorTest, LoopedLastIterationClamped)
{
  // clip_start=0, loop=[50,100), clip length=175
  // Intro: abs [0,100)
  // Loop 1: abs [100,150)
  // Loop 2: abs [150,175) — clamped, virt [50,75)
  auto segs =
    collect_segments (mk_tick (0), mk_tick (50), mk_tick (100), mk_tick (175));
  ASSERT_EQ (segs.size (), 3u);

  EXPECT_DOUBLE_EQ (segs[2].abs_start.asDouble (), 150.0);
  EXPECT_DOUBLE_EQ (segs[2].abs_end.asDouble (), 175.0);
  EXPECT_DOUBLE_EQ (segs[2].virt_start.asDouble (), 50.0);
  EXPECT_DOUBLE_EQ (segs[2].virt_end.asDouble (), 75.0);
}

TEST (LoopSegmentIteratorTest, EndTickExactlyAtLoopBoundary)
{
  // clip_start=0, loop=[50,100), clip length=100
  // Intro fills exactly [0,100), no loop iterations needed
  auto segs =
    collect_segments (mk_tick (0), mk_tick (50), mk_tick (100), mk_tick (100));
  ASSERT_EQ (segs.size (), 1u);
}

// ---------------------------------------------------------------------------
// Non-zero clip_start
// ---------------------------------------------------------------------------

TEST (LoopSegmentIteratorTest, NonZeroClipStart)
{
  // clip_start=10, loop=[50,100), clip length=140
  // Intro: virt [10,100) → abs [0,90)
  // Loop 1: virt [50,100) → abs [90,140)
  auto segs =
    collect_segments (mk_tick (10), mk_tick (50), mk_tick (100), mk_tick (140));
  ASSERT_EQ (segs.size (), 2u);

  EXPECT_DOUBLE_EQ (segs[0].virt_start.asDouble (), 10.0);
  EXPECT_DOUBLE_EQ (segs[0].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_end.asDouble (), 90.0);

  EXPECT_DOUBLE_EQ (segs[1].virt_start.asDouble (), 50.0);
  EXPECT_DOUBLE_EQ (segs[1].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_start.asDouble (), 90.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_end.asDouble (), 140.0);
}

// ---------------------------------------------------------------------------
// Loop preview (end_tick > clip length, as during drag)
// ---------------------------------------------------------------------------

TEST (LoopSegmentIteratorTest, LoopPreviewExtendsBeyondClipLength)
{
  // Non-looped clip but preview extends: clip_start=0, loop=[0,100),
  // clip_ticks=100, but end_tick=200 (drag preview)
  // Intro: abs [0,100)
  // Loop 1: abs [100,200)
  auto segs =
    collect_segments (mk_tick (0), mk_tick (0), mk_tick (100), mk_tick (200));
  ASSERT_EQ (segs.size (), 2u);

  EXPECT_DOUBLE_EQ (segs[0].virt_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].abs_end.asDouble (), 100.0);

  EXPECT_DOUBLE_EQ (segs[1].virt_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[1].virt_end.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_start.asDouble (), 100.0);
  EXPECT_DOUBLE_EQ (segs[1].abs_end.asDouble (), 200.0);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST (LoopSegmentIteratorTest, ZeroLengthLoopDoesNotInfLoop)
{
  // loop_start == loop_end → loop_length = 0
  // Should produce intro only then stop
  auto segs =
    collect_segments (mk_tick (0), mk_tick (50), mk_tick (50), mk_tick (100));
  // At least the intro segment; the zero-length loop leg is emitted
  // but the loop breaks immediately because current_len == 0
  EXPECT_GE (segs.size (), 1u);
  EXPECT_DOUBLE_EQ (segs[0].virt_start.asDouble (), 0.0);
  EXPECT_DOUBLE_EQ (segs[0].virt_end.asDouble (), 50.0);
}

TEST (LoopSegmentIteratorTest, EndTickZeroProducesNothing)
{
  auto segs =
    collect_segments (mk_tick (0), mk_tick (0), mk_tick (100), mk_tick (0));
  EXPECT_TRUE (segs.empty ());
}

TEST (LoopSegmentIteratorTest, LoopEndBeforeClipStart)
{
  // Degenerate: loop entirely before clip_start
  // loop_length = max(0, 50-0) = 50
  // Intro: virt [100, 50) → negative, but algorithm still runs
  // Just ensure no crash / infinite loop
  auto segs =
    collect_segments (mk_tick (100), mk_tick (0), mk_tick (50), mk_tick (100));
  // The intro leg has virt_start > virt_end (degenerate), current_len could
  // be negative → loop should break
  EXPECT_NO_FATAL_FAILURE ((void) segs);
}

// ===========================================================================
// compute_loop_boundary_deltas
// ===========================================================================

using TT = dsp::TimelineTick;
using CT = dsp::ContentTick;

auto
mk_ct (double v) -> CT
{
  return CT{ units::ticks (v) };
}

auto
mk_tt (double v) -> TT
{
  return TT{ units::ticks (v) };
}

auto
identity_warp_lookup (std::span<const dsp::ContentTimeWarp::WarpPoint> points)
{
  return [points] (dsp::ContentTick ct) {
    return dsp::warp_lookup (points, ct);
  };
}

// ---------------------------------------------------------------------------
// Identity warp (empty points) → uniform deltas
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, IdentityWarpProducesUniformDeltas)
{
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup ({}), mk_ct (0), mk_ct (0), mk_ct (1920), mk_tt (7680));
  ASSERT_EQ (deltas.size (), 3u);
  EXPECT_NEAR (deltas[0].asDouble (), 1920.0, 0.5);
  EXPECT_NEAR (deltas[1].asDouble (), 3840.0, 0.5);
  EXPECT_NEAR (deltas[2].asDouble (), 5760.0, 0.5);
}

// ---------------------------------------------------------------------------
// Non-linear warp (tempo ramp) → non-uniform deltas
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, NonLinearWarpProducesNonUniformDeltas)
{
  std::vector<dsp::ContentTimeWarp::WarpPoint> points = {
    { mk_ct (0),    mk_tt (0)    },
    { mk_ct (1920), mk_tt (1680) },
    { mk_ct (3840), mk_tt (2880) },
    { mk_ct (5760), mk_tt (3840) },
    { mk_ct (7680), mk_tt (4800) },
  };
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup (points), mk_ct (0), mk_ct (0), mk_ct (1920),
    mk_tt (4800));
  ASSERT_EQ (deltas.size (), 3u);
  EXPECT_NEAR (deltas[0].asDouble (), 1680.0, 0.5);
  EXPECT_NEAR (deltas[1].asDouble (), 2880.0, 0.5);
  EXPECT_NEAR (deltas[2].asDouble (), 3840.0, 0.5);
}

TEST (LoopBoundaryDeltasTest, NonLinearWarpSpacingsAreDecreasing)
{
  std::vector<dsp::ContentTimeWarp::WarpPoint> points = {
    { mk_ct (0),    mk_tt (0)    },
    { mk_ct (1920), mk_tt (1680) },
    { mk_ct (3840), mk_tt (2880) },
    { mk_ct (5760), mk_tt (3840) },
    { mk_ct (7680), mk_tt (4800) },
  };
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup (points), mk_ct (0), mk_ct (0), mk_ct (1920),
    mk_tt (4800));
  ASSERT_GE (deltas.size (), 2u);
  const auto spacing0 = deltas[1].asDouble () - deltas[0].asDouble ();
  const auto spacing1 = deltas[2].asDouble () - deltas[1].asDouble ();
  EXPECT_LT (spacing1, spacing0);
}

// ---------------------------------------------------------------------------
// Non-zero clip_start
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, NonZeroClipStart)
{
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup ({}), mk_ct (480), mk_ct (0), mk_ct (1920),
    mk_tt (5000));
  ASSERT_EQ (deltas.size (), 2u);
  // first boundary at cumulative = loop_end - clip_start = 1440
  EXPECT_NEAR (deltas[0].asDouble (), 1440.0, 0.5);
  EXPECT_NEAR (deltas[1].asDouble (), 3360.0, 0.5);
}

// ---------------------------------------------------------------------------
// max_delta_ticks cutoff
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, MaxDeltaTicksCutoff)
{
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup ({}), mk_ct (0), mk_ct (0), mk_ct (100), mk_tt (250));
  ASSERT_EQ (deltas.size (), 2u);
  EXPECT_NEAR (deltas[0].asDouble (), 100.0, 0.5);
  EXPECT_NEAR (deltas[1].asDouble (), 200.0, 0.5);
}

// ---------------------------------------------------------------------------
// Zero loop length
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, ZeroLoopLengthReturnsEmpty)
{
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup ({}), mk_ct (0), mk_ct (100), mk_ct (100), mk_tt (500));
  EXPECT_TRUE (deltas.empty ());
}

// ---------------------------------------------------------------------------
// max_iterations safety limit
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, MaxIterationsLimit)
{
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup ({}), mk_ct (0), mk_ct (0), mk_ct (1), mk_tt (1e9), 5);
  ASSERT_EQ (deltas.size (), 5u);
  EXPECT_NEAR (deltas[0].asDouble (), 1.0, 0.5);
  EXPECT_NEAR (deltas[4].asDouble (), 5.0, 0.5);
}

// ---------------------------------------------------------------------------
// Single iteration
// ---------------------------------------------------------------------------

TEST (LoopBoundaryDeltasTest, SingleIteration)
{
  auto deltas = compute_loop_boundary_deltas (
    identity_warp_lookup ({}), mk_ct (0), mk_ct (0), mk_ct (100), mk_tt (150));
  ASSERT_EQ (deltas.size (), 1u);
  EXPECT_NEAR (deltas[0].asDouble (), 100.0, 0.5);
}

} // namespace zrythm::structure::arrangement
