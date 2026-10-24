// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "agent_api.h"

#include <cmath>
#include <set>

#include "commands/add_arranger_object_command.h"
#include "commands/arranger_object_owner_ref.h"
#include "commands/change_qobject_property_command.h"
#include "commands/move_arranger_objects_command.h"
#include "commands/rename_track_command.h"
#include "plugins/plugin_group.h"
#include "structure/tracks/track_all.h"

#include <fmt/format.h>

#include "musical_time.h"
#include "offline_render.h"
#include "load_preset_command.h"
#include "set_params_command.h"

namespace flux::engine
{

using namespace zrythm;
using json = nlohmann::json;
using structure::tracks::Track;

namespace
{

// ---------------------------------------------------------------------------
// Parameter helpers
// ---------------------------------------------------------------------------

[[noreturn]] void
invalid (const std::string &message)
{
  throw RpcError (rpc_error::kInvalidParams, message);
}

const json &
require (const json &p, const char * key)
{
  if (!p.contains (key) || p[key].is_null ())
    invalid (fmt::format ("missing required parameter '{}'", key));
  return p[key];
}

std::string
get_string (const json &p, const char * key)
{
  const auto &v = require (p, key);
  if (!v.is_string ())
    invalid (fmt::format ("'{}' must be a string", key));
  return v.get<std::string> ();
}

std::optional<std::string>
opt_string (const json &p, const char * key)
{
  if (!p.contains (key) || p[key].is_null ())
    return std::nullopt;
  if (!p[key].is_string ())
    invalid (fmt::format ("'{}' must be a string", key));
  return p[key].get<std::string> ();
}

double
get_number (const json &p, const char * key)
{
  const auto &v = require (p, key);
  if (!v.is_number ())
    invalid (fmt::format ("'{}' must be a number", key));
  const auto d = v.get<double> ();
  if (!std::isfinite (d))
    invalid (fmt::format ("'{}' must be finite", key));
  return d;
}

std::optional<double>
opt_number (const json &p, const char * key)
{
  if (!p.contains (key) || p[key].is_null ())
    return std::nullopt;
  return get_number (p, key);
}

bool
opt_bool (const json &p, const char * key, bool def)
{
  if (!p.contains (key) || p[key].is_null ())
    return def;
  if (!p[key].is_boolean ())
    invalid (fmt::format ("'{}' must be true or false", key));
  return p[key].get<bool> ();
}

bool
is_dry_run (const json &p)
{
  return opt_bool (p, "dry_run", false);
}

double
check_bar (double bar, const char * key)
{
  if (bar < 1.0)
    invalid (fmt::format ("'{}' must be >= 1 (bars are 1-based), got {}", key, bar));
  if (bar > 100000.0)
    invalid (fmt::format ("'{}' is out of range (max 100000), got {}", key, bar));
  return bar;
}

double
round6 (double v)
{
  return round_decimals (v, 6);
}

// ---------------------------------------------------------------------------
// Naming / lookup
// ---------------------------------------------------------------------------

std::string
uuid_string (const QUuid &id)
{
  return id.toString (QUuid::WithoutBraces).toStdString ();
}

template <typename T>
std::string
id_of (const T &obj)
{
  return uuid_string (type_safe::get (obj.get_uuid ()));
}

QUuid
parse_uuid (const std::string &s, const char * what)
{
  const auto id = QUuid::fromString (QString::fromStdString (s));
  if (id.isNull ())
    {
      throw RpcError (
        rpc_error::kNotFound, fmt::format ("'{}' is not a valid {} id", s, what));
    }
  return id;
}

std::string
track_type_name (Track::Type type)
{
  switch (type)
    {
    case Track::Type::Instrument:
      return "instrument";
    case Track::Type::Audio:
      return "audio";
    case Track::Type::Master:
      return "master";
    case Track::Type::Chord:
      return "chord";
    case Track::Type::Marker:
      return "marker";
    case Track::Type::Modulator:
      return "modulator";
    case Track::Type::AudioBus:
      return "audio_bus";
    case Track::Type::AudioGroup:
      return "audio_group";
    case Track::Type::Midi:
      return "midi";
    case Track::Type::MidiBus:
      return "midi_bus";
    case Track::Type::MidiGroup:
      return "midi_group";
    case Track::Type::Folder:
      return "folder";
    }
  return "unknown";
}

Track *
find_track (EngineSession &s, const std::string &id_str)
{
  const auto id = parse_uuid (id_str, "track");
  for (const auto &ref : s.project ().tracklist ()->collection ()->tracks ())
    {
      auto * track = ref.get ();
      if (type_safe::get (track->get_uuid ()) == id)
        return track;
    }
  throw RpcError (rpc_error::kNotFound, fmt::format ("unknown track '{}'", id_str));
}

std::vector<plugins::PluginUuidReference>
track_plugins (const Track &track)
{
  std::vector<plugins::PluginUuidReference> refs;
  track.collect_plugins (refs);
  return refs;
}

std::pair<plugins::Plugin *, Track *>
find_plugin (EngineSession &s, const std::string &id_str)
{
  const auto id = parse_uuid (id_str, "plugin");
  for (const auto &ref : s.project ().tracklist ()->collection ()->tracks ())
    {
      auto * track = ref.get ();
      for (const auto &pl_ref : track_plugins (*track))
        {
          auto * plugin = pl_ref.get ();
          if (plugin != nullptr && type_safe::get (plugin->get_uuid ()) == id)
            return { plugin, track };
        }
    }
  throw RpcError (
    rpc_error::kNotFound, fmt::format ("unknown plugin '{}'", id_str));
}

std::string
plugin_slot (const Track &track, const plugins::Plugin &plugin)
{
  auto * channel = track.channel ();
  if (channel == nullptr)
    return "other";
  const auto contains = [&plugin] (plugins::PluginGroup * group) {
    if (group == nullptr)
      return false;
    std::vector<plugins::PluginUuidReference> refs;
    group->get_plugins (refs);
    return std::ranges::any_of (refs, [&plugin] (const auto &r) {
      return r.id () == plugin.get_uuid ();
    });
  };
  if (contains (channel->instruments ()))
    return "instrument";
  if (contains (channel->midiFx ()))
    return "midi_fx";
  if (contains (channel->inserts ()))
    return "insert";
  return "other";
}

std::string
instantiation_status (plugins::Plugin::InstantiationStatus status)
{
  switch (status)
    {
    case plugins::Plugin::InstantiationStatus::Pending:
      return "pending";
    case plugins::Plugin::InstantiationStatus::Successful:
      return "ok";
    case plugins::Plugin::InstantiationStatus::Failed:
      return "failed";
    }
  return "unknown";
}

std::string
format_name (const plugins::Plugin &plugin)
{
  auto s = plugin.configuration ()->descriptor ()->format ().toLower ();
  return s.toStdString ();
}

json
plugin_json (const Track &track, const plugins::Plugin &plugin)
{
  return {
    { "id",     id_of (plugin)                                       },
    { "name",   plugin.get_name ().str ()                            },
    { "format", format_name (plugin)                                 },
    { "slot",   plugin_slot (track, plugin)                          },
    { "status", instantiation_status (plugin.instantiationStatus ()) },
  };
}

std::string
param_type_name (dsp::ParameterRange::Type t)
{
  using T = dsp::ParameterRange::Type;
  switch (t)
    {
    case T::Linear:
      return "linear";
    case T::Toggle:
      return "toggle";
    case T::Integer:
      return "integer";
    case T::GainAmplitude:
      return "gain";
    case T::Logarithmic:
      return "logarithmic";
    case T::Enumeration:
      return "enum";
    case T::Trigger:
      return "trigger";
    }
  return "linear";
}

std::vector<dsp::ProcessorParameter *>
plugin_params (const plugins::Plugin &plugin)
{
  std::vector<dsp::ProcessorParameter *> out;
  for (const auto &ref : plugin.get_parameters ())
    {
      if (auto * p = ref.get_object_as<dsp::ProcessorParameter> ())
        out.push_back (p);
    }
  return out;
}

std::string
param_id (const dsp::ProcessorParameter &p)
{
  return type_safe::get (p.get_unique_id ()).str ();
}

json
param_json (const dsp::ProcessorParameter &p)
{
  const auto &r = p.range ();
  json        j{
    { "id",          param_id (p)                                       },
    { "uuid",        id_of (p)                                          },
    { "name",        p.label ().toStdString ()                          },
    { "type",        param_type_name (r.type_)                          },
    { "value",       round6 (p.baseValue ())                            },
    { "plain_value", round6 (r.convertFrom0To1 (p.baseValue ()))        },
    // The reachable plain range (a gain fader tops out below its nominal
    // maximum, for example)
    { "min",         round6 (r.convertFrom0To1 (0.f))                   },
    { "max",         round6 (r.convertFrom0To1 (1.f))                   },
    { "default",     round6 (r.defaultNormalizedValue ())               },
    { "unit",        dsp::ParameterRange::unit_to_string (r.unit_).str () },
    { "automatable", p.automatable ()                                   },
    { "hidden",      p.hidden ()                                        },
  };
  if (r.type_ == dsp::ParameterRange::Type::Enumeration)
    {
      json labels = json::array ();
      for (const auto &l : r.enum_labels_)
        labels.push_back (l.str ());
      j["enum_labels"] = labels;
    }
  return j;
}

std::string
lower (std::string s)
{
  std::ranges::transform (s, s.begin (), [] (unsigned char c) {
    return static_cast<char> (std::tolower (c));
  });
  return s;
}

/** Collects the clips on a track's lanes. */
template <typename ClipT, typename Fn>
void
for_each_clip (const Track &track, Fn &&fn)
{
  auto * lanes = track.lanes ();
  if (lanes == nullptr)
    return;
  for (size_t i = 0; i < lanes->size (); ++i)
    {
      auto * lane = lanes->at (i);
      for (auto * clip :
           lane->structure::arrangement::ArrangerObjectOwner<ClipT>::get_children_view ())
        fn (*clip, i);
    }
}

std::set<std::string>
all_clip_ids (EngineSession &s)
{
  std::set<std::string> ids;
  for (const auto &ref : s.project ().tracklist ()->collection ()->tracks ())
    {
      for_each_clip<structure::arrangement::MidiClip> (
        *ref.get (), [&ids] (const auto &c, size_t) { ids.insert (id_of (c)); });
      for_each_clip<structure::arrangement::AudioClip> (
        *ref.get (), [&ids] (const auto &c, size_t) { ids.insert (id_of (c)); });
    }
  return ids;
}

std::set<std::string>
all_track_ids (EngineSession &s)
{
  std::set<std::string> ids;
  for (const auto &ref : s.project ().tracklist ()->collection ()->tracks ())
    ids.insert (id_of (*ref.get ()));
  return ids;
}

json
dry_run_result (json changes)
{
  return {
    { "dry_run", true              },
    { "changes", std::move (changes) },
  };
}

} // namespace

// ===========================================================================
// Dispatch
// ===========================================================================

AgentApi::AgentApi (EngineHost &host, PluginCatalog &catalog)
    : host_ (host), catalog_ (catalog)
{
}

AgentApi::~AgentApi ()
{
  close_session ();
}

void
AgentApi::close_session ()
{
  if (turn_ && session_)
    session_->undo_stack ().endMacro ();
  turn_.reset ();
  session_.reset ();
}

EngineSession &
AgentApi::require_session ()
{
  if (!session_)
    {
      throw RpcError (
        rpc_error::kNoProject,
        "no project is open (call create_project or open_project first)");
    }
  return *session_;
}

void
AgentApi::open_project (const std::filesystem::path &dir)
{
  // Release the old project before loading the next one
  close_session ();
  session_ = EngineSession::open (host_, std::filesystem::absolute (dir));
}

std::optional<json>
AgentApi::handle_line (const std::string &line)
{
  json request;
  try
    {
      request = json::parse (line);
    }
  catch (const json::parse_error &e)
    {
      return json{
        { "jsonrpc", "2.0" },
        { "id", nullptr },
        { "error",
         { { "code", rpc_error::kParseError },
            { "message", fmt::format ("parse error: {}", e.what ()) } } },
      };
    }
  return handle (request);
}

std::optional<json>
AgentApi::handle (const json &request)
{
  if (request.is_array ())
    {
      if (request.empty ())
        {
          return json{
            { "jsonrpc", "2.0" },
            { "id", nullptr },
            { "error",
             { { "code", rpc_error::kInvalidRequest },
                { "message", "empty batch" } } },
          };
        }
      json responses = json::array ();
      for (const auto &r : request)
        {
          bool notification = false;
          auto resp = handle_one (r, notification);
          if (!notification)
            responses.push_back (std::move (resp));
        }
      if (responses.empty ())
        return std::nullopt;
      return responses;
    }
  bool notification = false;
  auto resp = handle_one (request, notification);
  if (notification)
    return std::nullopt;
  return resp;
}

json
AgentApi::handle_one (const json &request, bool &is_notification)
{
  is_notification = false;
  json id = nullptr;
  const auto error = [&id] (int code, const std::string &message) {
    return json{
      { "jsonrpc", "2.0" },
      { "id", id },
      { "error", { { "code", code }, { "message", message } } },
    };
  };

  if (!request.is_object ())
    return error (rpc_error::kInvalidRequest, "request must be a JSON object");
  if (request.contains ("id"))
    {
      const auto &rid = request["id"];
      if (!(rid.is_string () || rid.is_number () || rid.is_null ()))
        return error (rpc_error::kInvalidRequest, "'id' must be a string or number");
      id = rid;
    }
  if (
    !request.contains ("jsonrpc") || !request["jsonrpc"].is_string ()
    || request["jsonrpc"].get<std::string> () != "2.0")
    return error (rpc_error::kInvalidRequest, "'jsonrpc' must be \"2.0\"");
  if (!request.contains ("method") || !request["method"].is_string ())
    return error (rpc_error::kInvalidRequest, "'method' must be a string");
  is_notification = !request.contains ("id");
  current_id_ = id;

  json params = json::object ();
  if (request.contains ("params"))
    {
      params = request["params"];
      if (!params.is_object ())
        {
          return error (
            rpc_error::kInvalidParams,
            "'params' must be an object (named parameters)");
        }
    }

  try
    {
      auto result = dispatch (request["method"].get<std::string> (), params);
      return json{
        { "jsonrpc", "2.0"             },
        { "id",      id                },
        { "result",  std::move (result) },
      };
    }
  catch (const RpcError &e)
    {
      return error (e.code (), e.what ());
    }
  catch (const std::invalid_argument &e)
    {
      return error (rpc_error::kInvalidParams, e.what ());
    }
  catch (const std::exception &e)
    {
      return error (rpc_error::kOperationFailed, e.what ());
    }
  catch (...)
    {
      return error (rpc_error::kInternalError, "unknown internal error");
    }
}

json
AgentApi::dispatch (const std::string &method, const json &p)
{
  if (method == "describe_project")
    return describe_project (p);
  if (method == "create_project")
    return create_project (p);
  if (method == "open_project")
    return open_project_method (p);
  if (method == "save_project")
    return save_project (p);
  if (method == "add_track")
    return add_track (p);
  if (method == "list_plugins")
    return list_plugins (p);
  if (method == "load_plugin")
    return load_plugin (p);
  if (method == "list_params")
    return list_params (p);
  if (method == "set_params")
    return set_params (p);
  if (method == "list_presets")
    return list_presets (p);
  if (method == "load_preset")
    return load_preset (p);
  if (method == "write_notes")
    return write_notes (p);
  if (method == "import_audio")
    return import_file (p, false);
  if (method == "import_midi")
    return import_file (p, true);
  if (method == "set_tempo")
    return set_tempo (p);
  if (method == "add_marker")
    return add_marker (p);
  if (method == "set_loop_range")
    return set_loop_range (p);
  if (method == "set_range")
    return set_range (p);
  if (method == "render")
    return render (p);
  if (method == "undo")
    return undo_redo (p, true);
  if (method == "redo")
    return undo_redo (p, false);
  if (method == "begin_agent_turn")
    return begin_agent_turn (p);
  if (method == "end_agent_turn")
    return end_agent_turn (p);
  if (method == "get_history")
    return history ();
  if (method == "shutdown")
    {
      close_session ();
      shutdown_requested_ = true;
      return { { "ok", true } };
    }
  if (method == "ping")
    return { { "pong", true }, { "api_version", std::string (kApiVersion) } };
  throw RpcError (
    rpc_error::kMethodNotFound, fmt::format ("unknown method '{}'", method));
}

json
AgentApi::history () const
{
  if (!session_)
    return { { "open", false } };
  auto &u = session_->undo_stack ();
  json  j{
    { "can_undo",  u.canUndo ()                                                   },
    { "can_redo",  u.canRedo ()                                                   },
    { "index",     u.index ()                                                     },
    { "count",     u.count ()                                                     },
    { "undo_text", u.canUndo () ? u.text (u.index () - 1).toStdString () : "" },
    { "redo_text", u.canRedo () ? u.text (u.index ()).toStdString () : ""     },
    { "turn_open", turn_.has_value ()                                             },
  };
  if (turn_)
    j["turn_label"] = turn_->label.toStdString ();
  return j;
}

// ===========================================================================
// Queries
// ===========================================================================

json
AgentApi::describe_project (const json &p)
{
  auto       &s = require_session ();
  auto       &project = s.project ();
  const auto &tm = project.tempo_map ();
  const bool  include_notes = opt_bool (p, "include_notes", false);

  const auto bar_of = [&tm] (double ticks) {
    return round6 (ticks_to_bar (tm, ticks));
  };
  const auto sec_of = [&tm] (double ticks) {
    return round6 (
      tm.tick_to_seconds (dsp::TimelineTick{ units::ticks (ticks) })
        .in (units::seconds));
  };

  json out;
  out["api_version"] = std::string (kApiVersion);
  out["title"] = s.title ().toStdString ();
  out["dir"] = s.directory ().string ();
  out["sample_rate"] = host_.sample_rate ();
  out["ppq"] = static_cast<int64_t> (dsp::TempoMap::get_ppq ().in (units::ticks));

  // Tempo and time signature
  auto * tempo_mgr = project.tempoObjectManager ();
  {
    json changes = json::array ();
    for (auto * t :
         tempo_mgr->structure::arrangement::ArrangerObjectOwner<
           structure::arrangement::TempoObject>::get_sorted_children_view ())
      {
        changes.push_back (
          { { "id", id_of (*t) },
            { "bar", bar_of (t->position ()->ticks ()) },
            { "bpm", round6 (t->tempo ()) } });
      }
    out["tempo"] = {
      { "bpm",     round6 (tm.base_bpm ().in (units::bpm)) },
      { "changes", changes                                },
    };
  }
  {
    json changes = json::array ();
    for (auto * ts :
         tempo_mgr->structure::arrangement::ArrangerObjectOwner<
           structure::arrangement::TimeSignatureObject>::get_sorted_children_view ())
      {
        changes.push_back (
          { { "id", id_of (*ts) },
            { "bar", bar_of (ts->position ()->ticks ()) },
            { "numerator", ts->numerator () },
            { "denominator", ts->denominator () } });
      }
    const auto base = tm.base_time_signature ().time_signature;
    out["time_signature"] = {
      { "numerator",   base.numerator   },
      { "denominator", base.denominator },
      { "changes",     changes          },
    };
  }

  // Range (start/end markers), loop, custom markers
  auto * marker_track = project.tracklist ()->singletonTracks ()->markerTrack ();
  {
    const double start = marker_track->get_start_marker ()->position ()->ticks ();
    const double end = marker_track->get_end_marker ()->position ()->ticks ();
    out["range"] = {
      { "start_bar", bar_of (start) },
      { "end_bar",   bar_of (end)   },
      { "start_sec", sec_of (start) },
      { "end_sec",   sec_of (end)   },
    };
    auto * transport = project.getTransport ();
    out["loop"] = {
      { "enabled",   transport->loopEnabled ()                            },
      { "start_bar", bar_of (transport->loopStartPosition ()->ticks ()) },
      { "end_bar",   bar_of (transport->loopEndPosition ()->ticks ())   },
    };
    json markers = json::array ();
    for (auto * m : marker_track->get_children_view ())
      {
        if (m->markerType () != structure::arrangement::Marker::MarkerType::Custom)
          continue;
        markers.push_back (
          { { "id", id_of (*m) },
            { "name", m->name ()->get_name ().str () },
            { "bar", bar_of (m->position ()->ticks ()) } });
      }
    out["markers"] = markers;
  }

  // Tracks
  json tracks = json::array ();
  int  index = 0;
  for (const auto &ref : project.tracklist ()->collection ()->tracks ())
    {
      auto * track = ref.get ();
      json   tj{
        { "id",    id_of (*track)                       },
        { "index", index++                              },
        { "name",  track->name ().toStdString ()        },
        { "type",  track_type_name (track->type ())     },
      };
      json plugins_json = json::array ();
      for (const auto &pl_ref : track_plugins (*track))
        {
          if (auto * pl = pl_ref.get ())
            plugins_json.push_back (plugin_json (*track, *pl));
        }
      tj["plugins"] = plugins_json;

      json clips = json::array ();
      const auto clip_common = [&] (const auto &clip, size_t lane, const std::string &kind) {
        const double start = clip.position ()->ticks ();
        const double len = clip.length ()->ticks ();
        return json{
          { "id",          id_of (clip)                                  },
          { "name",        clip.name ()->get_name ().str ()              },
          { "kind",        kind                                          },
          { "lane",        lane                                          },
          { "start_bar",   bar_of (start)                                },
          { "end_bar",     bar_of (start + len)                          },
          { "length_beats", round6 (len / ticks_per_beat_at (tm, start)) },
        };
      };
      for_each_clip<structure::arrangement::MidiClip> (
        *track, [&] (const structure::arrangement::MidiClip &clip, size_t lane) {
          auto       cj = clip_common (clip, lane, "midi");
          const auto notes =
            clip.structure::arrangement::ArrangerObjectOwner<
              structure::arrangement::MidiNote>::get_sorted_children_view ();
          cj["note_count"] = std::ranges::distance (notes);
          if (include_notes)
            {
              const double beat = ticks_per_beat_at (tm, clip.position ()->ticks ());
              json         nj = json::array ();
              for (auto * n : notes)
                {
                  nj.push_back (
                    { { "pitch", n->pitch () },
                      { "start_beat", round6 (n->position ()->ticks () / beat) },
                      { "length_beats", round6 (n->length ()->ticks () / beat) },
                      { "velocity", n->velocity () } });
                }
              cj["notes"] = nj;
            }
          clips.push_back (cj);
        });
      for_each_clip<structure::arrangement::AudioClip> (
        *track, [&] (const structure::arrangement::AudioClip &clip, size_t lane) {
          clips.push_back (clip_common (clip, lane, "audio"));
        });
      tj["clips"] = clips;
      tracks.push_back (tj);
    }
  out["tracks"] = tracks;
  return out;
}

json
AgentApi::list_plugins (const json &p)
{
  const auto format = opt_string (p, "format");
  const auto query = opt_string (p, "query");
  if (format && *format != "clap" && *format != "vst3" && *format != "lv2")
    invalid ("'format' must be \"clap\", \"vst3\" or \"lv2\"");
  if (opt_bool (p, "rescan", false))
    catalog_.rescan ();
  const bool instruments_only = opt_bool (p, "instruments_only", false);

  json list = json::array ();
  for (const auto &e : catalog_.entries ())
    {
      if (format && e.format != *format)
        continue;
      if (instruments_only && !e.is_instrument)
        continue;
      if (query)
        {
          const auto q = lower (*query);
          if (
            lower (e.name).find (q) == std::string::npos
            && lower (e.vendor).find (q) == std::string::npos
            && lower (e.category).find (q) == std::string::npos)
            continue;
        }
      list.push_back (e.to_json ());
    }
  json out{ { "plugins", list } };
  if (format && *format == "lv2")
    out["note"] = "LV2 plugins are not scanned by flux-engine yet";
  return out;
}

json
AgentApi::list_params (const json &p)
{
  auto &s = require_session ();
  auto [plugin, track] = find_plugin (s, get_string (p, "plugin"));
  const bool include_hidden = opt_bool (p, "include_hidden", false);
  json       params = json::array ();
  for (auto * param : plugin_params (*plugin))
    {
      if (param->hidden () && !include_hidden)
        continue;
      params.push_back (param_json (*param));
    }
  return {
    { "plugin", id_of (*plugin)          },
    { "name",   plugin->get_name ().str () },
    { "track",  id_of (*track)           },
    { "params", params                   },
  };
}

// ===========================================================================
// Project lifecycle
// ===========================================================================

json
AgentApi::create_project (const json &p)
{
  const auto dir = std::filesystem::absolute (get_string (p, "dir"));
  const auto bpm = opt_number (p, "bpm");
  std::optional<EngineSession::TimeSignature> ts;
  if (p.contains ("time_signature") && !p["time_signature"].is_null ())
    {
      const auto &t = p["time_signature"];
      if (t.is_string ())
        {
          int        num = 0, den = 0;
          const auto s = t.get<std::string> ();
          if (std::sscanf (s.c_str (), "%d/%d", &num, &den) != 2)
            invalid ("'time_signature' must look like \"4/4\"");
          ts = EngineSession::TimeSignature{ num, den };
        }
      else if (t.is_array () && t.size () == 2 && t[0].is_number_integer () && t[1].is_number_integer ())
        ts = EngineSession::TimeSignature{ t[0].get<int> (), t[1].get<int> () };
      else
        invalid ("'time_signature' must be \"N/D\" or [N, D]");
      try
        {
          dsp::throw_if_invalid_time_signature ({ ts->numerator, ts->denominator });
        }
      catch (const std::invalid_argument &e)
        {
          invalid (e.what ());
        }
    }
  if (bpm && (*bpm < 20.0 || *bpm > 999.0))
    invalid (fmt::format ("'bpm' must be between 20 and 999, got {}", *bpm));
  if (
    std::filesystem::exists (dir)
    && (!std::filesystem::is_directory (dir) || !std::filesystem::is_empty (dir)))
    {
      invalid (fmt::format ("'{}' already exists and is not an empty directory", dir.string ()));
    }
  const auto title = opt_string (p, "title");

  json change{
    { "op",  "create_project" },
    { "dir", dir.string ()    },
    { "bpm", bpm.value_or (120.0) },
    { "time_signature",
     ts ? fmt::format ("{}/{}", ts->numerator, ts->denominator) : "4/4" },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  close_session ();
  session_ = EngineSession::create (host_, dir, bpm, ts);
  json out{
    { "dir",     session_->directory ().string () },
    { "title",   session_->title ().toStdString () },
    { "changes", json::array ({ change })         },
  };
  if (title && *title != session_->title ().toStdString ())
    out["note"] = "the saved title is the directory name";
  return out;
}

json
AgentApi::open_project_method (const json &p)
{
  plugin_state_dirty_ = true;
  const auto dir = std::filesystem::absolute (get_string (p, "dir"));
  if (!std::filesystem::is_directory (dir))
    {
      throw RpcError (
        rpc_error::kNotFound,
        fmt::format ("project directory does not exist: {}", dir.string ()));
    }
  json change{
    { "op",  "open_project" },
    { "dir", dir.string ()  }
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));
  open_project (dir);
  return {
    { "dir",     session_->directory ().string ()  },
    { "title",   session_->title ().toStdString () },
    { "changes", json::array ({ change })          },
  };
}

json
AgentApi::save_project (const json &p)
{
  auto      &s = require_session ();
  const auto dir_param = opt_string (p, "dir");
  const auto target =
    dir_param ? std::filesystem::absolute (*dir_param) : s.directory ();
  json change{
    { "op",  "save_project"  },
    { "dir", target.string () }
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));
  const auto saved = s.save (
    dir_param ? std::optional<std::filesystem::path> (target) : std::nullopt);
  return {
    { "dir",     saved.string ()          },
    { "changes", json::array ({ change }) },
  };
}

