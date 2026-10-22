# SPDX-FileCopyrightText: © 2022, 2024, 2026 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

# Disable AUTOMOC for all targets in this directory
# These are plain C++ files without Qt meta-objects
set(CMAKE_AUTOMOC OFF)

# Plugin definitions live in cmake/FaustPlugins.cmake (shared with
# src/plugins/faust/CMakeLists.txt): "Name|Description|underscored_name|category|is_instrument"
include(FaustPlugins)

set(pl_author "Zrythm")
set(pl_generate_targets)
set(generated_cpp_files)

# Output directory for generated C++ (checked into git)
set(generated_cpp_dir "${CMAKE_CURRENT_SOURCE_DIR}/generated-cpp")

foreach(plugin_info ${zrythm_faust_plugin_defs})
  string(REPLACE "|" ";" plugin_info_list ${plugin_info})
  list(GET plugin_info_list 0 pl_name)
  list(GET plugin_info_list 1 pl_descr)
  list(GET plugin_info_list 2 pl_underscored_name)

  # Configure DSP file from template
  set(configured_dsp "${CMAKE_CURRENT_BINARY_DIR}/${pl_underscored_name}.dsp")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/${pl_underscored_name}.dsp.in"
    "${configured_dsp}"
    @ONLY
  )

  set(pl_generated_cpp "${generated_cpp_dir}/${pl_underscored_name}.cpp")

  if(FAUST_EXECUTABLE)
    add_custom_command(
      OUTPUT "${pl_generated_cpp}"
      COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/gen-faust.py"
        "${configured_dsp}"
        "-o" "${generated_cpp_dir}"
        "-a" "${CMAKE_CURRENT_SOURCE_DIR}/zrythm-arch.cpp"
        "-c" "${pl_underscored_name}"
        "--lib" "${CMAKE_CURRENT_SOURCE_DIR}/zrythm-utils.lib"
        "--faust" "${FAUST_EXECUTABLE}"
        "--strip-prefix" "${CMAKE_SOURCE_DIR}"
      DEPENDS
        "${configured_dsp}"
        "${CMAKE_CURRENT_SOURCE_DIR}/zrythm-utils.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/zrythm-arch.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/gen-faust.py"
      COMMENT "Generating Faust C++ source for ${pl_name}"
    )
    list(APPEND pl_generate_targets "${pl_generated_cpp}")
  endif()

  if(EXISTS "${pl_generated_cpp}")
    list(APPEND generated_cpp_files "${pl_generated_cpp}")
  else()
    message(WARNING "${pl_generated_cpp} not found. Run `cmake --build . --target gen-faust-cpp` to generate it")
  endif()
endforeach()

# Generation target
if(FAUST_EXECUTABLE)
  add_custom_target(gen-faust-cpp
    COMMENT "Generating Faust C++ sources"
    DEPENDS ${pl_generate_targets}
  )
endif()

if(generated_cpp_files)
  add_library(zrythm_faust_plugins STATIC ${generated_cpp_files})
  target_include_directories(zrythm_faust_plugins PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
  )
  # Generated code - disable warnings
  zrythm_target_disable_warnings(zrythm_faust_plugins)
endif()
