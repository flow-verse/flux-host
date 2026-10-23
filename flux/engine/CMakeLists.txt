# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# flux-engine: headless offline render (P1) and agent control API (P2).

add_library(flux_engine_lib STATIC
  src/agent_api.cpp
  src/agent_api.h
  src/engine_host.cpp
  src/engine_host.h
  src/engine_session.cpp
  src/engine_session.h
  src/headless_backends.h
  src/musical_time.h
  src/offline_render.cpp
  src/offline_render.h
  src/plugin_catalog.cpp
  src/plugin_catalog.h
  src/load_preset_command.h
  src/set_params_command.h
)
target_include_directories(flux_engine_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(flux_engine_lib
  PUBLIC
    zrythm_actions_lib
    zrythm_controllers_lib
    zrythm::include_dirs
    zrythm::all_compile_options
)
set_target_properties(flux_engine_lib PROPERTIES AUTOMOC OFF)

add_executable(flux-engine src/main.cpp)
target_link_libraries(flux-engine PRIVATE flux_engine_lib)
set_target_properties(flux-engine PROPERTIES
  AUTOMOC OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/products/bin"
)

if(ZRYTHM_TESTS)
  add_subdirectory(tests)
endif()
