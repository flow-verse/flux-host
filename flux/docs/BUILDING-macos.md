# Building Flux on macOS (verified)

Verified on 2026-10-23 on a Mac Studio (Apple M1 Ultra, 20 cores, 64 GB, macOS 15.7.4) for flow-verse/flux-host#9.

`README.zrythm.md` says only CMake and Qt are needed because CMake fetches the rest. That is out of date for this Zrythm version: about 20 libraries (nlohmann_json, boost, TBB, lilv, …) come from Conan. Use the Conan route below.

## Toolchain

| Tool | Version | Source |
|---|---|---|
| Xcode | 26.3 (Apple clang 17.0.0, clang-1700.6.4.2) | App Store; selected with `DEVELOPER_DIR` (no `sudo xcode-select` needed) |
| Conan | 2.31.2 | `venv/` (pinned in `requirements.txt`) |
| CMake | 4.4.2 | `venv/` |
| Ninja | 1.13.0 | `venv/` |
| Qt | 6.11.1 | built from source by Conan (custom recipe in `ext/conan-center-index`) |
| librsvg (`rsvg-convert`) | Homebrew | required by `CMakeLists.txt` on macOS |

Homebrew's `qt` is not used by this route.

## Steps

```bash
brew install librsvg
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
uv venv -p 3.12 venv
VIRTUAL_ENV=$PWD/venv uv pip install conan==2.31.2 cmake==4.4.2 ninja==1.13.0
source venv/bin/activate
conan config install conan          # writes ~/.conan2 profiles (overwrites existing ones)
git submodule update --init --recursive ext/conan-center-index
conan remote add conan-local-index ext/conan-center-index
python3 tools/export_conan_recipes.py
conan install . -pr:h appleclang_release -pr:b appleclang_build \
  -s:h compiler.version=17 -s:b compiler.version=17 --build=missing
cmake --preset default
cmake --build conanbuild/Release
cd conanbuild/Release && QT_QPA_PLATFORM=offscreen ctest -j12 --timeout 300
```

The app is `conanbuild/Release/products/bin/Flux.app`.

## Fixes needed

- **Compiler version.** The upstream profile `conan/profiles/_apple_clang` sets `compiler.version=21` (Xcode 26.4+). Xcode 26.3 reports Apple clang 17, so pass `-s:h compiler.version=17 -s:b compiler.version=17`. The C++23 code compiles with it.
- **Xcode selection.** If `xcode-select -p` points at the Command Line Tools, set `DEVELOPER_DIR` as above; the tools-only clang (clang-1700.0.x) was not tried for the full build.
- **rsvg-convert.** Configure fails without it on macOS (`CMakeLists.txt` `find_program(RSVG_CONVERT_EXECUTABLE … REQUIRED)`).

## Timings (M1 Ultra)

| Step | Time |
|---|---|
| `conan install --build=missing` (incl. Qt built twice: host and build context) | 66 min |
| `cmake --preset default` (first run; CPM downloads JUCE, VST3 SDK, …) | 22 min |
| `cmake --build` (1,256 steps) | 8.5 min |
| `ctest` (3,709 tests, offscreen) | 56 s, all pass |

Re-running configure and build after source changes takes minutes, since Conan and CPM caches are reused.
