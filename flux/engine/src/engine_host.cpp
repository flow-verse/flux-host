// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "engine_host.h"

#include "plugins/lv2_plugin_format.h"
#include "plugins/lv2_world.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

namespace flux::engine
{

using namespace zrythm;

EngineHost::EngineHost (Options options)
    : options_ (options),
      audio_interface_ (options.sample_rate, options.block_length),
      plugin_format_manager_ (
        std::make_shared<juce::AudioPluginFormatManager> ())
{
  // Service JUCE's message queue (plugin timers, VST3 hosting) from Qt's
  // event loop. JUCE posts to the same Core Foundation run loop on macOS,
  // but the explicit pump keeps behaviour identical on every platform.
  juce_pump_.setInterval (5);
  QObject::connect (&juce_pump_, &QTimer::timeout, qApp, [] () {
    juce::MessageManager::getInstance ()->runDispatchLoopUntil (1);
  });
  juce_pump_.start ();
  juce::MessageManager::getInstance ()->runDispatchLoopUntil (0);

  juce::addDefaultFormatsToManager (*plugin_format_manager_);

  try
    {
      lv2_world_ = std::make_shared<plugins::Lv2World> (
        plugins::Lv2PluginFormat::get_spec_bundles_dir ());
    }
  catch (const std::exception &)
    {
      // LV2 plugins then fail to instantiate with a clear error
      lv2_world_ = nullptr;
    }

  app_settings_ = std::make_unique<utils::AppSettings> (
    std::make_unique<MemorySettingsBackend> ());

  registry_ = std::make_unique<utils::ObjectRegistry> (nullptr);
  monitor_fader_ = utils::make_qobject_unique<dsp::Fader> (
    *registry_, dsp::PortType::Audio,
    true,  // hard_limit_output
    false, // make_params_automatable
    [] () -> utils::Utf8String { return u8"Monitor"; },
    [] (bool) { return false; });

  // The metronome never reaches the master output (it feeds the monitor
  // only), and it is disabled; its samples are silent
  juce::AudioSampleBuffer emphasis (2, 512);
  juce::AudioSampleBuffer normal (2, 512);
  emphasis.clear ();
  normal.clear ();
  metronome_ = utils::make_qobject_unique<dsp::Metronome> (
    *registry_, emphasis, normal, false, 0.f, nullptr);
}

EngineHost::~EngineHost ()
{
  juce_pump_.stop ();
}

std::unique_ptr<structure::project::Project>
EngineHost::make_project (
  structure::project::Project::ProjectDirectoryPathProvider path_provider)
{
  auto project = std::make_unique<structure::project::Project> (
    *app_settings_, std::move (path_provider), audio_interface_,
    midi_interface_, plugin_format_manager_,
    [] (plugins::Plugin &) -> std::unique_ptr<plugins::PluginHostWindow> {
      return nullptr;
    },
    *metronome_, *monitor_fader_, lv2_world_);
  // Recording is not supported headless
  project->install_recording_callback (
    [] (
      const structure::tracks::Track::Uuid &, units::sample_t,
      const dsp::ITransport &, const dsp::MidiEventBuffer *,
      std::optional<structure::tracks::TrackProcessor::ConstStereoPortPair>,
      units::sample_u32_t) { });
  return project;
}

bool
process_events_until (
  const std::function<bool ()> &done,
  std::chrono::milliseconds     timeout)
{
  if (done ())
    return true;

  // A nested event loop sleeps until events arrive (unlike a
  // processEvents() spin, which would take a core from the renderer)
  QEventLoop    loop;
  QElapsedTimer elapsed;
  elapsed.start ();
  QTimer poll;
  poll.setInterval (2);
  QObject::connect (&poll, &QTimer::timeout, &loop, [&] () {
    if (done () || elapsed.elapsed () >= timeout.count ())
      loop.quit ();
  });
  poll.start ();
  loop.exec ();
  return done ();
}

} // namespace flux::engine
