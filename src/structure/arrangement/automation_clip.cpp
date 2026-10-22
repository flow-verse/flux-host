// SPDX-FileCopyrightText: © 2019-2022, 2024-2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/arrangement/arranger_object_all.h"
#include "structure/arrangement/automation_clip.h"

#include <nlohmann/json.hpp>

namespace zrythm::structure::arrangement
{
AutomationClip::AutomationClip (
  const dsp::TempoMapWrapper &tempo_map_wrapper,
  utils::IObjectRegistry     &registry,
  QObject *                   parent)
    : Clip (Type::AutomationClip, tempo_map_wrapper, parent),
      ArrangerObjectOwner (registry, *this)
{
  QObject::connect (
    ArrangerObjectOwner<AutomationPoint>::get_model (),
    &ArrangerObjectListModel::contentChanged, this, &Clip::contentChanged);
}

double
AutomationClip::get_normalized_value_in_curve (
  const AutomationPoint &ap,
  double                 x) const
{
  assert (x >= 0.0 && x <= 1.0);

  AutomationPoint * next_ap = get_next_ap (ap, true);
  if (next_ap == nullptr)
    {
      return ap.value ();
    }

  double dy;

  bool start_higher = next_ap->value () < ap.value ();
  dy = ap.curveOpts ()->normalizedY (x, start_higher);
  return dy;
}

std::pair<float, AutomationPoint *>
AutomationClip::get_value_at_virt_tick (dsp::ContentTick virt_tick) const
{
  const auto sorted_points =
    ArrangerObjectOwner<AutomationPoint>::get_sorted_children_view ();
  if (sorted_points.empty ())
    return { 0.0f, nullptr };

  const auto tick_of = [] (const AutomationPoint * ap) {
    return ap->position ()->asTick ();
  };

  const auto first_tick = tick_of (sorted_points.front ());
  const auto last_tick = tick_of (sorted_points.back ());

  if (virt_tick <= first_tick)
    return { sorted_points.front ()->value (), sorted_points.front () };
  if (virt_tick >= last_tick)
    return { sorted_points.back ()->value (), sorted_points.back () };

  auto it = std::ranges::lower_bound (sorted_points, virt_tick, {}, tick_of);
  if (it == sorted_points.begin () || it == sorted_points.end ())
    return { sorted_points.back ()->value (), sorted_points.back () };

  auto * prev_ap = *std::ranges::prev (it);
  auto * next_ap = *it;

  const auto prev_tick = tick_of (prev_ap);
  const auto next_tick = tick_of (next_ap);
  if (next_tick <= prev_tick)
    return { prev_ap->value (), prev_ap };

  double t = (virt_tick - prev_tick) / (next_tick - prev_tick);
  t = std::clamp (t, 0.0, 1.0);

  const float val = dsp::evaluate_curve (
    prev_ap->value (), next_ap->value (), prev_ap->curveOpts ()->algorithm (),
    static_cast<float> (prev_ap->curveOpts ()->curviness ()), t);
  return { val, prev_ap };
}

AutomationPoint *
AutomationClip::get_prev_ap (const AutomationPoint &ap) const
{
  auto it = std::ranges::find (
    get_children_vector (), ap.get_uuid (), &ArrangerObjectUuidReference::id);

  // if found and not the first element
  if (
    it != get_children_vector ().end () && it != get_children_vector ().begin ())
    {
      return (*std::ranges::prev (it)).template get_object_as<AutomationPoint> ();
    }

  return nullptr;
}

AutomationPoint *
AutomationClip::get_next_ap (const AutomationPoint &ap, bool check_positions) const
{
  if (check_positions)
    {
      AutomationPoint * next_ap = nullptr;
      for (auto * cur_ap_outer : get_children_view ())
        {
          AutomationPoint * cur_ap = cur_ap_outer;

          if (cur_ap->get_uuid () == ap.get_uuid ())
            continue;

          if (
            cur_ap->position ()->ticks() >= ap.position ()->ticks()
            && ((next_ap == nullptr) || cur_ap->position ()->ticks() < next_ap->position ()->ticks()))
            {
              next_ap = cur_ap;
            }
        }
      return next_ap;
    }

  auto it = std::ranges::find (
    get_children_vector (), ap.get_uuid (), &ArrangerObjectUuidReference::id);

  // if found and not the last element
  if (
    it != get_children_vector ().end ()
    && std::ranges::next (it) != get_children_vector ().end ())
    {

      return (*std::ranges::next (it)).template get_object_as<AutomationPoint> ();
    }

  return nullptr;
}

void
AutomationClip::shift_all_children (dsp::ContentTick delta)
{
  ArrangerObjectOwner<AutomationPoint>::add_ticks_to_children (delta);
}

std::optional<dsp::ContentTick>
AutomationClip::first_child_position () const
{
  const auto &children =
    ArrangerObjectOwner<AutomationPoint>::get_children_vector ();
  if (!children.empty ())
    return children.front ()
      .get_object_as<AutomationPoint> ()
      ->position ()
      ->asTick ();
  return std::nullopt;
}

void
init_from (
  AutomationClip        &obj,
  const AutomationClip  &other,
  utils::ObjectCloneType clone_type)
{
  init_from (
    static_cast<Clip &> (obj), static_cast<const Clip &> (other), clone_type);
  init_from (
    static_cast<ArrangerObjectOwner<AutomationPoint> &> (obj),
    static_cast<const ArrangerObjectOwner<AutomationPoint> &> (other),
    clone_type);
}

void
to_json (nlohmann::json &j, const AutomationClip &clip)
{
  to_json (j, static_cast<const Clip &> (clip));
  to_json (j, static_cast<const ArrangerObjectOwner<AutomationPoint> &> (clip));
}

void
from_json (const nlohmann::json &j, AutomationClip &clip)
{
  from_json (j, static_cast<Clip &> (clip));
  from_json (j, static_cast<ArrangerObjectOwner<AutomationPoint> &> (clip));
}
}
