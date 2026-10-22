// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <map>
#include <vector>

#include "plugins/faust/faust_base.h"
#include "utils/utf8_string.h"

namespace zrythm::plugins::faust
{

/** Description of a single Faust control (widget). */
struct FaustControl
{
  enum class Widget : std::uint8_t
  {
    Button,
    CheckButton,
    Slider,
    NumEntry,
    /** Passive (output) control, not exposed as a parameter. */
    Bargraph,
  };

  /** Hierarchical path, e.g. "/Synth/Envelope/Attack". */
  utils::Utf8String path;

  /** Last path component (widget label). */
  utils::Utf8String label;

  Widget widget{};

  FAUSTFLOAT * zone{};
  FAUSTFLOAT   min{};
  FAUSTFLOAT   max{};
  FAUSTFLOAT   init{};
  FAUSTFLOAT   step{};

  /** Value of the [unit:...] metadata, if any. */
  utils::Utf8String unit;

  /** Whether [scale:log] metadata was declared. */
  bool scale_log{};

  /** Value of the [midi:...] metadata, if any (e.g. "pitchwheel"). */
  utils::Utf8String midi;
};

/**
 * @brief Enumerates a Faust DSP's controls via buildUserInterface().
 *
 * Builds MapUI-style hierarchical paths and collects per-widget metadata
 * (unit, scale, midi) which the generated code declares just before each
 * widget call.
 */
class FaustControlEnumerator : public UI
{
public:
  const std::vector<FaustControl> &controls () const { return controls_; }
  void openTabBox (const char * label) override { push_group (label); }
  void openHorizontalBox (const char * label) override { push_group (label); }
  void openVerticalBox (const char * label) override { push_group (label); }
  void closeBox () override
  {
    if (!group_stack_.empty ())
      group_stack_.pop_back ();
  }

  void addButton (const char * label, FAUSTFLOAT * zone) override
  {
    add_control (label, zone, FaustControl::Widget::Button, 0.f, 1.f, 0.f, 1.f);
  }
  void addCheckButton (const char * label, FAUSTFLOAT * zone) override
  {
    add_control (
      label, zone, FaustControl::Widget::CheckButton, 0.f, 1.f, 0.f, 1.f);
  }
  void addVerticalSlider (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   init,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max,
    FAUSTFLOAT   step) override
  {
    add_control (
      label, zone, FaustControl::Widget::Slider, min, max, init, step);
  }
  void addHorizontalSlider (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   init,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max,
    FAUSTFLOAT   step) override
  {
    add_control (
      label, zone, FaustControl::Widget::Slider, min, max, init, step);
  }
  void addNumEntry (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   init,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max,
    FAUSTFLOAT   step) override
  {
    add_control (
      label, zone, FaustControl::Widget::NumEntry, min, max, init, step);
  }
  void addHorizontalBargraph (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max) override
  {
    add_control (
      label, zone, FaustControl::Widget::Bargraph, min, max, 0.f, 0.f);
  }
  void addVerticalBargraph (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max) override
  {
    add_control (
      label, zone, FaustControl::Widget::Bargraph, min, max, 0.f, 0.f);
  }

  void declare (FAUSTFLOAT * zone, const char * key, const char * val) override
  {
    if (zone == nullptr)
      return;
    pending_metadata_[zone][key] = (val != nullptr) ? val : "";
  }

private:
  void push_group (const char * label)
  {
    group_stack_.emplace_back (
      utils::Utf8String::from_utf8_encoded_string (
        (label != nullptr) ? label : ""));
  }

  void add_control (
    const char *         label,
    FAUSTFLOAT *         zone,
    FaustControl::Widget widget,
    FAUSTFLOAT           min,
    FAUSTFLOAT           max,
    FAUSTFLOAT           init,
    FAUSTFLOAT           step)
  {
    FaustControl control;
    control.widget = widget;
    control.zone = zone;
    control.min = min;
    control.max = max;
    control.init = init;
    control.step = step;
    control.label = utils::Utf8String::from_utf8_encoded_string (
      (label != nullptr) ? label : "");

    auto path = utils::Utf8String{ u8"/" };
    for (const auto &group : group_stack_)
      {
        if (!group.str ().empty ())
          {
            path = path + group + utils::Utf8String{ u8"/" };
          }
      }
    control.path = path + control.label;

    if (
      const auto it = pending_metadata_.find (zone);
      it != pending_metadata_.end ())
      {
        for (const auto &[key, val] : it->second)
          {
            if (key == "unit")
              control.unit = utils::Utf8String::from_utf8_encoded_string (val);
            else if (key == "scale" && val == "log")
              control.scale_log = true;
            else if (key == "midi")
              control.midi = utils::Utf8String::from_utf8_encoded_string (val);
          }
        pending_metadata_.erase (it);
      }

    controls_.push_back (std::move (control));
  }

private:
  std::vector<FaustControl>      controls_;
  std::vector<utils::Utf8String> group_stack_;

  /** Metadata declared per zone, applied when the zone's widget is added. */
  std::map<const FAUSTFLOAT *, std::map<std::string, std::string>>
    pending_metadata_;
};

/** Reads global metadata (name, author, nvoices, etc.) from a Faust DSP. */
class FaustMetaReader : public Meta
{
public:
  void declare (const char * key, const char * value) override
  {
    if (key != nullptr && value != nullptr)
      values_[key] = value;
  }

  [[nodiscard]] utils::Utf8String get (std::string_view key) const
  {
    const auto it = values_.find (key);
    return it != values_.end ()
             ? utils::Utf8String::from_utf8_encoded_string (it->second)
             : utils::Utf8String{};
  }

private:
  // std::less<> enables heterogeneous lookup so get(string_view) doesn't
  // allocate a temporary std::string key.
  std::map<std::string, std::string, std::less<>> values_;
};

} // namespace zrythm::plugins::faust
