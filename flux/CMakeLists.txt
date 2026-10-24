# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Flux-only targets. Added at the end of the root CMakeLists.txt so that
# upstream targets (zrythm, zrythm_*_lib) already exist.

add_subdirectory(rt)
add_subdirectory(engine)

# The executable users run: Flux.app on macOS, Flux.exe on Windows, flux on
# Linux (exe_name in flux/cmake/branding.cmake)
if(TARGET zrythm)
  set_target_properties(zrythm PROPERTIES OUTPUT_NAME "${exe_name}")
  if(APPLE)
    set_target_properties(zrythm PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "${prog_name}")
  endif()
endif()

# Linux: the application icon the desktop file names (upstream installs none)
if(UNIX AND NOT APPLE)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/branding/flux.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    RENAME "${app_id}.svg"
    COMPONENT Runtime)
endif()
