// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

import QtQuick
import Zrythm
import ZrythmGui
import ZrythmStyle

Item {
  id: root

  required property ChordClip chordClip
  required property int contentHeight
  required property int contentWidth
  property bool loopPreview: false
  property real referenceWidth: 0
  property real referenceX: 0

  ChordClipCanvas {
    chordClip: root.chordClip
    font: ZrythmTheme.arrangerObjectTextFont
    height: root.contentHeight
    loopPreview: root.loopPreview
    referenceWidth: root.referenceWidth
    referenceX: root.referenceX
    textColor: ZrythmTheme.clipContentColor
    width: root.contentWidth
  }
}
