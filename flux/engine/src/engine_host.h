// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>

#include "dsp/fader.h"
#include "dsp/metronome.h"
#include "plugins/plugin.h"
#include "structure/project/project.h"
#include "utils/app_settings.h"
#include "utils/object_registry.h"

#include <QTimer>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include "headless_backends.h"

namespace zrythm::plugins
{
class Lv2World;
}

namespace flux::engine
{

/**
 * @brief Process-wide services the headless engine needs to host projects.
 *
 * Owns what the GUI application normally provides (audio/MIDI "devices",
 * settings, plugin formats, control-room fader and metronome) and keeps the
 * JUCE message loop serviced from the Qt event loop.
 *
 * A QCoreApplication must exist before this is created, and everything here
 * lives on the Qt main thread.
 */
class EngineHost
{
public:
  struct Options
  {
    int sample_rate = 48000;
    int block_length = 256;
  };

  explicit EngineHost (Options options);
  ~EngineHost ();
  EngineHost (const EngineHost &) = delete;
  EngineHost &operator= (const EngineHost &) = delete;

  /**
   * @brief Creates an empty project bound to @p path_provider.
   *
   * Plugin editor windows are never created (null window factory).
   */
  std::unique_ptr<zrythm::structure::project::Project> make_project (
    zrythm::structure::project::Project::ProjectDirectoryPathProvider
      path_provider);

  zrythm::utils::AppSettings &app_settings () { return *app_settings_; }
  int                         sample_rate () const { return options_.sample_rate; }
  int block_length () const { return options_.block_length; }

private:
  Options                                          options_;
  juce::ScopedJuceInitialiser_GUI                  juce_initialiser_;
  QTimer                                           juce_pump_;
  NullAudioInterface                               audio_interface_;
  NullMidiInterface                                midi_interface_;
  std::shared_ptr<juce::AudioPluginFormatManager>  plugin_format_manager_;
  std::shared_ptr<zrythm::plugins::Lv2World>       lv2_world_;
  std::unique_ptr<zrythm::utils::AppSettings>      app_settings_;
  std::unique_ptr<zrythm::utils::ObjectRegistry>   registry_;
  zrythm::utils::QObjectUniquePtr<zrythm::dsp::Fader>     monitor_fader_;
  zrythm::utils::QObjectUniquePtr<zrythm::dsp::Metronome> metronome_;
};

/**
 * @brief Processes Qt (and, through the pump timer, JUCE) events until
 * @p done returns true or @p timeout passes.
 *
 * Must be called on the main thread. Returns the final value of @p done.
 */
bool
process_events_until (
  const std::function<bool ()> &done,
  std::chrono::milliseconds     timeout);

} // namespace flux::engine
