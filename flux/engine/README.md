# flux-engine

A headless Flux host. One executable with two jobs:

- **`flux-engine render`** renders a saved project offline to WAV or FLAC (P1).
- **`flux-engine serve`** exposes a small JSON-RPC 2.0 API over stdin/stdout so an agent can build, edit, save and render projects (P2). The methods are in [API.md](API.md).

It uses the same project model, undo stack, edit operators and offline renderer as the GUI. It opens no window and needs no window server: it runs on a `QCoreApplication`, with no `QT_QPA_PLATFORM` setting needed. Plugin editor windows are never created.

## Build

`flux-engine` is part of the normal build (`flux/engine/CMakeLists.txt`, added from `flux/CMakeLists.txt`). See [`flux/docs/BUILDING-macos.md`](../docs/BUILDING-macos.md).

```bash
cmake --build conanbuild/Release --target flux-engine
conanbuild/Release/products/bin/flux-engine --help
```

Tests (built when `ZRYTHM_TESTS` is on, as in the default preset):

```bash
cmake --build conanbuild/Release --target flux_engine_tests flux-engine
cd conanbuild/Release && QT_QPA_PLATFORM=offscreen ctest -R flux_engine --output-on-failure
```

- `flux_engine_tests` (gtest, `tests/engine_api_test.cpp`) builds a 2-track, 8-bar project through the API with the bundled CLAP "Test Synth", saves, reopens and renders it, checks length, level, file format and speed. It also checks that undoing an agent turn restores `describe_project` exactly, that `dry_run` changes nothing, and that malformed requests get the right JSON-RPC errors.
- `flux_engine_smoke` (`tests/smoke.sh`) starts `flux-engine serve` and drives a scripted session over stdio.
- `tests/bench_render.py` is the render benchmark below (not run by ctest).

## Render

```text
flux-engine render <project-dir> --out <file> [options]
    --start-bar N --end-bar M     range in bars (1-based, end exclusive)
    --start-sec S --end-sec E     range in seconds
                                  (default: the project's start and end markers)
    --format wav|flac             default: from the file extension, else wav
    --bits 16|24|32f              default 24 (32f = 32-bit float, WAV only)
    --sr RATE                     engine sample rate (default 48000)
    --block N                     render block length (default 1024)
    --stems                       also write one file per track
    --json                        print the result as one JSON line
```

Example:

```bash
flux-engine render ~/Music/song --out song.wav --start-bar 1 --end-bar 33 --json
```

```json
{"ok":true,"outputs":[{"path":"/…/song.wav","peak_dbfs":-14.12}],"format":"wav","bits":24,
 "sample_rate":48000,"start_sec":0.0,"end_sec":64.0,"duration_sec":64.0,"frames":3072000,
 "peak_dbfs":-14.12,"wall_sec":1.21,"speed_factor":52.9,"clap_offline_plugins":0,
 "project":"/…/song","load_sec":0.07}
```

`peak_dbfs` is the sample peak (`null` for digital silence). `speed_factor` is audio seconds rendered per wall-clock second, counting all files (mix and stems) and including the file writes. With `--stems`, each track with an audio output gets `<out-stem> - NN <track name>.<ext>` next to the mix; a stem is the track's channel output after its fader.

Exit status is 0 on success, 1 on a render error (the JSON line then has `"ok":false,"error":…`), 2 on bad arguments.

## Serve

```bash
flux-engine serve [--project DIR] [--sr RATE] [--block N] [--plugin-path DIR] [--vst3-path DIR] [--log]
```

The engine prints a `ready` notification, then reads one JSON-RPC request per line from stdin and writes one response per line to stdout. It exits on end of input or after a `shutdown` request. There is no network listener: talking to it means owning its stdin and stdout, so it is reachable only by the process that started it. An MCP server or other adapter wraps it as a child process (P3).

```text
→ {"jsonrpc":"2.0","id":1,"method":"create_project","params":{"dir":"/tmp/song","bpm":122}}
← {"id":1,"jsonrpc":"2.0","result":{"dir":"/tmp/song","title":"song","changes":[…]}}
```

Plugins are found in the platform's CLAP and VST3 folders, in `CLAP_PATH`, `FLUX_CLAP_PATH` and `FLUX_VST3_PATH` (colon-separated), and in folders passed with `--plugin-path` / `--vst3-path`.

Stdout carries only protocol lines. At start-up the engine duplicates the real stdout for the protocol and points file descriptor 1 at stderr, so text a plugin or library prints cannot corrupt the stream. `--log` writes the engine's log to stderr and a log file.

## Threading model

