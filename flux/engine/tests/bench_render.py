#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Render-speed benchmark for flux-engine.

Builds a project through `flux-engine serve` (N instrument tracks, each with
the given plugin and one note per beat for the whole length), saves it, then
renders it with `flux-engine render --json` and prints the result.

usage: bench_render.py <flux-engine> <clap-dir> <work-dir>
           [--tracks 8] [--minutes 10] [--bpm 120] [--plugin "Test Synth"]
           [--block 256]
"""

import argparse
import json
import os
import subprocess
import sys
import time


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("engine")
    ap.add_argument("clap_dir")
    ap.add_argument("work_dir")
    ap.add_argument("--tracks", type=int, default=8)
    ap.add_argument("--minutes", type=float, default=10)
    ap.add_argument("--bpm", type=float, default=120)
    ap.add_argument("--plugin", default="Test Synth")
    ap.add_argument("--block", type=int, default=256)
    args = ap.parse_args()

    project = os.path.join(args.work_dir, "bench-project")
    if os.path.exists(project):
        sys.exit(f"{project} exists; use an empty work dir")
    os.makedirs(args.work_dir, exist_ok=True)

    bars = int(round(args.minutes * args.bpm / 4))  # 4/4
    serve = subprocess.Popen(
        [args.engine, "serve", "--plugin-path", args.clap_dir],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=open(os.path.join(args.work_dir, "serve.log"), "w"), text=True)
    ready = json.loads(serve.stdout.readline())
    assert ready["method"] == "ready", ready
    next_id = [0]

    def call(method, **params):
        next_id[0] += 1
        serve.stdin.write(json.dumps({"jsonrpc": "2.0", "id": next_id[0],
                                      "method": method, "params": params}) + "\n")
        serve.stdin.flush()
        resp = json.loads(serve.stdout.readline())
        if "error" in resp:
            sys.exit(f"{method}: {resp['error']}")
        return resp["result"]

    t0 = time.monotonic()
    call("create_project", dir=project, bpm=args.bpm)
    call("begin_agent_turn", label="benchmark")
    for t in range(args.tracks):
        track = call("add_track", type="instrument", name=f"Synth {t + 1}",
                     plugin_id=args.plugin)["track"]
        root = 48 + 3 * t
        notes = [{"pitch": root + (b % 8), "start_beat": b, "length_beats": 0.9,
                  "velocity": 90} for b in range(bars * 4)]
        call("write_notes", track=track, start_bar=1, length_bars=bars, notes=notes)
    call("set_range", start_bar=1, end_bar=bars + 1)
    call("end_agent_turn")
    call("save_project")
    call("shutdown")
    serve.wait()
    build_sec = time.monotonic() - t0

    out = os.path.join(args.work_dir, "bench.wav")
    t0 = time.monotonic()
    proc = subprocess.run(
        [args.engine, "render", project, "--out", out, "--json",
         "--block", str(args.block), "--plugin-path", args.clap_dir],
        capture_output=True, text=True, check=True)
    total_sec = time.monotonic() - t0
    result = json.loads(proc.stdout.strip().splitlines()[-1])
    summary = {
        "tracks": args.tracks,
        "plugin": args.plugin,
        "bars": bars,
        "block": args.block,
        "audio_sec": result["duration_sec"],
        "render_wall_sec": result["wall_sec"],
        "speed_factor": result["speed_factor"],
        "process_wall_sec_incl_startup": round(total_sec, 2),
        "end_to_end_speed": round(result["duration_sec"] / total_sec, 2),
        "peak_dbfs": result["peak_dbfs"],
        "build_project_sec": round(build_sec, 2),
    }
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
