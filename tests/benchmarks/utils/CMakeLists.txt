# SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

add_executable(zrythm_utils_benchmarks
  float_ranges.cpp
)

set_target_properties(zrythm_utils_benchmarks PROPERTIES
  AUTOMOC OFF
)

target_link_libraries(zrythm_utils_benchmarks PRIVATE
  benchmark::benchmark_main
  GTest::gmock
  zrythm_utils_lib
)