- **Main (Qt) thread.** Owns the project, the undo stack and the operators. Every API method runs here. Edits go through `UndoStack::push()`, which pauses engine processing around commands that need it (`AudioEngine::execute_function_with_paused_processing_synchronously`), exactly as GUI edits do.
- **stdin reader thread.** Reads a line, hands it to the main thread with a blocking queued call (`QMetaObject::invokeMethod(…, Qt::BlockingQueuedConnection)`), then writes the response. Requests are therefore handled one at a time and in order. The reader never touches the project.
- **No live audio thread.** The engine's audio "device" (`NullAudioInterface`) reports 48 kHz (or `--sr`) and the block length, so the engine can build and prepare its graph, but it never starts an audio callback. Nothing processes audio except an offline render.
- **Offline render.** `render_project()` creates an `AudioEngine::OfflineRenderSession` (which pauses the engine and blocks paused-processing edits until it finishes), rebuilds the tracks' playback caches, switches CLAP plugins that support it to `CLAP_RENDER_OFFLINE`, and runs `dsp::GraphRenderer` on its own worker threads. The main thread keeps processing events while it waits, because the renderer prepares graph nodes on the main thread through blocking calls. The session is finished on every path, including errors.
- **Real-time rule.** Nothing in `flux/engine` runs on an audio or render thread; the code it adds runs on the main thread or the stdin reader. The one upstream change on the processing path (`ClapPlugin::set_clap_render_offline`) is a main-thread call.

## Measurements

First measured figure for the report's worked example ("4 hours of deep house"): a 10-minute project (300 bars at 120 bpm, 4/4) with 8 instrument tracks, each the bundled CLAP **Test Synth** (a sine synth) playing one note per beat. Measured with `tests/bench_render.py` on 2026-10-23, Mac Studio, Apple M1 Ultra (20 cores), macOS 15.7, Release build. The machine was also running other build jobs (load average 4–11), so these are conservative.

| Tracks | Block | Render + write | Speed factor |
|---|---|---|---|
| 8 | 256 | 30.7 s | **19.6× real time** |
| 8 | 1024 (default) | 8.5 s | **70.4× real time** |
| 1 | 1024 | 5.7 s | 105× |
| 16 | 1024 | 14.9 s | 40× |

At 70× a 4-hour set (24 such 10-minute renders) takes about 3.4 minutes of rendering with this plugin. Loading the 9,600-note project took a further 7.0 s; about half of it is upstream's JSON-schema validation, the rest deserialisation. The renderer's per-block cost dominates at small blocks, which is why 1024 is the default (block length changes only scheduling granularity, not the audio, for plugins that are block-size independent).

**Surge XT was not measured:** no Surge XT CLAP is installed on this Mac (`~/Library/Audio/Plug-Ins/CLAP` and `/Library/Audio/Plug-Ins/CLAP` do not exist), and this task did not install it. Re-run with `bench_render.py … --plugin "Surge XT"` once it is installed; expect a much lower factor than a sine synth.

## Limitations and follow-ups

- **Streaming render.** Blocks go straight to the file as they are produced (a Flux copy of upstream's render loop, `render_stream` in `src/offline_render.cpp`), so render memory does not grow with length: rendering one bar or the whole hour of a 1-hour, 4-track project peaks at 228 MB vs 233 MB; the rest is the project itself. Speed is unchanged (67x for 4 Test Synth tracks at block 1024). JUCE's WAV writer switches to RF64 past 4 GB; ranges up to 48 hours are accepted.
- **Latency.** The graph's plugin-latency preroll is processed but not written (`latency_trimmed_frames` in the result), so the output is exactly the range and plugin delay compensation keeps notes where they are without the latent plugin (tested from the second beat). One upstream quirk remains: an event exactly at the range start is already sent to instruments during the preroll, so with a latent plugin in the project the first note's attack can differ.
- **Loudness.** The result has `lufs` (ITU-R BS.1770-4 integrated loudness: K-weighting, 400 ms blocks, -70 LUFS and -10 LU gates) and `true_peak_dbtp` (4x oversampled, 48-tap interpolator), for the mix and each stem (`src/loudness_meter.h`). Measured against pyloudnorm and a scipy 4x resample: within 0.05 LU and 0.01 dB. They meter the float signal before the file's bit depth, so a hot 24-bit render can show peaks above 0 dBFS that were clipped in the file.
- **Progress.** In `serve`, a render sends `progress` notifications (at most one per percent) before its response; see API.md.
- **Parameters in plain units** use the parameter's range mapping (`unit: "plain"`); CLAP `value_to_text`/`text_to_value` (dB, Hz, ms strings) is not wired through yet. Enum parameters accept their labels.
- **Plugins.** CLAP and VST3 only; LV2 is not scanned (an LV2 world is created, so LV2 plugins already in a project can load). The scan runs in the engine's process, so a plugin binary that crashes while being scanned crashes the engine; the GUI's out-of-process scanner could be reused. `list_presets`/`load_preset` cover CLAP preset discovery only (no VST3 programs or LV2 presets); remote-controls pages are not implemented.
- **Edits not covered yet:** time-signature changes, deleting/moving tracks and clips, editing or removing automation (writing it is `write_automation`), MIDI export (`dsp::MidiFile::write_to_file` still has no caller), change notifications. `set_tempo` adds or changes tempo events; the base tempo of an existing project is not editable through the undo stack upstream.
- **Title.** The saved project title is its directory name (upstream format).
- **GUI exporter** is unchanged; the format, bit-depth and range options live in `flux/engine/src/offline_render.*`. Wiring `ExportDialog.qml` to them is a follow-up.
- **macOS packaging.** `flux-engine` is built into `products/bin` but not installed into the app bundle or packages yet.
