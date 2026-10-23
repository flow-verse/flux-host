# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Flux package names. Included from the root CMakeLists.txt right before
# include(CPack), so these override upstream's CPACK_ values.

set(CPACK_PACKAGE_NAME "${prog_name}")
set(CPACK_PACKAGE_VENDOR "Flux contributors")
set(CPACK_PACKAGE_INSTALL_DIRECTORY
  "${prog_name} ${ZRYTHM_VERSION_MAJOR}.${ZRYTHM_VERSION_MINOR}")
