# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Flux-only targets. Added at the end of the root CMakeLists.txt so that
# upstream targets (zrythm, zrythm_*_lib) already exist.

add_subdirectory(rt)

# The macOS bundle is what users see in Finder and the Dock.
if(APPLE AND TARGET zrythm)
  set_target_properties(zrythm PROPERTIES
    OUTPUT_NAME "${prog_name}"
    MACOSX_BUNDLE_BUNDLE_NAME "${prog_name}")
endif()
