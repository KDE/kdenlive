/*
    SPDX-FileCopyrightText: 2013 Meltytech LLC
    SPDX-FileCopyrightText: 2013 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import com.enums 1.0

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
    property int guideLabelHeight: timeline.showMarkers ? fontMetrics.height : 0
    property int previewHeight: Math.ceil(timecodeContainer.height / 5)
    property color dimmedColor: (activePalette.text.r + activePalette.text.g + activePalette.text.b > 1.5) ? Qt.darker(activePalette.text, 1.3) : Qt.lighter(activePalette.text, 1.3)
    property color dimmedColor2: (activePalette.text.r + activePalette.text.g + activePalette.text.b > 1.5) ? Qt.darker(activePalette.text, 2.2) : Qt.lighter(activePalette.text, 2.2)
    
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
        id: guidesRepeater
        model: guidesModel
        property int radiusSize: timeline.guidesLocked ? 0 : guideLabelHeight / 2
        delegate:
        Item {
            id: guideRoot
            property bool activated : proxy.position == model.frame
            z: activated ? 20 : 10
            Rectangle {
                id: markerBase
                width: 1
                height: rulerRoot.height
                x: Math.round(model.frame * timeline.scaleFactor)
                color: guideRoot.activated ? Qt.lighter(model.color, 1.3) : model.color
                property int markerId: model.id
                Rectangle {
                    
                    visible: timeline.showMarkers
                    width: mlabel.contentWidth + 4 - guidesRepeater.radiusSize
                    height: guideLabelHeight
                    color: markerBase.color
                    anchors {
                        top: parent.top
                        left: parent.left
                    }
                    ToolTip.visible: guideArea.containsMouse
                    ToolTip.text: model.comment
                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    Rectangle {
                        visible: !timeline.guidesLocked
                        color: markerBase.color
                        anchors.fill: parent
                        radius: guidesRepeater.radiusSize
                        anchors.rightMargin: -guidesRepeater.radiusSize - 2
                    }
                    Rectangle {
                        // Shadow delimiting marker start
                        width: 1
                        height: guideLabelHeight
                        color: activePalette.dark
                        anchors {
                            right: parent.left
                        }
                    }
                    Rectangle {
                        // Shadow on marker top
                        height: 1
                        width: parent.width + guidesRepeater.radiusSize / 2
                        color: Qt.darker(markerBase.color, 1.8)
                    }
                    Text {
                        id: mlabel
                        text: model.comment
                        topPadding: -1
                        leftPadding: 2
                        rightPadding: 2
                        font: miniFont
                        color: '#000'
                    }
                    MouseArea {
                        z: 10
                        id: guideArea
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: parent.width + guidesRepeater.radiusSize
                        height: parent.height
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        property int prevFrame
                        property int destFrame
                        property int movingMarkerId
                        property int xOffset: 0
                        drag.axis: Drag.XAxis
                        onPressed: {
                            prevFrame = model.frame
                            destFrame = prevFrame
                            xOffset = mouseX
                            anchors.left = undefined
                            movingMarkerId = markerBase.markerId
                        }
                        onReleased: {
                            if (prevFrame != destFrame) {
                                timeline.moveGuideWithoutUndo(movingMarkerId, prevFrame)
                                timeline.moveGuideById(movingMarkerId, destFrame)
                            }
                            movingMarkerId = -1
                            anchors.left = parent.left
                        }
                        onPositionChanged: mouse => {
                            if (pressed) {
                                var newFrame = Math.max(0, Math.round(model.frame + (mouseX - xOffset) / timeline.scaleFactor))
                                newFrame = controller.suggestSnapPoint(newFrame, mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping)
                                if (newFrame != destFrame) {
                                    var frame = timeline.moveGuideWithoutUndo(movingMarkerId, newFrame)
                                    if (frame > -1) {
                                        destFrame = frame
                                    }
                                }
                            }
                        }
                        drag.smoothed: false
                        onDoubleClicked: timeline.editGuide(model.frame)
                        onClicked: mouse => {
                            if (root.activeTool !== ProjectTool.SlipTool) {
                                proxy.position = model.frame
                            }
                            if (mouse.button == Qt.RightButton) {
                                root.showRulerMenu()
                            }
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
            x: Math.round(realPos * timeline.scaleFactor)
            height: parent.height
            property bool showText: (tickRepeater.offset + index)%rulerRoot.labelMod == 0
            Rectangle {
                anchors.bottom: parent.bottom
                height: parent.showText ? root.baseUnit * 0.8 : 4
                width: 1
                color: dimmedColor2
            }
            Label {
                visible: parent.showText
                anchors.top: parent.top
                text: timeline.timecode(parent.realPos)
                font: miniFont
                color: dimmedColor
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
            height: zoneHeight - 1
            width: (effectZones[index].y - effectZones[index].x) * timeline.scaleFactor
            color: "blueviolet"
            anchors.bottom: parent.bottom
            opacity: 0.4
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
        opacity: 0.7
        function updateZone(start, end, update)
        {
            timeline.updateEffectZone(start, end, update)
        }
    }
}

