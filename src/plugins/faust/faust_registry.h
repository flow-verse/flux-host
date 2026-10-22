// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <span>

#include "plugins/faust/faust_base.h"
#include "plugins/plugin_descriptor.h"
#include "utils/utf8_string.h"

namespace zrythm::plugins::faust
{

/**
 * @brief Static information about a bundled Faust plugin.
 *
 * The `create_` and `class_init_` functions are emitted by the Faust
 * architecture file into each generated source file.
 */
struct FaustPluginInfo
{
  /** Unique key, e.g. "zrythm.faust.compressor". */
  utils::Utf8String key_;

  PluginCategory category_{};

  /** Whether this is an instrument (hosted via juce::Synthesiser voices). */
  bool is_instrument_{};

  std::unique_ptr<dsp> (*create_) ();
  void (*class_init_) (int sample_rate);
};

/** Returns all available bundled Faust plugins. */
[[nodiscard]] std::span<const FaustPluginInfo>
available_faust_plugins ();

/** Finds a bundled Faust plugin by its key, or nullptr. */
[[nodiscard]] const FaustPluginInfo *
find_faust_plugin_by_key (const utils::Utf8String &key);

/**
 * @brief Creates a descriptor for a bundled Faust plugin.
 *
 * Instantiates a temporary dsp to read its metadata and port counts.
 */
[[nodiscard]] std::unique_ptr<PluginDescriptor>
make_faust_plugin_descriptor (const FaustPluginInfo &info);

} // namespace zrythm::plugins::faust
