// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include <nlohmann/json.hpp>

namespace zrythm::structure::project
{
class Project;
}

namespace flux::engine
{

/**
 * @brief Options for an offline render of a project's master output.
 *
 * The range defaults to the project's start and end markers. Bars are
 * 1-based musical positions (bar 1 starts at the beginning of the song); an
 * end bar is exclusive, so bars 1 to 9 render the first eight bars.
 */
struct RenderOptions
{
  std::filesystem::path out;

  std::optional<double> start_bar;
  std::optional<double> end_bar;
  std::optional<double> start_sec;
  std::optional<double> end_sec;

  /** "wav" or "flac". Empty: from the output file extension (default wav). */
  std::string format;

  /** 16, 24 or 32 (32 means 32-bit float; WAV only). */
  int bits = 24;

  /**
   * Must equal the engine's sample rate when set (choose the engine rate
   * with `--sr` when starting flux-engine).
   */
  std::optional<int> sample_rate;

  /** Also render each track's channel output to its own file. */
  bool stems = false;

  /** Metadata title written into the file. */
  QString title;

  /** Parses the JSON form used by the agent API (see API.md). */
  static RenderOptions from_json (const nlohmann::json &j);
};

struct RenderedFile
{
  std::filesystem::path path;
  /** Empty for the mix; track UUID for a stem. */
  std::string           track_id;
  std::string           track_name;
  /** Linear sample peak (1.0 = 0 dBFS). */
  float                 peak = 0.f;
};

struct RenderResult
{
  std::vector<RenderedFile> files;
  double                    start_sec = 0;
  double                    end_sec = 0;
  double                    duration_sec = 0;
  long long                 frames = 0;
  int                       sample_rate = 0;
  int                       bits = 0;
  std::string               format;
  /** Linear sample peak of the mix (1.0 = 0 dBFS). */
  float peak = 0.f;
  double wall_sec = 0;
  /** Audio seconds rendered per wall-clock second, over all files. */
  double speed_factor = 0;
  /** Number of CLAP plugins switched to offline render mode. */
  int clap_offline_plugins = 0;

  nlohmann::json to_json () const;
};

/**
 * @brief Renders @p project offline and writes the result.
 *
 * Pauses the engine for the duration (AudioEngine::OfflineRenderSession),
 * switches CLAP plugins that support it to offline render mode, renders the
 * graph pruned to the master output (and per-track outputs for stems) and
 * writes the files. Must be called on the main thread; it processes events
 * while the renderer runs.
 *
 * @throw std::invalid_argument for bad options, std::runtime_error on
 * render or write failure.
 */
/**
 * @brief Renders a short span to a discarded file.
 *
 * Some plugins apply state loaded on the main thread (a preset, an undo, a
 * project load) only during their next processing; Surge XT, for example,
 * plays the first render after a patch load partly with the old state. Call
 * this before a render that follows such a change.
 */
void
warm_up_plugins (zrythm::structure::project::Project &project);

RenderResult
render_project (zrythm::structure::project::Project &project, const RenderOptions &options);

} // namespace flux::engine
