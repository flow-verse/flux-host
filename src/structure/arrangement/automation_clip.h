// SPDX-FileCopyrightText: © 2019-2022, 2024-2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <utility>

#include "dsp/tick_types.h"
#include "structure/arrangement/arranger_object_owner.h"
#include "structure/arrangement/automation_point.h"
#include "structure/arrangement/clip.h"

namespace zrythm::structure::arrangement
{
/**
 * Represents an automation clip, which contains a collection of automation
 * points. The automation points must always stay sorted by position. The last
 * recorded automation point is also stored.
 */
class AutomationClip final
    : public Clip,
      public ArrangerObjectOwner<AutomationPoint>
{
  Q_OBJECT
  DEFINE_ARRANGER_OBJECT_OWNER_QML_PROPERTIES (
    AutomationClip,
    automationPoints,
    AutomationPoint)
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  AutomationClip (
    const dsp::TempoMapWrapper &tempo_map_wrapper,
    utils::IObjectRegistry     &registry,
    QObject *                   parent = nullptr);

  // ========================================================================
  // QML Interface
  // ========================================================================

  // ========================================================================

  /**
   * Returns the AutomationPoint before the given one.
   */
  AutomationPoint * get_prev_ap (const AutomationPoint &ap) const;

  /**
   * Returns the AutomationPoint after the given one.
   *
   * @param check_positions Compare positions instead of just getting the next
   * index.
   */
  [[gnu::hot]] AutomationPoint *
  get_next_ap (const AutomationPoint &ap, bool check_positions) const;

  /**
   * The function to return a point on the curve.
   *
   * See
   * https://stackoverflow.com/questions/17623152/how-map-tween-a-number-based-on-a-dynamic-curve
   *
   * @param x Normalized x.
   */
  [[gnu::hot]] double
  get_normalized_value_in_curve (const AutomationPoint &ap, double x) const;

  /**
   * @brief Evaluates the automation curve at a content-tick position.
   *
   * Finds the segment containing @p virt_tick and samples it with
   * @ref dsp::evaluate_curve using the curve options of the segment's start
   * point.
   *
   * @return The normalized value at @p virt_tick and the AutomationPoint whose
   *   curve segment governs that position (nullptr if there are no points).
   */
  std::pair<float, AutomationPoint *>
  get_value_at_virt_tick (dsp::ContentTick virt_tick) const;

  std::string
  get_field_name_for_serialization (const AutomationPoint *) const override
  {
    return "automationPoints";
  }

  std::vector<ArrangerObjectListModel *> get_child_list_models () const override
  {
    return { ArrangerObjectOwner<AutomationPoint>::get_model () };
  }

  void shift_all_children (dsp::ContentTick delta) override;

  std::optional<dsp::ContentTick> first_child_position () const override;

private:
  friend void init_from (
    AutomationClip        &obj,
    const AutomationClip  &other,
    utils::ObjectCloneType clone_type);

  friend void to_json (nlohmann::json &j, const AutomationClip &clip);
  friend void from_json (const nlohmann::json &j, AutomationClip &clip);

private:
  BOOST_DESCRIBE_CLASS (
    AutomationClip,
    (Clip, ArrangerObjectOwner<AutomationPoint>),
    (),
    (),
    ())
};
}
