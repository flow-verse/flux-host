// SPDX-FileCopyrightText: © 2019-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/arrangement/arranger_object_all.h"
#include "structure/arrangement/chord_clip.h"

namespace zrythm::structure::arrangement
{
ChordClip::ChordClip (
  const dsp::TempoMapWrapper &tempo_map_wrapper,
  utils::IObjectRegistry     &object_registry,
  QObject *                   parent)
    : Clip (Type::ChordClip, tempo_map_wrapper, parent),
      ArrangerObjectOwner (object_registry, *this)
{
  QObject::connect (
    ArrangerObjectOwner<ChordObject>::get_model (),
    &ArrangerObjectListModel::contentChanged, this, &Clip::contentChanged);
}

void
ChordClip::shift_all_children (dsp::ContentTick delta)
{
  ArrangerObjectOwner<ChordObject>::add_ticks_to_children (delta);
}

std::optional<dsp::ContentTick>
ChordClip::first_child_position () const
{
  const auto &children =
    ArrangerObjectOwner<ChordObject>::get_children_vector ();
  if (!children.empty ())
    return children.front ().get_object_as<ChordObject> ()->position ()->asTick ();
  return std::nullopt;
}

void
init_from (
  ChordClip             &obj,
  const ChordClip       &other,
  utils::ObjectCloneType clone_type)
{
  init_from (
    static_cast<Clip &> (obj), static_cast<const Clip &> (other), clone_type);
  init_from (
    static_cast<ArrangerObjectOwner<ChordObject> &> (obj),
    static_cast<const ArrangerObjectOwner<ChordObject> &> (other), clone_type);
}
}
