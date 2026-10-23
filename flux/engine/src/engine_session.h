// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "actions/arranger_object_creator.h"
#include "actions/file_importer.h"
#include "actions/plugin_importer.h"
#include "actions/track_creator.h"
#include "plugins/plugin_descriptor.h"
#include "structure/project/project.h"
#include "structure/project/project_ui_state.h"
#include "undo/undo_stack.h"

#include "engine_host.h"

namespace flux::engine
{

/**
 * @brief One open project with its undo history and edit operators.
 *
 * The headless counterpart of the GUI's ProjectSession, without QML or
 * plugin UI. Every edit goes through the UndoStack, which pauses engine
 * processing while a command runs. Main thread only.
 */
class EngineSession
{
public:
  struct TimeSignature
  {
    int numerator = 4;
    int denominator = 4;
  };

  /**
   * @brief Creates a project with the default tracks and saves it to @p dir.
   *
   * @throw std::exception if @p dir exists and is not empty, or saving fails.
   */
  static std::unique_ptr<EngineSession> create (
    EngineHost                   &host,
    const std::filesystem::path  &dir,
    std::optional<double>         bpm,
    std::optional<TimeSignature>  time_signature);

  /**
   * @brief Loads the project in @p dir and waits for its plugins to
   * instantiate.
   *
   * @throw std::exception if loading fails.
   */
  static std::unique_ptr<EngineSession>
  open (EngineHost &host, const std::filesystem::path &dir);

  ~EngineSession ();
  EngineSession (const EngineSession &) = delete;
  EngineSession &operator= (const EngineSession &) = delete;

  /**
   * @brief Saves to the current directory, or to @p dir ("save as"), which
   * then becomes the current directory.
   *
   * @return The directory saved to.
   * @throw std::exception on failure.
   */
  std::filesystem::path save (std::optional<std::filesystem::path> dir = {});

  /**
   * @brief Instantiates @p descriptor and adds it to @p track (or to a new
   * track of a matching type if @p track is null), waiting for it.
   *
   * @return The new plugin.
   * @throw std::runtime_error with the plugin's error if instantiation
   * fails or times out.
   */
  zrythm::plugins::Plugin * import_plugin (
    const zrythm::plugins::PluginDescriptor &descriptor,
    zrythm::structure::tracks::Track *       track);

  /**
   * @brief Waits until no plugin in the project is still instantiating.
   */
  bool wait_for_plugins (std::chrono::milliseconds timeout);

  zrythm::structure::project::Project &project () { return *project_; }
  zrythm::undo::UndoStack             &undo_stack () { return *undo_stack_; }
  zrythm::structure::project::ProjectUiState &ui_state () { return *ui_state_; }
  zrythm::actions::TrackCreator              &track_creator ()
  {
    return *track_creator_;
  }
  zrythm::actions::ArrangerObjectCreator &arranger_object_creator ()
  {
    return *arranger_object_creator_;
  }
  zrythm::actions::FileImporter &file_importer () { return *file_importer_; }

  const std::filesystem::path &directory () const { return *dir_; }
  QString                      title () const;

private:
  EngineSession (EngineHost &host, const std::filesystem::path &dir);
  void make_operators ();
  void start_engine ();

  EngineHost                                                &host_;
  std::shared_ptr<std::filesystem::path>                     dir_;
  std::unique_ptr<zrythm::structure::project::Project>       project_;
  zrythm::utils::QObjectUniquePtr<zrythm::structure::project::ProjectUiState>
                                                             ui_state_;
  zrythm::utils::QObjectUniquePtr<zrythm::undo::UndoStack>   undo_stack_;
  zrythm::utils::QObjectUniquePtr<zrythm::actions::TrackCreator> track_creator_;
  zrythm::utils::QObjectUniquePtr<zrythm::actions::ArrangerObjectCreator>
    arranger_object_creator_;
  zrythm::utils::QObjectUniquePtr<zrythm::actions::PluginImporter>
    plugin_importer_;
  zrythm::utils::QObjectUniquePtr<zrythm::actions::FileImporter>
    file_importer_;

  // State of the plugin import in progress (import_plugin() is synchronous)
  struct PendingImport
  {
    bool                     finished = false;
    zrythm::plugins::Plugin * plugin = nullptr;
    QString                  error;
  };
  std::shared_ptr<PendingImport> pending_import_;
};

} // namespace flux::engine
