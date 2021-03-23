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

import QtQuick 2.11
import QtQuick.Controls 2.4

Item {
    id: rulerRoot
    // The standard width for labels. Depends on format used (frame number or full timecode)
    property int labelSize: fontMetrics.boundingRect(timeline.timecode(36000)).width
    // The spacing between labels. Depends on labelSize
    property real labelSpacing: labelSize
    // The space we want between each ticks in the ruler
    property real tickSpacing: timeline.scaleFactor
    property alias rulerZone : zone
    property int workingPreview : timeline.workingPreview
    property int labelMod: 1
    property bool useTimelineRuler : timeline.useRuler
    property int zoneHeight: Math.ceil(root.baseUnit / 2) + 1
    property bool showZoneLabels: false
    property bool resizeActive: false // Used to decide which mouse cursor we should display
    property bool hoverGuide: false
    property int cursorShape: resizeActive ? Qt.SizeHorCursor : hoverGuide ? Qt.PointingHandCursor : Qt.ArrowCursor
    property var effectZones: timeline.masterEffectZones
    property int guideLabelHeight: timeline.showMarkers ? Math.round(fontMetrics.height) : 0
    property int previewHeight: Math.ceil(timecodeContainer.height / 5)
    
    function adjustStepSize() {
        if (timeline.scaleFactor > 19) {
            // Frame size >= 20 pixels
            rulerRoot.tickSpacing = timeline.scaleFactor
            // labelSpacing cannot be smaller than 1 frame
            rulerRoot.labelSpacing = timeline.scaleFactor > rulerRoot.labelSize * 1.3 ? timeline.scaleFactor : Math.floor(rulerRoot.labelSize/timeline.scaleFactor) * timeline.scaleFactor
        } else {
            rulerRoot.tickSpacing = Math.floor(3 * root.baseUnit / timeline.scaleFactor) * timeline.scaleFactor
            rulerRoot.labelSpacing = (Math.floor(rulerRoot.labelSize/rulerRoot.tickSpacing) + 1) * rulerRoot.tickSpacing
        }
        rulerRoot.labelMod = Math.max(1, Math.ceil((rulerRoot.labelSize + root.baseUnit) / rulerRoot.tickSpacing))
        //console.log('LABELMOD: ', Math.ceil((rulerRoot.labelSize + root.fontUnit) / rulerRoot.tickSpacing)))
        tickRepeater.model = Math.ceil(rulercontainer.width / rulerRoot.tickSpacing) + 2
    }

    function adjustFormat() {
        rulerRoot.labelSize = fontMetrics.boundingRect(timeline.timecode(36000)).width
        adjustStepSize()
        repaintRuler()
    }

    function repaintRuler() {
        // Enforce repaint
        tickRepeater.model = 0
        tickRepeater.model = Math.ceil(rulercontainer.width / rulerRoot.tickSpacing) + 2
    }

    // Timeline preview stuff
    Repeater {
        model: timeline.dirtyChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: modelData * timeline.scaleFactor
            anchors.bottom: parent.bottom
            anchors.bottomMargin: zoneHeight
            width: 25 * timeline.scaleFactor
            height: previewHeight
            color: 'darkred'
        }
    }

    Repeater {
        model: timeline.renderedChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: modelData * timeline.scaleFactor
            anchors.bottom: parent.bottom
            anchors.bottomMargin: zoneHeight
            width: 25 * timeline.scaleFactor
            height: previewHeight
            color: 'darkgreen'
        }
    }
    Rectangle {
        id: working
        x: rulerRoot.workingPreview * timeline.scaleFactor
        anchors.bottom: parent.bottom
        anchors.bottomMargin: zoneHeight
        width: 25 * timeline.scaleFactor
        height: previewHeight
        color: 'orange'
        visible: rulerRoot.workingPreview > -1
    }
    
    // Guides
    Repeater {
        model: guidesModel
        delegate:
        Item {
            id: guideRoot
            z: proxy.position == model.frame ? 20 : 10
            Rectangle {
                id: markerBase
                width: 1
                height: rulerRoot.height
                x: model.frame * timeline.scaleFactor
                color: model.color
                property int markerId: model.id
                Rectangle {
                    visible: timeline.showMarkers
                    width: mlabel.contentWidth + 4
                    height: guideLabelHeight
                    anchors {
                        top: parent.top
                        left: parent.left
                    }
                    color: model.color
                    Text {
                        id: mlabel
                        text: model.comment
                        topPadding: -2
                        leftPadding: 2
                        rightPadding: 2
                        font: miniFont
                        color: '#FFF'
                    }
                    MouseArea {
                        z: 10
                        id: guideArea
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: parent.width
                        height: parent.height
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        property int prevFrame
                        property int xOffset: 0
                        drag.axis: Drag.XAxis
                        onPressed: {
                            prevFrame = model.frame
                            xOffset = mouseX
                            anchors.left = undefined
                        }
                        onReleased: {
                            if (prevFrame != model.frame) {
                                var newFrame = model.frame
                                timeline.moveGuideWithoutUndo(markerBase.markerId,  prevFrame)
                                timeline.moveGuide(prevFrame, newFrame)
                            }
                            anchors.left = parent.left
                        }
                        onPositionChanged: {
                            if (pressed) {
                                var newFrame = Math.round(model.frame + (mouseX - xOffset) / timeline.scaleFactor)
                                newFrame = controller.suggestSnapPoint(newFrame, root.snapping)
                                timeline.moveGuideWithoutUndo(markerBase.markerId,  newFrame)
                            }
                        }
                        drag.smoothed: false
                        onDoubleClicked: timeline.editGuide(model.frame)
                        onClicked: {
                            proxy.position = model.frame
                        }
                        onEntered: {
                            rulerRoot.hoverGuide = true
                        }
                        onExited: {
                            rulerRoot.hoverGuide = false
                        }
                    }
                }
            }
        }
    }
    
    // Ruler marks
    Item {
        id: timecodeContainer
        anchors.top: parent.top
        anchors.topMargin: guideLabelHeight
        anchors.bottom: parent.bottom
        anchors.bottomMargin: zoneHeight
        anchors.left: parent.left
        anchors.right: parent.right
    Repeater {
        id: tickRepeater
        model: Math.ceil(rulercontainer.width / rulerRoot.tickSpacing) + 2
        property int offset: Math.floor(scrollView.contentX /rulerRoot.tickSpacing)
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
                opacity: 0.7
                text: timeline.timecode(parent.realPos)
                font: miniFont
                color: activePalette.windowText
            }
        }
    }
    }
    
    RulerZone {
        id: zone
        Binding {
            target: zone
            property: "frameIn"
            value: timeline.zoneIn
        }
        Binding {
            target: zone
            property: "frameOut"
            value: timeline.zoneOut
        }
        color: useTimelineRuler ? Qt.rgba(activePalette.highlight.r,activePalette.highlight.g,activePalette.highlight.b,0.9) :
        Qt.rgba(activePalette.highlight.r,activePalette.highlight.g,activePalette.highlight.b,0.5)
        anchors.bottom: parent.bottom
        height: zoneHeight
        function updateZone(start, end, update)
        {
            timeline.updateZone(start, end, update)
        }
    }

    // Master effect zones
    Repeater {
        model: effectZones
        Rectangle {
            x: effectZones[index].x * timeline.scaleFactor
            height: Math.ceil(zoneHeight/4)
            width: (effectZones[index].y - effectZones[index].x) * timeline.scaleFactor
            color: "blueviolet"
            anchors.bottom: parent.bottom
            anchors.bottomMargin: zoneHeight - height - 1
        }
    }

    // Effect zone
    RulerZone {
        id: effectZone
        Binding {
            target: effectZone
            property: "frameIn"
            value: timeline.effectZone.x
        }
        Binding {
            target: effectZone
            property: "frameOut"
            value: timeline.effectZone.y
        }
        color: "orchid"
        anchors.bottom: parent.bottom
        height: zoneHeight - 1
        function updateZone(start, end, update)
        {
            timeline.updateEffectZone(start, end, update)
        }
    }
}

