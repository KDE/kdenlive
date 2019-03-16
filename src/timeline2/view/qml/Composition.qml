/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *   Based on work by Dan Dennedy <dan@dennedy.org> (Shotcut)              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

import QtQuick 2.6
import QtQuick.Controls 2.2
import QtQml.Models 2.2
import QtQuick.Window 2.2
import 'Timeline.js' as Logic

Item {
    id: compositionRoot
    property real timeScale: 1.0
    property string clipName: ''
    property string clipResource: ''
    property string mltService: ''
    property int modelStart
    property int displayHeight: 0
    property var parentTrack: trackRoot
    property int inPoint: 0
    property int outPoint: 0
    property int clipDuration: 0
    property bool isAudio: false
    property bool showKeyframes: false
    property var itemType: 0
    property bool isGrabbed: false
    property var keyframeModel
    property bool grouped: false
    property int binId: 0
    property int scrollX: 0
    property int trackHeight
    property int trackIndex //Index in track repeater
    property int trackId: -42    //Id in the model
    property int aTrack: -1
    property int clipId     //Id of the clip in the model
    property int originalTrackId: trackId
    property int originalX: x
    property int originalDuration: clipDuration
    property int lastValidDuration: clipDuration
    property int draggedX: x
    property bool selected: false
    property double speed: 1.0
    property color color: displayRect.color
    property color borderColor: 'black'

    signal moved(var clip)
    signal dragged(var clip, var mouse)
    signal dropped(var clip)
    signal draggedToTrack(var clip, int pos, int xpos)
    signal trimmingIn(var clip, real newDuration, var mouse)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration, var mouse)
    signal trimmedOut(var clip)

    onKeyframeModelChanged: {
        if (effectRow.keyframecanvas) {
            effectRow.keyframecanvas.requestPaint()
        }
    }

    onModelStartChanged: {
        x = modelStart * timeScale;
    }

    onIsGrabbedChanged: {
        if (compositionRoot.isGrabbed) {
            compositionRoot.forceActiveFocus();
            mouseArea.focus = true
        }
    }
    onTrackIdChanged: {
        compositionRoot.parentTrack = Logic.getTrackById(trackId)
        compositionRoot.y = compositionRoot.originalTrackId == -1 || trackId == originalTrackId ? 0 : parentTrack.y - Logic.getTrackById(compositionRoot.originalTrackId).y;
    }
    onClipDurationChanged: {
        width = clipDuration * timeScale;
    }
    onTimeScaleChanged: {
        x = modelStart * timeScale;
        width = clipDuration * timeScale;
        labelRect.x = scrollX > modelStart * timeScale ? scrollX - modelStart * timeScale : 0
    }
    onScrollXChanged: {
        labelRect.x = scrollX > modelStart * timeScale ? scrollX - modelStart * timeScale : 0
    }
