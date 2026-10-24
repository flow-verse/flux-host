# SPDX-FileCopyrightText: © 2026 Flux contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Linux build environment for flux-host (see flux/docs/BUILDING-linux.md).
# Fedora 44 ships GCC 16, the version upstream's Conan profiles target.
FROM fedora:44

RUN dnf -y install \
      gcc-c++ libstdc++-static git make cmake ninja-build pkgconf-pkg-config perl python3 python3-pip \
      gettext flex which xz tar unzip patch findutils diffutils file \
      alsa-lib-devel pipewire-jack-audio-connection-kit-devel \
      freetype-devel fontconfig-devel libcurl-devel \
      libX11-devel libXext-devel libXrandr-devel libXinerama-devel libXcursor-devel \
      libXi-devel libXrender-devel libXfixes-devel libxkbcommon-devel libxkbcommon-x11-devel \
      libxcb-devel xcb-util-devel xcb-util-image-devel xcb-util-keysyms-devel \
      xcb-util-renderutil-devel xcb-util-wm-devel xcb-util-cursor-devel \
      mesa-libGL-devel mesa-libEGL-devel libdrm-devel wayland-devel \
      dbus-devel systemd-devel \
      lv2-devel \
    && dnf clean all

ENV QT_QPA_PLATFORM=offscreen
WORKDIR /work
