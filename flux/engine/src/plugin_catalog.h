// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "plugins/plugin_descriptor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <nlohmann/json.hpp>

namespace flux::engine
{

/**
 * @brief The plugins the engine can load, found by scanning CLAP and VST3
 * folders.
 *
 * Folders: the platform defaults (e.g. ~/Library/Audio/Plug-Ins/CLAP and
 * /Library/Audio/Plug-Ins/CLAP on macOS), `CLAP_PATH`, `FLUX_CLAP_PATH`,
 * `FLUX_VST3_PATH` and any added with add_search_path(). The scan runs
 * in-process on first use (see README: a crashing plugin binary crashes the
 * scan) and is cached until rescan().
 *
 * A plugin's id is JUCE's identifier string (format, name, file hash and
 * unique id), stable for a given installation.
 */
class PluginCatalog
{
public:
  struct Entry
  {
    std::string             id;
    std::string             name;
    std::string             vendor;
    std::string             format; ///< "clap" or "vst3"
    std::string             category;
    std::string             version;
    bool                    is_instrument = false;
    juce::PluginDescription description;

    nlohmann::json to_json () const;
  };

  PluginCatalog ();

  /** @param format "clap" or "vst3". */
  void add_search_path (const std::string &format, const std::string &path);

  const std::vector<Entry> &entries ();
  void                      rescan ();

  /**
   * @brief Finds a plugin by exact id, or else by case-insensitive exact
   * name (the first match, CLAP preferred).
   */
  const Entry * find (const std::string &id_or_name);

  static std::unique_ptr<zrythm::plugins::PluginDescriptor>
  make_descriptor (const Entry &entry);

private:
  void scan ();

  std::vector<std::string> clap_paths_;
  std::vector<std::string> vst3_paths_;
  std::vector<Entry>       entries_;
  bool                     scanned_ = false;
};

} // namespace flux::engine
