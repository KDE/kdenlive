/*
 * Copyright (c) 2021 Jean-Baptiste Mardelle
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.11
import QtQuick.Controls 2.4

// monitor zone
Rectangle {
    id: rzone
    property int frameIn: 0
    property int frameOut: 0
    x:  frameIn * timeline.scaleFactor
    width: (frameOut - frameIn) * timeline.scaleFactor
    visible: frameOut > frameIn
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
                startZone = Qt.point(frameIn, frameOut)
                anchors.left= undefined
            }
            onEntered: {
                resizeActive = true
            }
            onExited: {
                resizeActive = false
            }
            onReleased: {
                updateZone(startZone, Qt.point(frameIn, frameOut), true)
                resizeActive = false
                anchors.left= parent.left
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    resizeActive = true
                    var offset = Math.round(mouseX/ timeline.scaleFactor)
                    if (offset != 0) {
                        var newPos = Math.max(0, controller.suggestSnapPoint(frameIn + offset,mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping))
                        if (newPos == frameIn + offset) {
                            // No snap at start, check end
                            var newPos = Math.max(0, controller.suggestSnapPoint(frameOut + offset,mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping))
                            if (newPos == frameOut + offset) {
                                newPos = frameIn + offset
                            } else {
                                newPos = frameIn + (newPos - frameOut)
                            }
                        }
                        if (newPos < 0) {
                            newPos = frameIn + offset;
                        }
                        frameOut += newPos - frameIn
                        frameIn = newPos
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
            text: timeline.timecode(frameIn)
            font: miniFont
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
            text: timeline.timecode(frameOut)
            font: miniFont
            color: activePalette.highlightedText
        }
    }
    Rectangle {
        id: durationRect
        anchors.bottom: rzone.top
        visible: (!useTimelineRuler && moveMouseArea.containsMouse && !moveMouseArea.pressed) || ((useTimelineRuler || trimInMouseArea.drag.active || trimOutMouseArea.drag.active) && showZoneLabels && parent.width > 3 * width) || (useTimelineRuler && !trimInMouseArea.drag.active && !trimOutMouseArea.drag.active)
        anchors.horizontalCenter: parent.horizontalCenter
        width: durationLabel.contentWidth + 4
        height: durationLabel.contentHeight
        color: activePalette.highlight
        Label {
            id: durationLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            text: timeline.timecode(frameOut - frameIn)
            font: miniFont
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
                    resizeActive = true
                    parent.opacity = 1
                }
                onExited: {
                    resizeActive = false
                    parent.opacity = 0
                }
                onPressed: {
                    parent.anchors.left = undefined
                    parent.opacity = 1
                    startZone = Qt.point(frameIn, frameOut)
                }
                onReleased: {
                    resizeActive = false
                    parent.anchors.left = rzone.left
                    updateZone(startZone, Qt.point(frameIn, frameOut), true)
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        resizeActive = true
                        var newPos = controller.suggestSnapPoint(frameIn + Math.round(trimIn.x / timeline.scaleFactor), mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping)
                        if (newPos < 0) {
                            newPos = 0
                        }
                        frameIn = frameOut > -1 ? Math.min(newPos, frameOut - 1) : newPos
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
                    resizeActive = true
                    parent.opacity = 1
                }
                onExited: {
                    resizeActive = false
                    parent.opacity = 0
                }
                onPressed: {
                    parent.anchors.right = undefined
                    parent.opacity = 1
                    startZone = Qt.point(frameIn, frameOut)
                }
                onReleased: {
                    resizeActive = false
                    parent.anchors.right = rzone.right
                    updateZone(startZone, Qt.point(frameIn, frameOut), true)
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        resizeActive = true
                        frameOut = Math.max(controller.suggestSnapPoint(frameIn + Math.round((trimOut.x + trimOut.width) / timeline.scaleFactor), mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping), frameIn + 1)
                    }
                }
            }
        }
}


