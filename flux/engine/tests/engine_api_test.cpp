// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file
 * Integration tests for flux-engine's agent API and offline render, using
 * the bundled CLAP "Test Synth".
 */

#include <cmath>
#include <filesystem>
#include <fstream>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "agent_api.h"
#include "engine_host.h"
#include "plugin_catalog.h"

#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>

namespace flux::engine::tests
{

using json = nlohmann::json;

class EngineApiTest : public ::testing::Test
{
protected:
  void SetUp () override
  {
    int     argc = 0;
    char ** argv = nullptr;
    app_ = std::make_unique<QCoreApplication> (argc, argv);
    host_ = std::make_unique<EngineHost> (EngineHost::Options{});
    catalog_ = std::make_unique<PluginCatalog> ();
    const auto paths =
      QStringLiteral (TEST_CLAP_SEARCH_PATHS).split (QStringLiteral (":::"), Qt::SkipEmptyParts);
    for (const auto &p : paths)
      catalog_->add_search_path ("clap", p.toStdString ());
    api_ = std::make_unique<AgentApi> (*host_, *catalog_);
    ASSERT_TRUE (tmp_.isValid ());
    root_ = std::filesystem::path (tmp_.path ().toStdString ());
  }

  void TearDown () override
  {
    api_.reset ();
    catalog_.reset ();
    host_.reset ();
    app_.reset ();
  }

  json request (const std::string &method, json params = json::object ())
  {
    const json req{
      { "jsonrpc", "2.0"    },
      { "id",      next_id_++ },
      { "method",  method   },
      { "params",  params   },
    };
    auto resp = api_->handle (req);
    EXPECT_TRUE (resp.has_value ());
    return resp.value_or (json{});
  }

  /** Calls a method and expects success; returns the result. */
  json call (const std::string &method, json params = json::object ())
  {
    auto resp = request (method, std::move (params));
    EXPECT_TRUE (resp.contains ("result"))
      << method << " failed: " << resp.dump (2);
    return resp.value ("result", json{});
  }

  /** Calls a method and expects an error; returns the error code. */
  int call_error (const std::string &method, json params = json::object ())
  {
    auto resp = request (method, std::move (params));
    EXPECT_TRUE (resp.contains ("error"))
      << method << " unexpectedly succeeded: " << resp.dump (2);
    EXPECT_FALSE (resp.value ("error", json::object ())
                    .value ("message", std::string{})
                    .empty ());
    return resp.value ("error", json::object ()).value ("code", 0);
  }

  std::string find_test_synth_id ()
  {
    auto plugins = call ("list_plugins", { { "format", "clap" }, { "query", "Test Synth" } });
    for (const auto &p : plugins["plugins"])
      if (p["name"] == std::string ("Test Synth"))
        return p["id"];
    ADD_FAILURE () << "Test Synth CLAP not found: " << plugins.dump (2);
    return {};
  }

  static json one_note_per_beat (int bars, int beats_per_bar = 4)
  {
    json notes = json::array ();
    for (int b = 0; b < bars * beats_per_bar; ++b)
      {
        notes.push_back (
          { { "pitch", 60 + (b % 8) },
            { "start_beat", b },
            { "length_beats", 0.9 },
            { "velocity", 100 } });
      }
    return notes;
  }

  /** Builds the 2-track, 8-bar project used by several tests. */
  void build_two_track_project (const std::filesystem::path &dir)
  {
    call ("create_project", { { "dir", dir.string () }, { "bpm", 120 } });
    const auto synth = find_test_synth_id ();
    call ("begin_agent_turn", { { "label", "build" } });
    lead_ = call (
      "add_track",
      { { "type", "instrument" }, { "name", "Lead" }, { "plugin_id", synth } })["track"];
    call ("add_track", { { "type", "midi" }, { "name", "Ideas" } });
    call (
      "write_notes",
      { { "track", lead_ },
        { "start_bar", 1 },
        { "length_bars", 8 },
        { "notes", one_note_per_beat (8) } });
    call ("set_range", { { "start_bar", 1 }, { "end_bar", 9 } });
    call ("end_agent_turn");
  }

