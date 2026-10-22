# flux-host

The Flux DAW host: a **modified fork of [Zrythm](https://www.zrythm.org/)** (`v2.0.0-alpha.6`, imported with history).

> **Not affiliated with or endorsed by the Zrythm project.** Zrythm and its logo are trademarks of Alexandros Theodotou (see `TRADEMARKS.md`). This is a modified work in development; it is not a Zrythm release and no binaries are published. Original copyright notices and licences are preserved. The upstream readme is kept as `README.zrythm.md`.

## What it is
- C++23, Qt/QML and JUCE; CMake build. Has a clip launcher (scenes/clip slots) and hosts CLAP/VST3/LV2 plugins.
- Flux goals: single-window Session-first workflow, Flux theme and branding, trimmed menus, real-time-safe AI hand-off (`flux/rt/spsc`), Signalsmith Stretch warping, MIDI 2.0, Ambisonic routing. Work is tracked in this repo's issues (milestones P1 to P6).

## Layout
- Upstream tree at the repo root (unchanged unless noted in `flux/CHANGES.md`).
- `flux/`: Flux-only code and docs: `rt/spsc` (lock-free ring buffer), `docs/UPSTREAM.md` (sync procedure, licence and trademark notes), `UPSTREAM_VERSION` (the pin).

## Building
See `README.zrythm.md` (CMake + Qt >= 6.11.1, or Conan). The build has not been run in this repo yet.

## Licence
AGPL-3.0-or-later with the additional terms in `LICENSES/LicenseRef-ZrythmLicense.txt`; per-file licences follow REUSE (`REUSE.toml`).
