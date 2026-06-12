/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.kdenlive as K


Item {
    id: rulerRoot
    anchors.fill: parent
    SystemPalette { id: activePalette }
    // The standard width for labels. Depends on format used (frame number or full timecode)
    property int labelSize: fontMetrics.boundingRect(dopeRoot.proxy.toTimecode(36000)).width
    // The spacing between labels. Depends on labelSize
    property real labelSpacing: labelSize
    // The space we want between each ticks in the ruler
    property real scalingFactor: 1.
    property real tickSpacing: scalingFactor
    property int timecodeOffset : timeline.timecodeOffset
    property bool pressed: rulerMouseArea.pressed
    property int rulerOffset: 0
    property int labelMod: 1
    property color dimmedColor: (activePalette.text.r + activePalette.text.g + activePalette.text.b > 1.5) ? Qt.darker(activePalette.text, 1.3) : Qt.lighter(activePalette.text, 1.3)
    property color dimmedColor2: (activePalette.text.r + activePalette.text.g + activePalette.text.b > 1.5) ? Qt.darker(activePalette.text, 2.2) : Qt.lighter(activePalette.text, 2.2)

    onScalingFactorChanged: {
        console.log('FOT UPDATED SCALING: ', rulerRoot.scalingFactor)
        adjustStepSize()
    }
    
    function adjustStepSize() {
        rulerRoot.labelSize = fontMetrics.boundingRect(dopeRoot.proxy.toTimecode(36000)).width
        if (rulerRoot.scalingFactor > 19) {
            // Frame size >= 20 pixels
            rulerRoot.tickSpacing = rulerRoot.scalingFactor
            // labelSpacing cannot be smaller than 1 frame
            rulerRoot.labelSpacing = rulerRoot.scalingFactor > rulerRoot.labelSize * 1.3 ? rulerRoot.scalingFactor : Math.floor(rulerRoot.labelSize/rulerRoot.scalingFactor) * rulerRoot.scalingFactor
        } else {
            rulerRoot.tickSpacing = Math.floor(3 * K.UiUtils.baseSizeMedium / rulerRoot.scalingFactor) * rulerRoot.scalingFactor
            rulerRoot.labelSpacing = (Math.floor(rulerRoot.labelSize/rulerRoot.tickSpacing) + 1) * rulerRoot.tickSpacing
        }
        rulerRoot.labelMod = Math.max(1, Math.ceil((rulerRoot.labelSize + K.UiUtils.baseSizeMedium) / rulerRoot.tickSpacing))
        //console.log('LABELMOD: ', Math.ceil((rulerRoot.labelSize + root.fontUnit) / rulerRoot.tickSpacing)))
        tickRepeater.model = Math.ceil(rulercontainer.width / rulerRoot.tickSpacing) + 2
        console.log('TICK MODEL DEFINED: ', Math.ceil(rulercontainer.width / rulerRoot.tickSpacing), 'RULER WIDTH: ', rulercontainer.width, 'TiCK SPACNIOG: ', rulerRoot.tickSpacing)
    }

    function adjustFormat() {
        rulerRoot.labelSize = fontMetrics.boundingRect(dopeRoot.proxy.toTimecode(36000)).width
        adjustStepSize()
        repaintRuler()
    }

    function repaintRuler() {
        // Enforce repaint
        tickRepeater.model = 0
        tickRepeater.model = Math.ceil(rulercontainer.width / rulerRoot.tickSpacing) + 2
    }
    
    // Ruler marks
    Item {
        id: timecodeContainer
        anchors.fill: parent
        Repeater {
            id: tickRepeater
            model: Math.ceil(timecodeContainer.width / rulerRoot.tickSpacing) + 2
            property int offset: Math.floor(rulercontainer.contentX /rulerRoot.tickSpacing)
            Item {
                required property int index
                property int realPos: (tickRepeater.offset + index) * rulerRoot.tickSpacing / rulerRoot.scalingFactor
                x: Math.round(realPos * rulerRoot.scalingFactor)
                height: parent.height
                property bool showText: (tickRepeater.offset + index)%rulerRoot.labelMod == 0
                Rectangle {
                    anchors.bottom: parent.bottom
                    height: parent.showText ? K.UiUtils.baseSizeMedium * 0.8 : 4
                    width: 1
                    color: rulerRoot.dimmedColor2
                }
                Label {
                    visible: parent.showText
                    anchors.top: parent.top
                    //anchors.horizontalCenter: parent.horizontalCenter
                    text: dopeRoot.proxy.toTimecode(parent.realPos + rulerRoot.timecodeOffset)
                    font: K.UiUtils.smallestReadableFont
                    color: rulerRoot.dimmedColor
                }
            }
        }
    }

    MouseArea {
        id: rulerMouseArea
        anchors.fill: parent
        hoverEnabled: false
        acceptedButtons: Qt.LeftButton
        z: 1
        onPressed: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                var pos = Math.max(mouseX, 0)
                pos = Math.min(pos, width)
                var frame = Math.round(pos / rulerRoot.scalingFactor)
                if (mouse.modifiers & Qt.AltModifier) {
                    frame = controller.suggestPlayheadSnapPoint(frame, root.snapping)
                }
                console.log('PRESSED ON FRAME: ', frame, ', OFFSET: ', rulerRoot.rulerOffset)
                dopeRoot.proxy.position = frame + rulerRoot.rulerOffset
                mouse.accepted = true
            }
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton && pressed) {
                var pos = Math.max(mouseX, 0)
                pos = Math.min(pos, width)
                var frame = Math.round(pos / rulerRoot.scalingFactor)
                if (mouse.modifiers & Qt.AltModifier) {
                    frame = controller.suggestPlayheadSnapPoint(frame, root.snapping)
                }
                dopeRoot.proxy.position = frame + rulerRoot.rulerOffset
            }
        }
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                root.zoomByWheel(wheel)
            } else {
                wheel.accepted = false
            }
        }
    }
}

