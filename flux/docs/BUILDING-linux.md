# Building Flux on Linux (postponed)

Status, 2026-10-24: **not completed**. The owner postponed the Linux build to keep the Mac Studio's
disk free; macOS is the supported build for now (`BUILDING-macos.md`).

What exists, in `flux/ci/linux/`:

- `Dockerfile`: Fedora 44 (GCC 16.2, the version upstream's Conan profiles target) with the system
  libraries Qt, JUCE and the audio stack need, including `libstdc++-static` (Conan's Ninja build
  links statically).
- `profiles/gcc_release_arm64`: upstream's `gcc_release` for 64-bit ARM (upstream's targets x86_64
  with `-march=x86-64-v3`).
- `build.sh`: clones the repository into a volume and runs Conan, configure, build and ctest
  (`deps`, `build`, `test` or `all`).

```bash
docker build -t flux-linux flux/ci/linux
docker run --rm -v "$PWD":/src:ro -v flux-linux-work:/work \
  -v flux-linux-conan:/root/.conan2 flux-linux bash /src/flux/ci/linux/build.sh
```

What was learned:

- In an OrbStack container on the Mac Studio (20 CPUs, 45 GB) the Conan stage got Qt 6.11.1 74 %
  built in 71 minutes before the **host disk filled up** and took OrbStack down. The Conan cache
  volume alone reached 31.6 GB mid-build. Budget at least **40 GB free disk** before retrying.
- The first attempt failed in Conan's Ninja build until `libstdc++-static` was added.
- Not reached: configuring and compiling flux-host, the tests, and checking the Linux install tree
  (desktop file, icon, AppStream metadata, `flux` executable from `flux/cmake/branding.cmake`).

Options when this is picked up again: free disk first, build on a Linux machine or a GitHub Actions
runner (x86_64 can use upstream's `gcc_release` profile directly), or limit Qt's parallel jobs.
