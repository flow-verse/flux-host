// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "plugin_catalog.h"

#include <algorithm>
#include <cstdlib>

#include "plugins/CLAPPluginFormat.h"
#include "plugins/vst3_plugin_format.h"

namespace flux::engine
{

using namespace zrythm;

namespace
{
#ifdef _WIN32
constexpr char kPathSeparator = ';';
#else
constexpr char kPathSeparator = ':';
#endif

void
append_env_paths (const char * var, std::vector<std::string> &out)
{
  const char * value = std::getenv (var);
  if (value == nullptr)
    return;
  std::string s (value);
  size_t      start = 0;
  while (start <= s.size ())
    {
      const auto end = s.find (kPathSeparator, start);
      const auto item =
        s.substr (start, end == std::string::npos ? std::string::npos : end - start);
      if (!item.empty ())
        out.push_back (item);
      if (end == std::string::npos)
        break;
      start = end + 1;
    }
}

std::string
lower (std::string s)
{
  std::ranges::transform (s, s.begin (), [] (unsigned char c) {
    return static_cast<char> (std::tolower (c));
  });
  return s;
}

template <typename FormatT>
void
scan_format (
  const std::vector<std::string>      &paths,
  bool                                 add_defaults,
  const char *                         format_name,
  std::vector<PluginCatalog::Entry>   &out)
{
  FormatT              format;
  juce::FileSearchPath search_path;
  if (add_defaults)
    search_path = format.getDefaultLocationsToSearch ();
  for (const auto &p : paths)
    search_path.add (juce::File (juce::String::fromUTF8 (p.c_str ())));
  search_path.removeRedundantPaths ();
  search_path.removeNonExistentPaths ();

  const auto files = format.searchPathsForPlugins (search_path, true, false);
  for (const auto &file : files)
    {
      juce::OwnedArray<juce::PluginDescription> descriptions;
      format.findAllTypesForFile (descriptions, file);
      for (const auto * desc : descriptions)
        {
          PluginCatalog::Entry e;
          e.id = desc->createIdentifierString ().toStdString ();
          e.name = desc->name.toStdString ();
          e.vendor = desc->manufacturerName.toStdString ();
          e.format = format_name;
          e.category = desc->category.toStdString ();
          e.version = desc->version.toStdString ();
          e.is_instrument = desc->isInstrument;
          e.description = *desc;
          const bool duplicate =
            std::ranges::any_of (out, [&e] (const auto &o) { return o.id == e.id; });
          if (!duplicate)
            out.push_back (std::move (e));
        }
    }
}
} // namespace

nlohmann::json
PluginCatalog::Entry::to_json () const
{
  return {
    { "id",            id            },
    { "name",          name          },
    { "vendor",        vendor        },
    { "format",        format        },
    { "category",      category      },
    { "version",       version       },
    { "is_instrument", is_instrument },
  };
}

PluginCatalog::PluginCatalog ()
{
  append_env_paths ("CLAP_PATH", clap_paths_);
  append_env_paths ("FLUX_CLAP_PATH", clap_paths_);
  append_env_paths ("FLUX_VST3_PATH", vst3_paths_);
}

void
PluginCatalog::add_search_path (const std::string &format, const std::string &path)
{
  if (format == "vst3")
    vst3_paths_.push_back (path);
  else
    clap_paths_.push_back (path);
  scanned_ = false;
}

const std::vector<PluginCatalog::Entry> &
PluginCatalog::entries ()
{
  if (!scanned_)
    scan ();
  return entries_;
}

void
PluginCatalog::rescan ()
{
  scanned_ = false;
  scan ();
}

void
PluginCatalog::scan ()
{
  entries_.clear ();
  scan_format<plugins::CLAPPluginFormat> (clap_paths_, true, "clap", entries_);
  scan_format<plugins::Vst3PluginFormat> (vst3_paths_, true, "vst3", entries_);
  std::ranges::stable_sort (entries_, [] (const Entry &a, const Entry &b) {
    return std::tie (a.format, a.name) < std::tie (b.format, b.name);
  });
  scanned_ = true;
}

const PluginCatalog::Entry *
PluginCatalog::find (const std::string &id_or_name)
{
  const auto &all = entries ();
  for (const auto &e : all)
    if (e.id == id_or_name)
      return &e;
  const auto wanted = lower (id_or_name);
  for (const auto &e : all)
    if (lower (e.name) == wanted)
      return &e;
  return nullptr;
}

std::unique_ptr<plugins::PluginDescriptor>
PluginCatalog::make_descriptor (const Entry &entry)
{
  return plugins::PluginDescriptor::from_juce_description (entry.description);
}

} // namespace flux::engine
