// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "dsp/hardware_audio_interface.h"
#include "dsp/hardware_midi_interface.h"
#include "dsp/iaudio_callback.h"
#include "utils/isettings_backend.h"

#include <QHash>
#include <QString>
#include <QVariant>

namespace flux::engine
{

/**
 * @brief Audio "device" for the headless engine.
 *
 * Reports a fixed sample rate and block length so the engine can build and
 * prepare its graph, but never runs an audio thread: the engine only renders
 * offline. add_audio_callback()/remove_audio_callback() call
 * about_to_start()/stopped() on the main thread, as a real device would
 * before its first and after its last callback.
 */
class NullAudioInterface final : public zrythm::dsp::IHardwareAudioInterface
{
public:
  NullAudioInterface (int sample_rate, int block_length)
      : info_{
          .device_name = zrythm::utils::Utf8String::from_utf8_encoded_string (
            "Flux offline"),
          .sample_rate = zrythm::units::sample_rate (sample_rate),
          .block_length =
            zrythm::units::samples (static_cast<uint32_t> (block_length)),
          .input_channel_count = zrythm::units::channels (2),
          .output_channel_count = zrythm::units::channels (2),
        }
  {
  }

  [[nodiscard]] zrythm::dsp::AudioDeviceInfo get_device_info () const override
  {
    return info_;
  }

  void add_audio_callback (zrythm::dsp::IAudioCallback * callback) override
  {
    if (callback_ != nullptr)
      callback_->stopped ();
    callback_ = callback;
    if (callback_ != nullptr)
      callback_->about_to_start ();
  }

  void remove_audio_callback (zrythm::dsp::IAudioCallback * callback) override
  {
    if (callback_ != nullptr && callback_ == callback)
      {
        callback_->stopped ();
        callback_ = nullptr;
      }
  }

private:
  zrythm::dsp::AudioDeviceInfo  info_;
  zrythm::dsp::IAudioCallback * callback_ = nullptr;
};

/** MIDI "interface" with no devices. */
class NullMidiInterface final : public zrythm::dsp::IHardwareMidiInterface
{
public:
  void
  set_device_change_callback (std::optional<DeviceChangeCallback> cb) override
  {
    callback_ = std::move (cb);
  }
  BufferMap device_buffers () const override { return {}; }

private:
  std::optional<DeviceChangeCallback> callback_;
};

/**
 * @brief Settings kept in memory for the life of the process.
 *
 * The engine must not read or change the GUI's persisted settings.
 */
class MemorySettingsBackend final : public zrythm::utils::ISettingsBackend
{
public:
  QVariant
  value (QAnyStringView key, const QVariant &default_value) const override
  {
    const auto it = values_.find (key.toString ());
    return it != values_.end () ? *it : default_value;
  }
  void setValue (QAnyStringView key, const QVariant &val) override
  {
    values_[key.toString ()] = val;
  }

private:
  QHash<QString, QVariant> values_;
};

} // namespace flux::engine
