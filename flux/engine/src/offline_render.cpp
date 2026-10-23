// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "zrythm-config.h"

#include "offline_render.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

#include "dsp/graph_pruner.h"
#include "dsp/graph_renderer.h"
#include "plugins/clap_plugin.h"
#include "structure/project/project.h"
#include "structure/project/project_graph_builder.h"
#include "structure/tracks/track_all.h"

#include <fmt/format.h>
#include <QFuture>
#include <QTemporaryDir>

#include <juce_audio_formats/juce_audio_formats.h>

#include "engine_host.h"
#include "musical_time.h"

namespace flux::engine
{

using namespace zrythm;
using namespace std::chrono_literals;

namespace
{

/** dBFS of a linear peak, or null for digital silence. */
nlohmann::json
peak_to_dbfs_json (float gain)
{
  if (!(gain > 0.f))
    return nullptr;
  return round_decimals (20.0 * std::log10 (static_cast<double> (gain)), 2);
}

std::string
lowercase_extension (const std::filesystem::path &path)
{
  auto ext = path.extension ().string ();
  std::ranges::transform (ext, ext.begin (), [] (unsigned char c) {
    return static_cast<char> (std::tolower (c));
  });
  return ext;
}

void
write_audio_file (
  const std::filesystem::path   &path,
  const juce::AudioSampleBuffer &buffer,
  const std::string             &format,
  int                            bits,
  int                            sample_rate,
  const QString                 &title)
{
  const auto file = juce::File (juce::String::fromUTF8 (path.string ().c_str ()));
  if (!file.getParentDirectory ().createDirectory ())
    {
      throw std::runtime_error (
        fmt::format ("cannot create directory for {}", path.string ()));
    }
  if (file.exists () && !file.deleteFile ())
    throw std::runtime_error (fmt::format ("cannot replace {}", path.string ()));

  std::unique_ptr<juce::AudioFormat> audio_format;
  if (format == "flac")
    audio_format = std::make_unique<juce::FlacAudioFormat> ();
  else
    audio_format = std::make_unique<juce::WavAudioFormat> ();

  std::unordered_map<juce::String, juce::String> metadata;
  metadata.emplace ("software", juce::String (PROGRAM_NAME));
  if (!title.isEmpty ())
    metadata.emplace ("title", juce::String (title.toUtf8 ().constData ()));

  auto writer_options =
    juce::AudioFormatWriterOptions{}
      .withSampleRate (sample_rate)
      .withNumChannels (buffer.getNumChannels ())
      .withBitsPerSample (bits)
      .withSampleFormat (
        bits == 32
          ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
          : juce::AudioFormatWriterOptions::SampleFormat::integral)
      .withQualityOptionIndex (0);
  // WAV metadata (BWF/INFO); the FLAC writer takes none
  if (format == "wav")
    writer_options = writer_options.withMetadataValues (metadata);

  std::unique_ptr<juce::OutputStream> stream =
    std::make_unique<juce::FileOutputStream> (file);
  if (!dynamic_cast<juce::FileOutputStream &> (*stream).openedOk ())
    throw std::runtime_error (fmt::format ("cannot open {}", path.string ()));
  auto writer = audio_format->createWriterFor (stream, writer_options);
  if (writer == nullptr)
    {
      throw std::runtime_error (fmt::format (
        "cannot create a {} writer ({} bit) for {}", format, bits,
        path.string ()));
    }
  if (!writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples ()))
    throw std::runtime_error (fmt::format ("failed writing {}", path.string ()));
}

/**
 * @brief Renders the project's graph pruned to @p terminal_port.
 */
juce::AudioSampleBuffer
render_to_buffer (
  structure::project::Project &project,
  const dsp::graph::IProcessable     &terminal_port,
  dsp::GraphRenderer::SampleRange range)
{
  structure::project::ProjectGraphBuilder builder (
    project, project.metronome (), project.monitor_fader ());
  dsp::graph::Graph graph;
  builder.build_graph (graph);

  auto * node = graph.get_nodes ().find_node_for_processable (terminal_port);
  if (node == nullptr)
    throw std::runtime_error ("output port is not part of the graph");
  std::vector<std::reference_wrapper<dsp::graph::GraphNode>> terminals;
  terminals.emplace_back (*node);
  dsp::graph::GraphPruner::prune_graph_to_terminals (graph, terminals);

  dsp::GraphRenderer::RenderOptions options{
    .sample_rate_ = project.engine ()->sample_rate (),
    .block_length_ = units::samples (
      static_cast<int64_t> (
        project.engine ()->block_length ().in (units::samples)))
  };
  auto future = dsp::GraphRenderer::render_async (
    options, graph.steal_nodes (),
    [context = &project] (std::function<void ()> func) {
      QMetaObject::invokeMethod (context, func, Qt::BlockingQueuedConnection);
    },
    range, project.tempo_map ());

  // Node preparation runs on this (main) thread via blocking invocations,
  // so keep processing events while the render thread works
  process_events_until ([&future] () { return future.isFinished (); }, 24h);
  if (!future.isFinished ())
    {
      future.cancel ();
      future.waitForFinished ();
      throw std::runtime_error ("render timed out");
    }
  try
    {
      // Rethrows an exception stored by the renderer
      future.waitForFinished ();
    }
  catch (const std::exception &e)
    {
      throw std::runtime_error (fmt::format ("render failed: {}", e.what ()));
    }
  if (future.isCanceled () || future.resultCount () < 1)
    throw std::runtime_error ("render was cancelled");
  return future.takeResult ();
}

} // namespace

