# SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

# Generate faust_registry.cpp from the shared plugin list
include(FaustPlugins)
set(FAUST_EXTERN_DECLARATIONS "")
set(FAUST_REGISTRY_ENTRIES "")
foreach(plugin_info ${zrythm_faust_plugin_defs})
  string(REPLACE "|" ";" plugin_info_list ${plugin_info})
  list(GET plugin_info_list 2 pl_underscored_name)
  list(GET plugin_info_list 3 pl_category)
  list(GET plugin_info_list 4 pl_is_instrument)
  string(APPEND FAUST_EXTERN_DECLARATIONS
    "std::unique_ptr<zrythm::plugins::faust::dsp> create_${pl_underscored_name} ();
void class_init_${pl_underscored_name} (int);
")
  string(APPEND FAUST_REGISTRY_ENTRIES
    "  FaustPluginInfo{
    u8\"zrythm.faust.${pl_underscored_name}\", PluginCategory::${pl_category},
    ${pl_is_instrument}, &zrythm_faust::create_${pl_underscored_name},
    &zrythm_faust::class_init_${pl_underscored_name} },
")
endforeach()
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/faust_registry.cpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/faust_registry.cpp"
  @ONLY
)

target_sources(zrythm_plugins_lib
  PRIVATE
    faust_plugin.cpp
    faust_synth_voice.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/faust_registry.cpp"
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "../.."
    FILES
      faust_base.h
      faust_controls.h
      faust_plugin.h
      faust_registry.h
      faust_synth_voice.h
)
