// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <QString>

#include <nlohmann/json.hpp>

#include "engine_host.h"
#include "engine_session.h"
#include "plugin_catalog.h"

namespace flux::engine
{

/** JSON-RPC error codes used by the agent API (see API.md). */
namespace rpc_error
{
inline constexpr int kParseError = -32700;
inline constexpr int kInvalidRequest = -32600;
inline constexpr int kMethodNotFound = -32601;
inline constexpr int kInvalidParams = -32602;
inline constexpr int kInternalError = -32603;
inline constexpr int kNoProject = -32000;
inline constexpr int kNotFound = -32001;
inline constexpr int kWrongType = -32002;
inline constexpr int kOperationFailed = -32003;
inline constexpr int kStateConflict = -32004;
}

class RpcError : public std::runtime_error
{
public:
  RpcError (int code, const std::string &message)
      : std::runtime_error (message), code_ (code)
  {
  }
  int code () const { return code_; }

private:
  int code_;
};

/**
 * @brief The agent control API: JSON-RPC 2.0 methods over one project.
 *
 * Transport-independent: handle() takes one request object and returns the
 * response (nothing for notifications). It must be called on the Qt main
 * thread; every edit goes through the session's UndoStack.
 *
 * Methods and parameters are documented in flux/engine/API.md.
 */
class AgentApi
{
public:
  static constexpr const char * kApiVersion = "0.1";

  AgentApi (EngineHost &host, PluginCatalog &catalog);
  ~AgentApi ();

  /** Handles one request object (or a batch array). */
  std::optional<nlohmann::json> handle (const nlohmann::json &request);

  /** Parses and handles one line of newline-delimited JSON-RPC. */
  std::optional<nlohmann::json> handle_line (const std::string &line);

  /** Opens a project (as open_project does). @throw std::exception. */
  void open_project (const std::filesystem::path &dir);

  EngineSession * session () { return session_.get (); }

  /**
   * Sends a JSON-RPC notification to the client (thread-safe if the sink is).
   * flux-engine serve uses it for render progress.
   */
  void set_notifier (std::function<void (const nlohmann::json &)> notify)
  {
    notify_ = std::move (notify);
  }

  /** True once a "shutdown" request has been handled. */
  bool shutdown_requested () const { return shutdown_requested_; }

private:
  nlohmann::json handle_one (const nlohmann::json &request, bool &is_notification);
  nlohmann::json dispatch (const std::string &method, const nlohmann::json &params);

  EngineSession &require_session ();
  void           close_session ();

  // Methods
  nlohmann::json describe_project (const nlohmann::json &p);
  nlohmann::json create_project (const nlohmann::json &p);
  nlohmann::json open_project_method (const nlohmann::json &p);
  nlohmann::json save_project (const nlohmann::json &p);
  nlohmann::json add_track (const nlohmann::json &p);
  nlohmann::json list_plugins (const nlohmann::json &p);
  nlohmann::json load_plugin (const nlohmann::json &p);
  nlohmann::json list_params (const nlohmann::json &p);
  nlohmann::json set_params (const nlohmann::json &p);
  nlohmann::json list_presets (const nlohmann::json &p);
  nlohmann::json load_preset (const nlohmann::json &p);
  nlohmann::json write_notes (const nlohmann::json &p);
  nlohmann::json import_file (const nlohmann::json &p, bool midi);
  nlohmann::json set_tempo (const nlohmann::json &p);
  nlohmann::json add_marker (const nlohmann::json &p);
  nlohmann::json set_loop_range (const nlohmann::json &p);
  nlohmann::json set_range (const nlohmann::json &p);
  nlohmann::json render (const nlohmann::json &p);
  nlohmann::json undo_redo (const nlohmann::json &p, bool undo);
  nlohmann::json begin_agent_turn (const nlohmann::json &p);
  nlohmann::json end_agent_turn (const nlohmann::json &p);
  nlohmann::json history () const;
  nlohmann::json get_history (const nlohmann::json &p) const;
  nlohmann::json close_project (const nlohmann::json &p);
  /** Sends project_changed after a successful edit (see API.md). */
  void notify_changed (const std::string &method, const nlohmann::json &result);

  EngineHost                    &host_;
  PluginCatalog                 &catalog_;
  std::unique_ptr<EngineSession> session_;

  struct Turn
  {
    QString label;
    int     start_index = 0;
  };
  std::optional<Turn> turn_;
  bool                shutdown_requested_ = false;
  std::function<void (const nlohmann::json &)> notify_;
  /** id of the request being handled (for progress notifications). */
  nlohmann::json      current_id_;
  /** Edits since the project was created, opened or last saved. */
  bool                unsaved_ = false;
  /** Incremented on every change notification. */
  int64_t             revision_ = 0;
  /** Plugin state changed since the last render (see warm_up_plugins). */
  bool                plugin_state_dirty_ = true;
};

} // namespace flux::engine