  std::unique_ptr<QCoreApplication> app_;
  std::unique_ptr<EngineHost>       host_;
  std::unique_ptr<PluginCatalog>    catalog_;
  std::unique_ptr<AgentApi>         api_;
  QTemporaryDir                     tmp_;
  std::filesystem::path             root_;
  int                               next_id_ = 1;
  std::string                       lead_;
};

TEST_F (EngineApiTest, BuildSaveReopenRender)
{
  const auto dir = root_ / "song";
  build_two_track_project (dir);

  auto before = call ("describe_project", { { "include_notes", true } });
  ASSERT_EQ (before["tracks"].size () >= 2, true);
  call ("save_project");

  // Reopen in a fresh session
  call ("open_project", { { "dir", dir.string () } });
  auto after = call ("describe_project", { { "include_notes", true } });
  EXPECT_EQ (before["tracks"], after["tracks"]);
  EXPECT_EQ (before["range"], after["range"]);
  EXPECT_EQ (after["tempo"]["bpm"], 120.0);

  // The lead track still has its plugin and an 8-bar clip with 32 notes
  bool found = false;
  for (const auto &t : after["tracks"])
    {
      if (t["id"] != lead_)
        continue;
      found = true;
      ASSERT_EQ (t["plugins"].size (), 1u);
      EXPECT_EQ (t["plugins"][0]["name"], std::string ("Test Synth"));
      EXPECT_EQ (t["plugins"][0]["status"], std::string ("ok"));
      ASSERT_EQ (t["clips"].size (), 1u);
      EXPECT_DOUBLE_EQ (t["clips"][0]["start_bar"].get<double> (), 1.0);
      EXPECT_DOUBLE_EQ (t["clips"][0]["end_bar"].get<double> (), 9.0);
      EXPECT_EQ (t["clips"][0]["note_count"], 32);
      EXPECT_EQ (t["clips"][0]["notes"][3]["velocity"], 100);
      EXPECT_DOUBLE_EQ (t["clips"][0]["notes"][3]["length_beats"].get<double> (), 0.9);
    }
  EXPECT_TRUE (found);

  // Render the start/end marker range (8 bars at 120 bpm = 16 s)
  const auto out = root_ / "song.wav";
  auto       result = call ("render", { { "out", out.string () } });
  ASSERT_TRUE (std::filesystem::exists (out));
  const double expected_frames = 16.0 * 48000.0;
  EXPECT_NEAR (result["frames"].get<double> (), expected_frames, 256.0);
  EXPECT_NEAR (result["duration_sec"].get<double> (), 16.0, 256.0 / 48000.0);
  ASSERT_FALSE (result["peak_dbfs"].is_null ()) << "render is silent";
  EXPECT_GT (result["peak_dbfs"].get<double> (), -40.0);
  EXPECT_GT (result["speed_factor"].get<double> (), 1.0);
  EXPECT_EQ (result["bits"], 24);

  // Check the file itself
  juce::AudioFormatManager formats;
  formats.registerBasicFormats ();
  std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (
    juce::File (juce::String (out.string ()))));
  ASSERT_NE (reader, nullptr);
  EXPECT_EQ (reader->sampleRate, 48000.0);
  EXPECT_EQ (reader->bitsPerSample, 24u);
  EXPECT_EQ (reader->numChannels, 2u);
  EXPECT_NEAR (static_cast<double> (reader->lengthInSamples), expected_frames, 256.0);
  juce::AudioSampleBuffer buffer (2, static_cast<int> (reader->lengthInSamples));
  reader->read (&buffer, 0, buffer.getNumSamples (), 0, true, true);
  EXPECT_GT (buffer.getRMSLevel (0, 0, buffer.getNumSamples ()), 1e-4f);

  // Stems: the mix plus one file per audio-output track (the MIDI track
  // "Ideas" has no audio output)
  const auto stems = call (
    "render", { { "out", (root_ / "stems" / "mix.wav").string () }, { "stems", true } });
  ASSERT_EQ (stems["outputs"].size (), 2u) << stems.dump (2);
  EXPECT_EQ (stems["outputs"][1]["track"], lead_);
  EXPECT_FALSE (stems["outputs"][1]["peak_dbfs"].is_null ());
  EXPECT_NEAR (
    stems["outputs"][1]["peak_dbfs"].get<double> (),
    stems["outputs"][0]["peak_dbfs"].get<double> (), 0.1)
    << "the only sounding track's stem matches the mix";
  for (const auto &o : stems["outputs"])
    EXPECT_TRUE (std::filesystem::exists (o["path"].get<std::string> ()));

  // A bar range and FLAC
  const auto flac = root_ / "bars.flac";
  auto       r2 = call (
    "render",
    { { "out", flac.string () }, { "start_bar", 3 }, { "end_bar", 5 }, { "bits", 16 } });
  EXPECT_EQ (r2["format"], std::string ("flac"));
  EXPECT_NEAR (r2["duration_sec"].get<double> (), 4.0, 256.0 / 48000.0);
  EXPECT_TRUE (std::filesystem::exists (flac));
}

