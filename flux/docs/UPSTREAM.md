# Syncing with Zrythm

`flux-host` is a modified copy of [Zrythm](https://www.zrythm.org/) with the upstream history imported
(not a GitHub fork), pinned to the release recorded in `flux/UPSTREAM_VERSION`.

## What upstream is
- C++23, Qt/QML (Qt >= 6.11.1) and JUCE. Built with CMake; Conan is optional.
- Clip launcher / scenes system (`src/gui/qml/components/ClipLauncherView.qml`, `doc/dev/scenes_architecture.md`).
- Hosts CLAP, VST3, LV2, LADSPA, AU plugins (`src/plugins/CLAPPluginFormat.*`).
- Earlier v1.x was C with GTK4. Documents saying "written in C" describe v1.

## Remote
```bash
git remote add upstream https://github.com/zrythm/zrythm.git
```

## Moving to a newer release
```bash
git fetch upstream tag <new-tag> --no-tags
git merge <new-tag-commit>          # expect README.md / CONTRIBUTING.md conflicts
git checkout --ours README.md CONTRIBUTING.md   # keep the Flux versions
git show <new-tag-commit>:README.md > README.zrythm.md
git show <new-tag-commit>:CONTRIBUTING.md > CONTRIBUTING.zrythm.md
```
Then update `flux/UPSTREAM_VERSION`.

## Layout rules (to keep merges small)
- Flux-only code lives under `flux/`.
- Changes to upstream files stay minimal and are listed in `flux/CHANGES.md` (to be created with the first one).
- Upstream's `README.md` and `CONTRIBUTING.md` are kept as `*.zrythm.md`.

## Licence and trademarks (must read)
- Licence: AGPL-3.0-or-later **with additional terms** (`LICENSES/LicenseRef-ZrythmLicense.txt`); each file's licence is given per REUSE (`REUSE.toml`). Our additions are AGPL-3.0-or-later.
- "Zrythm" and its logo are trademarks of Alexandros Theodotou (`TRADEMARKS.md`). A **modified** version may not be distributed under the Zrythm name or with its logo without written consent.
- Therefore before any binary release: rename the product and all user-visible strings, replace the logo files (those under `LicenseRef-ZrythmLogoLicense`), and keep a clear "not affiliated with or endorsed by Zrythm" notice. See the branding issue.
