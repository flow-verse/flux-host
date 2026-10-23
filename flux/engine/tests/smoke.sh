#!/usr/bin/env bash
# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# End-to-end smoke test: drives `flux-engine serve` over stdio with a scripted
# JSON-RPC session (build a project with the CLAP Test Synth, save, reopen,
# render) and checks every response.
#
# usage: smoke.sh <path/to/flux-engine> <clap-plugin-dir>
# Works with the macOS system bash (3.2): no coproc, no associative arrays.

set -euo pipefail

ENGINE=${1:?usage: smoke.sh <flux-engine> <clap-plugin-dir>}
CLAP_DIR=${2:?usage: smoke.sh <flux-engine> <clap-plugin-dir>}

WORK=$(mktemp -d "${TMPDIR:-/tmp}/flux-engine-smoke.XXXXXX")
ENGINE_PID=""
cleanup () {
  exec 3>&- 4<&- 2>/dev/null || true
  if [ -n "$ENGINE_PID" ] && kill -0 "$ENGINE_PID" 2>/dev/null; then
    kill "$ENGINE_PID" 2>/dev/null || true
  fi
  rm -rf "$WORK"
}
trap cleanup EXIT

fail () {
  echo "FAIL: $*" >&2
  echo "--- engine stderr (tail) ---" >&2
  tail -n 30 "$WORK/stderr.log" >&2 || true
  exit 1
}

mkfifo "$WORK/in" "$WORK/out"
"$ENGINE" serve --plugin-path "$CLAP_DIR" <"$WORK/in" >"$WORK/out" 2>"$WORK/stderr.log" &
ENGINE_PID=$!
exec 3>"$WORK/in"
exec 4<"$WORK/out"

# Reads one line from the engine (60 s timeout)
recv () {
  local line
  if ! IFS= read -r -t 60 line <&4; then
    fail "no response from the engine"
  fi
  printf '%s' "$line"
}

ID=0
# call <method> <params-json>: prints the response line; fails on a JSON-RPC error
call () {
  ID=$((ID + 1))
  printf '{"jsonrpc":"2.0","id":%d,"method":"%s","params":%s}\n' "$ID" "$1" "$2" >&3
  local resp
  resp=$(recv)
  case "$resp" in
    *'"result":'*) ;;
    *) fail "$1 failed: $resp" ;;
  esac
  echo "ok   $1" >&2
  printf '%s' "$resp"
}

# field <json> <key>: first string value of "key" (UUIDs, paths)
field () {
  printf '%s' "$1" | grep -o "\"$2\":\"[^\"]*\"" | head -n 1 | sed -e "s/^\"$2\":\"//" -e 's/"$//'
}

READY=$(recv)
case "$READY" in
  *'"method":"ready"'*) echo "ok   ready" >&2 ;;
  *) fail "expected a ready notification, got: $READY" ;;
esac

call ping '{}' >/dev/null
PROJECT="$WORK/song"
call create_project "{\"dir\":\"$PROJECT\",\"bpm\":120,\"time_signature\":\"4/4\"}" >/dev/null

PLUGINS=$(call list_plugins '{"format":"clap","query":"Test Synth"}')
case "$PLUGINS" in
  *'"name":"Test Synth"'*) ;;
  *) fail "Test Synth not listed: $PLUGINS" ;;
esac

call begin_agent_turn '{"label":"smoke"}' >/dev/null
ADD=$(call add_track '{"type":"instrument","name":"Lead","plugin_id":"Test Synth"}')
TRACK=$(field "$ADD" track)
[ -n "$TRACK" ] || fail "add_track returned no track id: $ADD"

NOTES='[{"pitch":60,"start_beat":0,"length_beats":1,"velocity":100},
        {"pitch":64,"start_beat":1,"length_beats":1,"velocity":90},
        {"pitch":67,"start_beat":2,"length_beats":1,"velocity":80},
        {"pitch":72,"start_beat":3,"length_beats":1,"velocity":70}]'
NOTES=$(printf '%s' "$NOTES" | tr -d '\n ')
call write_notes "{\"track\":\"$TRACK\",\"start_bar\":1,\"length_bars\":4,\"notes\":$NOTES}" >/dev/null
call write_notes "{\"track\":\"$TRACK\",\"start_bar\":3,\"length_bars\":1,\"notes\":$NOTES,\"dry_run\":true}" >/dev/null
call set_tempo '{"bpm":120}' >/dev/null
call add_marker '{"name":"Verse","bar":1}' >/dev/null
call set_range '{"start_bar":1,"end_bar":5}' >/dev/null
call end_agent_turn '{}' >/dev/null

# Malformed input must produce errors, not a crash
printf '{"jsonrpc":"2.0","id":"bad1","method":\n' >&3
BAD=$(recv)
case "$BAD" in
  *'"code":-32700'*) echo "ok   parse error" >&2 ;;
  *) fail "expected a parse error, got: $BAD" ;;
esac
printf '{"jsonrpc":"2.0","id":"bad2","method":"write_notes","params":{"track":"%s","start_bar":1,"length_bars":1,"notes":[{"pitch":300,"start_beat":0,"length_beats":1}]}}\n' "$TRACK" >&3
BAD=$(recv)
case "$BAD" in
  *'"code":-32602'*) echo "ok   invalid params" >&2 ;;
  *) fail "expected invalid params, got: $BAD" ;;
esac

call save_project '{}' >/dev/null
call open_project "{\"dir\":\"$PROJECT\"}" >/dev/null
DESC=$(call describe_project '{}')
case "$DESC" in
  *"\"id\":\"$TRACK\""*) ;;
  *) fail "reopened project lacks the track: $DESC" ;;
esac

RENDER=$(call render "{\"out\":\"$WORK/out.wav\"}")
case "$RENDER" in
  *'"peak_dbfs":null'*) fail "render is silent: $RENDER" ;;
esac
[ -s "$WORK/out.wav" ] || fail "no output file"
echo "     $RENDER" >&2

call shutdown '{}' >/dev/null
exec 3>&-
if ! wait "$ENGINE_PID"; then
  ENGINE_PID=""
  fail "engine exited with an error"
fi
ENGINE_PID=""
echo "PASS" >&2
