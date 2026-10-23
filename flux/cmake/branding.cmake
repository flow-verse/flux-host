# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Flux product identity. Included from the root CMakeLists.txt right after the
# upstream "Common Variables" block, so these values override upstream's.
# Modified builds may not use the Zrythm name or logo (TRADEMARKS.md).

set(prog_name "Flux")
# prog_name_lowercase stays "zrythm": upstream hardcodes it in install and data
# paths (src/utils/directory_manager.cpp), so it is an internal identifier only.
set(bundle_identifier "io.github.flow-verse.Flux")
set(copyright_name "Flux contributors and Alexandros Theodotou")
set(main_repo_url "https://github.com/flow-verse/flux-host")
set(issue_tracker_url "${main_repo_url}/issues")
set(new_issue_url "${issue_tracker_url}/new")
set(chatroom_url "https://github.com/orgs/flow-verse/discussions")
set(user_manual_url "https://github.com/flow-verse/flux-docs")
set(faq_url "${user_manual_url}")
set(donation_url "https://github.com/flow-verse")
set(purchase_url "https://github.com/flow-verse")
set(privacy_policy_url "${main_repo_url}/blob/main/flux/docs/PRIVACY.md")
set(license_url "${main_repo_url}/blob/main/LICENSES/LicenseRef-ZrythmLicense.txt")
set(trademark_policy_url "${main_repo_url}/blob/main/TRADEMARKS.md")
# No upstream telemetry or update checks: these endpoints belong to Zrythm.
set(bug_report_api_endpoint "")
set(latest_release_ver_url "")
set(CHECK_UPDATES OFF)

# Paths are unchanged (the files under them now hold Flux artwork); only the
# generated bundle names change.
set(ZRYTHM_ICNS_FILE_NAME "Flux.icns")
set(ZRYTHM_ICNS_FILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ZRYTHM_ICNS_FILE_NAME}")
set(ZRYTHM_BMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/Flux.bmp")
