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
import QtGraphicalEffects 1.0
import QtQml.Models 2.2
import QtQuick.Window 2.2


Item {
    id: compositionRoot
    property real timeScale: 1.0
    property string clipName: ''
    property string clipResource: ''
    property string mltService: ''
    property int modelStart: x
    property int displayHeight: 0
    property var parentTrack: trackRoot
    property int inPoint: 0
    property int outPoint: 0
    property int clipDuration: 0
    property bool isAudio: false
    property bool groupDrag: false
    property bool isComposition: true
    property bool showKeyframes: false
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

    signal clicked(var clip, int shiftClick)
    signal moved(var clip)
    signal dragged(var clip, var mouse)
    signal dropped(var clip)
    signal draggedToTrack(var clip, int pos, int xpos)
    signal trimmingIn(var clip, real newDuration, var mouse)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration, var mouse)
    signal trimmedOut(var clip)

    onGroupDragChanged: {
        // Clip belonging to current timeline selection changed, update list
        if (compositionRoot.groupDrag) {
            if (root.dragList.indexOf(compositionRoot) == -1) {
                root.dragList.push(compositionRoot)
            }
        } else {
            var index = root.dragList.indexOf(compositionRoot);
            if (index > -1) {
                root.dragList.splice(index, 1)
            }
            root.dragList.pop(compositionRoot)
        }
    }

    onKeyframeModelChanged: {
        if (effectRow.keyframecanvas) {
            effectRow.keyframecanvas.requestPaint()
        }
    }

    onModelStartChanged: {
        x = modelStart * timeScale;
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
    function reparent(track) {
        parent = track
        isAudio = track.isAudio
        parentTrack = track
        displayHeight = track.height / 2
        compositionRoot.trackId = parentTrack.trackId
    }

    SystemPalette { id: activePalette }
    Rectangle {
        id: displayRect
        anchors.top: compositionRoot.top
        anchors.right: compositionRoot.right
        anchors.left: compositionRoot.left
        anchors.topMargin: displayHeight
        height: displayHeight
        color: Qt.darker('mediumpurple')
        border.color: selected? 'red' : borderColor
        border.width: 1.5
        opacity: Drag.active? 0.5 : 1.0
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
                visible: compositionRoot.showKeyframes && keyframeModel
                selected: compositionRoot.selected
                inPoint: 0
                outPoint: compositionRoot.clipDuration
            }
        }
        Drag.active: mouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction

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

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        drag.target: compositionRoot
        drag.axis: Drag.XAxis
        drag.smoothed: false
        property int startX

        onPressed: {
            root.stopScrolling = true
            originalX = compositionRoot.x
            originalTrackId = compositionRoot.trackId
            startX = compositionRoot.x
            compositionRoot.forceActiveFocus();
            if (!compositionRoot.selected) {
                compositionRoot.clicked(compositionRoot, mouse.modifiers === Qt.ShiftModifier)
            }
            if (root.dragList.length > 1) {
                // Dragging multiple items, reparent all to this dragged clip
                // Cleanup list
                var tmp = []
                for (var i = 0; i < root.dragList.length; i++) {
                    if (root.dragList[i] && root.dragList[i].clipId != undefined)
                        tmp.push(root.dragList[i])
                }
                root.dragList = tmp
                for (var i = 0; i < root.dragList.length; i++) {
                    console.log('CHILD: ', root.dragList[i].clipId, ' > ', root.dragList[i].trackId)
                    if (root.dragList[i] != compositionRoot) {
                        var clipX = root.dragList[i].x - compositionRoot.x
                        var clipY = root.dragList[i].parentTrack.y - compositionRoot.parentTrack.y
                        root.dragList[i].parent = compositionRoot
                        root.dragList[i].x = clipX
                        root.dragList[i].y = clipY
                    }
                }
            }
        }
        onPositionChanged: {
            if (mouse.y < -height || (mouse.y > height && parentTrack.rootIndex.row < tracksRepeater.count - 1)) {
                var mapped = parentTrack.mapFromItem(compositionRoot, mouse.x, mouse.y).x
                compositionRoot.draggedToTrack(compositionRoot, mapToItem(null, 0, mouse.y).y, mapped)
            } else {
                compositionRoot.dragged(compositionRoot, mouse)
            }
        }
        onReleased: {
            root.stopScrolling = false
            var delta = compositionRoot.x - startX
            if (Math.abs(delta) >= 1.0 || trackId !== originalTrackId) {
                compositionRoot.moved(compositionRoot)
                originalX = compositionRoot.x
                originalTrackId = trackId
            } else if (Math.abs(delta) >= 1.0) {
                compositionRoot.dropped(compositionRoot)
            }
            var tmp = []
            for (var i = 0; i < root.dragList.length; i++) {
                if (root.dragList[i] && root.dragList[i].clipId != undefined)
                    tmp.push(root.dragList[i])
            }
            root.dragList = tmp
            for (var i = 0; i < root.dragList.length; i++) {
                //console.log('CHILD: ', root.dragList[i].clipId, ' > ', root.dragList[i].trackId, ' > ', root.dragList[i].parent.clipId)
                if (root.dragList[i].parent == compositionRoot) {
                    root.dragList[i].parent = tracksContainerArea
                    root.dragList[i].x += compositionRoot.x
                    root.dragList[i].y = root.dragList[i].parentTrack.y
                }
            }
        }
        onDoubleClicked: timeline.position = compositionRoot.x / timeline.scaleFactor
        onWheel: zoomByWheel(wheel)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            propagateComposedEvents: true
            cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor :
                drag.active? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: {
                root.stopScrolling = true
                compositionRoot.forceActiveFocus();
                if (!compositionRoot.selected) {
                    compositionRoot.clicked(compositionRoot, false)
                }
            }
            onClicked: {
                compositionMenu.clipId = compositionRoot.clipId
                compositionMenu.grouped = compositionRoot.grouped
                compositionMenu.trackId = compositionRoot.trackId
                compositionMenu.popup()
            }
        }
    }

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
        visible: root.activeTool === 0 && !mouseArea.drag.active

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
        visible: root.activeTool === 0 && !mouseArea.drag.active

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