TEST_F (EngineApiTest, UndoOfAgentTurnRestoresDescription)
{
  call ("create_project", { { "dir", (root_ / "undo").string () } });
  const auto synth = find_test_synth_id ();
  call ("begin_agent_turn", { { "label", "setup" } });
  const auto t = call (
    "add_track",
    { { "type", "instrument" }, { "name", "Keys" }, { "plugin_id", synth } })["track"];
  call (
    "write_notes",
    { { "track", t }, { "start_bar", 1 }, { "length_bars", 2 }, { "notes", one_note_per_beat (2) } });
  call ("end_agent_turn");
  const auto snapshot_a = call ("describe_project", { { "include_notes", true } }).dump ();

  // Turn 2: many edits, one undo step
  call ("begin_agent_turn", { { "label", "arrange" } });
  const auto plugin_id = call ("describe_project")["tracks"];
  std::string plugin;
  for (const auto &tr : plugin_id)
    if (tr["id"] == t)
      plugin = tr["plugins"][0]["id"];
  ASSERT_FALSE (plugin.empty ());
  const auto params = call ("list_params", { { "plugin", plugin } })["params"];
  ASSERT_FALSE (params.empty ());
  call (
    "set_params",
    { { "plugin", plugin },
      { "values", json::array ({ { { "id", params[0]["id"] }, { "value", 0.25 } } }) } });
  call ("add_track", { { "type", "audio" }, { "name", "Vox" } });
  call (
    "write_notes",
    { { "track", t }, { "start_bar", 3 }, { "length_bars", 4 }, { "notes", one_note_per_beat (4) } });
  call ("set_tempo", { { "bpm", 128 } });
  call ("set_tempo", { { "bpm", 100 }, { "at_bar", 5 } });
  call ("add_marker", { { "name", "Drop" }, { "bar", 5 } });
  call ("set_loop_range", { { "start_bar", 3 }, { "end_bar", 7 } });
  call ("set_range", { { "start_bar", 1 }, { "end_bar", 17 } });
  const auto end = call ("end_agent_turn");
  EXPECT_TRUE (end["recorded"].get<bool> ());
  const auto snapshot_b = call ("describe_project", { { "include_notes", true } }).dump ();
  EXPECT_NE (snapshot_a, snapshot_b);

  const auto params_after = call ("list_params", { { "plugin", plugin } })["params"];
  EXPECT_NEAR (params_after[0]["value"].get<double> (), 0.25, 1e-6);

  call ("undo");
  EXPECT_EQ (call ("describe_project", { { "include_notes", true } }).dump (), snapshot_a);
  const auto params_undone = call ("list_params", { { "plugin", plugin } })["params"];
  EXPECT_EQ (params_undone[0]["value"], params[0]["value"]);

  call ("redo");
  EXPECT_EQ (call ("describe_project", { { "include_notes", true } }).dump (), snapshot_b);

  // A discarded turn leaves nothing behind
  call ("begin_agent_turn", { { "label", "scratch" } });
  call ("add_track", { { "type", "midi" }, { "name", "Temp" } });
  call ("end_agent_turn", { { "discard", true } });
  EXPECT_EQ (call ("describe_project", { { "include_notes", true } }).dump (), snapshot_b);
}

TEST_F (EngineApiTest, DryRunChangesNothing)
{
  call ("create_project", { { "dir", (root_ / "dry").string () } });
  const auto before = call ("describe_project").dump ();
  auto       r = call (
    "add_track", { { "type", "midi" }, { "name", "X" }, { "dry_run", true } });
  EXPECT_TRUE (r["dry_run"].get<bool> ());
  EXPECT_EQ (r["changes"][0]["op"], std::string ("add_track"));
  call ("set_tempo", { { "bpm", 90 }, { "dry_run", true } });
  call ("add_marker", { { "name", "A" }, { "bar", 2 }, { "dry_run", true } });
  EXPECT_EQ (call ("describe_project").dump (), before);
  EXPECT_FALSE (call ("get_history")["can_undo"].get<bool> ());

  // The applied call reports the same change list
  auto applied = call ("add_track", { { "type", "midi" }, { "name", "X" } });
  EXPECT_EQ (applied["changes"], r["changes"]);

  // An empty clip is allowed
  const auto empty = call (
    "write_notes",
    { { "track", applied["track"] }, { "start_bar", 2 }, { "length_bars", 1 }, { "notes", json::array () } });
  EXPECT_FALSE (empty["clip"].get<std::string> ().empty ());
}

