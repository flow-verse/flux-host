// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string>

#include "plugins/plugin.h"

#include <QPointer>
#include <QUndoCommand>

namespace flux::engine
{

/**
 * @brief Loads a plugin preset as one undo step.
 *
 * Upstream applies presets directly (Plugin::setPresetIndex, not undoable).
 * Undo restores the plugin state saved before the load, so an agent turn
 * that loaded a preset reverts like any other edit.
 */
class LoadPresetCommand final : public QUndoCommand
{
public:
  LoadPresetCommand (
    zrythm::plugins::Plugin &plugin,
    int                      index,
    const QString           &preset_name)
      : QUndoCommand (QObject::tr ("Load preset %1").arg (preset_name)),
        plugin_ (&plugin), index_ (index),
        previous_index_ (plugin.presetIndex ()),
        previous_state_ (plugin.save_state ())
  {
  }

  void redo () override
  {
    if (plugin_ != nullptr)
      {
        plugin_->setPresetIndex (index_);
        applied_ = plugin_->presetIndex () == index_;
      }
  }

  void undo () override
  {
    if (plugin_ == nullptr)
      return;
    plugin_->load_state (previous_state_);
    // Restores host-side selection only; the state above holds the sound
    if (previous_index_ != plugin_->presetIndex ())
      plugin_->setPresetIndex (-1);
  }

  /** Whether the last redo() applied the preset. */
  bool applied () const { return applied_; }

private:
  QPointer<zrythm::plugins::Plugin> plugin_;
  int                               index_;
  int                               previous_index_;
  std::string                       previous_state_;
  bool                              applied_ = false;
};

/**
 * @brief Loads a preset by its durable id (a file path encoded as a preset
 * location) as one undo step; undo restores the plugin state saved before.
 */
class LoadPresetIdCommand final : public QUndoCommand
{
public:
  LoadPresetIdCommand (
    zrythm::plugins::Plugin           &plugin,
    zrythm::plugins::Plugin::PresetId  id,
    const QString                     &label)
      : QUndoCommand (QObject::tr ("Load preset %1").arg (label)),
        plugin_ (&plugin), id_ (std::move (id)),
        previous_index_ (plugin.presetIndex ()),
        previous_state_ (plugin.save_state ())
  {
  }

  void redo () override
  {
    if (plugin_ != nullptr)
      applied_ = plugin_->apply_preset_id (id_);
  }

  void undo () override
  {
    if (plugin_ == nullptr)
      return;
    plugin_->load_state (previous_state_);
    if (previous_index_ != plugin_->presetIndex ())
      plugin_->setPresetIndex (previous_index_);
  }

  bool applied () const { return applied_; }

private:
  QPointer<zrythm::plugins::Plugin> plugin_;
  zrythm::plugins::Plugin::PresetId id_;
  int                               previous_index_;
  std::string                       previous_state_;
  bool                              applied_ = false;
};

} // namespace flux::engine
