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

} // namespace flux::engine