TEST_F (EngineApiTest, MalformedRequestsReturnErrors)
{
  // Transport-level errors
  auto parse = api_->handle_line ("{not json");
  ASSERT_TRUE (parse.has_value ());
  EXPECT_EQ ((*parse)["error"]["code"], rpc_error::kParseError);
  auto no_method = api_->handle (json{ { "jsonrpc", "2.0" }, { "id", 1 } });
  EXPECT_EQ ((*no_method)["error"]["code"], rpc_error::kInvalidRequest);
  auto bad_version =
    api_->handle (json{ { "jsonrpc", "1.0" }, { "id", 1 }, { "method", "ping" } });
  EXPECT_EQ ((*bad_version)["error"]["code"], rpc_error::kInvalidRequest);
  auto positional = api_->handle (
    json{ { "jsonrpc", "2.0" }, { "id", 1 }, { "method", "ping" }, { "params", json::array () } });
  EXPECT_EQ ((*positional)["error"]["code"], rpc_error::kInvalidParams);
  EXPECT_FALSE (
    api_->handle (json{ { "jsonrpc", "2.0" }, { "method", "ping" } }).has_value ())
    << "notifications get no response";
  EXPECT_EQ (call_error ("no_such_method"), rpc_error::kMethodNotFound);

  // No project yet
  EXPECT_EQ (call_error ("describe_project"), rpc_error::kNoProject);
  EXPECT_EQ (call_error ("undo"), rpc_error::kNoProject);

  call ("create_project", { { "dir", (root_ / "bad").string () } });
  EXPECT_EQ (
    call_error ("create_project", { { "dir", (root_ / "bad").string () } }),
    rpc_error::kInvalidParams)
    << "directory exists and is not empty";
  EXPECT_EQ (call_error ("open_project", { { "dir", (root_ / "missing").string () } }), rpc_error::kNotFound);

  const auto midi = call ("add_track", { { "type", "midi" }, { "name", "M" } })["track"];
  const auto audio = call ("add_track", { { "type", "audio" }, { "name", "A" } })["track"];
  EXPECT_EQ (call_error ("add_track", { { "type", "banjo" } }), rpc_error::kInvalidParams);
  EXPECT_EQ (call_error ("add_track", { { "type", 3 } }), rpc_error::kInvalidParams);
  EXPECT_EQ (
    call_error ("add_track", { { "type", "instrument" }, { "plugin_id", "nope" } }),
    rpc_error::kNotFound);

  const auto notes = [] (json n) {
    return json{ { "start_bar", 1 }, { "length_bars", 1 }, { "notes", json::array ({ n }) } };
  };
  auto p = notes ({ { "pitch", 200 }, { "start_beat", 0 }, { "length_beats", 1 } });
  p["track"] = midi;
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kInvalidParams);
  p = notes ({ { "pitch", 60 }, { "start_beat", 3.5 }, { "length_beats", 1 } });
  p["track"] = midi;
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kInvalidParams)
    << "note crosses the clip end";
  p = notes ({ { "pitch", 60 }, { "start_beat", 0 }, { "length_beats", 1 }, { "velocity", 0 } });
  p["track"] = midi;
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kInvalidParams);
  p = notes ({ { "pitch", 60 }, { "start_beat", 0 }, { "length_beats", 1 } });
  p["track"] = audio;
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kWrongType);
  p["track"] = "00000000-0000-0000-0000-000000000001";
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kNotFound);
  p["track"] = "not-a-uuid";
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kNotFound);
  p["track"] = midi;
  p["start_bar"] = 0;
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kInvalidParams);
  p["start_bar"] = "one";
  EXPECT_EQ (call_error ("write_notes", p), rpc_error::kInvalidParams);

  EXPECT_EQ (call_error ("set_tempo", { { "bpm", 5000 } }), rpc_error::kInvalidParams);
  EXPECT_EQ (call_error ("set_loop_range", { { "start_bar", 5 }, { "end_bar", 2 } }), rpc_error::kInvalidParams);
  EXPECT_EQ (call_error ("set_params", { { "plugin", "x" } }), rpc_error::kNotFound);
  EXPECT_EQ (call_error ("import_audio", { { "path", "/no/such.wav" } }), rpc_error::kNotFound);
  EXPECT_EQ (
    call_error ("render", { { "out", (root_ / "x.wav").string () }, { "bits", 12 } }),
    rpc_error::kInvalidParams);
  EXPECT_EQ (
    call_error ("render", { { "out", (root_ / "x.flac").string () }, { "bits", "32f" } }),
    rpc_error::kInvalidParams);
  EXPECT_EQ (
    call_error ("render", { { "out", (root_ / "x.wav").string () }, { "sr", 44100 } }),
    rpc_error::kInvalidParams);

  // History state errors
  EXPECT_EQ (call_error ("end_agent_turn"), rpc_error::kStateConflict);
  call ("begin_agent_turn", { { "label", "t" } });
  EXPECT_EQ (call_error ("begin_agent_turn", { { "label", "t2" } }), rpc_error::kStateConflict);
  EXPECT_EQ (call_error ("undo"), rpc_error::kStateConflict);
  call ("end_agent_turn");

  // Still healthy afterwards
  EXPECT_EQ (call ("ping")["pong"], true);
  EXPECT_EQ (call ("describe_project")["tracks"].is_array (), true);
}

TEST_F (EngineApiTest, ImportMidiAndSetParamsInPlainUnits)
{
  call ("create_project", { { "dir", (root_ / "imp").string () } });
  const auto synth = find_test_synth_id ();
  const auto added = call (
    "add_track", { { "type", "instrument" }, { "name", "S" }, { "plugin_id", synth } });
  const std::string plugin = added["plugin"]["id"];
  const auto        params = call ("list_params", { { "plugin", plugin } })["params"];
  // Find a parameter with a non-trivial plain range
  for (const auto &prm : params)
    {
      const double lo = prm["min"], hi = prm["max"];
      if (hi <= lo || prm["type"] == std::string ("enum") || prm["type"] == std::string ("toggle"))
        continue;
      const double mid = lo + (hi - lo) / 2.0;
      auto         r = call (
        "set_params",
        { { "plugin", plugin },
          { "values", json::array ({ { { "id", prm["id"] }, { "value", mid }, { "unit", "plain" } } }) } });
      EXPECT_NEAR (r["changes"][0]["plain_to"].get<double> (), mid, (hi - lo) * 1e-3);
      EXPECT_EQ (
        call_error (
          "set_params",
          { { "plugin", plugin },
            { "values", json::array ({ { { "id", prm["id"] }, { "value", hi + (hi - lo) }, { "unit", "plain" } } }) } }),
        rpc_error::kInvalidParams);
      break;
    }

  // Import a MIDI file from the upstream test data
  const auto midi_file =
    std::filesystem::path (TEST_MIDI_FILES_DIR) / "1_track_with_data.mid";
  ASSERT_TRUE (std::filesystem::exists (midi_file));
  auto r = call ("import_midi", { { "path", midi_file.string () }, { "start_bar", 2 } });
  EXPECT_FALSE (r["clips"].empty ());

  // Audio import into an existing audio track
  const auto audio = call ("add_track", { { "type", "audio" }, { "name", "Loop" } })["track"];
  auto       ra = call (
    "import_audio",
    { { "track", audio }, { "path", std::string (TEST_WAV_FILE_PATH) }, { "start_bar", 1 } });
  ASSERT_EQ (ra["clips"].size (), 1u);
  EXPECT_TRUE (ra["tracks"].empty ());
  EXPECT_EQ (
    call_error (
      "import_audio",
      { { "track", added["track"] }, { "path", std::string (TEST_WAV_FILE_PATH) } }),
    rpc_error::kWrongType)
    << "audio needs an audio track";

  // A second instrument on the same track is refused
  EXPECT_EQ (
    call_error ("load_plugin", { { "track", added["track"] }, { "plugin_id", synth } }),
    rpc_error::kStateConflict);
}

