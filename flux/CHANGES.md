# Changes to upstream files

Flux keeps its own code under `flux/`. Every change to an upstream (Zrythm) file is listed here so merges from a newer Zrythm release stay small. See `flux/docs/UPSTREAM.md`.

## Branding (flow-verse/flux-host#2)

Modified builds may not use the Zrythm name or logo (`TRADEMARKS.md`). The product name comes from `prog_name`, which `flux/cmake/branding.cmake` sets to "Flux".

| File | Change |
|---|---|
| `CMakeLists.txt` | `include(flux/cmake/branding.cmake)` after the "Common Variables" block; JUCE company name and website use `prog_name` and `main_repo_url`; `add_subdirectory(flux)` and `include(flux/cmake/packaging.cmake)` before `include(CPack)` |
| `REUSE.toml` | Logo paths now carry the Flux logo (CC-BY-SA-4.0); `flux/branding/**` added |
| `src/gui/resources/icons/zrythm-dark/scalable/apps/zrythm.svg`, `zrythm-monochrome.svg`, `zrythm-splash.svg`, `zrythm-splash-png.png` | Content replaced with the Flux logo (paths unchanged so upstream references keep working) |
| `src/gui/resources/icons/zrythm-dark/scalable/apps/zrythm-splash-png.xcf` | Removed (source of the Zrythm splash) |
| `data/platform/windows/zrythm.ico`, `data/platform/macos/dmg_background_512.png`, `doc/user/_static/favicon.ico`, `doc/user/_static/z.svg` | Content replaced with the Flux logo |
| `data/platform/macos/Info.plist.in` | Display/spoken name and document type names use `${MACOSX_BUNDLE_BUNDLE_NAME}` |
| `src/gui/js/config.js.in` | Adds `PROGRAM_NAME` for QML |
| `src/gui/backend/zrythm_application.cpp` | Application, organisation and display names, log and error strings use `PROGRAM_NAME`; support URL is the Flux repo |
| `src/plugins/clap_plugin.cpp`, `vst3_plugin.cpp`, `clap_preset_discovery.cpp`, `plugin-scanner/plugin_scanner_subprocess.h` | Host name reported to plugins uses `PROGRAM_NAME` |
| `src/gui/qml/Greeter.qml`, `components/AboutDialog.qml`, `components/MainMenuBar.qml`, `DemoView.qml` | User-visible strings use `Config.PROGRAM_NAME`; donation text points to the Flux project; About dialog states Flux is based on Zrythm and is not affiliated with it; "Zrythm Orange" theme renamed "Classic Orange" |

Deliberately unchanged (internal identifiers, or true factual references):

- `prog_name_lowercase` stays `zrythm`: upstream hardcodes it in install and data paths (`src/utils/directory_manager.cpp`). The CMake target, QML module (`import Zrythm`), C++ namespaces and file-format identifiers (`.zpj`, "Zrythm Plugin Collections") are internal.
- Messages such as "saved by an older version of Zrythm" and the "Zrythm License" / "Zrythm Trademark Policy" labels in the About dialog name Zrythm truthfully, which `TRADEMARKS.md` allows.
- Linux/Windows executable and package file names (`bin/zrythm`, AppImage desktop file, Inno Setup) are not renamed yet; only the macOS bundle is (`flux/CMakeLists.txt`). Do this before the first Linux or Windows release.
- Translations in `i18n/` still hold the old strings; the changed `qsTr()` source strings fall back to English until re-translated. The old entries are unreachable (no source string matches them) but are still compiled into the binary's `.qm` catalogues, so `strings` on the binary finds them. Running `lupdate -no-obsolete` would drop them, at the cost of touching all 50+ upstream `.ts` files; do it before the first public release.

## Headless engine (flux/engine, P1/P2)

`flux-engine` lives in `flux/engine/` and links the upstream libraries unchanged, except for:

| File | Change |
|---|---|
| `flux/CMakeLists.txt` | `add_subdirectory(engine)` (Flux file, listed for completeness) |
| `src/plugins/clap_plugin.h`, `src/plugins/clap_plugin.cpp` | New `ClapPlugin::set_clap_render_offline(bool)`: main-thread call to the CLAP render extension (`render.set(CLAP_RENDER_OFFLINE / REALTIME)`), used by flux-engine around offline renders. The GUI exporter does not call it, so its behaviour is unchanged |
| `src/plugins/clap_plugin.cpp` | Bug fix: the input note ports' supported dialects were queried only when ports were created for a new instance. A plugin loaded from a saved project kept an empty list, so the host sent MIDI-dialect events, which a CLAP-note-only instrument ignores: it was silent after a reload. `load_plugin()` now queries the dialects for restored ports too (`ClapPluginImpl::query_note_in_dialects()`, shared with `create_ports_from_clap_plugin()`). Worth sending upstream |
