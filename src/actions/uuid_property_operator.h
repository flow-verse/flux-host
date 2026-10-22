// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "undo/undo_stack.h"
#include "utils/iobject_registry.h"

#include <QtQmlIntegration/qqmlintegration.h>

namespace zrythm::actions
{

/**
 * @brief QML-accessible operator for undoable property changes on
 * UUID-identifiable objects.
 *
 * Unlike @ref QObjectPropertyOperator, this operator creates
 * @ref commands::ChangeUuidIdentifiableObjectPropertyCommand instances that
 * hold a UUID reference keeping the target alive on the undo stack. This makes
 * it safe for objects that can be deleted by other commands (e.g. automation
 * points removed via undo of point creation).
 *
 * The registry is injected at construction time (C++ side) and never exposed
 * to QML.
 */
class UuidPropertyOperator : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  explicit UuidPropertyOperator (
    undo::UndoStack        &undo_stack,
    utils::IObjectRegistry &registry,
    QObject *               parent = nullptr)
      : QObject (parent), undo_stack_ (&undo_stack), registry_ (&registry)
  {
  }

  /**
   * @brief Commits a property change on a UUID-identifiable QObject.
   *
   * The object is kept alive on the undo stack via UUID reference.
   *
   * @param uuidObject The UUID-identifiable QObject whose property to change.
   * @param propertyName The Q_PROPERTY name.
   * @param value The new value.
   */
  Q_INVOKABLE void
  setValue (QObject * uuidObject, QString propertyName, QVariant value);

  /**
   * @brief Commits a property change on a sub-object of a UUID-identifiable
   * object.
   *
   * The UUID-identifiable parent is kept alive, which transitively keeps the
   * target alive. Use this when the property lives on a child QObject (e.g.
   * a CurveOptionsQmlAdapter inside an AutomationPoint).
   *
   * @param uuidLifecycleObject The UUID-identifiable parent whose lifetime
   *   guarantees @p target's lifetime.
   * @param target The QObject whose property is changed.
   * @param propertyName The Q_PROPERTY name.
   * @param value The new value.
   */
  Q_INVOKABLE void setValueOnSubObject (
    QObject * uuidLifecycleObject,
    QObject * target,
    QString   propertyName,
    QVariant  value);

private:
  undo::UndoStack *        undo_stack_;
  utils::IObjectRegistry * registry_;
};

} // namespace zrythm::actions