TEST_F (EngineApiTest, PresetMethodsValidateInput)
{
  call ("create_project", { { "dir", (root_ / "pre").string () } });
  const auto synth = find_test_synth_id ();
  const std::string plugin = call (
    "add_track", { { "type", "instrument" }, { "name", "S" }, { "plugin_id", synth } })["plugin"]["id"];
  // Test Synth has no CLAP preset-discovery provider
  const auto listed = call ("list_presets", { { "plugin", plugin } });
  EXPECT_EQ (listed["total"], 0);
  EXPECT_TRUE (listed["presets"].empty ());
  EXPECT_TRUE (listed["selected"].is_null ());
  EXPECT_EQ (
    call_error ("load_preset", { { "plugin", plugin }, { "preset", "Anything" } }),
    rpc_error::kWrongType);
  EXPECT_EQ (
    call_error ("list_presets", { { "plugin", plugin }, { "limit", 0 } }),
    rpc_error::kInvalidParams);
  EXPECT_EQ (
    call_error ("load_preset", { { "plugin", "not-a-uuid" }, { "preset", 0 } }),
    rpc_error::kNotFound);
}

TEST_F (EngineApiTest, WriteNotesCarriesChannelsAndControlEvents)
{
  call ("create_project", { { "dir", (root_ / "ev").string () } });
  const auto synth = find_test_synth_id ();
  const std::string track = call (
    "add_track", { { "type", "instrument" }, { "name", "S" }, { "plugin_id", synth } })["track"];
  json notes = json::array ({
    { { "pitch", 60 }, { "start_beat", 0 }, { "length_beats", 1 }, { "channel", 3 } } });
  json events = json::array ({
    { { "type", "pitch_bend" }, { "beat", 0 }, { "value", 0.25 }, { "channel", 3 } },
    { { "type", "cc" }, { "beat", 1 }, { "controller", 74 }, { "value", 90 } } });
  auto r = call (
    "write_notes",
    { { "track", track }, { "start_bar", 1 }, { "length_bars", 1 }, { "notes", notes }, { "events", events } });
  EXPECT_EQ (r["changes"][0]["events"], 2);
  const auto bad = [&] (json ev) {
    return call_error (
      "write_notes",
      { { "track", track }, { "start_bar", 2 }, { "length_bars", 1 }, { "notes", notes },
        { "events", json::array ({ ev }) } });
  };
  EXPECT_EQ (bad ({ { "type", "pitch_bend" }, { "beat", 0 }, { "value", 1.5 } }), rpc_error::kInvalidParams);
  EXPECT_EQ (bad ({ { "type", "cc" }, { "beat", 0 }, { "value", 10 } }), rpc_error::kInvalidParams)
    << "cc needs a controller";
  EXPECT_EQ (bad ({ { "type", "aftertouch" }, { "beat", 0 }, { "value", 1 } }), rpc_error::kInvalidParams);
  EXPECT_EQ (bad ({ { "type", "cc" }, { "beat", 9 }, { "controller", 1 }, { "value", 1 } }), rpc_error::kInvalidParams)
    << "beat outside the clip";
  json bad_note = json::array ({ { { "pitch", 60 }, { "start_beat", 0 }, { "length_beats", 1 }, { "channel", 16 } } });
  EXPECT_EQ (
    call_error ("write_notes", { { "track", track }, { "start_bar", 2 }, { "length_bars", 1 }, { "notes", bad_note } }),
    rpc_error::kInvalidParams);
  // Survives save and reopen
  call ("save_project");
  const auto dir = (root_ / "ev").string ();
  call ("open_project", { { "dir", dir } });
  const auto d = call ("describe_project", { { "include_notes", true } });
  EXPECT_FALSE (d["tracks"].empty ());
}

