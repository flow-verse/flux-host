// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file
 * flux-engine: headless Flux host.
 *
 *   flux-engine render <project-dir> --out <file> [options]   (offline render)
 *   flux-engine serve [--project <dir>]                        (JSON-RPC on stdio)
 *
 * See flux/engine/README.md and flux/engine/API.md.
 */

#include "zrythm-config.h"

#include <atomic>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "utils/logger.h"

#include <QCoreApplication>
#include <QMetaObject>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "agent_api.h"
#include "engine_host.h"
#include "engine_session.h"
#include "offline_render.h"
#include "plugin_catalog.h"

#ifndef _WIN32
#  include <unistd.h>
#endif

using namespace flux::engine;
using json = nlohmann::json;

namespace
{

constexpr const char * kUsage = R"(usage:
  flux-engine render <project-dir> --out <file> [options]
      --start-bar N --end-bar M     range in bars (1-based, end exclusive)
      --start-sec S --end-sec E     range in seconds
                                    (default: the project's start/end markers)
      --format wav|flac             default: from the file extension, else wav
      --bits 16|24|32f              default 24 (32f is 32-bit float, WAV only)
      --sr RATE                     engine sample rate (default 48000)
      --block N                     render block length (default 256)
      --stems                       also write one file per track
      --json                        print the result as one JSON line
  flux-engine serve [--project <dir>] [--sr RATE] [--block N]
      JSON-RPC 2.0 over stdin/stdout, one request per line (see API.md)
  common options:
      --plugin-path DIR             extra CLAP folder (repeatable)
      --vst3-path DIR               extra VST3 folder (repeatable)
      --log                         write the engine log (to stderr and a file)
)";

/**
 * @brief Protocol output channel.
 *
 * Plugins and libraries may print to stdout. To keep the JSON channel clean,
 * the original stdout is duplicated for protocol output and fd 1 is pointed
 * at stderr.
 */
class ProtocolOut
{
public:
  ProtocolOut ()
  {
#ifndef _WIN32
    fd_ = ::dup (STDOUT_FILENO);
    if (fd_ >= 0)
      ::dup2 (STDERR_FILENO, STDOUT_FILENO);
#endif
  }

