// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "engine_session.h"

#include <stdexcept>

#include "controllers/project_loader.h"
#include "controllers/project_saver.h"
#include "utils/version.h"

#include <fmt/format.h>

namespace flux::engine
{

using namespace zrythm;
using namespace std::chrono_literals;

namespace
{
constexpr auto kPluginTimeout = 60s;
constexpr auto kSaveTimeout = 120s;
}

EngineSession::EngineSession (EngineHost &host, const std::filesystem::path &dir)
    : host_ (host), dir_ (std::make_shared<std::filesystem::path> (dir))
{
  project_ = host_.make_project ([dir = dir_] (bool for_backup) {
    return for_backup ? *dir / "backups" : *dir;
  });
  ui_state_ = utils::make_qobject_unique<structure::project::ProjectUiState> (
    *project_, host_.app_settings ());
  undo_stack_ = utils::make_qobject_unique<undo::UndoStack> (
    [proj = project_.get ()] (
      const std::function<void ()> &action, bool recalculate_graph) {
      proj->engine ()->execute_function_with_paused_processing_synchronously (
        action, recalculate_graph);
    },
    nullptr);
}

void
EngineSession::make_operators ()
{
  auto * tracklist = project_->tracklist ();
  track_creator_ = utils::make_qobject_unique<actions::TrackCreator> (
    *undo_stack_, *project_->track_factory_, *tracklist->collection (),
    *tracklist->trackRouting (), *tracklist->singletonTracks ());
  arranger_object_creator_ =
    utils::make_qobject_unique<actions::ArrangerObjectCreator> (
      *undo_stack_, *project_->arrangerObjectFactory (),
      *ui_state_->snapGridTimeline (), *ui_state_->snapGridEditor ());
  plugin_importer_ = utils::make_qobject_unique<actions::PluginImporter> (
    *undo_stack_, *project_->plugin_factory_, *track_creator_,
    [this] (plugins::PluginUuidReference plugin_ref) {
      if (pending_import_)
        {
          pending_import_->plugin = plugin_ref.get ();
          pending_import_->finished = true;
        }
    });
  QObject::connect (
    plugin_importer_.get (), &actions::PluginImporter::instantiationFailed,
    plugin_importer_.get (), [this] (const QString &name, const QString &error) {
      if (pending_import_)
        {
          pending_import_->error =
            QStringLiteral ("%1: %2").arg (name, error);
          pending_import_->finished = true;
        }
    });
  file_importer_ = utils::make_qobject_unique<actions::FileImporter> (
    *undo_stack_, *arranger_object_creator_, *track_creator_,
    *tracklist->collection (), project_->tempo_map ());
}

void
EngineSession::start_engine ()
{
  // Builds and prepares the graph. The engine never runs live: the null
  // audio device has no audio thread, so nothing here processes audio
  project_->engine ()->activate ();
}

std::unique_ptr<EngineSession>
EngineSession::create (
  EngineHost                  &host,
  const std::filesystem::path &dir,
  std::optional<double>        bpm,
  std::optional<TimeSignature> time_signature)
{
  if (
    std::filesystem::exists (dir)
    && (!std::filesystem::is_directory (dir) || !std::filesystem::is_empty (dir)))
    {
      throw std::invalid_argument (fmt::format (
        "'{}' already exists and is not an empty directory", dir.string ()));
    }
  if (bpm && (*bpm < 20.0 || *bpm > 999.0))
    throw std::invalid_argument ("bpm must be between 20 and 999");
  if (time_signature)
    {
      dsp::throw_if_invalid_time_signature (
        { time_signature->numerator, time_signature->denominator });
    }

  auto session = std::unique_ptr<EngineSession> (new EngineSession (host, dir));
  auto &project = *session->project_;
  project.add_default_tracks ();
  {
    // Initial state, not an edit: set the map's base values directly
    auto * tempo_map = project.getTempoMap ();
    if (bpm)
      tempo_map->setBaseBpm (*bpm);
    if (time_signature)
      {
        tempo_map->setBaseTimeSignatureNumerator (time_signature->numerator);
        tempo_map->setBaseTimeSignatureDenominator (
          time_signature->denominator);
      }
  }
  session->make_operators ();
  session->start_engine ();
  session->save ();
  return session;
}

std::unique_ptr<EngineSession>
EngineSession::open (EngineHost &host, const std::filesystem::path &dir)
{
  if (!std::filesystem::is_directory (dir))
    {
      throw std::invalid_argument (
        fmt::format ("project directory does not exist: {}", dir.string ()));
    }
  const auto json_str =
    controllers::ProjectLoader::get_uncompressed_project_text (dir);
  const auto json = controllers::ProjectLoader::parse_and_validate (json_str);

  auto session = std::unique_ptr<EngineSession> (new EngineSession (host, dir));
  controllers::ProjectLoader::deserialize (
    json, *session->project_, *session->ui_state_, *session->undo_stack_);
  session->make_operators ();
  session->start_engine ();
  if (!session->wait_for_plugins (kPluginTimeout))
    throw std::runtime_error ("timed out waiting for plugins to instantiate");
  return session;
}

EngineSession::~EngineSession ()
{
  // Operators reference the undo stack, which references the project
  file_importer_.reset ();
  plugin_importer_.reset ();
  arranger_object_creator_.reset ();
  track_creator_.reset ();
  undo_stack_.reset ();
  ui_state_.reset ();
  project_.reset ();
}

QString
EngineSession::title () const
{
  return utils::Utf8String::from_path (dir_->filename ()).to_qstring ();
}

std::filesystem::path
EngineSession::save (std::optional<std::filesystem::path> dir)
{
  const auto previous_dir = *dir_;
  if (dir)
    *dir_ = std::filesystem::absolute (*dir);
  try
    {
      auto future = controllers::ProjectSaver::save (
        *project_, *ui_state_, *undo_stack_, utils::get_app_version (), *dir_,
        false);
      // The saver runs steps on the main thread via blocking invocations,
      // so wait while processing events
      if (!process_events_until (
            [&future] () { return future.isFinished (); }, kSaveTimeout))
        throw std::runtime_error ("timed out saving the project");
      // Rethrows the save error, if any
      const auto saved = future.result ();
      (void) saved;
    }
  catch (...)
    {
      *dir_ = previous_dir;
      throw;
    }
  return *dir_;
}

plugins::Plugin *
EngineSession::import_plugin (
  const plugins::PluginDescriptor &descriptor,
  structure::tracks::Track *       track)
{
  pending_import_ = std::make_shared<PendingImport> ();
  auto pending = pending_import_;
  if (track != nullptr)
    plugin_importer_->importPluginToTrack (&descriptor, track);
  else
    plugin_importer_->importPluginToNewTrack (&descriptor);

  const bool finished = process_events_until (
    [pending] () { return pending->finished; }, kPluginTimeout);
  pending_import_.reset ();
  if (!finished)
    {
      throw std::runtime_error (fmt::format (
        "timed out instantiating plugin '{}'", descriptor.name ().toStdString ()));
    }
  if (pending->plugin == nullptr)
    {
      throw std::runtime_error (fmt::format (
        "plugin failed to instantiate: {}", pending->error.toStdString ()));
    }
  return pending->plugin;
}

bool
EngineSession::wait_for_plugins (std::chrono::milliseconds timeout)
{
  return process_events_until (
    [this] () {
      bool pending = false;
      project_->get_registry ().for_each_matching<plugins::Plugin> (
        [&pending] (plugins::Plugin &plugin) {
          if (
            plugin.instantiationStatus ()
            == plugins::Plugin::InstantiationStatus::Pending)
            pending = true;
        });
      return !pending;
    },
    timeout);
}

} // namespace flux::engine