namespace
{
juce::AudioSampleBuffer
read_audio (const std::string &path)
{
  juce::AudioFormatManager fm;
  fm.registerBasicFormats ();
  std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (path))));
  if (!r)
    return {};
  juce::AudioSampleBuffer buf (static_cast<int> (r->numChannels), static_cast<int> (r->lengthInSamples));
  r->read (&buf, 0, buf.getNumSamples (), 0, true, true);
  return buf;
}

/** Lag (in frames, within ±max_lag) that best aligns b to a over [from, to). */
int
best_lag (const juce::AudioSampleBuffer &a, const juce::AudioSampleBuffer &b, int from, int to, int max_lag)
{
  int    best = 0;
  double best_err = std::numeric_limits<double>::max ();
  for (int lag = -max_lag; lag <= max_lag; ++lag)
    {
      double err = 0;
      for (int i = from; i < to; i += 4)
        {
          const int k = i + lag;
          if (k < 0 || k >= b.getNumSamples () || i >= a.getNumSamples ())
            continue;
          const double d = a.getSample (0, i) - b.getSample (0, k);
          err += d * d;
        }
      if (err < best_err)
        {
          best_err = err;
          best = lag;
        }
    }
  return best;
}
} // namespace

TEST_F (EngineApiTest, RenderStreamsMetersAndTrimsLatency)
{
  std::vector<json> progress;
  api_->set_notifier ([&progress] (const json &n) { progress.push_back (n); });
  const auto synth = find_test_synth_id ();
  std::string latency_id;
  for (const auto &pl : call ("list_plugins", { { "query", "Test Latency" } })["plugins"])
    latency_id = pl["id"];
  ASSERT_FALSE (latency_id.empty ()) << "Test Latency CLAP not found";

  const auto make = [&] (const std::string &dir, bool with_latency) {
    call ("create_project", { { "dir", (root_ / dir).string () }, { "bpm", 120 } });
    const std::string t = call (
      "add_track", { { "type", "instrument" }, { "name", "S" }, { "plugin_id", synth } })["track"];
    call ("write_notes", { { "track", t }, { "start_bar", 1 }, { "length_bars", 2 },
                           { "notes", one_note_per_beat (2) } });
    if (with_latency)
      {
        const std::string fx =
          call ("add_track", { { "type", "audio" }, { "name", "FX" } })["track"];
        call ("load_plugin", { { "track", fx }, { "plugin_id", latency_id } });
      }
    call ("set_range", { { "start_bar", 1 }, { "end_bar", 3 } });
  };

  make ("plain", false);
  progress.clear ();
  const auto plain = call ("render", { { "out", (root_ / "plain.wav").string () }, { "bits", "32f" } });
  EXPECT_EQ (plain["frames"], 4 * 48000) << "2 bars at 120 BPM, nothing added";
  EXPECT_EQ (plain["latency_trimmed_frames"], 0);
  ASSERT_TRUE (plain["lufs"].is_number ());
  ASSERT_TRUE (plain["true_peak_dbtp"].is_number ());
  EXPECT_GE (plain["true_peak_dbtp"].get<double> () + 1e-6, plain["peak_dbfs"].get<double> ());
  EXPECT_LT (plain["lufs"].get<double> (), plain["peak_dbfs"].get<double> ());
  ASSERT_FALSE (progress.empty ());
  double prev = -1;
  for (const auto &n : progress)
    {
      EXPECT_EQ (n["method"].get<std::string> (), "progress");
      const double f = n["params"]["fraction"];
      EXPECT_GT (f, prev);
      prev = f;
    }
  EXPECT_DOUBLE_EQ (prev, 1.0);
  EXPECT_LE (progress.size (), 101u) << "at most one notification per percent";

  make ("latent", true);
  const auto latent = call ("render", { { "out", (root_ / "latent.wav").string () }, { "bits", "32f" } });
  EXPECT_EQ (latent["latency_trimmed_frames"], 256);
  EXPECT_EQ (latent["frames"], 4 * 48000) << "the preroll is not written";
  // Plugin delay compensation keeps notes where they are without the latent
  // plugin. Compare from the second beat: an event exactly at the range start
  // is already sent to instruments during the preroll (upstream scheduler
  // behaviour), so the first note's attack differs
  const auto plain_audio = read_audio ((root_ / "plain.wav").string ());
  const auto latent_audio = read_audio ((root_ / "latent.wav").string ());
  EXPECT_EQ (best_lag (plain_audio, latent_audio, 24000, 96000, 512), 0);
}

