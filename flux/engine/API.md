# flux-engine agent API (v0.1)

JSON-RPC 2.0 spoken by `flux-engine serve` over stdin/stdout, one JSON object per line. This is Flux's own versioned contract: it does not expose the upstream project file format, which is unstable.

## Conventions

- **Requests** use named parameters: `"params"` must be an object (or absent). A request without `"id"` is a notification and gets no response. Batches (arrays) are accepted.
- **Ids.** Tracks, clips, plugins, markers and tempo events are addressed by their UUIDs (from `describe_project` or the method that created them), never by index. Parameters are addressed by their stable plugin parameter id (the CLAP param id as a string) or by name.
- **Time.** Bars are 1-based and may be fractional: bar 1.0 is the start of the song, 2.5 is half-way through bar 2. Range ends are exclusive (bars 1 to 9 is eight bars). Inside a clip, `start_beat` and `length_beats` count from the clip start in the time signature's beat (a quarter note in 4/4, an eighth in 6/8).
- **Changes and dry runs.** Every mutating method accepts `"dry_run": true`. It then validates everything, changes nothing and returns `{"dry_run": true, "changes": [...]}`. The real call returns the same `changes` list with its result, so the two can be compared.
- **Undo.** Every edit is an undo step. `begin_agent_turn`/`end_agent_turn` group all edits in between into one step named "Agent: <label>".
- **Start-up.** The engine first prints `{"jsonrpc":"2.0","method":"ready","params":{"api_version":"0.1","sample_rate":48000,"project":null}}`.

### Errors

| Code | Meaning |
|---|---|
| -32700 | The line is not valid JSON |
| -32600 | Not a valid JSON-RPC 2.0 request |
| -32601 | Unknown method |
| -32602 | Invalid or out-of-range parameter (the message says which and why) |
| -32603 | Internal error |
| -32000 | No project is open |
| -32001 | Unknown id (track, plugin, parameter, file, directory) |
| -32002 | Wrong type (e.g. notes on an audio track, an effect where an instrument is needed) |
| -32003 | The operation failed (plugin failed to load, save or render failed) |
| -32004 | State conflict (turn already open, undo during a turn, nothing to undo) |

```json
{"jsonrpc":"2.0","id":7,"error":{"code":-32602,"message":"notes[3] ends at beat 17 but the clip is 16 beats long (4 bars); notes must lie within the clip"}}
```

## Project

### `create_project {dir, title?, bpm?, time_signature?}`

Creates a project in `dir` (must not exist, or be empty) with the default tracks, saves it and makes it the open project. `bpm` 20–999 (default 120). `time_signature` is `"6/8"` or `[6, 8]` (default 4/4). The saved title is the directory name.

```json
→ {"jsonrpc":"2.0","id":1,"method":"create_project","params":{"dir":"/tmp/song","bpm":122}}
← {"jsonrpc":"2.0","id":1,"result":{"dir":"/tmp/song","title":"song",
   "changes":[{"op":"create_project","dir":"/tmp/song","bpm":122.0,"time_signature":"4/4"}]}}
```

### `open_project {dir}`

Loads a saved project and waits for its plugins. Any open project is closed (an open agent turn is closed with it).

### `save_project {dir?}`

Saves to the current directory, or to `dir` ("save as"), which then becomes the current directory. Returns `{"dir": ...}`.

### `describe_project {include_notes?}`

A snapshot of the open project. It contains no undo-history fields, so undoing a turn restores it exactly. With `include_notes: true` each MIDI clip lists its notes.

```json
← {"result":{
  "api_version":"0.1","title":"song","dir":"/tmp/song","sample_rate":48000,"ppq":960,
  "tempo":{"bpm":122.0,"changes":[{"id":"…","bar":17.0,"bpm":124.0}]},
  "time_signature":{"numerator":4,"denominator":4,"changes":[]},
  "range":{"start_bar":1.0,"end_bar":33.0,"start_sec":0.0,"end_sec":62.95},
  "loop":{"enabled":true,"start_bar":1.0,"end_bar":5.0},
  "markers":[{"id":"…","name":"Drop","bar":17.0}],
  "tracks":[
    {"id":"…","index":3,"name":"Master","type":"master","plugins":[],"clips":[]},
    {"id":"bb4f…","index":4,"name":"Lead","type":"instrument",
     "plugins":[{"id":"fa2c…","name":"Test Synth","format":"clap","slot":"instrument","status":"ok"}],
     "clips":[{"id":"ccab…","name":"Lead","kind":"midi","lane":0,"start_bar":1.0,"end_bar":9.0,
               "length_beats":32.0,"note_count":32,
               "notes":[{"pitch":60,"start_beat":0.0,"length_beats":0.9,"velocity":100}]}]}]}}
```