// ===========================================================================
// Tracks and plugins
// ===========================================================================

json
AgentApi::add_track (const json &p)
{
  auto      &s = require_session ();
  const auto type_str = get_string (p, "type");
  Track::Type type{};
  if (type_str == "midi")
    type = Track::Type::Midi;
  else if (type_str == "instrument")
    type = Track::Type::Instrument;
  else if (type_str == "audio")
    type = Track::Type::Audio;
  else
    invalid (fmt::format (
      "'type' must be \"midi\", \"instrument\" or \"audio\", got \"{}\"", type_str));
  const auto name = opt_string (p, "name");
  if (name && name->empty ())
    invalid ("'name' must not be empty");
  const auto plugin_id = opt_string (p, "plugin_id");

  const PluginCatalog::Entry * entry = nullptr;
  if (plugin_id)
    {
      entry = catalog_.find (*plugin_id);
      if (entry == nullptr)
        {
          throw RpcError (
            rpc_error::kNotFound,
            fmt::format ("unknown plugin '{}' (see list_plugins)", *plugin_id));
        }
      if (type == Track::Type::Instrument && !entry->is_instrument)
        {
          throw RpcError (
            rpc_error::kWrongType,
            fmt::format ("'{}' is not an instrument", entry->name));
        }
      if (type != Track::Type::Instrument && entry->is_instrument)
        {
          throw RpcError (
            rpc_error::kWrongType,
            fmt::format (
              "'{}' is an instrument; use an instrument track", entry->name));
        }
    }

  json change{
    { "op",   "add_track" },
    { "type", type_str    },
  };
  if (name)
    change["name"] = *name;
  if (entry)
    change["plugin"] = { { "id", entry->id }, { "name", entry->name } };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  json        out;
  std::string plugin_error;
  {
    undo::UndoStack::ScopedMacro macro (
      s.undo_stack (),
      QObject::tr ("Add track %1")
        .arg (
          name ? QString::fromStdString (*name)
               : QString::fromStdString (type_str)));
    auto * track =
      s.track_creator ().addEmptyTrackFromType (type).value<Track *> ();
    if (track == nullptr)
      throw std::runtime_error ("failed to create the track");
    if (name)
      {
        const auto unique =
          s.project ().tracklist ()->collection ()->get_unique_name_for_track (
            track->get_uuid (),
            utils::Utf8String::from_utf8_encoded_string (*name));
        s.undo_stack ().push (
          new commands::RenameTrackCommand (*track, unique.to_qstring ()));
      }
    out = json{
      { "track", id_of (*track)                   },
      { "name",  track->name ().toStdString ()    },
      { "type",  track_type_name (track->type ()) },
    };
    if (entry)
      {
        try
          {
            const auto descriptor = PluginCatalog::make_descriptor (*entry);
            auto *     plugin = s.import_plugin (*descriptor, track);
            out["plugin"] = plugin_json (*track, *plugin);
          }
        catch (const std::exception &e)
          {
            plugin_error = e.what ();
          }
      }
  }
  if (!plugin_error.empty ())
    {
      // Roll back the empty track (inside an open turn the macro cannot be
      // undone yet; the turn can be discarded instead)
      if (!turn_)
        {
          s.undo_stack ().undo ();
          throw RpcError (
            rpc_error::kOperationFailed,
            fmt::format ("{}; the track was not added", plugin_error));
        }
      throw RpcError (
        rpc_error::kOperationFailed,
        fmt::format (
          "{}; the empty track {} stays in the open turn (end_agent_turn "
          "with discard: true drops the whole turn)",
          plugin_error, out["track"].get<std::string> ()));
    }
  out["changes"] = json::array ({ change });
  return out;
}

