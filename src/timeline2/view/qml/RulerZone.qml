/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.kdenlive as K

// monitor zone
Rectangle {
    id: rzone
    required property K.TimelineController timeline
    property int frameIn: 0
    property int frameOut: 0
    property bool resizeActive: false

    x:  frameIn * timeline.scaleFactor
    width: (frameOut - frameIn) * timeline.scaleFactor
    visible: frameOut > frameIn


    SystemPalette { id: activePalette }

    Rectangle {
        anchors.left: parent.left
        height: parent.height
        width: 2
        color: 'white'
        opacity: 0.5
    }
    Rectangle {
        anchors.right: parent.right
        height: parent.height
        width: 2
        color: 'white'
        opacity: 0.5
    }
    
    
    Rectangle {
        id: centerDrag
        anchors.centerIn: parent
        height: parent.height
        width: height
        color: moveMouseArea.containsMouse || moveMouseArea.drag.active ? 'white' : 'transparent'
        border.color: 'white'
        border.width: 1.5
        opacity: 0.5
        Drag.active: moveMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        MouseArea {
            id: moveMouseArea
            anchors.left: parent.left
            anchors.top: parent.top
            width: parent.width
            height: parent.height
            hoverEnabled: true
            drag.axis: Drag.XAxis
            drag.smoothed: false
            property var startZone
            onPressed: {
                startZone = Qt.point(rzone.frameIn, rzone.frameOut)
                anchors.left= undefined
            }
            onEntered: {
                rzone.resizeActive = true
            }
            onExited: {
                if (!pressed) {
                    rzone.resizeActive = false
                }
            }
            onReleased: {
                updateZone(startZone, Qt.point(rzone.frameIn, rzone.frameOut), true)
                rzone.resizeActive = false
                anchors.left= parent.left
            }
            onPositionChanged: mouse => {
                if (mouse.buttons === Qt.LeftButton) {
                    rzone.resizeActive = true
                    var offset = Math.round(mouseX/ rzone.timeline.scaleFactor)
                    if (offset != 0) {
                        var newPos = Math.max(0, controller.suggestSnapPoint(rzone.frameIn + offset,mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping))
                        if (newPos == rzone.frameIn + offset) {
                            // No snap at start, check end
                            var newPos = Math.max(0, controller.suggestSnapPoint(rzone.frameOut + offset,mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping))
                            if (newPos == rzone.frameOut + offset) {
                                newPos = rzone.frameIn + offset
                            } else {
                                newPos = rzone.frameIn + (newPos - rzone.frameOut)
                            }
                        }
                        if (newPos < 0) {
                            newPos = rzone.frameIn + offset;
                        }
                        rzone.frameOut += newPos - rzone.frameIn
                        rzone.frameIn = newPos
                    }
                }
            }
        }
    }
    // Zone frame indicator
    Rectangle {
        visible: trimInMouseArea.drag.active || trimInMouseArea.containsMouse
        width: inLabel.contentWidth + 4
        height: inLabel.contentHeight
        anchors.bottom: rzone.top
        color: activePalette.highlight
        Label {
            id: inLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            text: K.Core.timecodeString(rzone.frameIn)
            font: K.UiUtils.smallestReadableFont
            color: activePalette.highlightedText
        }
    }
    Rectangle {
        visible: trimOutMouseArea.drag.active || trimOutMouseArea.containsMouse
        width: outLabel.contentWidth + 4
        height: outLabel.contentHeight
        anchors.bottom: rzone.top
        color: activePalette.highlight
        x: rzone.width - (outLabel.contentWidth + 4)
        Label {
            id: outLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            text: K.Core.timecodeString(rzone.frameOut)
            font: K.UiUtils.smallestReadableFont
            color: activePalette.highlightedText
        }
    }
    Rectangle {
        id: durationRect
        anchors.bottom: rzone.top
        visible: (!rzone.timeline.useRuler && moveMouseArea.containsMouse && !moveMouseArea.pressed)
                 || ((rzone.timeline.useRuler || trimInMouseArea.drag.active || trimOutMouseArea.drag.active) && showZoneLabels && parent.width > 3 * width)
                 || (rzone.timeline.useRuler && !trimInMouseArea.drag.active && !trimOutMouseArea.drag.active)
        anchors.horizontalCenter: parent.horizontalCenter
        width: durationLabel.contentWidth + 4
        height: durationLabel.contentHeight
        color: activePalette.highlight
        Label {
            id: durationLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            text: K.Core.timecodeString(rzone.frameOut - rzone.frameIn)
            font: K.UiUtils.smallestReadableFont
            color: activePalette.highlightedText
        }
    }
    Rectangle {
            id: trimIn
            anchors.left: parent.left
            anchors.leftMargin: 0
            height: parent.height
            width: 5
            color: 'lawngreen'
            opacity: 0
            Drag.active: trimInMouseArea.drag.active
            Drag.proposedAction: Qt.MoveAction

            MouseArea {
                id: trimInMouseArea
                anchors.fill: parent
                hoverEnabled: true
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property var startZone
                onEntered: {
                    rzone.resizeActive = true
                    parent.opacity = 1
                }
                onExited: {
                    rzone.resizeActive = false
                    parent.opacity = 0
                }
                onPressed: {
                    parent.anchors.left = undefined
                    parent.opacity = 1
                    startZone = Qt.point(rzone.frameIn, rzone.frameOut)
                }
                onReleased: {
                    rzone.resizeActive = false
                    parent.anchors.left = rzone.left
                    updateZone(startZone, Qt.point(rzone.frameIn, rzone.frameOut), true)
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        rzone.resizeActive = true
                        var newPos = controller.suggestSnapPoint(rzone.frameIn + Math.round(trimIn.x / rzone.timeline.scaleFactor), mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping)
                        if (newPos < 0) {
                            newPos = 0
                        }
                        rzone.frameIn = rzone.frameOut > -1 ? Math.min(newPos, rzone.frameOut - 1) : newPos
                    }
                }
            }
        }
        Rectangle {
            id: trimOut
            anchors.right: parent.right
            anchors.rightMargin: 0
            height: parent.height
            width: 5
            color: 'darkred'
            opacity: 0
            Drag.active: trimOutMouseArea.drag.active
            Drag.proposedAction: Qt.MoveAction

            MouseArea {
                id: trimOutMouseArea
                anchors.fill: parent
                hoverEnabled: true
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property var startZone
                onEntered: {
                    rzone.resizeActive = true
                    parent.opacity = 1
                }
                onExited: {
                    rzone.resizeActive = false
                    parent.opacity = 0
                }
                onPressed: {
                    parent.anchors.right = undefined
                    parent.opacity = 1
                    startZone = Qt.point(rzone.frameIn, rzone.frameOut)
                }
                onReleased: {
                    rzone.resizeActive = false
                    parent.anchors.right = rzone.right
                    updateZone(startZone, Qt.point(rzone.frameIn, rzone.frameOut), true)
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        rzone.resizeActive = true
                        rzone.frameOut = Math.max(
                                               controller.suggestSnapPoint(rzone.frameIn + Math.round((trimOut.x + trimOut.width) / rzone.timeline.scaleFactor),
                                                                           mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping),
                                               rzone.frameIn + 1)
                    }
                }
            }
        }
}