Track `type` is one of `instrument`, `midi`, `audio`, `master`, `chord`, `marker`, `modulator`, `audio_bus`, `audio_group`, `midi_bus`, `midi_group`, `folder`. Plugin `slot` is `instrument`, `midi_fx` or `insert`; `status` is `ok`, `pending` or `failed`. `range` is the start/end markers (the default render range).

## Tracks and plugins

### `add_track {type, name?, plugin_id?}`

`type` is `"instrument"`, `"midi"` or `"audio"`. `name` is made unique if needed. `plugin_id` (from `list_plugins`, or an exact plugin name) loads a plugin into the new track: an instrument for an instrument track, an effect otherwise. Track, rename and plugin form one undo step.

```json
→ {"jsonrpc":"2.0","id":2,"method":"add_track","params":{"type":"instrument","name":"Lead","plugin_id":"Test Synth"}}
← {"result":{"track":"bb4f…","name":"Lead","type":"instrument",
   "plugin":{"id":"fa2c…","name":"Test Synth","format":"clap","slot":"instrument","status":"ok"},
   "changes":[{"op":"add_track","type":"instrument","name":"Lead",
               "plugin":{"id":"CLAP-Test Synth-a63f744a-8de1759e","name":"Test Synth"}}]}}
```

### `list_plugins {format?, query?, instruments_only?, rescan?}`

Plugins found on disk. `format` is `"clap"`, `"vst3"` or `"lv2"` (LV2 is not scanned yet and returns an empty list with a note). `query` matches name, vendor or category, case-insensitively. The first call scans; `rescan: true` scans again.

```json
← {"result":{"plugins":[{"id":"CLAP-Test Synth-a63f744a-8de1759e","name":"Test Synth","vendor":"Zrythm",
   "format":"clap","category":"stereo","version":"1.0.0","is_instrument":true}]}}
```

### `load_plugin {track, plugin_id}`

Adds a plugin to an existing track. Instruments need an instrument track without one (-32004 otherwise); effects go to the track's inserts (MIDI effects to its MIDI-effect slot). Returns `{"plugin": {...}, "track": ...}`.

### `list_params {plugin, include_hidden?}`

```json
← {"result":{"plugin":"fa2c…","name":"Test Synth","track":"bb4f…","params":[
   {"id":"0","uuid":"…","name":"Level","type":"linear","value":1.0,"plain_value":1.0,
    "min":0.0,"max":1.0,"default":1.0,"unit":"","automatable":true,"hidden":false}]}}
```

`value` and `default` are normalised (0–1). `plain_value`, `min` and `max` are in the parameter's own units; for gain parameters `max` is the highest value the fader curve reaches. `type` is `linear`, `logarithmic`, `integer`, `toggle`, `enum` (with `enum_labels`), `gain` or `trigger`. Parameters include the host's own `Bypass` and `Gain` for every plugin.

### `set_params {plugin, values: [{id | name, value, unit?}]}`

Sets several parameters as one undo step. `unit` is `"normalized"` (default, 0–1) or `"plain"` (within `min`..`max`). `value` may also be an enum label, or `"on"`/`"off"`/`true`/`false` for toggles. All values are validated before any is applied.

```json
→ {"jsonrpc":"2.0","id":5,"method":"set_params","params":{"plugin":"fa2c…",
   "values":[{"name":"Level","value":0.5,"unit":"plain"}]}}
← {"result":{"plugin":"fa2c…","changes":[{"op":"set_param","plugin":"fa2c…","param":"0",
   "name":"Level","from":1.0,"to":0.5,"plain_to":0.5}]}}
```

### `list_presets {plugin, query?, limit?}`

Lists the plugin's presets from CLAP preset discovery (CLAP only; other formats return none). `query` matches name or collection, case-insensitively; `limit` defaults to 200. `selected` is the preset last loaded, or `null`.

```json
→ {"jsonrpc":"2.0","id":6,"method":"list_presets","params":{"plugin":"fa2c…","query":"house organ"}}
← {"result":{"plugin":"fa2c…","total":2977,"matched":1,"selected":null,"presets":[{"index":1194,
   "name":"House Organ","group":"Surge XT Factory Presets","id":"F\u001f/…/Keys/House Organ.fxp\u001f"}]}}
```

### `load_preset {plugin, preset}`