  void write_line (const std::string &line)
  {
    std::string data = line;
    data.push_back ('\n');
#ifndef _WIN32
    if (fd_ >= 0)
      {
        const char * p = data.data ();
        size_t       left = data.size ();
        while (left > 0)
          {
            const auto n = ::write (fd_, p, left);
            if (n <= 0)
              return;
            p += n;
            left -= static_cast<size_t> (n);
          }
        return;
      }
#endif
    std::fwrite (data.data (), 1, data.size (), stdout);
    std::fflush (stdout);
  }

private:
  int fd_ = -1;
};

std::string
dump (const json &j)
{
  return j.dump (-1, ' ', false, json::error_handler_t::replace);
}

struct Args
{
  std::string              command;
  std::vector<std::string> positional;
  std::string              out;
  std::optional<double>    start_bar, end_bar, start_sec, end_sec;
  std::string              format;
  std::string              bits = "24";
  int                      sr = 48000;
  int                      block = 256;
  bool                     stems = false;
  bool                     json_output = false;
  bool                     log = false;
  std::string              project;
  std::vector<std::string> clap_paths;
  std::vector<std::string> vst3_paths;
};

Args
parse_args (int argc, char ** argv)
{
  Args a;
  if (argc < 2)
    throw std::invalid_argument ("missing command");
  a.command = argv[1];
  for (int i = 2; i < argc; ++i)
    {
      const std::string arg = argv[i];
      const auto        value = [&] () -> std::string {
        if (i + 1 >= argc)
          throw std::invalid_argument (fmt::format ("{} needs a value", arg));
        return argv[++i];
      };
      const auto number = [&] () {
        const auto v = value ();
        try
          {
            size_t pos = 0;
            double d = std::stod (v, &pos);
            if (pos != v.size ())
              throw std::invalid_argument ("");
            return d;
          }
        catch (...)
          {
            throw std::invalid_argument (
              fmt::format ("{} needs a number, got '{}'", arg, v));
          }
      };
      if (arg == "--out" || arg == "-o")
        a.out = value ();
      else if (arg == "--start-bar")
        a.start_bar = number ();
      else if (arg == "--end-bar")
        a.end_bar = number ();
      else if (arg == "--start-sec")
        a.start_sec = number ();
      else if (arg == "--end-sec")
        a.end_sec = number ();
      else if (arg == "--format")
        a.format = value ();
      else if (arg == "--bits")
        a.bits = value ();
      else if (arg == "--sr")
        a.sr = static_cast<int> (number ());
      else if (arg == "--block")
        a.block = static_cast<int> (number ());
      else if (arg == "--stems")
        a.stems = true;
      else if (arg == "--json")
        a.json_output = true;
      else if (arg == "--log")
        a.log = true;
      else if (arg == "--project")
        a.project = value ();
      else if (arg == "--plugin-path")
        a.clap_paths.push_back (value ());
      else if (arg == "--vst3-path")
        a.vst3_paths.push_back (value ());
      else if (arg == "--help" || arg == "-h")
        throw std::invalid_argument ("");
      else if (!arg.empty () && arg[0] == '-')
        throw std::invalid_argument (fmt::format ("unknown option {}", arg));
      else
        a.positional.push_back (arg);
    }
  if (a.sr < 8000 || a.sr > 384000)
    throw std::invalid_argument ("--sr must be between 8000 and 384000");
  if (a.block < 16 || a.block > 8192)
    throw std::invalid_argument ("--block must be between 16 and 8192");
  return a;
}

int
run_render (const Args &args, ProtocolOut &out)
{
  const auto fail = [&] (const std::string &message) {
    if (args.json_output)
      out.write_line (dump ({ { "ok", false }, { "error", message } }));
    else
      std::cerr << "flux-engine: " << message << "\n";
    return 1;
  };
  if (args.positional.size () != 1)
    return fail ("render needs exactly one project directory");
  if (args.out.empty ())
    return fail ("render needs --out <file>");

  try
    {
      RenderOptions opts;
      opts.out = std::filesystem::absolute (args.out);
      opts.start_bar = args.start_bar;
      opts.end_bar = args.end_bar;
      opts.start_sec = args.start_sec;
      opts.end_sec = args.end_sec;
      opts.format = args.format;
      opts.stems = args.stems;
      if (args.bits == "32f" || args.bits == "32")
        opts.bits = 32;
      else if (args.bits == "16")
        opts.bits = 16;
      else if (args.bits == "24")
        opts.bits = 24;
      else
        return fail ("--bits must be 16, 24 or 32f");

      EngineHost host ({ .sample_rate = args.sr, .block_length = args.block });
      const auto load_start = std::chrono::steady_clock::now ();
      auto       session = EngineSession::open (host, args.positional.front ());
      const auto load_sec = std::chrono::duration<double> (
                              std::chrono::steady_clock::now () - load_start)
                              .count ();
      opts.title = session->title ();
      const auto result = render_project (session->project (), opts);
      if (args.json_output)
        {
          auto j = result.to_json ();
          j["ok"] = true;
          j["project"] = session->directory ().string ();
          j["load_sec"] = load_sec;
          out.write_line (dump (j));
        }
      else
        {
          for (const auto &f : result.files)
            std::cerr << "wrote " << f.path.string () << "\n";
          std::cerr << fmt::format (
            "{:.2f} s of audio in {:.2f} s ({:.1f}x real time), peak {} dBFS\n",
            result.duration_sec, result.wall_sec, result.speed_factor,
            result.to_json ()["peak_dbfs"].dump ());
        }
      session.reset ();
      return 0;
    }
  catch (const std::exception &e)
    {
      return fail (e.what ());
    }
}

int
run_serve (QCoreApplication &app, const Args &args, ProtocolOut &out)
{
  EngineHost    host ({ .sample_rate = args.sr, .block_length = args.block });
  PluginCatalog catalog;
  for (const auto &p : args.clap_paths)
    catalog.add_search_path ("clap", p);
  for (const auto &p : args.vst3_paths)
    catalog.add_search_path ("vst3", p);
  AgentApi api (host, catalog);

  if (!args.project.empty ())
    {
      try
        {
          api.open_project (args.project);
        }
      catch (const std::exception &e)
        {
          std::cerr << "flux-engine: cannot open " << args.project << ": "
                    << e.what () << "\n";
          return 1;
        }
    }

  // Announce readiness (a JSON-RPC notification)
  out.write_line (dump (
    { { "jsonrpc", "2.0" },
      { "method", "ready" },
      { "params",
        { { "api_version", std::string (AgentApi::kApiVersion) },
          { "sample_rate", host.sample_rate () },
          { "project", api.session () ? json (api.session ()->directory ().string ()) : json (nullptr) } } } }));

  // The reader thread only reads lines and writes responses; every request
  // runs on the main thread (blocking queued call), so requests are handled
  // one at a time, in order
  QObject           context;
  std::atomic<bool> stop{ false };
  std::thread       reader ([&] () {
    std::string line;
    while (!stop.load () && std::getline (std::cin, line))
      {
        if (!line.empty () && line.back () == '\r')
          line.pop_back ();
        if (line.find_first_not_of (" \t") == std::string::npos)
          continue;
        std::optional<json> response;
        bool                shutdown = false;
        QMetaObject::invokeMethod (
          &context,
          [&] () {
            try
              {
                response = api.handle_line (line);
              }
            catch (const std::exception &e)
              {
                response = json{
                  { "jsonrpc", "2.0" },
                  { "id", nullptr },
                  { "error",
                   { { "code", rpc_error::kInternalError },
                      { "message", e.what () } } },
                };
              }
            shutdown = api.shutdown_requested ();
          },
          Qt::BlockingQueuedConnection);
        if (response)
          out.write_line (dump (*response));
        if (shutdown)
          break;
      }
    QMetaObject::invokeMethod (&app, &QCoreApplication::quit, Qt::QueuedConnection);
  });

  const int rc = app.exec ();
  stop.store (true);
  reader.join ();
  return rc;
}

} // namespace

int
main (int argc, char ** argv)
{
  ProtocolOut out;

  Args args;
  try
    {
      args = parse_args (argc, argv);
      if (args.command != "render" && args.command != "serve")
        throw std::invalid_argument (fmt::format ("unknown command '{}'", args.command));
    }
  catch (const std::invalid_argument &e)
    {
      if (e.what ()[0] != '\0')
        std::cerr << "flux-engine: " << e.what () << "\n";
      std::cerr << kUsage;
      return 2;
    }

  QCoreApplication app (argc, argv);
  QCoreApplication::setApplicationName (QStringLiteral ("flux-engine"));
  QCoreApplication::setOrganizationName (QStringLiteral (PROGRAM_NAME));
  if (args.log)
    zrythm::utils::init_logging (zrythm::utils::LoggerType::GUI);

  if (args.command == "render")
    return run_render (args, out);
  return run_serve (app, args, out);
}
