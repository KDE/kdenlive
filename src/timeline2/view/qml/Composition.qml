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
    signal trimmingIn(var clip, real newDuration)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration)
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

    Rectangle {
        id: displayRect
        anchors.top: compositionRoot.top
        anchors.right: compositionRoot.right
        anchors.left: compositionRoot.left
        anchors.topMargin: displayHeight
        height: parentTrack.height - displayHeight
        color: Qt.darker('mediumpurple')
        border.color: grouped ? root.groupColor : mouseArea.containsMouse ? activePalette.highlight : borderColor
        border.width: isGrabbed ? 8 : 2
        opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.5 : 1.0
        onWidthChanged: {
            console.log('TRIM AREA ENABLED: ',trimOutMouseArea.enabled)
        }
        Item {
            // clipping container
            id: container
            anchors.fill: displayRect
            anchors.margins: displayRect.border.width
            clip: true
            Rectangle {
                // text background
                id: labelRect
                color: compositionRoot.aTrack > -1 ? 'yellow' : 'lightgray'
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
                visible: compositionRoot.showKeyframes && compositionRoot.keyframeModel && compositionRoot.width > 2 * root.baseUnit
                selected: compositionRoot.selected
                inPoint: 0
                outPoint: compositionRoot.clipDuration
                masterObject: compositionRoot
                kfrModel: compositionRoot.hideCompoViews ? 0 : compositionRoot.keyframeModel
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
                }
                PropertyChanges {
                    target: displayRect
                    height: parentTrack.height - displayHeight + Math.min(Logic.getTrackHeightByPos(Logic.getTrackIndexFromId(parentTrack.trackInternalId) + 1) / 3, root.baseUnit)
                    color: 'mediumpurple'
                    border.color: root.selectionColor
                }
            }
        ]
        transitions: Transition {
            NumberAnimation {
                properties: "height";
                easing.type: Easing.InOutQuad;
                duration: 150;
            }
        }
        MouseArea {
            id: mouseArea
            anchors.fill: parent
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
                    if (timeline.selection.indexOf(compositionRoot.clipId) === -1) {
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
                var s = i18n("%1, Position: %2, Duration: %3".arg(label.text).arg(timeline.simplifiedTC(compositionRoot.modelStart)).arg(timeline.simplifiedTC(compositionRoot.clipDuration)))
                timeline.showToolTip(s)
            }
            onExited: {
                endDrag()
                if (!trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse) {
                    timeline.showToolTip()
                }
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

            MouseArea {
                id: trimInMouseArea
                x: enabled ? -displayRect.border.width : 0
                height: parent.height
                width: root.baseUnit / 2
                visible: root.activeTool === 0
                enabled: !compositionRoot.grouped && (pressed || displayRect.width > 3 * width)
                hoverEnabled: true
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                onPressed: {
                    root.autoScrolling = false
                    compositionRoot.originalX = compositionRoot.x
                    compositionRoot.originalDuration = clipDuration
                    anchors.left = undefined
                    trimIn.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    anchors.left = parent.left
                    compositionRoot.trimmedIn(compositionRoot)
                    trimIn.opacity = 0
                    updateDrag()
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var delta = Math.round(x / timeScale)
                        if (delta < -modelStart) {
                            delta = -modelStart
                        }
                        if (delta !== 0) {
                            var newDuration = compositionRoot.clipDuration - delta
                            compositionRoot.trimmingIn(compositionRoot, newDuration)
                        }
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimIn.opacity = 1
                        timeline.showKeyBinding(i18n("<b>Drag</b> to resize"))
                    }
                }
                onExited: {
                    trimIn.opacity = 0
                    if (!mouseArea.containsMouse) {
                        timeline.showKeyBinding()
                        timeline.showToolTip()
                    }
                }
                Rectangle {
                    id: trimIn
                    anchors.left: parent.left
                    width: displayRect.border.width
                    height: parent.height
                    color: 'lawngreen'
                    opacity: 0
                    Drag.active: trimInMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimInMouseArea.pressed || (root.activeTool === 0 && !mouseArea.drag.active && parent.enabled)
                }
            }

            MouseArea {
                id: trimOutMouseArea
                anchors.right: parent.right
                anchors.rightMargin: enabled ? -displayRect.border.width : 0
                height: displayRect.height
                width: root.baseUnit / 2
                hoverEnabled: true
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimOutMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                visible: root.activeTool === 0
                enabled: !compositionRoot.grouped && (pressed || displayRect.width > 3 * width)

                onPressed: {
                    root.autoScrolling = false
                    compositionRoot.originalDuration = clipDuration
                    anchors.right = undefined
                    trimOut.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    anchors.right = parent.right
                    compositionRoot.trimmedOut(compositionRoot)
                    updateDrag()
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newDuration = Math.round((x + width) / timeScale)
                        compositionRoot.trimmingOut(compositionRoot, newDuration)
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimOut.opacity = 1
                        timeline.showKeyBinding(i18n("<b>Drag</b> to resize"))
                    }
                }
                onExited: {
                    trimOut.opacity = 0
                    if (!mouseArea.containsMouse) {
                        timeline.showKeyBinding()
                        timeline.showToolTip()
                    }
                }
                Rectangle {
                    id: trimOut
                    anchors.right: parent.right
                    width: displayRect.border.width
                    height: parent.height
                    color: 'red'
                    opacity: 0
                    Drag.active: trimOutMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimOutMouseArea.pressed || (root.activeTool === 0 && !mouseArea.drag.active && parent.enabled)
                }
            }
        }
    }
}