/*    function reparent(track) {
        parent = track
        isAudio = track.isAudio
        parentTrack = track
        displayHeight = track.height / 2
        compositionRoot.trackId = parentTrack.trackId
    }
*/
    function updateDrag() {
        var itemPos = mapToItem(tracksContainerArea, 0, displayRect.y, displayRect.width, displayRect.height)
        initDrag(compositionRoot, itemPos, compositionRoot.clipId, compositionRoot.modelStart, compositionRoot.trackId, true)
    }
    MouseArea {
        id: mouseArea
        anchors.fill: displayRect
        acceptedButtons: Qt.RightButton
        hoverEnabled: true
        /*onPressed: {
            root.stopScrolling = true
            originalX = compositionRoot.x
            originalTrackId = compositionRoot.trackId
            startX = compositionRoot.x
            compositionRoot.forceActiveFocus();
            focus = true
            if (!compositionRoot.selected) {
                compositionRoot.clicked(compositionRoot, mouse.modifiers === Qt.ShiftModifier)
            }
        }*/
        Keys.onShortcutOverride: event.accepted = compositionRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down)
        Keys.onLeftPressed: {
            controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart - 1, true, true)
        }
        Keys.onRightPressed: {
            controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart + 1, true, true)
        }
        Keys.onUpPressed: {
            controller.requestCompositionMove(compositionRoot.clipId, controller.getNextTrackId(compositionRoot.originalTrackId), compositionRoot.modelStart, true, true)
        }
        Keys.onDownPressed: {
            controller.requestCompositionMove(compositionRoot.clipId, controller.getPreviousTrackId(compositionRoot.originalTrackId), compositionRoot.modelStart, true, true)
        }
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        onPressed: {
                root.stopScrolling = true
                compositionRoot.forceActiveFocus();
                if (mouse.button == Qt.RightButton) {
                    if (timeline.selection.indexOf(compositionRoot.clipId) == -1) {
                        timeline.addSelection(compositionRoot.clipId, true)
                    }
                    compositionMenu.clipId = compositionRoot.clipId
                    compositionMenu.grouped = compositionRoot.grouped
                    compositionMenu.trackId = compositionRoot.trackId
                    compositionMenu.popup()
                }
            }
        onEntered: {
            var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
            initDrag(compositionRoot, itemPos, compositionRoot.clipId, compositionRoot.modelStart, compositionRoot.trackId, true)
        }
        onExited: {
            endDrag()
        }
        onDoubleClicked: {
            if (mouse.modifiers & Qt.ShiftModifier) {
                if (keyframeModel && showKeyframes) {
                    // Add new keyframe
                    var xPos = Math.round(mouse.x  / timeline.scaleFactor)
                    var yPos = (compositionRoot.height - mouse.y) / compositionRoot.height
                    keyframeModel.addKeyframe(xPos + compositionRoot.inPoint, yPos)
                } else {
                    timeline.position = compositionRoot.x / timeline.scaleFactor
                }
            } else {
                timeline.editItemDuration(clipId)
            }
        }
        onPositionChanged: {
            if (parentTrack) {
                var mapped = parentTrack.mapFromItem(compositionRoot, mouse.x, mouse.y).x
                root.mousePosChanged(Math.round(mapped / timeline.scaleFactor))
                if (mouse.modifiers & Qt.ShiftModifier) {
                    timeline.position = Math.round(mapped / timeline.scaleFactor)
                }
            }
        }
        onWheel: zoomByWheel(wheel)
    }

    Rectangle {
        id: displayRect
        anchors.top: compositionRoot.top
        anchors.right: compositionRoot.right
        anchors.left: compositionRoot.left
        anchors.topMargin: displayHeight * 1.2
        height: displayHeight * 1.3
        color: Qt.darker('mediumpurple')
        border.color: selected ? activePalette.highlight : grouped ? root.groupColor : borderColor
        border.width: isGrabbed ? 8 : 1.5
        opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.5 : 1.0
        Item {
            // clipping container
            id: container
            anchors.fill: displayRect
            anchors.margins:1.5
            clip: true

            Rectangle {
                // text background
                id: labelRect
                color: compositionRoot.aTrack >= 0 ? 'yellow' : 'lightgray'
                opacity: 0.7
                anchors.top: container.top
                width: label.width + 2
                height: label.height
                Text {
                    id: label
                    text: clipName + (compositionRoot.aTrack >= 0 ? ' > ' + timeline.getTrackNameFromMltIndex(compositionRoot.aTrack) : '')
                    font.pixelSize: root.baseUnit
                    anchors {
                        top: labelRect.top
                        left: labelRect.left
                        topMargin: 1
                        leftMargin: 1
                    }
                    color: 'black'
                }
            }
            KeyframeView {
                id: effectRow
                visible: compositionRoot.showKeyframes && compositionRoot.keyframeModel
                selected: compositionRoot.selected
                inPoint: 0
                outPoint: compositionRoot.clipDuration
                masterObject: compositionRoot
                kfrModel: compositionRoot.keyframeModel
            }
        }
        /*Drag.active: mouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction*/

    states: [
        State {
            name: 'normal'
            when: !compositionRoot.selected
            PropertyChanges {
                target: compositionRoot
                z: 0
            }
        },
        State {
            name: 'selected'
            when: compositionRoot.selected
            PropertyChanges {
                target: compositionRoot
                z: 1
                color: 'mediumpurple'
            }
        }
    ]

    Rectangle {
        id: trimIn
        anchors.left: displayRect.left
        anchors.leftMargin: 0
        height: displayRect.height
        width: 5
        color: isAudio? 'green' : 'lawngreen'
        opacity: 0
        Drag.active: trimInMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        enabled: !compositionRoot.grouped
        visible: root.activeTool === 0 && !dragProxyArea.drag.active

        MouseArea {
            id: trimInMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false

            onPressed: {
                root.stopScrolling = true
                compositionRoot.originalX = compositionRoot.x
                compositionRoot.originalDuration = clipDuration
                parent.anchors.left = undefined
            }
            onReleased: {
                root.stopScrolling = false
                parent.anchors.left = displayRect.left
                compositionRoot.trimmedIn(compositionRoot)
                parent.opacity = 0
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = Math.round((trimIn.x) / timeScale)
                    if (delta < -modelStart) {
                        delta = -modelStart
                    }
                    if (delta !== 0) {
                        var newDuration = compositionRoot.clipDuration - delta
                        compositionRoot.trimmingIn(compositionRoot, newDuration, mouse)
                    }
                }
            }
            onEntered: parent.opacity = 0.5
            onExited: parent.opacity = 0
        }
    }
    Rectangle {
        id: trimOut
        anchors.right: displayRect.right
        anchors.rightMargin: 0
        height: displayRect.height
        width: 5
        color: 'red'
        opacity: 0
        Drag.active: trimOutMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        enabled: !compositionRoot.grouped
        visible: root.activeTool === 0 && !dragProxyArea.drag.active

        MouseArea {
            id: trimOutMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false

            onPressed: {
                root.stopScrolling = true
                compositionRoot.originalDuration = clipDuration
                parent.anchors.right = undefined
            }
            onReleased: {
                root.stopScrolling = false
                parent.anchors.right = displayRect.right
                compositionRoot.trimmedOut(compositionRoot)
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var newDuration = Math.round((parent.x + parent.width) / timeScale)
                    compositionRoot.trimmingOut(compositionRoot, newDuration, mouse)
                }
            }
            onEntered: parent.opacity = 0.5
            onExited: parent.opacity = 0
        }
    }
}
}