json
AgentApi::load_plugin (const json &p)
{
  auto      &s = require_session ();
  auto *     track = find_track (s, get_string (p, "track"));
  const auto plugin_id = get_string (p, "plugin_id");
  const auto * entry = catalog_.find (plugin_id);
  if (entry == nullptr)
    {
      throw RpcError (
        rpc_error::kNotFound,
        fmt::format ("unknown plugin '{}' (see list_plugins)", plugin_id));
    }
  if (track->channel () == nullptr)
    {
      throw RpcError (
        rpc_error::kWrongType,
        fmt::format (
          "track '{}' ({}) cannot host plugins", track->name ().toStdString (),
          track_type_name (track->type ())));
    }
  if (entry->is_instrument && track->type () != Track::Type::Instrument)
    {
      throw RpcError (
        rpc_error::kWrongType,
        fmt::format (
          "'{}' is an instrument and needs an instrument track; track '{}' is {}",
          entry->name, track->name ().toStdString (),
          track_type_name (track->type ())));
    }
  if (entry->is_instrument && track->channel ()->instruments () != nullptr)
    {
      std::vector<plugins::PluginUuidReference> existing;
      track->channel ()->instruments ()->get_plugins (existing);
      if (!existing.empty ())
        {
          throw RpcError (
            rpc_error::kStateConflict,
            fmt::format (
              "track '{}' already has an instrument; add a new instrument track",
              track->name ().toStdString ()));
        }
    }
  json change{
    { "op",     "load_plugin"                                     },
    { "track",  id_of (*track)                                    },
    { "plugin", { { "id", entry->id }, { "name", entry->name } } },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));
  const auto descriptor = PluginCatalog::make_descriptor (*entry);
  auto *     plugin = s.import_plugin (*descriptor, track);
  return {
    { "plugin",  plugin_json (*track, *plugin) },
    { "track",   id_of (*track)                },
    { "changes", json::array ({ change })      },
  };
}

