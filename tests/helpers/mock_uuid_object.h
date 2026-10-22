// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "utils/uuid_identifiable.h"

#include <QVariant>

#include "helpers/mock_qobject.h"

/// Mock UUID-identifiable QObject for testing.
///
/// Inherits @ref UuidIdentifiableBase so it can be registered in an
/// @ref IObjectRegistry and targeted by UUID-safe commands. Uses dynamic
/// properties (no Q_OBJECT) — @c QObject::property() / @c setProperty()
/// work through the meta-object inherited from the base.
class MockUuidObject : public zrythm::utils::UuidIdentifiableBase
{
public:
  explicit MockUuidObject (QObject * parent = nullptr)
      : UuidIdentifiableBase (parent)
  {
    setProperty ("intValue", 42);
  }
};
