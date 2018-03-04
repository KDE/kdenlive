/*
 * Copyright (c) 2013 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
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

import QtQuick 2.6
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: rulerRoot
    property real timeScale: timeline.scaleFactor
    property int labelSize: fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
    property real stepSize: labelSize
    property real frameSize: labelSize
    property real fontUnit: root.baseUnit * 0.9
    property alias rulerZone : zone
    property int workingPreview : timeline.workingPreview
    property bool useTimelineRuler : timeline.useRuler
    SystemPalette { id: activePalette }

    function adjustStepSize() {
        if (rulerRoot.timeScale > 19) {
            rulerRoot.frameSize = rulerRoot.timeScale
            rulerRoot.stepSize = rulerRoot.timeScale > rulerRoot.labelSize * 1.3 ? rulerRoot.timeScale : Math.floor(rulerRoot.labelSize/rulerRoot.timeScale+1) * rulerRoot.timeScale
        } else {
            rulerRoot.frameSize = rulerRoot.labelSize / 3
            rulerRoot.stepSize = rulerRoot.labelSize * 4 / 3
        }
    }

    function adjustFormat() {
        rulerRoot.labelSize = fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
        adjustStepSize()
    }
    height: fontMetrics.font.pixelSize * 2
    color: activePalette.window
    clip: true

    // Timeline preview stuff
    Repeater {
        model: timeline.dirtyChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: scrollView.flickableItem.contentX / rulerRoot.stepSize + modelData * rulerRoot.timeScale
            y: 0
            width: 25 * rulerRoot.timeScale
            height: parent.height / 4
            color: 'darkred'
        }
    }

    Repeater {
        model: timeline.renderedChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: scrollView.flickableItem.contentX / rulerRoot.stepSize + modelData * rulerRoot.timeScale
            y: 0
            width: 25 * rulerRoot.timeScale
            height: parent.height / 4
            color: 'darkgreen'
        }
    }
    Rectangle {
        id: working
        x: scrollView.flickableItem.contentX / rulerRoot.stepSize + rulerRoot.workingPreview * rulerRoot.timeScale
        y: 0
        width: 25 * rulerRoot.timeScale
        height: parent.height / 4
        color: 'orange'
        visible: rulerRoot.workingPreview > -1
    }

    // Ruler marks
    Repeater {
        model: scrollView.width / rulerRoot.frameSize + 2
        Rectangle {
            property int realPos: scrollView.flickableItem.contentX / rulerRoot.frameSize + index
            x: realPos * rulerRoot.frameSize
            anchors.bottom: parent.bottom
            height: (realPos % 4)? ((realPos % 2) ? 3 : 7) : 12
            width: 1
            color: activePalette.windowText
            opacity: 0.5
        }
    }
    // Ruler labels
    Repeater {
        model: scrollView.width / rulerRoot.stepSize + 2
        Label {
            property int realPos: scrollView.flickableItem.contentX / rulerRoot.stepSize + index
            anchors.top: parent.top
            anchors.topMargin: 2
            x: realPos * rulerRoot.stepSize
            text: timeline.timecode(realPos * rulerRoot.stepSize / rulerRoot.timeScale)
            font.pointSize: rulerRoot.fontUnit
            color: activePalette.windowText
        }
    }

    // monitor zone
    Rectangle {
        id: zone
        visible: timeline.zoneOut > timeline.zoneIn
        color: activePalette.highlight
        x: timeline.zoneIn * rulerRoot.timeScale
        width: (timeline.zoneOut - timeline.zoneIn) * rulerRoot.timeScale
        anchors.bottom: parent.bottom
        height: parent.height / 3
        opacity: useTimelineRuler ? 0.4 : 0.1
        Rectangle {
            id: centerDrag
            anchors.centerIn: parent
            height: parent.height
            width: height
            color: moveMouseArea.containsMouse || moveMouseArea.drag.active ? 'white' : 'transparent'
            border.color: 'white'
            border.width: 1.5
            Drag.active: moveMouseArea.drag.active
            Drag.proposedAction: Qt.MoveAction
            MouseArea {
                id: moveMouseArea
                anchors.fill: parent
                property double startX
                hoverEnabled: true
                cursorShape: Qt.SizeHorCursor
                drag.target: zone
                drag.axis: Drag.XAxis
                drag.smoothed: false
                onPressed: {
                    startX = zone.x
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var offset = Math.round(zone.x/ rulerRoot.timeScale) - timeline.zoneIn
                        if (offset != 0) {
                            var newPos = Math.max(0, controller.suggestSnapPoint(timeline.zoneIn + offset,root.snapping))
                            timeline.zoneOut += newPos - timeline.zoneIn
                            timeline.zoneIn = newPos
                        }
                    }
                }
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
                    cursorShape: Qt.SizeHorCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.smoothed: false

                    onPressed: {
                        parent.anchors.left = undefined
                        parent.opacity = 1
                    }
                    onReleased: {
                        parent.anchors.left = zone.left
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            var newPos = controller.suggestSnapPoint(timeline.zoneIn + Math.round(trimIn.x / rulerRoot.timeScale), root.snapping)
                            timeline.zoneIn = timeline.zoneOut > -1 ? Math.min(newPos, timeline.zoneOut - 1) : newPos
                        }
                    }
                    onEntered: parent.opacity = 1
                    onExited: parent.opacity = 0
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
                    cursorShape: Qt.SizeHorCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.smoothed: false

                    onPressed: {
                        parent.anchors.right = undefined
                        parent.opacity = 1
                    }
                    onReleased: {
                        parent.anchors.right = zone.right
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            timeline.zoneOut = Math.max(controller.suggestSnapPoint(timeline.zoneIn + Math.round((trimOut.x + trimOut.width) / rulerRoot.timeScale), root.snapping), timeline.zoneIn + 1)
                        }
                    }
                    onEntered: parent.opacity = 1
                    onExited: parent.opacity = 0
                }
            }
    }
}