TEST_F (EngineApiTest, WriteAutomationSweepsAParameterInTheRender)
{
  call ("create_project", { { "dir", (root_ / "auto").string () }, { "bpm", 120 } });
  const auto        synth = find_test_synth_id ();
  const auto        added = call (
    "add_track", { { "type", "instrument" }, { "name", "S" }, { "plugin_id", synth } });
  const std::string track = added["track"], plugin = added["plugin"]["id"];
  call (
    "write_notes",
    { { "track", track }, { "start_bar", 1 }, { "length_bars", 4 }, { "notes", one_note_per_beat (4) } });
  call ("set_range", { { "start_bar", 1 }, { "end_bar", 5 } });

  // Validation
  EXPECT_EQ (
    call_error ("write_automation", { { "plugin", plugin }, { "param", "Level" }, { "points", json::array () } }),
    rpc_error::kInvalidParams);
  EXPECT_EQ (
    call_error (
      "write_automation",
      { { "plugin", plugin }, { "param", "No Such" }, { "points", json::array ({ { { "beat", 0 }, { "value", 1 } } }) } }),
    rpc_error::kNotFound);
  EXPECT_EQ (
    call_error (
      "write_automation",
      { { "plugin", plugin }, { "param", "Level" }, { "length_bars", 1 },
        { "points", json::array ({ { { "beat", 9 }, { "value", 1 } } }) } }),
    rpc_error::kInvalidParams)
    << "a point outside the clip is refused";

  // Quiet rendering first: level fixed low
  call ("set_params", { { "plugin", plugin }, { "values", json::array ({ { { "name", "Level" }, { "value", 0.05 } } }) } });
  const auto fixed = call ("render", { { "out", (root_ / "fixed.wav").string () }, { "bits", "32f" } });

  // Then a sweep from silent to full over the 4 bars (the last point at beat 16 needs 4 bars: default length)
  const auto r = call (
    "write_automation",
    { { "plugin", plugin }, { "param", "Level" }, { "start_bar", 1 },
      { "points", json::array ({ { { "beat", 0 }, { "value", 0.0 } }, { { "beat", 16 }, { "value", 1.0 } } }) } });
  EXPECT_EQ (r["points"], 2);
  EXPECT_EQ (r["changes"][0]["end_bar"], 5);
  const auto desc = call ("describe_project", json::object ());
  bool       listed = false;
  for (const auto &t : desc["tracks"])
    for (const auto &a : t["automation"])
      if (
        a["name"].get<std::string> () == "Level" && a["clips"].size () == 1
        && a["clips"][0]["point_count"].get<int> () == 2)
        listed = true;
  EXPECT_TRUE (listed) << desc.dump (2);

  call ("render", { { "out", (root_ / "swept.wav").string () }, { "bits", "32f" } });
  const auto swept = read_audio ((root_ / "swept.wav").string ());
  ASSERT_GT (swept.getNumSamples (), 0);
  const auto rms_of = [&] (const juce::AudioSampleBuffer &b, int from, int to) {
    double acc = 0;
    for (int i = from; i < to; ++i)
      acc += b.getSample (0, i) * b.getSample (0, i);
    return std::sqrt (acc / std::max (1, to - from));
  };
  const int bar = 2 * 48000; // 4 beats at 120 BPM
  const double first = rms_of (swept, 0, bar), last = rms_of (swept, 3 * bar, 4 * bar);
  EXPECT_GT (last, 4 * first) << "level automation 0 -> 1 should make bar 4 much louder than bar 1 ("
                              << first << " vs " << last << ")";
  const auto fixed_audio = read_audio ((root_ / "fixed.wav").string ());
  EXPECT_GT (last, 2 * rms_of (fixed_audio, 3 * bar, 4 * bar)) << "and louder than the fixed low level";

  // Undo removes the clip again
  call ("undo", json::object ());
  bool still = false;
  for (const auto &t : call ("describe_project", json::object ())["tracks"])
    if (!t["automation"].empty ())
      still = true;
  EXPECT_FALSE (still);
}

TEST_F (EngineApiTest, LoadPresetFromAFilePath)
{
  call ("create_project", { { "dir", (root_ / "pf").string () } });
  const auto        synth = find_test_synth_id ();
  const auto        added = call (
    "add_track", { { "type", "instrument" }, { "name", "S" }, { "plugin_id", synth } });
  const std::string plugin = added["plugin"]["id"];
  EXPECT_EQ (
    call_error ("load_preset", { { "plugin", plugin }, { "preset", (root_ / "missing.fxp").string () } }),
    rpc_error::kNotFound);
  {
    std::ofstream f (root_ / "not-a-preset.fxp");
    f << "nope";
  }
  const auto code = call_error (
    "load_preset", { { "plugin", plugin }, { "preset", (root_ / "not-a-preset.fxp").string () } });
  EXPECT_TRUE (code == rpc_error::kWrongType || code == rpc_error::kInternalError) << code;

  // With Surge XT installed, a factory patch loads from its path and changes the parameters
  std::string surge;
  for (const auto &pl : call ("list_plugins", { { "instruments_only", true } })["plugins"])
    if (pl["name"].get<std::string> () == "Surge XT")
      surge = pl["id"].get<std::string> ();
  const char * home = std::getenv ("HOME");
  const auto   fxp = std::filesystem::path (home ? home : "")
                   / "Library/Application Support/Surge XT/patches_factory/Basses/Deep End.fxp";
  if (surge.empty () || !std::filesystem::exists (fxp))
    GTEST_SKIP () << "Surge XT or its factory patches are not installed";
  const auto        st = call ("add_track", { { "type", "instrument" }, { "name", "Surge" }, { "plugin_id", surge } });
  const std::string sp = st["plugin"]["id"];
  const auto        before = call ("list_params", { { "plugin", sp } })["params"];
  const auto        r = call ("load_preset", { { "plugin", sp }, { "preset", fxp.string () } });
  EXPECT_EQ (r["preset"]["source"].get<std::string> (), "file");
  EXPECT_EQ (r["preset"]["name"].get<std::string> (), "Deep End");
  const auto after = call ("list_params", { { "plugin", sp } })["params"];
  int        changed = 0;
  for (size_t i = 0; i < before.size () && i < after.size (); ++i)
    if (before[i]["value"] != after[i]["value"])
      ++changed;
  EXPECT_GT (changed, 0) << "loading the patch should move parameters away from the init patch";
  call ("undo", json::object ());
  const auto restored = call ("list_params", { { "plugin", sp } })["params"];
  int        still = 0;
  for (size_t i = 0; i < before.size () && i < restored.size (); ++i)
    if (before[i]["value"] != restored[i]["value"])
      ++still;
  EXPECT_EQ (still, 0) << "undo restores the previous plugin state";
}