json
AgentApi::set_params (const json &p)
{
  auto &s = require_session ();
  auto [plugin, track] = find_plugin (s, get_string (p, "plugin"));
  const auto &values = require (p, "values");
  if (!values.is_array () || values.empty ())
    invalid ("'values' must be a non-empty array of {id|name, value, unit?}");
  const auto params = plugin_params (*plugin);

  std::vector<SetParamsCommand::Change> changes;
  json                                  changes_json = json::array ();
  for (size_t i = 0; i < values.size (); ++i)
    {
      const auto &v = values[i];
      const auto  where = fmt::format ("values[{}]", i);
      if (!v.is_object ())
        invalid (fmt::format ("{} must be an object", where));
      dsp::ProcessorParameter * param = nullptr;
      if (v.contains ("id"))
        {
          if (!v["id"].is_string ())
            invalid (fmt::format ("{}.id must be a string", where));
          const auto id = v["id"].get<std::string> ();
          for (auto * cand : params)
            if (param_id (*cand) == id || id_of (*cand) == id)
              param = cand;
          if (param == nullptr)
            {
              throw RpcError (
                rpc_error::kNotFound,
                fmt::format ("{}: plugin has no parameter with id '{}'", where, id));
            }
        }
      else if (v.contains ("name"))
        {
          if (!v["name"].is_string ())
            invalid (fmt::format ("{}.name must be a string", where));
          const auto name = lower (v["name"].get<std::string> ());
          for (auto * cand : params)
            if (lower (cand->label ().toStdString ()) == name)
              {
                param = cand;
                break;
              }
          if (param == nullptr)
            {
              throw RpcError (
                rpc_error::kNotFound,
                fmt::format (
                  "{}: plugin has no parameter named '{}' (see list_params)",
                  where, v["name"].get<std::string> ()));
            }
        }
      else
        invalid (fmt::format ("{} needs 'id' or 'name'", where));

      if (!v.contains ("value"))
        invalid (fmt::format ("{} needs 'value'", where));
      const auto &range = param->range ();
      std::string unit = "normalized";
      if (v.contains ("unit"))
        {
          if (!v["unit"].is_string ())
            invalid (fmt::format ("{}.unit must be \"normalized\" or \"plain\"", where));
          unit = v["unit"].get<std::string> ();
        }
      float normalized = 0.f;
      const auto &value = v["value"];
      if (value.is_string ())
        {
          // Enumeration label, or on/off for toggles
          const auto label = lower (value.get<std::string> ());
          bool       found = false;
          if (range.type_ == dsp::ParameterRange::Type::Enumeration)
            {
              for (size_t k = 0; k < range.enum_labels_.size (); ++k)
                if (lower (range.enum_labels_[k].str ()) == label)
                  {
                    normalized = range.normalizedEnumValue (k);
                    found = true;
                  }
            }
          else if (range.type_ == dsp::ParameterRange::Type::Toggle)
            {
              if (label == "on" || label == "true")
                {
                  normalized = 1.f;
                  found = true;
                }
              else if (label == "off" || label == "false")
                {
                  normalized = 0.f;
                  found = true;
                }
            }
          if (!found)
            {
              invalid (fmt::format (
                "{}: '{}' is not a valid value for '{}' ({})", where,
                value.get<std::string> (), param->label ().toStdString (),
                param_type_name (range.type_)));
            }
        }
      else if (value.is_boolean ())
        normalized = value.get<bool> () ? 1.f : 0.f;
      else if (value.is_number ())
        {
          const double d = value.get<double> ();
          if (!std::isfinite (d))
            invalid (fmt::format ("{}.value must be finite", where));
          if (unit == "normalized")
            {
              if (d < 0.0 || d > 1.0)
                {
                  invalid (fmt::format (
                    "{}: normalized value {} for '{}' is outside 0..1 (use "
                    "unit \"plain\" for values in {}..{})",
                    where, d, param->label ().toStdString (),
                    range.convertFrom0To1 (0.f), range.convertFrom0To1 (1.f)));
                }
              normalized = static_cast<float> (d);
            }
          else if (unit == "plain")
            {
              const auto a = range.convertFrom0To1 (0.f);
              const auto b = range.convertFrom0To1 (1.f);
              const auto lo = std::min (a, b);
              const auto hi = std::max (a, b);
              const auto tol = 1e-6 * std::max (1.0, static_cast<double> (hi - lo));
              if (d < lo - tol || d > hi + tol)
                {
                  invalid (fmt::format (
                    "{}: plain value {} for '{}' is outside its range {}..{}",
                    where, d, param->label ().toStdString (), lo, hi));
                }
              normalized = range.convertTo0To1 (static_cast<float> (d));
            }
          else
            invalid (fmt::format (
              "{}.unit must be \"normalized\" or \"plain\", got \"{}\"", where, unit));
        }
      else
        invalid (fmt::format ("{}.value must be a number, boolean or label", where));

      normalized = std::clamp (normalized, 0.f, 1.f);
      changes.push_back ({ param, param->baseValue (), normalized });
      changes_json.push_back (
        { { "op", "set_param" },
          { "plugin", id_of (*plugin) },
          { "param", param_id (*param) },
          { "name", param->label ().toStdString () },
          { "from", round6 (param->baseValue ()) },
          { "to", round6 (normalized) },
          { "plain_to", round6 (range.convertFrom0To1 (normalized)) } });
    }

  if (is_dry_run (p))
    return dry_run_result (changes_json);
  s.undo_stack ().push (new SetParamsCommand (std::move (changes)));
  return {
    { "plugin",  id_of (*plugin) },
    { "changes", changes_json    },
  };
}

