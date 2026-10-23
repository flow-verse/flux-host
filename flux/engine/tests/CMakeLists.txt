# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later

add_executable(flux_engine_tests
  engine_api_test.cpp
)
set_target_properties(flux_engine_tests PROPERTIES AUTOMOC OFF)
target_link_libraries(flux_engine_tests PRIVATE
  flux_engine_lib
  zrythm_test_helpers_lib
)
zrythm_discover_tests(flux_engine_tests)

# End-to-end: drive `flux-engine serve` over stdio with a scripted session
add_test(NAME flux_engine_smoke
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/smoke.sh"
    "$<TARGET_FILE:flux-engine>"
    "${CMAKE_BINARY_DIR}/tests/test_plugins/CLAP/$<CONFIG>"
)
set_tests_properties(flux_engine_smoke PROPERTIES TIMEOUT 300)