RenderOptions
RenderOptions::from_json (const nlohmann::json &j)
{
  if (!j.is_object ())
    throw std::invalid_argument ("render options must be an object");
  RenderOptions o;
  if (!j.contains ("out") || !j["out"].is_string ())
    throw std::invalid_argument ("'out' (output file path) is required");
  o.out = std::filesystem::path (j["out"].get<std::string> ());
  const auto opt_number = [&j] (const char * key) -> std::optional<double> {
    if (!j.contains (key) || j[key].is_null ())
      return std::nullopt;
    if (!j[key].is_number ())
      throw std::invalid_argument (fmt::format ("'{}' must be a number", key));
    return j[key].get<double> ();
  };
  o.start_bar = opt_number ("start_bar");
  o.end_bar = opt_number ("end_bar");
  o.start_sec = opt_number ("start_sec");
  o.end_sec = opt_number ("end_sec");
  if (j.contains ("format"))
    {
      if (!j["format"].is_string ())
        throw std::invalid_argument ("'format' must be \"wav\" or \"flac\"");
      o.format = j["format"].get<std::string> ();
    }
  if (j.contains ("bits"))
    {
      if (j["bits"].is_string () && j["bits"].get<std::string> () == "32f")
        o.bits = 32;
      else if (j["bits"].is_number_integer ())
        o.bits = j["bits"].get<int> ();
      else
        throw std::invalid_argument ("'bits' must be 16, 24 or \"32f\"");
    }
  if (const auto sr = opt_number ("sr"))
    o.sample_rate = static_cast<int> (*sr);
  if (j.contains ("stems"))
    {
      if (!j["stems"].is_boolean ())
        throw std::invalid_argument ("'stems' must be a boolean");
      o.stems = j["stems"].get<bool> ();
    }
  return o;
}

nlohmann::json
RenderResult::to_json () const
{
  nlohmann::json files_json = nlohmann::json::array ();
  for (const auto &f : files)
    {
      nlohmann::json fj{
        { "path",      f.path.string () },
        { "peak_dbfs", peak_to_dbfs_json (f.peak) },
      };
      if (!f.track_id.empty ())
        {
          fj["track"] = f.track_id;
          fj["track_name"] = f.track_name;
        }
      files_json.push_back (fj);
    }
  return {
    { "outputs",              files_json                                      },
    { "format",               format                                          },
    { "bits",                 bits == 32 ? nlohmann::json ("32f") : nlohmann::json (bits) },
    { "sample_rate",          sample_rate                                     },
    { "start_sec",            start_sec                                       },
    { "end_sec",              end_sec                                         },
    { "duration_sec",         duration_sec                                    },
    { "frames",               frames                                          },
    { "peak_dbfs",            peak_to_dbfs_json (peak)                        },
    { "wall_sec",             wall_sec                                        },
    { "speed_factor",         round_decimals (speed_factor, 2)      },
    { "clap_offline_plugins", clap_offline_plugins                            },
  };
}

void
warm_up_plugins (structure::project::Project &project)
{
  QTemporaryDir dir;
  if (!dir.isValid ())
    return;
  RenderOptions o;
  o.out = std::filesystem::path (dir.path ().toStdString ()) / "warmup.wav";
  o.start_sec = 0.0;
  o.end_sec = 1.0;
  o.bits = 16;
  render_project (project, o);
}