Loads a preset as one undo step (undo restores the plugin's previous state). `preset` is an `index` or `id` from `list_presets`, an exact name (or `"collection/name"`), or a substring that matches exactly one name; ambiguous names return an error listing the matches. The next `render` first runs a one-second discarded warm-up, because some plugins (Surge XT) apply a loaded patch only while processing.

```json
→ {"jsonrpc":"2.0","id":7,"method":"load_preset","params":{"plugin":"fa2c…","preset":"House Organ"}}
← {"result":{"plugin":"fa2c…","preset":{"index":1194,"name":"House Organ","group":"Surge XT Factory Presets",
   "id":"…"},"changes":[{"op":"load_preset","plugin":"fa2c…","preset":{…}}]}}
```

## Arrangement

### `write_notes {track, start_bar, length_bars, notes, name?}`

Creates one MIDI clip of `length_bars` bars at `start_bar` on a MIDI or instrument track and fills it, as one undo step. Each note is `{pitch 0–127, start_beat >= 0, length_beats > 0, velocity 1–127 (default 100)}`. A note that ends after the clip is rejected (bars are never silently shifted or truncated). An empty `notes` list makes an empty clip.

```json
→ {"jsonrpc":"2.0","id":3,"method":"write_notes","params":{"track":"bb4f…","start_bar":1,"length_bars":2,
   "notes":[{"pitch":60,"start_beat":0,"length_beats":1,"velocity":100},
            {"pitch":64,"start_beat":1,"length_beats":0.5,"velocity":90}]}}
← {"result":{"clip":"ccab…","track":"bb4f…",
   "changes":[{"op":"add_midi_clip","track":"bb4f…","start_bar":1.0,"length_bars":2.0,"notes":2}]}}
```

### `import_audio {path, start_bar?, track?}` / `import_midi {path, start_bar?, track?}`

Imports a file at `start_bar` (default 1). Without `track`, a new track of the right type is created and named after the file. Audio needs an audio track; MIDI a MIDI or instrument track. Returns the new `tracks` and `clips` ids.

### `set_tempo {bpm, at_bar?}`

Sets the tempo from `at_bar` (default 1) onwards: changes the tempo event at that bar if one exists, otherwise adds one. `bpm` 20–999.

### `add_marker {name, bar}`

Adds a named marker. Returns `{"marker": id}`.

### `set_range {start_bar, end_bar}`

Moves the song start and end markers (the default render range).

### `set_loop_range {start_bar, end_bar, enabled?}`

Sets the transport loop range; `enabled` defaults to true.

## Render

### `render {out, start_bar?, end_bar?, start_sec?, end_sec?, format?, bits?, sr?, stems?}`

Same options and result as `flux-engine render --json` (see README): range in bars or seconds (default: start/end markers), `format` `"wav"`/`"flac"` (default from the extension), `bits` `16`, `24` (default) or `"32f"`, `stems: true` for per-track files. `sr` must equal the engine's sample rate (start the engine with `--sr` to change it). Rendering is not an edit and may run inside an agent turn.

```json
→ {"jsonrpc":"2.0","id":9,"method":"render","params":{"out":"/tmp/song.wav","start_bar":1,"end_bar":9}}
← {"result":{"outputs":[{"path":"/tmp/song.wav","peak_dbfs":-14.12}],"format":"wav","bits":24,
   "sample_rate":48000,"start_sec":0.0,"end_sec":16.0,"duration_sec":16.0,"frames":768000,
   "peak_dbfs":-14.12,"wall_sec":0.54,"speed_factor":29.7,"clap_offline_plugins":0}}
```

## History

### `begin_agent_turn {label}` / `end_agent_turn {discard?}`

Everything between the two becomes one undo step, "Agent: <label>". Turns do not nest; `undo`/`redo` are refused while one is open. `end_agent_turn` returns `{"label", "recorded", "discarded", "history"}`; with `discard: true` the turn's edits are undone immediately.

### `undo {}` / `redo {}`

Undoes or redoes one step (a whole agent turn, if it was one). Returns `{"changes":[{"op":"undo","text":"Agent: arrange"}], "history": {...}}`.

### `get_history {}`

```json
← {"result":{"can_undo":true,"can_redo":false,"index":3,"count":3,"undo_text":"Agent: arrange",
   "redo_text":"","turn_open":false}}
```

## Other

- `ping {}` → `{"pong": true, "api_version": "0.1"}`
- `shutdown {}` → `{"ok": true}`; the engine closes the project (without saving) and exits after replying.
