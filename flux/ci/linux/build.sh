#!/usr/bin/env bash
# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Builds and tests flux-host inside the container from flux/ci/linux/Dockerfile.
# Expects the repository mounted read-only at /src; builds a clean clone in
# /work/flux-host (a volume), with the Conan cache in /root/.conan2 (a volume).
#
#   docker build -t flux-linux flux/ci/linux
#   docker run --rm -v "$PWD":/src:ro -v flux-linux-work:/work \
#     -v flux-linux-conan:/root/.conan2 flux-linux bash /src/flux/ci/linux/build.sh [stage]
#
# Stages: deps (Conan), build (configure + compile), test (ctest), all (default).
set -euo pipefail
STAGE="${1:-all}"
JOBS="${JOBS:-$(nproc)}"
SRC=/src
W=/work/flux-host
PROFILE="$SRC/flux/ci/linux/profiles/gcc_release_arm64"
if [ "$(uname -m)" = "x86_64" ]; then PROFILE=gcc_release; fi

stamp () { echo "== $(date -u +%H:%M:%S) $*"; }

if [ ! -d "$W/.git" ]; then
  stamp "cloning"
  git clone -q "$SRC" "$W"
fi
git -C "$W" fetch -q "$SRC" HEAD
git -C "$W" checkout -q --force FETCH_HEAD
stamp "at $(git -C "$W" log --oneline -1)"
cd "$W"

if [ ! -x venv/bin/conan ]; then
  python3 -m venv venv
  venv/bin/pip -q install conan==2.31.2 cmake==4.4.2 ninja==1.13.0
fi
# shellcheck disable=SC1091
source venv/bin/activate

if [ "$STAGE" = deps ] || [ "$STAGE" = all ]; then
  stamp "conan config + recipes"
  conan config install conan >/dev/null
  git submodule update --init --recursive ext/conan-center-index
  conan remote add conan-local-index ext/conan-center-index 2>/dev/null || true
  python3 tools/export_conan_recipes.py >/dev/null
  stamp "conan install ($PROFILE)"
  time conan install . -pr:h "$PROFILE" -pr:b "$PROFILE" --build=missing
fi

if [ "$STAGE" = build ] || [ "$STAGE" = all ]; then
  stamp "configure"
  time cmake --preset default
  stamp "build (-j$JOBS)"
  time cmake --build conanbuild/Release --parallel "$JOBS"
fi

if [ "$STAGE" = test ] || [ "$STAGE" = all ]; then
  stamp "test"
  # shellcheck disable=SC1091
  source conanbuild/Release/generators/conanrun.sh
  time ctest --test-dir conanbuild/Release -j"$JOBS" --timeout 300 --output-on-failure
fi
stamp "done"
