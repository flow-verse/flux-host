// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "commands/set_clip_loop_points_command.h"
#include "dsp/tick_types.h"
#include "structure/arrangement/clip.h"
#include "undo/undo_stack.h"
#include "utils/iobject_registry.h"

#include <QtQmlIntegration/qqmlintegration.h>

namespace zrythm::actions
{

/**
 * @brief QML-facing operator for edits on a single clip (the clip-editor
 * scope), mirroring the @c ArrangerObjectCreator pattern: one instance per
 * project, owned by @c ProjectSession, with its dependencies (object registry
 * and undo stack) injected in C++.
 *
 * Loop-point dragging uses a preview-then-commit lifecycle: @ref
 * updateClipLoopPointsDrag mutates the clip directly for live visual feedback,
 * and @ref endClipLoopPointsDrag pushes a single immutable
 * @c SetClipLoopPointsCommand capturing the before/after states. An entire drag
 * is therefore one undo entry, and no command is ever mutated once it is on the
 * stack. @ref abortClipLoopPointsDrag reverts the live preview without pushing.
 */
class ClipOperator : public QObject
{
  Q_OBJECT
  Q_PROPERTY (
    zrythm::structure::arrangement::Clip * clip READ clip WRITE setClip NOTIFY
      clipChanged)
  QML_ELEMENT
  QML_UNCREATABLE ("One instance per project")

public:
  ClipOperator (
    utils::IObjectRegistry &registry,
    undo::UndoStack        &undoStack,
    QObject *               parent = nullptr);

  structure::arrangement::Clip * clip () const { return clip_; }
  void                           setClip (structure::arrangement::Clip * clip);
  Q_SIGNAL void                  clipChanged ();

  /// Begin a loop-points drag gesture. Captures the pre-drag state; the command
  /// is pushed only on @ref endClipLoopPointsDrag.
  Q_INVOKABLE void beginClipLoopPointsDrag ();

  /// Live-update the clip's loop points to the given content-tick positions for
  /// visual feedback. The caller passes the pre-drag values for axes not being
  /// dragged.
  Q_INVOKABLE void updateClipLoopPointsDrag (
    double clipStartTicks,
    double loopStartTicks,
    double loopEndTicks);

  /// Commit the drag: push one @c SetClipLoopPointsCommand capturing the
  /// before/after states. A no-op gesture (no net change) pushes nothing.
  Q_INVOKABLE void endClipLoopPointsDrag ();

  /// Revert the live preview to the pre-drag state without pushing a command.
  Q_INVOKABLE void abortClipLoopPointsDrag ();

private:
  utils::IObjectRegistry                      &registry_;
  undo::UndoStack                             &undo_stack_;
  structure::arrangement::Clip *               clip_{};
  bool                                         drag_active_{ false };
  commands::SetClipLoopPointsCommand::Snapshot drag_before_;
};

} // namespace zrythm::actions
