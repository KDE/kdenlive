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

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.11
import QtQuick.Window 2.2
import 'Timeline.js' as Logic

Item {
    id: compositionRoot
    property real timeScale: 1
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
    property bool isComposition: true
    property int originalX: x
    property int originalDuration: clipDuration
    property int lastValidDuration: clipDuration
    property int draggedX: x
    property bool selected: false
    property double speed: 1.0
    property color color: displayRect.color
    property color borderColor: 'black'
    property bool hideCompoViews
    property int scrollStart: scrollView.contentX - modelStart * timeline.scaleFactor
    property int mouseXPos: mouseArea.mouseX

    signal moved(var clip)
    signal dragged(var clip, var mouse)
    signal dropped(var clip)
    signal draggedToTrack(var clip, int pos, int xpos)
    signal trimmingIn(var clip, real newDuration, var mouse)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration, var mouse)
    signal trimmedOut(var clip)

    onScrollStartChanged: {
        compositionRoot.hideCompoViews = compositionRoot.scrollStart > width || compositionRoot.scrollStart + scrollView.contentItem.width < 0
    }

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
            grabItem()
        }
    }

    function grabItem() {
        compositionRoot.forceActiveFocus()
        mouseArea.focus = true
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
        console.log('XXXXXXXXXXXXXXX\n\nXXXXXXXXXXXXX \nUPDATING COMPO DRAG')
        var itemPos = mapToItem(tracksContainerArea, 0, displayRect.y, displayRect.width, displayRect.height)
        initDrag(compositionRoot, itemPos, compositionRoot.clipId, compositionRoot.modelStart, compositionRoot.trackId, true)
    }
    MouseArea {
        id: mouseArea
        anchors.fill: displayRect
        acceptedButtons: Qt.RightButton
        enabled: root.activeTool === 0
        hoverEnabled: root.activeTool === 0
        Keys.onShortcutOverride: event.accepted = compositionRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)
        Keys.onLeftPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart - offset, true, true)
        }
        Keys.onRightPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart + offset, true, true)
        }
        Keys.onUpPressed: {
            controller.requestCompositionMove(compositionRoot.clipId, controller.getNextTrackId(compositionRoot.originalTrackId), compositionRoot.modelStart, true, true)
        }
        Keys.onDownPressed: {
            controller.requestCompositionMove(compositionRoot.clipId, controller.getPreviousTrackId(compositionRoot.originalTrackId), compositionRoot.modelStart, true, true)
        }
        Keys.onEscapePressed: {
            timeline.grabCurrent()
        }
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape

        onPressed: {
                root.autoScrolling = false
                compositionRoot.forceActiveFocus();
                root.mainItemId = compositionRoot.clipId
                if (mouse.button == Qt.RightButton) {
                    if (timeline.selection.indexOf(compositionRoot.clipId) == -1) {
                        controller.requestAddToSelection(compositionRoot.clipId, true)
                    }
                    root.showCompositionMenu()
                }
            }
        onReleased: {
            root.autoScrolling = timeline.autoScroll
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
                    proxy.position = compositionRoot.x / timeline.scaleFactor
                }
            } else {
                timeline.editItemDuration(clipId)
            }
        }
        onPositionChanged: {
            if (parentTrack) {
                var mapped = parentTrack.mapFromItem(compositionRoot, mouse.x, mouse.y).x
                root.mousePosChanged(Math.round(mapped / timeline.scaleFactor))
            }
        }
        onWheel: zoomByWheel(wheel)
    }

    Rectangle {
        id: displayRect
        anchors.top: compositionRoot.top
        anchors.right: compositionRoot.right
        anchors.left: compositionRoot.left
        anchors.topMargin: displayHeight
        height: parentTrack.height - displayHeight + (compositionRoot.selected ? Math.min(Logic.getTrackHeightByPos(Logic.getTrackIndexFromId(parentTrack.trackInternalId) + 1) / 3, root.baseUnit) : 0)
        color: selected ? 'mediumpurple' : Qt.darker('mediumpurple')
        border.color: selected ? root.selectionColor : grouped ? root.groupColor : borderColor
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
                color: compositionRoot.aTrack > -1 ? 'yellow' : 'lightgray'
                anchors.top: container.top
                width: label.width + 2
                height: label.height
                Text {
                    id: label
                    text: clipName + (compositionRoot.aTrack > -1 ? ' > ' + timeline.getTrackNameFromMltIndex(compositionRoot.aTrack) : '')
                    font: miniFont
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
                kfrModel: compositionRoot.hideCompoViews ? 0 : compositionRoot.keyframeModel
            }
            ToolTip {
                visible: mouseArea.containsMouse && !trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse && !trimInMouseArea.drag.active && !trimOutMouseArea.drag.active
                delay: 1000
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    font: miniFont
                    text: '%1\n%4: %5'.arg(label.text)
                        .arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(compositionRoot.clipDuration))
                }
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
        width: root.baseUnit / 3
        color: isAudio? 'green' : 'lawngreen'
        opacity: 0
        Drag.active: trimInMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        enabled: !compositionRoot.grouped
        visible: trimInMouseArea.pressed || (root.activeTool === 0 && !dragProxyArea.drag.active && compositionRoot.width > 4 * width)

        MouseArea {
            id: trimInMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false
            visible: root.activeTool === 0

            onPressed: {
                root.autoScrolling = false
                compositionRoot.originalX = compositionRoot.x
                compositionRoot.originalDuration = clipDuration
                parent.anchors.left = undefined
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
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
        width: root.baseUnit / 3
        color: 'red'
        opacity: 0
        Drag.active: trimOutMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        enabled: !compositionRoot.grouped
        visible: trimOutMouseArea.pressed || (root.activeTool === 0 && !dragProxyArea.drag.active && compositionRoot.width > 4 * width)

        MouseArea {
            id: trimOutMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false
            visible: root.activeTool === 0

            onPressed: {
                root.autoScrolling = false
                compositionRoot.originalDuration = clipDuration
                parent.anchors.right = undefined
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
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