RenderResult
render_project (
  structure::project::Project &project,
  const RenderOptions         &options)
{
  // --- Validate options ---
  if (options.out.empty ())
    throw std::invalid_argument ("an output path is required");
  std::string format = options.format;
  if (format.empty ())
    format = lowercase_extension (options.out) == ".flac" ? "flac" : "wav";
  if (format != "wav" && format != "flac")
    {
      throw std::invalid_argument (
        fmt::format ("unsupported format '{}' (use wav or flac)", format));
    }
  if (options.bits != 16 && options.bits != 24 && options.bits != 32)
    throw std::invalid_argument ("bits must be 16, 24 or 32f");
  if (format == "flac" && options.bits == 32)
    throw std::invalid_argument ("FLAC supports 16 or 24 bits, not 32f");

  const int engine_sr = static_cast<int> (
    project.engine ()->sample_rate ().in (units::sample_rate));
  if (options.sample_rate && *options.sample_rate != engine_sr)
    {
      throw std::invalid_argument (fmt::format (
        "sample rate {} differs from the engine's {}; start flux-engine "
        "with --sr {}",
        *options.sample_rate, engine_sr, *options.sample_rate));
    }

  const bool has_bars = options.start_bar || options.end_bar;
  const bool has_secs = options.start_sec || options.end_sec;
  if (has_bars && has_secs)
    throw std::invalid_argument ("give the range in bars or seconds, not both");

  // --- Resolve the range in samples ---
  const auto &tempo_map = project.tempo_map ();
  const auto  ticks_to_samples = [&tempo_map] (double ticks) {
    return tempo_map
      .tick_to_samples_rounded (dsp::TimelineTick{ units::ticks (ticks) })
      .in (units::samples);
  };
  const auto * marker_track =
    project.tracklist ()->singletonTracks ()->markerTrack ();
  int64_t start_frames =
    ticks_to_samples (marker_track->get_start_marker ()->position ()->ticks ());
  int64_t end_frames =
    ticks_to_samples (marker_track->get_end_marker ()->position ()->ticks ());
  if (has_bars)
    {
      if (options.start_bar)
        {
          if (*options.start_bar < 1.0)
            throw std::invalid_argument ("start_bar must be >= 1");
          start_frames =
            ticks_to_samples (bar_to_ticks (tempo_map, *options.start_bar));
        }
      if (options.end_bar)
        {
          if (*options.end_bar < 1.0)
            throw std::invalid_argument ("end_bar must be >= 1");
          end_frames =
            ticks_to_samples (bar_to_ticks (tempo_map, *options.end_bar));
        }
    }
  else if (has_secs)
    {
      if (options.start_sec)
        {
          if (*options.start_sec < 0.0)
            throw std::invalid_argument ("start_sec must be >= 0");
          start_frames = std::llround (*options.start_sec * engine_sr);
        }
      if (options.end_sec)
        {
          if (*options.end_sec < 0.0)
            throw std::invalid_argument ("end_sec must be >= 0");
          end_frames = std::llround (*options.end_sec * engine_sr);
        }
    }
  if (end_frames <= start_frames)
    {
      throw std::invalid_argument (fmt::format (
        "empty render range ({:.3f} s to {:.3f} s)",
        static_cast<double> (start_frames) / engine_sr,
        static_cast<double> (end_frames) / engine_sr));
    }
  if (end_frames - start_frames > std::numeric_limits<int>::max ())
    throw std::invalid_argument ("render range too long");
  const dsp::GraphRenderer::SampleRange range{
    units::samples (start_frames), units::samples (end_frames)
  };

  // --- Targets: the mix, then one per track for stems ---
  struct Target
  {
    const dsp::graph::IProcessable * port;
    std::filesystem::path     path;
    std::string               track_id;
    std::string               track_name;
  };
  std::vector<Target> targets;
  auto * master = project.tracklist ()->singletonTracks ()->masterTrack ();
  targets.push_back ({ master->channel ()->audioOutPort (), options.out, {}, {} });
  if (options.stems)
    {
      const auto stem_dir = options.out.parent_path ();
      const auto stem_base = options.out.stem ().string ();
      const auto ext = format == "flac" ? ".flac" : ".wav";
      int        index = 0;
      for (const auto &track_ref : project.tracklist ()->collection ()->tracks ())
        {
          auto * track = track_ref.get ();
          if (track == master || track->channel () == nullptr)
            continue;
          if (track->output_signal_type () != dsp::PortType::Audio)
            continue;
          ++index;
          auto safe_name = track->name ().toStdString ();
          std::ranges::replace_if (
            safe_name,
            [] (char c) {
              return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?'
                     || c == '"' || c == '<' || c == '>' || c == '|';
            },
            '_');
          targets.push_back (
            { track->channel ()->audioOutPort (),
              stem_dir
                / fmt::format ("{} - {:02} {}{}", stem_base, index, safe_name, ext),
              type_safe::get (track->get_uuid ())
                .toString (QUuid::WithoutBraces)
                .toStdString (),
              track->name ().toStdString () });
        }
    }

  RenderResult result;
  result.format = format;
  result.bits = options.bits;
  result.sample_rate = engine_sr;
  result.start_sec = static_cast<double> (start_frames) / engine_sr;
  result.end_sec = static_cast<double> (end_frames) / engine_sr;

  const auto wall_start = std::chrono::steady_clock::now ();

  // --- Render ---
  // Pauses the engine and marks processing as owned by the render; the
  // finish() below (or the destructor, on an exception) hands it back
  dsp::AudioEngine::OfflineRenderSession render_session (*project.engine ());

  // Playback caches (the MIDI/audio events tracks play) are rebuilt on a
  // debounce timer after edits and loads. Rebuild them now, while processing
  // is paused, so the render sees the current content
  for (const auto &track_ref : project.tracklist ()->collection ()->tracks ())
    {
      auto * track = track_ref.get ();
      // Only tracks with a processor play back content
      if (track->get_track_processor () != nullptr)
        track->regeneratePlaybackCaches ({});
      if (auto * atl = track->automationTracklist ())
        {
          for (auto * at : atl->automation_tracks ())
            at->regeneratePlaybackCaches ({});
        }
    }

  // render.set() is a main-thread call, so switch CLAP plugins here (the
  // renderer's own offline toggle runs on its render thread)
  std::vector<plugins::ClapPlugin *> clap_offline;
  project.get_registry ().for_each_matching<plugins::Plugin> (
    [&clap_offline] (plugins::Plugin &plugin) {
      if (auto * clap = qobject_cast<plugins::ClapPlugin *> (&plugin))
        {
          if (clap->set_clap_render_offline (true))
            clap_offline.push_back (clap);
        }
    });
  result.clap_offline_plugins = static_cast<int> (clap_offline.size ());
  const auto restore_realtime = [&clap_offline] () {
    for (auto * clap : clap_offline)
      clap->set_clap_render_offline (false);
    clap_offline.clear ();
  };

  try
    {
      double total_audio_sec = 0;
      bool first = true;
      for (const auto &target : targets)
        {
          if (!first)
            {
              // A finished render releases its nodes' resources, including
              // faders shared with other tracks' solo checks. Re-prepare the
              // engine graph so every processor is ready for the next render
              // (OfflineRenderSession::finish() does the same at the end)
              project.engine ()->graph_dispatcher ().recalc_graph (false);
            }
          first = false;
          auto buffer = render_to_buffer (project, *target.port, range);
          const float peak =
            buffer.getMagnitude (0, buffer.getNumSamples ());
          write_audio_file (
            target.path, buffer, format, options.bits, engine_sr,
            options.title);
          RenderedFile file{
            .path = std::filesystem::absolute (target.path),
            .track_id = target.track_id,
            .track_name = target.track_name,
            .peak = peak,
          };
          if (target.track_id.empty ())
            {
              result.frames = buffer.getNumSamples ();
              result.duration_sec =
                static_cast<double> (result.frames) / engine_sr;
              result.peak = file.peak;
            }
          total_audio_sec +=
            static_cast<double> (buffer.getNumSamples ()) / engine_sr;
          result.files.push_back (std::move (file));
        }
      result.wall_sec =
        std::chrono::duration<double> (
          std::chrono::steady_clock::now () - wall_start)
          .count ();
      result.speed_factor =
        result.wall_sec > 0 ? total_audio_sec / result.wall_sec : 0;
    }
  catch (...)
    {
      restore_realtime ();
      render_session.finish ();
      throw;
    }
  restore_realtime ();
  render_session.finish ();
  return result;
}

} // namespace flux::engine
