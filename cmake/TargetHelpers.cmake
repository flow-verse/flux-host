# SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

# Miscellaneous target helpers.

# Disables all compiler warnings for the given target (used for third-party
# code or generated code that we don't control).
function (zrythm_target_disable_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/w>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
  )
endfunction ()
