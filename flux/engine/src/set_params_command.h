// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <vector>

#include "dsp/parameter.h"

#include <QPointer>
#include <QUndoCommand>

namespace flux::engine
{

/**
 * @brief Sets several parameters' normalized base values as one undo step.
 *
 * Unlike upstream's ChangeParameterValueCommand it never merges with other
 * commands, so values set in quick succession (as an agent does) keep their
 * own undo entries and restore the right parameters.
 */
class SetParamsCommand final : public QUndoCommand
{
public:
  struct Change
  {
    QPointer<zrythm::dsp::ProcessorParameter> param;
    float                                     before{};
    float                                     after{};
  };

  explicit SetParamsCommand (std::vector<Change> changes)
      : QUndoCommand (QObject::tr ("Set %n parameter(s)", nullptr, static_cast<int> (changes.size ()))),
        changes_ (std::move (changes))
  {
  }

  void redo () override
  {
    for (auto &c : changes_)
      {
        if (c.param != nullptr)
          c.param->setBaseValueByUser (c.after);
      }
  }

  void undo () override
  {
    for (auto &c : changes_)
      {
        if (c.param != nullptr)
          c.param->setBaseValueByUser (c.before);
      }
  }

private:
  std::vector<Change> changes_;
};

} // namespace flux::engine