namespace
{
std::string
preset_id_string (const plugins::Plugin::PresetId &id)
{
  return std::visit (
    [] (const auto &v) -> std::string {
      if constexpr (std::is_same_v<std::decay_t<decltype (v)>, int>)
        return std::to_string (v);
      else
        return v.toStdString ();
    },
    id);
}

json
preset_json (const plugins::Plugin::PresetEntry &e, size_t index)
{
  return {
    { "index", index                       },
    { "name",  e.name.toStdString ()       },
    { "group", e.group.toStdString ()      },
    { "id",    preset_id_string (e.id)     },
  };
}
} // namespace

json
AgentApi::list_presets (const json &p)
{
  auto &s = require_session ();
  auto [plugin, track] = find_plugin (s, get_string (p, "plugin"));
  std::string query;
  if (p.contains ("query"))
    query = lower (get_string (p, "query"));
  size_t limit = 200;
  if (p.contains ("limit"))
    {
      if (!p["limit"].is_number_integer () || p["limit"].get<int64_t> () < 1)
        invalid ("'limit' must be a positive integer");
      limit = static_cast<size_t> (p["limit"].get<int64_t> ());
    }
  const auto entries = plugin->presetEntries ();
  json       out = json::array ();
  size_t     matched = 0;
  for (size_t i = 0; i < entries.size (); ++i)
    {
      const auto &e = entries[i];
      if (
        !query.empty ()
        && lower (e.name.toStdString ()).find (query) == std::string::npos
        && lower (e.group.toStdString ()).find (query) == std::string::npos)
        continue;
      ++matched;
      if (out.size () < limit)
        out.push_back (preset_json (e, i));
    }
  const auto sel = plugin->presetIndex ();
  return {
    { "plugin",   id_of (*plugin)                                        },
    { "total",    entries.size ()                                        },
    { "matched",  matched                                                },
    { "presets",  out                                                    },
    { "selected", sel >= 0 ? json (preset_json (entries[static_cast<size_t> (sel)], static_cast<size_t> (sel))) : json (nullptr) },
  };
}

