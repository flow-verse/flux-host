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

| `CMakeLists.txt` | Legacy GTK sources (`src/gui/backend/gtk_widgets`, `src/gui/resources/ui`, no longer compiled) excluded from translation scanning |
| `i18n/*.ts` | Obsolete entries dropped (`lupdate -no-obsolete`, about 460,000 lines); five unfinished machine guesses that said "Zrythm" blanked |
| `data/CMakeLists.txt` | `app_id`, `exe_name` and the AppStream values (URLs, developer, contact, brand colours) are only defaulted when not set beforehand; launcher scripts are named after `exe_name`; the MIME XML is configured |
| `data/zrythm.desktop.in`, `data/zrythm_launch.in`, `data/zrythm_gdb.in` | Executable and icon come from `@exe_name@` / `@app_id@` |
| `data/gen-appdata-xml.py` | id, name, developer, description, contact, keywords, project group, launcher binary and brand colours come from CMake variables |
| `data/org.zrythm.Zrythm-mime.xml` | File-type comments use `@prog_name@` (the `application/x-zrythm-project` type and `.zpj` glob are unchanged) |
| `cmake/cpack_pre_build.cmake` | AppImage `AppRun` starts `bin/${CPACK_APPRUN_EXECUTABLE}` |
| `src/gui/qml/ZrythmStyle/ZrythmTheme.qml`, `components/MainMenuBar.qml` | "Flux Teal" (`#1FB5A8`, from the logo) is the default accent and a menu choice |

Flux values for all of these are in `flux/cmake/branding.cmake` and `flux/cmake/packaging.cmake`: executable `flux` (Linux), `Flux.exe` (Windows), `Flux.app` (macOS); app id `io.github.flow_verse.Flux`; the Linux icon is installed by `flux/CMakeLists.txt`.

Deliberately unchanged (internal identifiers, or true factual references):

- `prog_name_lowercase` stays `zrythm`: upstream hardcodes it in install and data paths (`src/utils/directory_manager.cpp`). The CMake target, QML module (`import Zrythm`), C++ namespaces and file-format identifiers (`.zpj`, "Zrythm Plugin Collections") are internal.
- Messages such as "saved by an older version of Zrythm" and the "Zrythm License" / "Zrythm Trademark Policy" labels in the About dialog name Zrythm truthfully, which `TRADEMARKS.md` allows.
- The translation catalogues keep upstream's file names (`i18n/zrythm_*.ts`) and gettext domain; the changed `qsTr()` strings fall back to English until re-translated.
- The legacy debug launchers `zrythm_valgrind.in` / `zrythm_lldb.in` still read upstream's old GSettings schema; they are developer tools, renamed with `exe_name` but otherwise untouched.

## Headless engine (flux/engine, P1/P2)

`flux-engine` lives in `flux/engine/` and links the upstream libraries unchanged, except for:

| File | Change |
|---|---|
| `flux/CMakeLists.txt` | `add_subdirectory(engine)` (Flux file, listed for completeness) |
| `src/plugins/clap_plugin.h`, `src/plugins/clap_plugin.cpp` | New `ClapPlugin::set_clap_render_offline(bool)`: main-thread call to the CLAP render extension (`render.set(CLAP_RENDER_OFFLINE / REALTIME)`), used by flux-engine around offline renders. The GUI exporter does not call it, so its behaviour is unchanged |
| `src/plugins/clap_plugin.cpp` | Bug fix: the input note ports' supported dialects were queried only when ports were created for a new instance. A plugin loaded from a saved project kept an empty list, so the host sent MIDI-dialect events, which a CLAP-note-only instrument ignores: it was silent after a reload. `load_plugin()` now queries the dialects for restored ports too (`ClapPluginImpl::query_note_in_dialects()`, shared with `create_ports_from_clap_plugin()`). Worth sending upstream |