TEST_F (EngineApiTest, HistoryLabelsCloseProjectAndChangeNotifications)
{
  std::vector<json> changed;
  api_->set_notifier ([&changed] (const json &n) {
    if (n["method"].get<std::string> () == "project_changed")
      changed.push_back (n["params"]);
  });
  const auto dir = (root_ / "hist").string ();
  call ("create_project", { { "dir", dir } });
  ASSERT_EQ (changed.size (), 1u);
  EXPECT_EQ (changed.back ()["method"].get<std::string> (), "create_project");
  EXPECT_FALSE (changed.back ()["unsaved"].get<bool> ());

  const auto synth = find_test_synth_id ();
  call ("begin_agent_turn", { { "label", "add lead" } });
  const std::string t = call (
    "add_track", { { "type", "instrument" }, { "name", "Lead" }, { "plugin_id", synth } })["track"];
  call ("end_agent_turn");
  call ("begin_agent_turn", { { "label", "write melody" } });
  call ("write_notes", { { "track", t }, { "start_bar", 1 }, { "length_bars", 1 }, { "notes", one_note_per_beat (1) } });
  call ("end_agent_turn");
  call ("begin_agent_turn", { { "label", "nothing" } });
  const auto before_empty = changed.size ();
  call ("end_agent_turn");
  EXPECT_EQ (changed.size (), before_empty) << "an empty turn records nothing and does not notify";
  call ("describe_project");
  call ("add_marker", { { "name", "x" }, { "bar", 2 }, { "dry_run", true } });
  EXPECT_EQ (changed.size (), before_empty) << "reads and dry runs do not notify";

  // Revisions increase, edits mark the project unsaved
  for (size_t i = 1; i < changed.size (); ++i)
    EXPECT_GT (changed[i]["revision"].get<int64_t> (), changed[i - 1]["revision"].get<int64_t> ());
  EXPECT_TRUE (changed.back ()["unsaved"].get<bool> ());
  EXPECT_TRUE (changed.back ().contains ("history"));

  call ("undo");
  const auto h = call ("get_history");
  ASSERT_EQ (h["agent_turns"].size (), 2u);
  EXPECT_EQ (h["agent_turns"][0]["label"].get<std::string> (), "add lead");
  EXPECT_TRUE (h["agent_turns"][0]["done"].get<bool> ());
  EXPECT_EQ (h["agent_turns"][1]["label"].get<std::string> (), "write melody");
  EXPECT_FALSE (h["agent_turns"][1]["done"].get<bool> ()) << "undone";
  EXPECT_EQ (h["entries"].size (), 2u);
  EXPECT_TRUE (h["unsaved"].get<bool> ());
  EXPECT_EQ (call ("get_history", { { "limit", 1 } })["entries"].size (), 1u);

  // close_project guards unsaved work and open turns
  EXPECT_EQ (call_error ("close_project"), rpc_error::kStateConflict);
  call ("save_project");
  EXPECT_FALSE (changed.back ()["unsaved"].get<bool> ());
  call ("begin_agent_turn", { { "label", "open turn" } });
  EXPECT_EQ (call_error ("close_project"), rpc_error::kStateConflict);
  call ("end_agent_turn");
  const auto closed = call ("close_project");
  EXPECT_TRUE (closed["closed"].get<bool> ());
  EXPECT_EQ (changed.back ()["method"].get<std::string> (), "close_project");
  EXPECT_FALSE (changed.back ()["open"].get<bool> ());
  EXPECT_FALSE (call ("get_history")["open"].get<bool> ());
  EXPECT_EQ (call_error ("describe_project"), rpc_error::kNoProject);
  EXPECT_EQ (call_error ("close_project"), rpc_error::kStateConflict);

  // Reopen, edit, then close discarding the edit
  call ("open_project", { { "dir", dir } });
  call ("add_marker", { { "name", "m" }, { "bar", 2 } });
  EXPECT_EQ (call_error ("close_project"), rpc_error::kStateConflict);
  EXPECT_TRUE (call ("close_project", { { "discard_unsaved", true } })["closed"].get<bool> ());
}

} // namespace flux::engine::tests
