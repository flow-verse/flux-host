// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/tick_types.h"
#include "structure/arrangement/arranger_object_all.h"
#include "structure/arrangement/loop_segment_iterator.h"
#include "utils/audio.h"
#include "utils/units.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace zrythm::structure::arrangement
{

/**
 * @brief A class that converts clip data to serialized formats.
 *
 * This class provides methods to serialize both MIDI and Audio clips,
 * handling looped playback and various constraints.
 */
class ClipRenderer final
{
public:
  using TimelineRange = std::pair<dsp::TimelineTick, dsp::TimelineTick>;

  /**
   * @brief Serializes a MIDI clip to a MIDI message sequence.
   *
   * Event timestamps are in ticks relative to the clip start (tick 0 = clip
   * start). Looping and clip-start offsets are applied. Callers that need
   * absolute timeline positions should add @c clip.position()->ticks() to
   * each event timestamp (e.g. via @c events.addTimeToMessages()).
   *
   * @param clip The MIDI clip to serialize.
   * @param events Output MIDI message sequence.
   * @param timeline_range_ticks Currently unused.
   */
  static void serialize_to_sequence (
    const MidiClip              &clip,
    juce::MidiMessageSequence   &events,
    std::optional<TimelineRange> timeline_range_ticks = std::nullopt);

  /**
   * @brief Serializes a Chord clip to a MIDI message sequence.
   *
   * Event timestamps are in ticks relative to the clip start (tick 0 = clip
   * start). See the MidiClip overload for details.
   *
   * @param clip The Chord clip to serialize.
   * @param events Output MIDI message sequence.
   * @param timeline_range_ticks Currently unused.
   */
  static void serialize_to_sequence (
    const ChordClip             &clip,
    juce::MidiMessageSequence   &events,
    std::optional<TimelineRange> timeline_range_ticks = std::nullopt);

  /**
   * @brief Serializes an Audio clip to an audio sample buffer.
   *
   * The buffer represents the clip's audio as played back with looping and
   * clip-start applied. Buffer index 0 corresponds to the clip start.
   *
   * @param clip The Audio clip to serialize.
   * @param buffer Output audio sample buffer.
   * @param timeline_range_ticks Currently unused.
   */
  static void serialize_to_buffer (
    const AudioClip             &clip,
    juce::AudioSampleBuffer     &buffer,
    std::optional<TimelineRange> timeline_range_ticks = std::nullopt);

  /**
   * @brief A single control point in a rendered automation curve.
   *
   * Positions are timeline ticks relative to the clip start (tick 0 = clip
   * start), matching how @c serialize_to_sequence produces tick timestamps.
   * Curve parameters come from the AutomationPoint whose curve segment drives
   * the region starting at this point.
   *
   * The @c curve_origin_* fields carry the original AP-pair domain so that
   * when a boundary point splits an AP pair (e.g. clip-start or loop-wrap),
   * the consumer can evaluate the curve at the correct ratio within the
   * original domain — not the truncated [boundary, next-AP] span.
   */
  struct RenderedAutomationPoint
  {
    dsp::TimelineTick            position;
    float                        value;
    dsp::CurveOptions::Algorithm curve_algo;
    float                        curve_curviness;

    /// The AP pair that drives the curve starting at this point.
    /// For actual APs: origin_start == position, value_a == value.
    /// For boundary interpolations: spans the full original AP-to-AP range.
    dsp::TimelineTick curve_origin_start;
    dsp::TimelineTick curve_origin_end;
    float             curve_origin_value_a;
    float             curve_origin_value_b;
  };

  /**
   * @brief Serializes an AutomationClip to a list of control points in
   * timeline-tick space (relative to clip start).
   *
   * Uses the same @c serialize_clip loop walk as MIDI/Audio/Chord. When a
   * loop iteration boundary falls between two automation points, an
   * interpolated control point is inserted so the curve resumes correctly.
   *
   * Points are sorted by position. Callers that need absolute timeline
   * positions should add @c clip.position()->asTick() to each point's
   * position (cf. @c MidiMessageSequence::addTimeToMessages).
   *
   * @param clip The automation clip to serialize.
   * @param points Output vector of control points.
   * @param timeline_range_ticks Currently unused.
   */
  static void serialize_to_points (
    const AutomationClip                 &clip,
    std::vector<RenderedAutomationPoint> &points,
    std::optional<TimelineRange>          timeline_range_ticks = std::nullopt)
    [[clang::blocking]];

private:
  /**
   * @brief Common loop parameters extracted from a clip.
   */
  struct LoopParameters
  {
    dsp::ContentTick loop_start;
    dsp::ContentTick loop_end;
    dsp::ContentTick clip_start;
    dsp::ContentTick loop_length;
    dsp::ContentTick clip_length;
    int              num_loops;

    LoopParameters (const Clip &clip);
  };

  template <ClipObject ClipT, typename EventsT>
  static void serialize_clip (
    const ClipT &clip,
    EventsT     &events,
    // currently unused
    std::optional<TimelineRange> timeline_range_ticks = std::nullopt)
  {
    const auto clip_length = clip.length ()->asTick ();

    for_each_loop_segment (
      clip.clipStartPosition ()->asTick (),
      clip.loopStartPosition ()->asTick (), clip.loopEndPosition ()->asTick (),
      clip_length, [&] (const LoopSegment &segment) {
        if constexpr (std::is_same_v<ClipT, MidiClip>)
          handle_midi_clip_range (clip, events, segment);
        else if constexpr (std::is_same_v<ClipT, ChordClip>)
          handle_chord_clip_range (clip, events, segment);
        else if constexpr (std::is_same_v<ClipT, AutomationClip>)
          handle_automation_clip_range (clip, events, segment);
      });
  }

  static void handle_midi_clip_range (
    const MidiClip            &clip,
    juce::MidiMessageSequence &events,
    const LoopSegment         &segment);

  static void handle_chord_clip_range (
    const ChordClip           &clip,
    juce::MidiMessageSequence &events,
    const LoopSegment         &segment);

  static void handle_automation_clip_range (
    const AutomationClip                 &clip,
    std::vector<RenderedAutomationPoint> &points,
    const LoopSegment                    &segment);

  /**
   * @brief Reads the clip's content for an output sample range, in playback
   * (loop) order at the clip's native sample rate.
   *
   * @p out_start and @p out_end are output positions in native samples relative
   * to the clip start. The clip read position for @p out_start is computed
   * directly in O(1) (modular arithmetic on the loop length), so requesting a
   * sub-range never touches samples before @p out_start. This keeps
   * incremental/range renders (e.g. per-frame recording waveform updates)
   * O(range) instead of O(clip).
   */
  static utils::audio::AudioBuffer build_native_looped_buffer (
    const AudioClip &clip,
    units::sample_t  out_start,
    units::sample_t  out_end);

  /**
   * Applies gain to the entire audio buffer as a separate pass.
   */
  static void
  apply_gain_pass (const AudioClip &clip, juce::AudioSampleBuffer &buffer);

  /**
   * Applies clip (object) fades to the audio buffer as a separate pass.
   */
  static void
  apply_clip_fades_pass (const AudioClip &clip, juce::AudioSampleBuffer &buffer);

  /**
   * Applies built-in fades to the audio buffer as a separate pass.
   */
  static void apply_builtin_fades_pass (
    const AudioClip         &clip,
    juce::AudioSampleBuffer &buffer,
    int                      builtin_fade_frames);
};

} // namespace zrythm::structure::arrangement
