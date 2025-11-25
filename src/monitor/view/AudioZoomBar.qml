/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls
import QtQuick

import org.kde.kdenlive as K

Rectangle {
    id: audioSeekZone
    property int zoomZoneBorder: root.baseUnit * 0.24
    property bool containsMouse: containerArea.containsMouse || mainHandleArea.containsMouse || leftHandle.containsMouse || mainHandleArea.pressed || leftHandle.pressed || rightHandle.pressed || rightHandle.containsMouse
    color: activePalette.midlight
    width: parent.width
    MouseArea {
        id: containerArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.MiddleButton | Qt.LeftButton
        onWheel: wheel => {
            if (wheel.modifiers & Qt.AltModifier) {
                wheel.accepted = false
                return
            }

            if (wheel.modifiers & Qt.ControlModifier) {
                // Zoom in audio wave
                if (wheel.angleDelta.y == 0) {
                 // Don't trigger zoom if delta is null
                    return
                }
                if (wheel.angleDelta.y < 0) {
                    // zoom out
                    clipMonitorRuler.zoomOutRuler(wheel.x)
                } else {
                    // zoom in
                    clipMonitorRuler.zoomInRuler(wheel.x)
                }
            } else {
                var newPos = zoomRef.x
                if (wheel.angleDelta.y < 0) {
                    newPos = newPos + 10
                } else {
                    newPos = newPos - 10
                }
                var zs = Math.max(0, newPos / audioSeekZone.width)
                zs = Math.min((audioSeekZone.width - zoomRef.width) / audioSeekZone.width, zs)
                root.zoomStart = zs
            }
        }
        onPressed: mouse => {
            root.captureRightClick = true
            if (mouse.buttons === Qt.LeftButton) {
                var updatedPos = Math.max(0, mouseX - zoomRef.width / 2)
                updatedPos = Math.min(audioSeekZone.width - zoomRef.width, updatedPos)
                var zs = updatedPos / audioSeekZone.width
                root.zoomStart = zs
            }
        }
        onReleased: mouse => {
            root.captureRightClick = false
        }
    }
    Item {
        id: thumbsContainer
        anchors.fill: audioSeekZone
        anchors.topMargin: 2 * audioSeekZone.zoomZoneBorder
        anchors.bottomMargin: 2 * audioSeekZone.zoomZoneBorder
        property double streamHeight: (audioSeekZone.height - (4 * audioSeekZone.zoomZoneBorder) - (2 * controller.audioStreams.length - 1)) / controller.audioStreams.length
        visible: height > 8

        Repeater {
            id: streamThumbMini
            anchors.fill: parent
            model: controller.audioStreams.length
            onCountChanged: {
                thumbTimer.start()
            }
            Item {
                // Color for the waveform (behind the wave, will be seen by transparency)
                id: streamContainer
                y: model.index * (thumbsContainer.streamHeight + 2)
                height: thumbsContainer.streamHeight
                width: streamThumbMini.width
                // Normal color for the audio wave
                Rectangle {
                    height: thumbsContainer.streamHeight - 2
                    anchors.right: parent.right
                    anchors.left: parent.left
                    color: Utils.mixColors(activePalette.midlight, activePalette.text, 0.3)
                }
                // Highlight color for the selected wave part
                Rectangle {
                    x: controller.zoneIn * audioThumb.width / root.duration
                    width: (controller.zoneOut - controller.zoneIn) * audioThumb.width / root.duration
                    height: thumbsContainer.streamHeight - 2
                    color:  Utils.mixColors(activePalette.midlight, activePalette.highlight, 0.7)
                        //Utils.desaturateColor(activePalette.highlight, 0.6, 1)
                    visible: controller.zoneOut > controller.zoneIn
                }
                K.TimelineWaveform {
                    id: waveform2
                    isOpaque: false
                    anchors.right: parent.right
                    anchors.left: parent.left
                    height: thumbsContainer.streamHeight
                    property int aChannels: controller.audioChannels[model.index]
                    channels: aChannels
                    binId: controller.clipId
                    audioStream: controller.audioStreams[model.index]
                    format: controller.clipHasAV ? false : K.KdenliveSettings.displayallchannels
                    normalize: K.KdenliveSettings.normalizechannels
                    property int aClipDuration: root.duration + 1
                    scaleFactor: audioThumb.width / aClipDuration
                    waveInPoint: 0
                    waveOutPoint: waveform2.aClipDuration
                    fgColorEven: "#00000000"//K.KdenliveSettings.thumbColor1
                    fgColorOdd: "#00000000" //K.KdenliveSettings.thumbColor2
                    bgColorEven: audioSeekZone.color
                    bgColorOdd: audioSeekZone.color
                    Rectangle {
                        width: streamContainer.width
                        height: 2
                        y: -2
                        visible: model.index > 0
                        color: Qt.darker(audioSeekZone.color) //activePalette.base
                    }
                }
                // fade a bit the not viewed zone audio wave
                Rectangle {
                    visible: root.zoomStart > 0
                    height: thumbsContainer.streamHeight - 2
                    anchors.left: parent.left
                    width: streamThumbMini.width * root.zoomStart
                    color: audioSeekZone.color
                    opacity: 0.3
                }
                Rectangle {
                    visible: root.zoomFactor < 1
                    height: thumbsContainer.streamHeight - 2
                    width: streamThumbMini.width * (1 - root.zoomStart - root.zoomFactor)
                    anchors.right: parent.right
                    color: audioSeekZone.color
                    opacity: 0.3
                }
            }
        }
    }
    // Playhead position
    Rectangle {
        color: "#99FF0000"
        width: 2
        height: parent.height - 2 * zoomRef.border.width - 1
        x: controller.position * audioThumb.width / root.duration
        y: zoomRef.border.width
    }
    // Current view reference
    Rectangle {
        id: zoomRef
        x: audioSeekZone.width * root.zoomStart
        width: audioSeekZone.width * root.zoomFactor
        height: audioSeekZone.height - 1
        opacity: mainHandleArea.containsMouse || mainHandleArea.pressed ? 1 : root.zoomFactor === 1. ? 0.5 : 0.8
        radius: 2
        border.width: controller.clipHasAV ? 2 : 2
        border.color: mainHandleArea.containsMouse || mainHandleArea.pressed ? activePalette.highlight : activePalette.text
        color: 'transparent'
    }

    MouseArea {
        // Inside rect handle
        id: mainHandleArea
        anchors.fill: zoomRef
        property int clickPos
        anchors {
            left: pressed ? undefined : zoomRef.left
        }
        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor
        onPressed: mouse => {
            root.captureRightClick = true
            clickPos = mouseX
            cursorShape = Qt.ClosedHandCursor
        }
        onReleased: {
            root.captureRightClick = false
            cursorShape = Qt.OpenHandCursor
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                var updatedPos = Math.max(0, x + mouseX - clickPos)
                updatedPos = Math.max(0, updatedPos)
                updatedPos = Math.min(audioSeekZone.width - mainHandleArea.width, updatedPos)
                var zs = updatedPos / audioSeekZone.width
                root.zoomStart = zs
            }
        }
    }
    MouseArea {
        // Left handle
        id: leftHandle
        width: root.baseUnit * 2
        height: zoomRef.height
        anchors {
            left: pressed ? undefined : zoomRef.left
            leftMargin: zoomRef.width > 2 * root.baseUnit ? -root.baseUnit : -2 * root.baseUnit
        }
        cursorShape: Qt.SizeHorCursor
        onPressed: {
            root.captureRightClick = true
        }
        onReleased: {
            root.captureRightClick = false
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                var updatedPos = Math.max(0, x + mouseX)
                updatedPos = Math.min(updatedPos, zoomRef.x + zoomRef.width)
                var zs = updatedPos / audioSeekZone.width
                var zf = (zoomRef.x + zoomRef.width - updatedPos) / audioSeekZone.width
                root.zoomStart = zs
                root.zoomFactor = zf
            }
        }
    }
    MouseArea {
        // Right handle
        id: rightHandle
        width: root.baseUnit * 2
        height: zoomRef.height
        anchors {
            right: pressed ? undefined : zoomRef.right
            rightMargin: zoomRef.width > 2 * root.baseUnit ? -root.baseUnit : -2 * root.baseUnit
        }
        cursorShape: Qt.SizeHorCursor
        onPressed: {
            root.captureRightClick = true
        }
        onReleased: {
            root.captureRightClick = false
        }

        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                var updatedPos = Math.min(audioSeekZone.width, x + mouseX)
                updatedPos = Math.max(updatedPos, zoomRef.x)
                var zf = (updatedPos - zoomRef.x) / audioSeekZone.width
                root.zoomFactor = zf
            }
        }
    }
}