json
AgentApi::load_preset (const json &p)
{
  auto &s = require_session ();
  auto [plugin, track] = find_plugin (s, get_string (p, "plugin"));
  const auto entries = plugin->presetEntries ();
  if (entries.empty ())
    {
      throw RpcError (
        rpc_error::kWrongType,
        fmt::format (
          "plugin '{}' offers no presets (CLAP preset-discovery only)",
          plugin->get_name ().str ()));
    }
  if (!p.contains ("preset"))
    invalid ("load_preset needs 'preset' (index, id or name; see list_presets)");
  const auto &want = p["preset"];
  std::optional<size_t> index;
  if (want.is_number_integer ())
    {
      const auto i = want.get<int64_t> ();
      if (i < 0 || static_cast<size_t> (i) >= entries.size ())
        invalid (fmt::format ("preset index {} is outside 0..{}", i, entries.size () - 1));
      index = static_cast<size_t> (i);
    }
  else if (want.is_string ())
    {
      const auto key = want.get<std::string> ();
      const auto key_l = lower (key);
      // Exact id first, then exact name (optionally "group/name"), then unique substring
      for (size_t i = 0; i < entries.size () && !index; ++i)
        if (preset_id_string (entries[i].id) == key)
          index = i;
      for (size_t i = 0; i < entries.size () && !index; ++i)
        {
          const auto name = lower (entries[i].name.toStdString ());
          const auto full = lower (entries[i].group.toStdString ()) + "/" + name;
          if (name == key_l || full == key_l)
            index = i;
        }
      if (!index)
        {
          std::vector<size_t> hits;
          for (size_t i = 0; i < entries.size (); ++i)
            if (lower (entries[i].name.toStdString ()).find (key_l) != std::string::npos)
              hits.push_back (i);
          if (hits.size () == 1)
            index = hits.front ();
          else if (hits.size () > 1)
            {
              std::string names;
              for (size_t k = 0; k < std::min<size_t> (hits.size (), 8); ++k)
                names += (k ? ", " : "") + entries[hits[k]].name.toStdString ();
              throw RpcError (
                rpc_error::kInvalidParams,
                fmt::format (
                  "'{}' matches {} presets ({}{}); use an exact name or the id",
                  key, hits.size (), names, hits.size () > 8 ? ", ..." : ""));
            }
        }
      if (!index)
        {
          throw RpcError (
            rpc_error::kNotFound,
            fmt::format ("no preset matches '{}' (see list_presets)", key));
        }
    }
  else
    invalid ("'preset' must be an index (integer) or an id/name (string)");

  const auto &entry = entries[*index];
  json change{
    { "op",     "load_preset"                },
    { "plugin", id_of (*plugin)              },
    { "preset", preset_json (entry, *index)  },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));
  auto * cmd = new LoadPresetCommand (*plugin, static_cast<int> (*index), entry.name);
  s.undo_stack ().push (cmd);
  plugin_state_dirty_ = true;
  if (!cmd->applied ())
    {
      s.undo_stack ().undo ();
      throw RpcError (
        rpc_error::kInternalError,
        fmt::format (
          "plugin '{}' refused preset '{}'", plugin->get_name ().str (),
          entry.name.toStdString ()));
    }
  return {
    { "plugin",  id_of (*plugin)          },
    { "preset",  preset_json (entry, *index) },
    { "changes", json::array ({ change }) },
  };
}

// ===========================================================================
// Arrangement
// ===========================================================================

