/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick 2.15

import org.kde.kdenlive as K

Rectangle {
    id: audioSeekZone
    width: parent.width
    height: parent.height / 6
    property int seekViewMargin: root.baseUnit * 0.8
    property int zoomZoneBorder: root.baseUnit * 0.24
    anchors.centerIn: parent
    anchors.verticalCenterOffset: audioThumb.isAudioClip ? parent.height * 5 / 12 : 0
    color: activePalette.midlight
    MouseArea {
        id: containerArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.MiddleButton
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                wheel.accepted = false
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
    }
    Repeater {
        id: streamThumbMini
        model: controller.audioStreams.length
        anchors.fill: parent
        property double streamHeight: audioSeekZone.height / streamThumbMini.count - (4 * audioSeekZone.zoomZoneBorder)
        onCountChanged: {
            thumbTimer.start()
        }
        Item {
            // Color for the waveform (behind the wave, will be seen by transparency)
            y: model.index * streamThumbMini.streamHeight + 2 * audioSeekZone.zoomZoneBorder
            height: streamThumbMini.streamHeight
            width: streamThumbMini.width
            Rectangle {
                height: streamThumbMini.streamHeight - 1
                anchors.right: parent.right
                anchors.left: parent.left
                color: Qt.darker(activePalette.text)
            }
            // Selected zone highlight
            Rectangle {
                x: controller.zoneIn * audioThumb.width / root.duration
                width: (controller.zoneOut - controller.zoneIn) * audioThumb.width / root.duration
                height: waveform2.height
                color: activePalette.highlight
                visible: controller.zoneOut > controller.zoneIn && (controller.zoneIn > 0 || controller.zoneOut < root.duration)
            }
            K.TimelineWaveform {
                id: waveform2
                isOpaque: false
                anchors.right: parent.right
                anchors.left: parent.left
                height: streamThumbMini.streamHeight
                property int aChannels: controller.audioChannels[model.index]
                channels: aChannels
                binId: controller.clipId
                audioStream: controller.audioStreams[model.index]
                format: K.KdenliveSettings.displayallchannels
                normalize: K.KdenliveSettings.normalizechannels
                property int aClipDuration: root.duration + 1
                scaleFactor: audioThumb.width / aClipDuration
                waveInPoint: 0
                waveOutPoint: waveform2.aClipDuration
                fgColorEven: "#00000000"//K.KdenliveSettings.thumbColor1
                fgColorOdd: "#00000000" //K.KdenliveSettings.thumbColor2
                bgColorEven: audioSeekZone.color
                bgColorOdd: audioSeekZone.color
            }
            Rectangle {
                width: parent.width
                y: (model.index + 1) * (streamThumbMini.streamHeight + audioSeekZone.seekViewMargin * 0.5)
                height: 1
                visible: streamThumbMini.count > 1 && model.index < streamThumbMini.count - 1
                color: 'red'
            }
        }
    }
    // Playhead position
    Rectangle {
        color: "#99FF0000"
        width: 2
        height: parent.height
        x: controller.position * audioThumb.width / root.duration
    }
    // Current view reference
    Rectangle {
        id: zoomRef
        x: audioSeekZone.width * root.zoomStart
        width: audioSeekZone.width * root.zoomFactor
        height: parent.height
        border.width: audioSeekZone.zoomZoneBorder
        border.color: activePalette.text
        color: 'transparent'
    }

    // Top handle rect
    Rectangle {
        id: dragRect
        height: audioSeekZone.zoomZoneBorder
        visible: topHandleArea.containsMouse || topHandleArea.pressed
        color: activePalette.highlight
        anchors.left: zoomRef.left
        anchors.right: zoomRef.right
    }
    // Bottom handle rect
    Rectangle {
        id: dragBottomRect
        height: audioSeekZone.zoomZoneBorder
        visible: bottomHandleArea.containsMouse || bottomHandleArea.pressed
        color: activePalette.highlight
        anchors.left: zoomRef.left
        anchors.right: zoomRef.right
        anchors.bottom: zoomRef.bottom
    }
    // Current view left corner
    Rectangle {
        id: zoomViewLeft
        visible: leftHandle.containsMouse || leftHandle.pressed
        anchors.left: zoomRef.left
        height: audioSeekZone.height
        width: audioSeekZone.zoomZoneBorder
        color: activePalette.highlight
        radius: 2
    }

    // Current view right corner
    Rectangle {
        id: zoomViewRight
        height: audioSeekZone.height
        width: audioSeekZone.zoomZoneBorder
        anchors.right: zoomRef.right
        visible: rightHandle.containsMouse || rightHandle.pressed
        color: activePalette.highlight
        radius: 2
    }
    // Current view top corner
    MouseArea {
        // Top handle
        id: topHandleArea
        height: audioSeekZone.seekViewMargin
        //width: audioSeekZone.width * root.zoomFactor
        width: zoomRef.width
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
                updatedPos = Math.min(audioSeekZone.width - topHandleArea.width, updatedPos)
                var zs = updatedPos / audioSeekZone.width
                root.zoomStart = zs
            }
        }
    }
    // Current view top corner
    MouseArea {
        // Top handle
        id: bottomHandleArea
        height: audioSeekZone.seekViewMargin
        //width: audioSeekZone.width * root.zoomFactor
        width: zoomRef.width
        property int clickPos
        anchors {
            left: pressed ? undefined : zoomRef.left
            bottom: zoomRef.bottom
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
                updatedPos = Math.min(audioSeekZone.width - topHandleArea.width, updatedPos)
                var zs = updatedPos / audioSeekZone.width
                root.zoomStart = zs
            }
        }
    }
    MouseArea {
        // Left handle
        id: leftHandle
        width: root.baseUnit
        height: zoomRef.height
        anchors.left: pressed ? undefined : zoomRef.left
        hoverEnabled: true
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
                console.log('setting zoom start pos: ', updatedPos, ', ZOOM RIGHT: ', (zoomRef.x + topHandleArea.width))
                updatedPos = Math.min(updatedPos, zoomRef.x + topHandleArea.width)
                var zs = updatedPos / audioSeekZone.width
                var zf = (zoomRef.x + topHandleArea.width - updatedPos) / audioSeekZone.width
                root.zoomStart = zs
                root.zoomFactor = zf
            }
        }
    }
    MouseArea {
        // Right handle
        id: rightHandle
        width: root.baseUnit
        height: zoomRef.height
        anchors {
            right: pressed ? undefined : zoomRef.right
        }
        hoverEnabled: true
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
