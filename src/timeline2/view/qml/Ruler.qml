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
    // The standard width for labels. Depends on format used (frame number or full timecode)
    property int labelSize: fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
    // The spacing between labels. Depends on labelSize
    property real labelSpacing: labelSize
    // The space we want between each ticks in the ruler
    property real tickSpacing: timeline.scaleFactor
    property real fontUnit: root.baseUnit * 0.9
    property alias rulerZone : zone
    property int workingPreview : timeline.workingPreview
    property int labelMod: 1
    property bool useTimelineRuler : timeline.useRuler
    property bool showZoneLabels: false
    SystemPalette { id: activePalette }

    function adjustStepSize() {
        if (timeline.scaleFactor > 19) {
            // Frame size >= 20 pixels
            rulerRoot.tickSpacing = timeline.scaleFactor
            // labelSpacing cannot be smaller than 1 frame
            rulerRoot.labelSpacing = timeline.scaleFactor > rulerRoot.labelSize * 1.3 ? timeline.scaleFactor : Math.floor(rulerRoot.labelSize/timeline.scaleFactor) * timeline.scaleFactor
        } else {
            rulerRoot.tickSpacing = Math.floor(3 * rulerRoot.fontUnit / timeline.scaleFactor) * timeline.scaleFactor
            rulerRoot.labelSpacing = (Math.floor(rulerRoot.labelSize/rulerRoot.tickSpacing) + 1) * rulerRoot.tickSpacing
        }
        rulerRoot.labelMod = Math.max((1, Math.ceil((rulerRoot.labelSize + rulerRoot.fontUnit) / rulerRoot.tickSpacing)))
        //console.log('LABELMOD: ', Math.ceil((rulerRoot.labelSize + rulerRoot.fontUnit) / rulerRoot.tickSpacing)))
    }

    function adjustFormat() {
        rulerRoot.labelSize = fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
        adjustStepSize()
    }

    color: activePalette.window
    clip: true

    // Timeline preview stuff
    Repeater {
        model: timeline.dirtyChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: modelData * timeline.scaleFactor
            y: 0
            width: 25 * timeline.scaleFactor
            height: parent.height / 4
            color: 'darkred'
        }
    }

    Repeater {
        model: timeline.renderedChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: modelData * timeline.scaleFactor
            y: 0
            width: 25 * timeline.scaleFactor
            height: parent.height / 4
            color: 'darkgreen'
        }
    }
    Rectangle {
        id: working
        x: rulerRoot.workingPreview * timeline.scaleFactor
        y: 0
        width: 25 * timeline.scaleFactor
        height: parent.height / 4
        color: 'orange'
        visible: rulerRoot.workingPreview > -1
    }

    // Ruler marks
    Repeater {
        id: tickRepeater
        model: scrollView.width / rulerRoot.tickSpacing + 2
        property int offset: Math.floor(scrollView.flickableItem.contentX /rulerRoot.tickSpacing)
        Item {
            property int realPos: (tickRepeater.offset + index) * rulerRoot.tickSpacing / timeline.scaleFactor
            x: realPos * timeline.scaleFactor
            height: parent.height
            property bool showText: (tickRepeater.offset + index)%rulerRoot.labelMod == 0
            Rectangle {
                anchors.bottom: parent.bottom
                height: parent.showText ? 8 : 4
                width: 1
                color: activePalette.windowText
                opacity: 0.5
            }
            Label {
                visible: parent.showText
                anchors.top: parent.top
                anchors.topMargin: 2
                text: timeline.timecode(parent.realPos)
                font.pointSize: rulerRoot.fontUnit
                color: activePalette.windowText
            }
        }
    }

    // monitor zone
    Rectangle {
        id: zone
        visible: timeline.zoneOut > timeline.zoneIn
        color: Qt.rgba(activePalette.highlight.r,activePalette.highlight.g,activePalette.highlight.b,0.5)
        x: timeline.zoneIn * timeline.scaleFactor
        width: (timeline.zoneOut - timeline.zoneIn) * timeline.scaleFactor
        anchors.bottom: parent.bottom
        height: parent.height / 3
        opacity: useTimelineRuler ? 1 : 0.5
        Rectangle {
            id: centerDrag
            visible: !durationRect.visible
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
                        var offset = Math.round(zone.x/ timeline.scaleFactor) - timeline.zoneIn
                        if (offset != 0) {
                            var newPos = Math.max(0, controller.suggestSnapPoint(timeline.zoneIn + offset,root.snapping))
                            timeline.zoneOut += newPos - timeline.zoneIn
                            timeline.zoneIn = newPos
                        }
                    }
                }
            }
        }
        // Zone frame indicator
        Rectangle {
            visible: useTimelineRuler && (trimInMouseArea.drag.active || moveMouseArea2.drag.active || (useTimelineRuler && showZoneLabels && labelSize * 2 < zone.width))
            width: inLabel.contentWidth
            height: inLabel.contentHeight
            anchors.bottom: zone.top
            color: activePalette.highlight
            opacity: 0.8
            Label {
                id: inLabel
                anchors.fill: parent
                text: timeline.timecode(timeline.zoneIn)
                font.pointSize: root.baseUnit
                color: activePalette.highlightedText
            }
        }
        Rectangle {
            visible: useTimelineRuler && (trimOutMouseArea.drag.active || (useTimelineRuler && showZoneLabels && labelSize * 2 < zone.width))
            width: outLabel.contentWidth
            height: outLabel.contentHeight
            anchors.bottom: zone.top
            color: activePalette.highlight
            opacity: 0.8
            x: zone.width - outLabel.contentWidth
            Label {
                id: outLabel
                anchors.fill: parent
                text: timeline.timecode(timeline.zoneOut)
                font.pointSize: root.baseUnit
                color: activePalette.highlightedText
            }
        }
        Rectangle {
            id: durationRect
            anchors.bottom: zone.top
            visible: useTimelineRuler && (parent.width > 2 * width && !trimInMouseArea.drag.active && !trimOutMouseArea.drag.active)
            anchors.horizontalCenter: parent.horizontalCenter
            width: durationLabel.contentWidth + 4
            height: durationLabel.contentHeight
            color: activePalette.highlight
            opacity: moveMouseArea2.drag.active ? 0 : 1
            MouseArea {
                id: moveMouseArea2
                anchors.fill: parent
                property double startX
                hoverEnabled: true
                cursorShape: Qt.SizeHorCursor
                drag.target: zone
                drag.axis: Drag.XAxis
                drag.smoothed: false
                onEntered: {
                    showZoneLabels = true
                }
                onPressed: {
                    startX = zone.x
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var offset = Math.round(zone.x/ timeline.scaleFactor) - timeline.zoneIn
                        if (offset != 0) {
                            var newPos = Math.max(0, controller.suggestSnapPoint(timeline.zoneIn + offset,root.snapping))
                            timeline.zoneOut += newPos - timeline.zoneIn
                            timeline.zoneIn = newPos
                        }
                    }
                }
            }

            Label {
                id: durationLabel
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                text: timeline.timecode(timeline.zoneOut - timeline.zoneIn)
                font.pointSize: root.baseUnit
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
                            var newPos = controller.suggestSnapPoint(timeline.zoneIn + Math.round(trimIn.x / timeline.scaleFactor), root.snapping)
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
                            timeline.zoneOut = Math.max(controller.suggestSnapPoint(timeline.zoneIn + Math.round((trimOut.x + trimOut.width) / timeline.scaleFactor), root.snapping), timeline.zoneIn + 1)
                        }
                    }
                    onEntered: parent.opacity = 1
                    onExited: parent.opacity = 0
                }
            }
    }
}

