// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

pragma ComponentBehavior: Bound

import QtQuick
import Zrythm
import ZrythmGui

Item {
  id: root

  required property AudioClip audioClip
  required property int contentHeight
  required property int contentWidth
  property bool loopPreview: false
  property real referenceWidth: 0
  property real referenceX: 0
  required property TempoMap tempoMap

  AudioClipWaveformCanvas {
    audioClip: root.audioClip
    height: root.contentHeight
    loopPreview: root.loopPreview
    outlineColor: Qt.lighter(ZrythmTheme.clipContentColor, 1.4)
    referenceWidth: root.referenceWidth
    referenceX: root.referenceX
    tempoMap: root.tempoMap
    waveformColor: ZrythmTheme.clipContentColor
    width: root.contentWidth
  }
}
