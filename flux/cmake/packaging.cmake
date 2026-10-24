# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Flux package names. Included from the root CMakeLists.txt right before
# include(CPack), so these override upstream's CPACK_ values.

set(CPACK_PACKAGE_NAME "${prog_name}")
set(CPACK_PACKAGE_VENDOR "Flux contributors")
set(CPACK_PACKAGE_INSTALL_DIRECTORY
  "${prog_name} ${ZRYTHM_VERSION_MAJOR}.${ZRYTHM_VERSION_MINOR}")
set(CPACK_PACKAGE_EXECUTABLES "${exe_name};${prog_name}")
set(CPACK_INNOSETUP_RUN_EXECUTABLES "${exe_name}")
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "${prog_name} Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "${prog_name} Runtime")
# AppImage: desktop file, icon and the binary AppRun starts (cmake/cpack_pre_build.cmake)
set(CPACK_APPIMAGE_DESKTOP_FILE "${app_id}.desktop")
set(CPACK_APPRUN_EXECUTABLE "${exe_name}")
if(UNIX AND NOT APPLE)
  set(CPACK_PACKAGE_ICON "${app_id}.svg")
endif()