json
AgentApi::write_notes (const json &p)
{
  auto  &s = require_session ();
  auto * track = find_track (s, get_string (p, "track"));
  if (track->type () != Track::Type::Midi && track->type () != Track::Type::Instrument)
    {
      throw RpcError (
        rpc_error::kWrongType,
        fmt::format (
          "track '{}' is {}; notes need a midi or instrument track",
          track->name ().toStdString (), track_type_name (track->type ())));
    }
  const double start_bar = check_bar (get_number (p, "start_bar"), "start_bar");
  const double length_bars = get_number (p, "length_bars");
  if (length_bars <= 0.0 || length_bars > 10000.0)
    invalid (fmt::format ("'length_bars' must be > 0 and <= 10000, got {}", length_bars));
  const auto  name = opt_string (p, "name");
  const auto &notes = require (p, "notes");
  if (!notes.is_array ())
    invalid ("'notes' must be an array of {pitch, start_beat, length_beats, velocity}");

  const auto &tm = s.project ().tempo_map ();
  const double clip_start = bar_to_ticks (tm, start_bar);
  const double clip_end = bar_to_ticks (tm, start_bar + length_bars);
  const double clip_len = clip_end - clip_start;
  const double ticks_per_beat = ticks_per_beat_at (tm, clip_start);
  const double clip_beats = clip_len / ticks_per_beat;

  struct NoteSpec
  {
    int    pitch;
    int    velocity;
    double start;
    double end;
    int    channel;
  };
  std::vector<NoteSpec> specs;
  specs.reserve (notes.size ());
  constexpr double kEps = 1e-6;
  for (size_t i = 0; i < notes.size (); ++i)
    {
      const auto &n = notes[i];
      const auto  where = fmt::format ("notes[{}]", i);
      if (!n.is_object ())
        invalid (fmt::format ("{} must be an object", where));
      const auto num = [&] (const char * key) {
        if (!n.contains (key) || !n[key].is_number ())
          invalid (fmt::format ("{}.{} must be a number", where, key));
        const auto d = n[key].get<double> ();
        if (!std::isfinite (d))
          invalid (fmt::format ("{}.{} must be finite", where, key));
        return d;
      };
      const double pitch = num ("pitch");
      if (pitch != std::floor (pitch) || pitch < 0 || pitch > 127)
        invalid (fmt::format ("{}.pitch must be an integer 0..127, got {}", where, pitch));
      double velocity = 100;
      if (n.contains ("velocity"))
        {
          velocity = num ("velocity");
          if (velocity != std::floor (velocity) || velocity < 1 || velocity > 127)
            invalid (fmt::format (
              "{}.velocity must be an integer 1..127, got {}", where, velocity));
        }
      double channel = 0;
      if (n.contains ("channel"))
        {
          channel = num ("channel");
          if (channel != std::floor (channel) || channel < 0 || channel > 15)
            invalid (fmt::format ("{}.channel must be an integer 0..15, got {}", where, channel));
        }
      const double start_beat = num ("start_beat");
      const double length_beats = num ("length_beats");
      if (start_beat < 0)
        invalid (fmt::format ("{}.start_beat must be >= 0, got {}", where, start_beat));
      if (length_beats <= 0)
        invalid (fmt::format ("{}.length_beats must be > 0, got {}", where, length_beats));
      if (start_beat + length_beats > clip_beats + kEps)
        {
          invalid (fmt::format (
            "{} ends at beat {} but the clip is {} beats long ({} bars); notes "
            "must lie within the clip",
            where, start_beat + length_beats, round6 (clip_beats), length_bars));
        }
      specs.push_back (
        { static_cast<int> (pitch), static_cast<int> (velocity),
          start_beat * ticks_per_beat,
          std::min ((start_beat + length_beats) * ticks_per_beat, clip_len),
          static_cast<int> (channel) });
    }

  // Optional channel events: pitch bend (-1..1 of the instrument's bend range)
  // and control change (0..127), placed in beats from the clip start
  using EventType = structure::arrangement::MidiControlEvent::EventType;
  struct EventSpec
  {
    EventType type;
    int       channel;
    int       controller;
    int       value;
    double    start;
  };
  std::vector<EventSpec> events;
  if (p.contains ("events"))
    {
      const auto &evs = p["events"];
      if (!evs.is_array ())
        invalid ("'events' must be an array of {type, beat, value, channel?, controller?}");
      for (size_t i = 0; i < evs.size (); ++i)
        {
          const auto &e = evs[i];
          const auto  where = fmt::format ("events[{}]", i);
          if (!e.is_object ())
            invalid (fmt::format ("{} must be an object", where));
          const auto num = [&] (const char * key) {
            if (!e.contains (key) || !e[key].is_number ())
              invalid (fmt::format ("{}.{} must be a number", where, key));
            const auto d = e[key].get<double> ();
            if (!std::isfinite (d))
              invalid (fmt::format ("{}.{} must be finite", where, key));
            return d;
          };
          if (!e.contains ("type") || !e["type"].is_string ())
            invalid (fmt::format ("{}.type must be \"pitch_bend\" or \"cc\"", where));
          const auto   type = e["type"].get<std::string> ();
          const double beat = num ("beat");
          if (beat < 0 || beat > clip_beats + kEps)
            invalid (fmt::format ("{}.beat {} is outside the clip (0..{})", where, beat, round6 (clip_beats)));
          double channel = 0;
          if (e.contains ("channel"))
            {
              channel = num ("channel");
              if (channel != std::floor (channel) || channel < 0 || channel > 15)
                invalid (fmt::format ("{}.channel must be an integer 0..15, got {}", where, channel));
            }
          const double value = num ("value");
          EventSpec spec{ EventType::ControlChange, static_cast<int> (channel), 0, 0,
                          std::min (beat * ticks_per_beat, clip_len) };
          if (type == "pitch_bend")
            {
              if (value < -1.0 || value > 1.0)
                invalid (fmt::format ("{}.value for pitch_bend must be -1..1, got {}", where, value));
              spec.type = EventType::PitchBend;
              spec.value = std::clamp (
                static_cast<int> (std::lround (8192.0 + value * 8192.0)), 0,
                structure::arrangement::MidiControlEvent::kMaxValuePitchBend);
            }
          else if (type == "cc")
            {
              const double ctrl = num ("controller");
              if (ctrl != std::floor (ctrl) || ctrl < 0 || ctrl > 127)
                invalid (fmt::format ("{}.controller must be an integer 0..127, got {}", where, ctrl));
              if (value != std::floor (value) || value < 0 || value > 127)
                invalid (fmt::format ("{}.value for cc must be an integer 0..127, got {}", where, value));
              spec.controller = static_cast<int> (ctrl);
              spec.value = static_cast<int> (value);
            }
          else
            invalid (fmt::format ("{}.type must be \"pitch_bend\" or \"cc\", got \"{}\"", where, type));
          events.push_back (spec);
        }
    }

  json change{
    { "op",          "add_midi_clip"   },
    { "track",       id_of (*track)    },
    { "start_bar",   start_bar         },
    { "length_bars", length_bars       },
    { "notes",       specs.size ()     },
    { "events",      events.size ()    },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  // Build the clip completely before it joins the project, then add it with
  // one undoable command
  auto &factory = *s.project ().arrangerObjectFactory ();
  auto  clip_ref = factory.get_builder<structure::arrangement::MidiClip> ()
                    .with_start_ticks (units::ticks (clip_start))
                    .build_in_registry ();
  auto * clip = clip_ref.get_object_as<structure::arrangement::MidiClip> ();
  for (const auto &spec : specs)
    {
      auto note_ref = factory.get_builder<structure::arrangement::MidiNote> ()
                        .with_pitch (spec.pitch)
                        .with_velocity (spec.velocity)
                        .with_start_ticks (units::ticks (spec.start))
                        .with_end_ticks (units::ticks (spec.end))
                        .with_midi_channel (spec.channel)
                        .build_in_registry ();
      clip->structure::arrangement::ArrangerObjectOwner<
        structure::arrangement::MidiNote>::add_object (note_ref);
    }
  for (const auto &ev : events)
    {
      auto ev_ref = factory.get_builder<structure::arrangement::MidiControlEvent> ()
                      .with_start_ticks (units::ticks (ev.start))
                      .build_in_registry ();
      auto * obj = ev_ref.get_object_as<structure::arrangement::MidiControlEvent> ();
      obj->setControlType (ev.type);
      obj->setChannel (ev.channel);
      obj->setController (ev.controller);
      obj->setValue (ev.value);
      clip->structure::arrangement::ArrangerObjectOwner<
        structure::arrangement::MidiControlEvent>::add_object (ev_ref);
    }
  clip->set_loop_range (
    dsp::ContentTick (units::ticks (0.0)), dsp::ContentTick (units::ticks (0.0)),
    dsp::ContentTick (units::ticks (clip_len)));
  clip->length ()->setTicks (clip_len);
  clip->setTrackBounds (true);
  clip->name ()->setName (
    name ? QString::fromStdString (*name)
         : track->generate_name_for_clip (*clip).to_qstring ());

  auto * lane = track->lanes ()->at (0);
  s.undo_stack ().push (
    new commands::AddArrangerObjectCommand<structure::arrangement::MidiClip> (
      commands::make_owner_ref (*lane, factory.registry ()), clip_ref));

  return {
    { "clip",    id_of (*clip)            },
    { "track",   id_of (*track)           },
    { "changes", json::array ({ change }) },
  };
}

json
AgentApi::import_file (const json &p, bool midi)
{
  auto      &s = require_session ();
  const auto path = std::filesystem::absolute (get_string (p, "path"));
  const double start_bar = check_bar (
    opt_number (p, "start_bar").value_or (1.0), "start_bar");
  if (!std::filesystem::is_regular_file (path))
    {
      throw RpcError (
        rpc_error::kNotFound, fmt::format ("file not found: {}", path.string ()));
    }
  auto      &importer = s.file_importer ();
  const auto qpath = QString::fromStdString (path.string ());
  if (midi ? !importer.isMidiFile (qpath) : !importer.isAudioFile (qpath))
    {
      throw RpcError (
        rpc_error::kWrongType,
        fmt::format (
          "{} is not a readable {} file", path.string (), midi ? "MIDI" : "audio"));
    }
  Track * track = nullptr;
  if (const auto tid = opt_string (p, "track"))
    {
      track = find_track (s, *tid);
      const bool ok =
        midi ? (track->type () == Track::Type::Midi || track->type () == Track::Type::Instrument)
             : track->type () == Track::Type::Audio;
      if (!ok)
        {
          throw RpcError (
            rpc_error::kWrongType,
            fmt::format (
              "track '{}' is {}; {} files need {}", track->name ().toStdString (),
              track_type_name (track->type ()), midi ? "MIDI" : "audio",
              midi ? "a midi or instrument track" : "an audio track"));
        }
    }
  json change{
    { "op",        std::string (midi ? "import_midi" : "import_audio") },
    { "path",      path.string ()                        },
    { "start_bar", start_bar                             },
    { "track",     track ? json (id_of (*track)) : json (nullptr) },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  const auto tracks_before = all_track_ids (s);
  const auto clips_before = all_clip_ids (s);
  QString    failure;
  const auto conn = QObject::connect (
    &importer, &actions::FileImporter::importFailed, &importer,
    [&failure] (const QString &, const QString &reason) { failure = reason; });
  importer.importFiles (
    { qpath }, bar_to_ticks (s.project ().tempo_map (), start_bar), track);
  QObject::disconnect (conn);
  if (!failure.isEmpty ())
    throw std::runtime_error (failure.toStdString ());

  json new_tracks = json::array ();
  for (const auto &id : all_track_ids (s))
    if (!tracks_before.contains (id))
      new_tracks.push_back (id);
  json new_clips = json::array ();
  for (const auto &id : all_clip_ids (s))
    if (!clips_before.contains (id))
      new_clips.push_back (id);
  if (new_clips.empty ())
    throw std::runtime_error ("the file was read but produced no clip");
  return {
    { "tracks",  new_tracks               },
    { "clips",   new_clips                },
    { "changes", json::array ({ change }) },
  };
}

json
AgentApi::set_tempo (const json &p)
{
  auto        &s = require_session ();
  const double bpm = get_number (p, "bpm");
  if (bpm < 20.0 || bpm > 999.0)
    invalid (fmt::format ("'bpm' must be between 20 and 999, got {}", bpm));
  const double at_bar = check_bar (opt_number (p, "at_bar").value_or (1.0), "at_bar");
  auto        &project = s.project ();
  const double ticks = bar_to_ticks (project.tempo_map (), at_bar);
  auto *       manager = project.tempoObjectManager ();

  structure::arrangement::TempoObject * existing = nullptr;
  for (auto * t :
       manager->structure::arrangement::ArrangerObjectOwner<
         structure::arrangement::TempoObject>::get_children_view ())
    if (std::abs (t->position ()->ticks () - ticks) < 0.5)
      existing = t;

  json change{
    { "op",     std::string (existing ? "change_tempo" : "add_tempo") },
    { "bar",    at_bar                                   },
    { "bpm",    bpm                                      },
  };
  if (existing)
    change["from"] = existing->tempo ();
  else if (at_bar == 1.0)
    change["from"] = project.tempo_map ().base_bpm ().in (units::bpm);
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  if (existing)
    {
      s.undo_stack ().push (
        new commands::ChangeTempoMapAffectingQObjectPropertyCommand (
          *existing, QStringLiteral ("tempo"), bpm));
      return {
        { "tempo",   id_of (*existing)        },
        { "changes", json::array ({ change }) },
      };
    }
  auto &factory = *project.arrangerObjectFactory ();
  auto  ref = factory.get_builder<structure::arrangement::TempoObject> ()
               .with_start_ticks (units::ticks (ticks))
               .build_in_registry ();
  auto * tempo = ref.get_object_as<structure::arrangement::TempoObject> ();
  tempo->setTempo (bpm);
  s.undo_stack ().push (
    new commands::AddArrangerObjectCommand<structure::arrangement::TempoObject> (
      commands::make_owner_ref (*manager, factory.registry ()), ref));
  return {
    { "tempo",   id_of (*tempo)           },
    { "changes", json::array ({ change }) },
  };
}

json
AgentApi::add_marker (const json &p)
{
  auto      &s = require_session ();
  const auto name = get_string (p, "name");
  if (name.empty ())
    invalid ("'name' must not be empty");
  const double bar = check_bar (get_number (p, "bar"), "bar");
  json         change{
    { "op",   "add_marker" },
    { "name", name         },
    { "bar",  bar          },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));
  auto * marker_track =
    s.project ().tracklist ()->singletonTracks ()->markerTrack ();
  auto * marker = s.arranger_object_creator ().addMarker (
    structure::arrangement::Marker::MarkerType::Custom, marker_track,
    QString::fromStdString (name), bar_to_ticks (s.project ().tempo_map (), bar));
  if (marker == nullptr)
    throw std::runtime_error ("failed to add the marker");
  return {
    { "marker",  id_of (*marker)          },
    { "changes", json::array ({ change }) },
  };
}

json
AgentApi::set_loop_range (const json &p)
{
  auto        &s = require_session ();
  const double start_bar = check_bar (get_number (p, "start_bar"), "start_bar");
  const double end_bar = check_bar (get_number (p, "end_bar"), "end_bar");
  if (end_bar <= start_bar)
    invalid ("'end_bar' must be after 'start_bar'");
  const bool enabled = opt_bool (p, "enabled", true);
  json       change{
    { "op",        "set_loop_range" },
    { "start_bar", start_bar        },
    { "end_bar",   end_bar          },
    { "enabled",   enabled          },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  auto      &tm = s.project ().tempo_map ();
  auto *     transport = s.project ().getTransport ();
  auto *     loop_start = transport->loopStartPosition ();
  auto *     loop_end = transport->loopEndPosition ();
  const auto start = bar_to_ticks (tm, start_bar);
  const auto end = bar_to_ticks (tm, end_bar);
  undo::UndoStack::ScopedMacro macro (s.undo_stack (), QObject::tr ("Set loop range"));
  // Order the moves so the start never passes the end
  if (start >= loop_end->ticks ())
    {
      s.undo_stack ().push (new commands::ChangeQObjectPropertyCommand (
        *loop_end, QStringLiteral ("ticks"), end));
      s.undo_stack ().push (new commands::ChangeQObjectPropertyCommand (
        *loop_start, QStringLiteral ("ticks"), start));
    }
  else
    {
      s.undo_stack ().push (new commands::ChangeQObjectPropertyCommand (
        *loop_start, QStringLiteral ("ticks"), start));
      s.undo_stack ().push (new commands::ChangeQObjectPropertyCommand (
        *loop_end, QStringLiteral ("ticks"), end));
    }
  if (transport->loopEnabled () != enabled)
    {
      s.undo_stack ().push (new commands::ChangeQObjectPropertyCommand (
        *transport, QStringLiteral ("loopEnabled"), enabled));
    }
  return { { "changes", json::array ({ change }) } };
}

json
AgentApi::set_range (const json &p)
{
  auto        &s = require_session ();
  const double start_bar = check_bar (get_number (p, "start_bar"), "start_bar");
  const double end_bar = check_bar (get_number (p, "end_bar"), "end_bar");
  if (end_bar <= start_bar)
    invalid ("'end_bar' must be after 'start_bar'");
  json change{
    { "op",        "set_range" },
    { "start_bar", start_bar   },
    { "end_bar",   end_bar     },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));

  auto      &project = s.project ();
  auto      &tm = project.tempo_map ();
  auto *     marker_track = project.tracklist ()->singletonTracks ()->markerTrack ();
  auto *     start_marker = marker_track->get_start_marker ();
  auto *     end_marker = marker_track->get_end_marker ();
  const auto start = bar_to_ticks (tm, start_bar);
  const auto end = bar_to_ticks (tm, end_bar);
  const auto move = [&] (structure::arrangement::Marker * m, double target) {
    const double delta = target - m->position ()->ticks ();
    if (std::abs (delta) < 1e-9)
      return;
    std::vector<structure::arrangement::ArrangerObjectUuidReference> objs;
    objs.emplace_back (m->get_uuid (), project.get_registry ());
    s.undo_stack ().push (
      new commands::MoveArrangerObjectsCommand (std::move (objs), units::ticks (delta)));
  };
  undo::UndoStack::ScopedMacro macro (s.undo_stack (), QObject::tr ("Set song range"));
  if (start >= end_marker->position ()->ticks ())
    {
      move (end_marker, end);
      move (start_marker, start);
    }
  else
    {
      move (start_marker, start);
      move (end_marker, end);
    }
  return { { "changes", json::array ({ change }) } };
}

json
AgentApi::render (const json &p)
{
  auto &s = require_session ();
  auto  opts = RenderOptions::from_json (p);
  opts.title = s.title ();
  if (is_dry_run (p))
    {
      // Resolve and validate without rendering
      json change{
        { "op",     "render"         },
        { "out",    std::filesystem::absolute (opts.out).string () },
        { "stems",  opts.stems       },
        { "bits",   opts.bits == 32 ? json ("32f") : json (opts.bits) },
      };
      return dry_run_result (json::array ({ change }));
    }
  if (turn_)
    {
      // Rendering is not an edit; it is allowed inside a turn
    }
  if (plugin_state_dirty_)
    {
      warm_up_plugins (s.project ());
      plugin_state_dirty_ = false;
    }
  if (notify_)
    {
      // At most one notification per percent (and always the last block)
      auto last = std::make_shared<int> (-1);
      opts.progress = [notify = notify_, id = current_id_, last] (
                        int64_t done, int64_t total, const std::string &track) {
        const int pct = total > 0 ? static_cast<int> (done * 100 / total) : 100;
        if (pct == *last)
          return;
        *last = pct;
        notify (json{
          { "jsonrpc", "2.0" },
          { "method", "progress" },
          { "params",
            { { "request", id },
              { "op", "render" },
              { "fraction", total > 0 ? round6 (static_cast<double> (done) / static_cast<double> (total)) : 1.0 },
              { "frames", done },
              { "total_frames", total },
              { "track", track.empty () ? json (nullptr) : json (track) } } } });
      };
    }
  return render_project (s.project (), opts).to_json ();
}

// ===========================================================================
// History
// ===========================================================================

json
AgentApi::undo_redo (const json &p, bool undo)
{
  auto &s = require_session ();
  plugin_state_dirty_ = true;
  auto &stack = s.undo_stack ();
  if (turn_)
    {
      throw RpcError (
        rpc_error::kStateConflict,
        fmt::format (
          "cannot {} while agent turn '{}' is open (end_agent_turn first)",
          undo ? "undo" : "redo", turn_->label.toStdString ()));
    }
  if (undo ? !stack.canUndo () : !stack.canRedo ())
    {
      throw RpcError (
        rpc_error::kStateConflict, undo ? "nothing to undo" : "nothing to redo");
    }
  const auto text = undo ? stack.text (stack.index () - 1) : stack.text (stack.index ());
  json       change{
    { "op",   std::string (undo ? "undo" : "redo") },
    { "text", text.toStdString ()    },
  };
  if (is_dry_run (p))
    return dry_run_result (json::array ({ change }));
  if (undo)
    stack.undo ();
  else
    stack.redo ();
  return {
    { "changes", json::array ({ change }) },
    { "history", history ()               },
  };
}

json
AgentApi::begin_agent_turn (const json &p)
{
  auto      &s = require_session ();
  const auto label = get_string (p, "label");
  if (label.empty ())
    invalid ("'label' must not be empty");
  if (turn_)
    {
      throw RpcError (
        rpc_error::kStateConflict,
        fmt::format ("agent turn '{}' is already open", turn_->label.toStdString ()));
    }
  if (is_dry_run (p))
    return dry_run_result (json::array ({ { { "op", "begin_agent_turn" }, { "label", label } } }));
  const auto qlabel = QString::fromStdString (label);
  s.undo_stack ().beginMacro (QStringLiteral ("Agent: %1").arg (qlabel));
  turn_ = Turn{ qlabel, s.undo_stack ().index () };
  return { { "label", label }, { "history", history () } };
}

json
AgentApi::end_agent_turn (const json &p)
{
  auto &s = require_session ();
  if (!turn_)
    throw RpcError (rpc_error::kStateConflict, "no agent turn is open");
  const bool discard = opt_bool (p, "discard", false);
  if (is_dry_run (p))
    {
      return dry_run_result (json::array (
        { { { "op", "end_agent_turn" },
            { "label", turn_->label.toStdString () },
            { "discard", discard } } }));
    }
  auto &stack = s.undo_stack ();
  stack.endMacro ();
  const auto label = turn_->label;
  const bool recorded = stack.index () != turn_->start_index;
  turn_.reset ();
  if (discard && recorded)
    stack.undo ();
  return {
    { "label",     label.toStdString () },
    { "recorded",  recorded && !discard },
    { "discarded", discard && recorded  },
    { "history",   history ()           },
  };
}

} // namespace flux::engine
