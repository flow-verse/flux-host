# SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

# Shared list of bundled Faust plugins, consumed by:
#   - data/plugins/CMakeLists.txt (Faust DSP/C++ generation)
#   - src/plugins/CMakeLists.txt (faust_registry.cpp generation)
#
# Entry format: "Name|Description|underscored_name|category|is_instrument"
#   - Name: display name (used for the generated .dsp file)
#   - Description: short description (used for the generated .dsp file)
#   - underscored_name: identifier used for file/class/factory names and the
#     plugin key ("zrythm.faust.<underscored_name>")
#   - category: a plugins::PluginCategory enumerator
#   - is_instrument: "true" or "false"
set(zrythm_faust_plugin_defs
  "Compressor|Basic compressor|compressor|COMPRESSOR|false"
  "Cubic Distortion|Cubic distortion|cubic_distortion|DISTORTION|false"
  "Flanger|Flanger effect|flanger|FLANGER|false"
  "Gate Stereo|Stereo gate|gate_stereo|GATE|false"
  "Highpass Filter|2nd-order Butterworth highpass filter|highpass_filter|HIGHPASS_FILTER|false"
  "Lowpass Filter|2nd-order Butterworth lowpass filter|lowpass_filter|LOWPASS_FILTER|false"
  "White Noise|White noise generator|white_noise|GENERATOR|false"
  "Parametric EQ|Parametric equalizer with high/low shelves|parametric_eq|PARA_EQ|false"
  "Peak Limiter|1176 Peak limiter|peak_limiter|LIMITER|false"
  "Phaser|Phaser effect|phaser|PHASER|false"
  "Smooth Delay|Delay plugin|smooth_delay|Delay|false"
  "Triple Synth|Synth with 3 detuned oscillators|triple_synth|Instrument|true"
  "Wah4|Wah pedal|wah4|FILTER|false"
  "Zita Rev1|Zita reverb algorithm|zita_rev1|REVERB|false")
