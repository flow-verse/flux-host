# flux-host

The Flux DAW engine and UI — a branded fork of [Zrythm](https://www.zrythm.org/).

**Principle:** do not rewrite the audio engine. Keep the upstream C/C++ core intact and limit changes to
(1) theme/branding, (2) trimmed menus, (3) default Session-view template, (4) the real-time-safe AI hand-off.

## Roadmap
- [ ] Fork upstream Zrythm as a git remote (`upstream`) and track a pinned release
- [ ] Flux theme (palette, logos) in `ui/theme`
- [ ] Default project template that opens in the clip-launcher (Session) view — `templates/`
- [ ] Session → Arrangement record-through
- [ ] `rt/spsc`: lock-free SPSC ring buffer between AI inference thread and the audio callback (silence/dry passthrough on underrun, never block)
- [ ] Signalsmith Stretch (MIT) integration for phase-coherent warping
- [ ] MIDI 2.0 (32-bit, MIDI-CI auto-mapping)
- [ ] Ambisonic routing up to 16 channels with XYZ automation

## Real-time rules
No allocation, locks, I/O or GC languages on the audio thread. Anything slow talks to it through `rt/spsc`.

## Upstream
See `docs/UPSTREAM.md` for the fork/sync procedure.
