/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
    This file is part of Kdenlive. See www.kdenlive.org.
    Based on work by Dan Dennedy <dan@dennedy.org> (Shotcut)
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15
import QtQuick.Window 2.15
import 'Timeline.js' as Logic
import com.enums 1.0

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
    property bool hideCompoViews: !visible || width < root.minClipWidthForViews
    property int scrollStart: scrollView.contentX - (compositionRoot.modelStart * root.timeScale)
    visible: scrollView.width + compositionRoot.scrollStart >= 0 && compositionRoot.scrollStart < compositionRoot.width

    property int mouseXPos: mouseArea.mouseX
    // We set coordinates to ensure the item can be found using childAt in timeline.qml getItemAtPosq
    property int trackOffset: 5
    y: trackOffset
    height: 5
    enabled: !compoArea.containsDrag


    signal trimmingIn(var clip, real newDuration)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration)
    signal trimmedOut(var clip)

    onScrollStartChanged: {
        if (!compositionRoot.visible) {
            return
        }
        updateLabelOffset()
        if (!compositionRoot.hideClipViews && compositionRoot.width > scrollView.width) {
            if (effectRow.item && effectRow.item.kfrCanvas) {
                effectRow.item.kfrCanvas.requestPaint()
            }
        }
    }

    /*onKeyframeModelChanged: {
        if (effectRow.item && effectRow.item.keyframecanvas) {
            effectRow.item.keyframecanvas.requestPaint()
        }
    }*/

    onModelStartChanged: {
        x = modelStart * timeScale;
    }

    onIsGrabbedChanged: {
        if (compositionRoot.isGrabbed) {
            grabItem()
        }
    }

    function itemHeight() {
        return displayRect.height
    }

    function grabItem() {
        compositionRoot.forceActiveFocus()
        mouseArea.focus = true
    }

    function resetSelection() {
        if (effectRow.visible) {
            effectRow.item.resetSelection()
        }
    }

    function doesContainMouse(pnt) {
        return pnt.x >= 0 && pnt.x < displayRect.width && (pnt.y > displayRect.y - trackOffset)
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
        if (compositionRoot.visible) {
            updateLabelOffset()
            if (!compositionRoot.hideClipViews) {
                if (effectRow.item && effectRow.item.kfrCanvas) {
                    effectRow.item.kfrCanvas.requestPaint()
                }
            }
        }
    }

    function updateLabelOffset()
    {
        labelRect.anchors.leftMargin = compositionRoot.scrollStart > 0 ? (labelRect.width > compositionRoot.width ? 0 : compositionRoot.scrollStart) : 0
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
        anchors.topMargin: displayHeight - compositionRoot.trackOffset
        height: parentTrack.height - displayHeight
        color: Qt.darker('mediumpurple')
        border.color: grouped ? root.groupColor : mouseArea.containsMouse ? activePalette.highlight : borderColor
        border.width: isGrabbed ? 8 : 2
        opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.5 : 1.0
        onWidthChanged: {
            console.log('TRIM AREA ENABLED: ',trimOutMouseArea.enabled)
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
            enabled: root.activeTool === ProjectTool.SelectTool && !dragProxyArea.pressed
            hoverEnabled: root.activeTool === ProjectTool.SelectTool
            Keys.onShortcutOverride: event => {event.accepted = compositionRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)}
            Keys.onLeftPressed: event => {
                var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
                controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart - offset, true, true)
            }
            Keys.onRightPressed: event => {
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

            onPressed: mouse => {
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
            onDoubleClicked: mouse => {
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
            onWheel: wheel => zoomByWheel(wheel)

            MouseArea {
                id: trimInMouseArea
                x: enabled ? -displayRect.border.width : 0
                height: parent.height
                width: root.baseUnit / 2
                visible: enabled && root.activeTool === ProjectTool.SelectTool
                enabled: !compositionRoot.grouped && (pressed || displayRect.width > 3 * width)
                hoverEnabled: true
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                onPressed: {
                    root.autoScrolling = false
                    root.trimInProgress = true;
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
                    root.trimInProgress = false;
                }
                onPositionChanged: mouse => {
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
                        timeline.showToolTip()
                    }
                    if (!trimInMouseArea.containsMouse) {
                        timeline.showKeyBinding()
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
                    visible: trimInMouseArea.pressed || (root.activeTool === ProjectTool.SelectTool && !mouseArea.drag.active && parent.enabled)
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
                visible: enabled && root.activeTool === ProjectTool.SelectTool
                enabled: !compositionRoot.grouped && (pressed || displayRect.width > 3 * width)

                onPressed: {
                    root.autoScrolling = false
                    root.trimInProgress = true;
                    compositionRoot.originalDuration = clipDuration
                    anchors.right = undefined
                    trimOut.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    anchors.right = parent.right
                    compositionRoot.trimmedOut(compositionRoot)
                    updateDrag()
                    root.trimInProgress = false;
                }
                onPositionChanged: mouse => {
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
                        timeline.showToolTip()
                    }
                    if (!trimOutMouseArea.containsMouse) {
                        timeline.showKeyBinding()
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
                    visible: trimOutMouseArea.pressed || (root.activeTool === ProjectTool.SelectTool && !mouseArea.drag.active && parent.enabled)
                }
            }
            Item {
            // clipping container
            id: container
            anchors.fill: parent
            anchors.margins: displayRect.border.width
            clip: true
            Rectangle {
                // Debug: Clip Id background
                id: debugCidRect
                color: 'magenta'
                width: debugCid.width
                height: debugCid.height
                visible: root.debugmode
                anchors.left: labelRect.right
                Text {
                    // Composition ID text
                    id: debugCid
                    text: compositionRoot.clipId
                    font: miniFont
                    anchors {
                        top: debugCidRect.top
                        left: debugCidRect.left
                        topMargin: 1
                        leftMargin: 1
                    }
                    color: 'white'
                }
            }
            Rectangle {
                // text background
                id: labelRect
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.leftMargin: compositionRoot.scrollStart > 0 ? (labelRect.width > compositionRoot.width ? 0 : compositionRoot.scrollStart) : 0
                color: compositionRoot.aTrack > -1 ? 'yellow' : 'lightgray'
                visible: compositionRoot.width > root.baseUnit
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
        }
        Loader {
            // keyframes container
            id: effectRow
            clip: true
            anchors.fill: parent
            //asynchronous: true
            visible: status == Loader.Ready && compositionRoot.showKeyframes && compositionRoot.keyframeModel && compositionRoot.width > 2 * root.baseUnit
            source: compositionRoot.hideClipViews || compositionRoot.keyframeModel == undefined ? "" : "KeyframeView.qml"
            Binding {
                    target: effectRow.item
                    property: "kfrModel"
                    value: compositionRoot.hideClipViews ? undefined : compositionRoot.keyframeModel
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
                Binding {
                    target: effectRow.item
                    property: "selected"
                    value: compositionRoot.selected
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
                Binding {
                    target: effectRow.item
                    property: "inPoint"
                    value: 0
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
                Binding {
                    target: effectRow.item
                    property: "outPoint"
                    value: compositionRoot.clipDuration
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
                Binding {
                    target: effectRow.item
                    property: "modelStart"
                    value: compositionRoot.modelStart
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
                Binding {
                    target: effectRow.item
                    property: "scrollStart"
                    value: compositionRoot.scrollStart
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
                Binding {
                    target: effectRow.item
                    property: "clipId"
                    value: compositionRoot.clipId
                    when: effectRow.status == Loader.Ready && effectRow.item
                }
            }
            Connections {
                target: effectRow.item
                function onSeek(position) { proxy.position = position }
            }
        }
    }
}
