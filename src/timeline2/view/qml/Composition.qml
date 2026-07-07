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

import org.kde.ki18n

import 'TimelineLogic.js' as Logic
import org.kde.kdenlive as K

Item {
    id: compositionRoot

    required property K.TimelineController timeline
    required property K.TimelineItemModel controller
    required property bool isPanning

    SystemPalette { id: activePalette }

    required property real timeScale
    required property string clipName
    required property string mltService
    required property int modelStart
    required property var parentTrack
    required property int inPoint
    required property int outPoint
    required property int clipDuration
    required property bool showKeyframes
    required property bool isGrabbed
    required property var keyframeModel
    required property bool grouped
    required property int trackId// Id in the model
    required property int fakeTid
    required property int fakePosition
    required property int aTrack
    required property int clipId // Id of the clip in the model
    required property int displayHeight

    property bool isAudio: false
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
    readonly property bool trimInProgress: trimInMouseArea.pressed || trimOutMouseArea.pressed
    readonly property bool isUserInteracting: mouseArea.pressed || trimInProgress
    readonly property bool hideCompoViews: !visible || width < root.minClipWidthForViews
    readonly property bool hideDecorations: !K.KdenliveSettings.showClipOverlays || trimInMouseArea.drag.active || trimOutMouseArea.drag.active
    visible: scrollView.lastVisibleFrame > compositionRoot.modelStart && scrollView.firstVisibleFrame <= (compositionRoot.modelStart + compositionRoot.clipDuration)
    property int scrollStart: visible ? scrollView.contentX - (compositionRoot.modelStart * root.timeScale) : 0

    // We set coordinates to ensure the item can be found using childAt in timeline.qml getItemAtPosq
    property int trackOffset: 0
    y: trackOffset
    height: 5
    enabled: !compoArea.containsDrag && !clipDropArea.containsDrag


    signal trimmingIn(var clip, real newDuration)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration)
    signal trimmedOut(var clip)


    onScrollStartChanged: {
        if (!compositionRoot.visible) {
            return
        }
        updateLabelOffset()
        if (compositionRoot.width > scrollView.width) {
            let kfrView = effectRow.item as KeyframeView
            if (kfrView && kfrView.kfrCanvas) {
                kfrView.kfrCanvas.requestPaint()
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
    onFakePositionChanged: {
        console.log('COMPOSITION POS CHANGED TO: ', fakePosition)
        if (fakePosition > -1) {
            x = fakePosition * timeScale
        }
    }
    onFakeTidChanged: {
        if (compositionRoot.fakeTid > -1 && parentTrack) {
            if (compositionRoot.parent != dragContainer) {
                var pos = compositionRoot.mapToGlobal(compositionRoot.x, compositionRoot.y);
                compositionRoot.parent = dragContainer
                pos = compositionRoot.mapFromGlobal(pos.x, pos.y)
                compositionRoot.x = pos.x
                compositionRoot.y = pos.y
            }
            compositionRoot.y = Logic.getTrackById(compositionRoot.fakeTid).y
            compositionRoot.height = Logic.getTrackById(compositionRoot.fakeTid).height
        } else {
            compositionRoot.height = Qt.binding(function () {
                return parentTrack.height
            })
        }
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
            (effectRow.item as KeyframeView).resetSelection()
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
            let kfrView = effectRow.item as KeyframeView
            if (kfrView && kfrView.kfrCanvas) {
                kfrView.kfrCanvas.requestPaint()
            }
        }
    }

    function updateLabelOffset()
    {
        labelRect.anchors.leftMargin = compositionRoot.scrollStart > 0 ? (labelRect.width > compositionRoot.width ? 0 : compositionRoot.scrollStart) : 0
    }

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
        anchors.topMargin: compositionRoot.displayHeight - compositionRoot.trackOffset
        height: compositionRoot.parentTrack.height - compositionRoot.displayHeight
        property int handleWidth: Math.max(2, Math.ceil(K.UiUtils.baseSizeMedium / 4))
        color: Qt.darker('mediumpurple')
        border.color: compositionRoot.grouped ? compositionRoot.timeline.groupColor : mouseArea.containsMouse ? activePalette.highlight : compositionRoot.borderColor
        border.width: compositionRoot.isGrabbed ? 8 : 2
        opacity: dragProxyArea.drag.active && dragProxy.draggedItem == compositionRoot.clipId ? 0.5 : 1.0

        states: [
            State {
                name: 'normal'
                when: !compositionRoot.selected
                PropertyChanges {
                    compositionRoot.z: 0
                }
            },
            State {
                name: 'selected'
                when: compositionRoot.selected
                PropertyChanges {
                    compositionRoot.z: 1
                }
                PropertyChanges {
                    displayRect.height: parentTrack.height - displayHeight + Math.min(Logic.getTrackHeightByPos(Logic.getTrackIndexFromId(parentTrack.trackInternalId) + 1) / 3, K.UiUtils.baseSizeMedium)
                    displayRect.color: 'mediumpurple'
                    displayRect.border.color: compositionRoot.timeline.selectionColor
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
            enabled: !compositionRoot.isPanning && K.Core.activeTool === K.ToolType.SelectTool && !dragProxyArea.pressed
            hoverEnabled: !compositionRoot.isPanning && K.Core.activeTool === K.ToolType.SelectTool
            Keys.onShortcutOverride: event => {event.accepted = compositionRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)}
            Keys.onLeftPressed: event => {
                var offset = event.modifiers === Qt.ShiftModifier ? K.Core.getCurrentFps() : 1
                compositionRoot.controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart - offset, true, true)
            }
            Keys.onRightPressed: event => {
                var offset = event.modifiers === Qt.ShiftModifier ? K.Core.getCurrentFps() : 1
                compositionRoot.controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.originalTrackId, compositionRoot.modelStart + offset, true, true)
            }
            Keys.onUpPressed: {
                compositionRoot.controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.controller.getNextTrackId(compositionRoot.originalTrackId), compositionRoot.modelStart, true, true)
            }
            Keys.onDownPressed: {
                compositionRoot.controller.requestCompositionMove(compositionRoot.clipId, compositionRoot.controller.getPreviousTrackId(compositionRoot.originalTrackId), compositionRoot.modelStart, true, true)
            }
            Keys.onEscapePressed: {
                compositionRoot.timeline.grabCurrent()
            }
            cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape

            onPressed: mouse => {           
                compositionRoot.forceActiveFocus();
                root.mainItemId = compositionRoot.clipId
                if (mouse.button == Qt.RightButton) {
                    if (compositionRoot.timeline.selection.indexOf(compositionRoot.clipId) === -1) {
                        compositionRoot.controller.requestAddToSelection(compositionRoot.clipId, true)
                    }
                    root.showCompositionMenu()
                }
            }
            onEntered: {
                compositionRoot.updateDrag()
                var s = KI18n.i18n("%1, Position: %2, Duration: %3".arg(label.text).arg(compositionRoot.timeline.simplifiedTC(compositionRoot.modelStart)).arg(compositionRoot.timeline.simplifiedTC(compositionRoot.clipDuration)))
                compositionRoot.timeline.showToolTip(s)
            }
            onExited: {
                root.endDragIfFocused(compositionRoot.clipId)
                if (!trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse) {
                    compositionRoot.timeline.showToolTip()
                }
            }
            onDoubleClicked: mouse => {
                if (mouse.modifiers & Qt.ShiftModifier) {
                    if (compositionRoot.keyframeModel && compositionRoot.showKeyframes) {
                        // Add new keyframe
                        var xPos = Math.round(mouse.x  / compositionRoot.timeline.scaleFactor)
                        var yPos = (compositionRoot.height - mouse.y) / compositionRoot.height
                        compositionRoot.keyframeModel.addKeyframe(xPos + compositionRoot.inPoint, yPos)
                    } else {
                        proxy.position = compositionRoot.x / compositionRoot.timeline.scaleFactor
                    }
                } else {
                    compositionRoot.timeline.editItemDuration()
                }
            }
            onWheel: wheel => zoomByWheel(wheel)

            MouseArea {
                id: trimInMouseArea
                x: enabled ? -displayRect.border.width : 0
                height: mouseArea.height
                width: K.UiUtils.baseSizeMedium
                visible: enabled && K.Core.activeTool === K.ToolType.SelectTool
                enabled: !compositionRoot.grouped && (pressed || displayRect.width > 3 * width)
                hoverEnabled: true
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                onPressed: mouse => {
                    if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                        mouse.accepted = false
                        return
                    }
                    compositionRoot.originalX = compositionRoot.x
                    compositionRoot.originalDuration = compositionRoot.clipDuration
                    anchors.left = undefined
                }
                onReleased: {
                    anchors.left = parent.left
                    compositionRoot.trimmedIn(compositionRoot)
                    trimIn.opacity = 0
                    compositionRoot.updateDrag()
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        var delta = Math.round(x / compositionRoot.timeScale)
                        if (delta < -compositionRoot.modelStart) {
                            delta = -compositionRoot.modelStart
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
                        compositionRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to resize"))
                    }
                }
                onExited: {
                    if (!pressed) {
                        trimIn.opacity = 0
                    }
                    if (!mouseArea.containsMouse) {
                        compositionRoot.timeline.showToolTip()
                    }
                    if (!trimInMouseArea.containsMouse) {
                        compositionRoot.timeline.showKeyBinding()
                    }
                }
                Rectangle {
                    id: trimIn
                    anchors.left: trimInMouseArea.left
                    width: displayRect.handleWidth
                    height: mouseArea.height
                    color: 'lawngreen'
                    opacity: 0
                    Drag.active: trimInMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimInMouseArea.pressed || (K.Core.activeTool === K.ToolType.SelectTool && !mouseArea.drag.active && trimInMouseArea.enabled)
                }
            }

            MouseArea {
                id: trimOutMouseArea
                anchors.right: mouseArea.right
                anchors.rightMargin: enabled ? -displayRect.border.width : 0
                height: displayRect.height
                width: K.UiUtils.baseSizeMedium
                hoverEnabled: true
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimOutMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                visible: enabled && K.Core.activeTool === K.ToolType.SelectTool
                enabled: !compositionRoot.grouped && (pressed || displayRect.width > 3 * width)

                onPressed: mouse => {
                    if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                        mouse.accepted = false
                        return
                    }
                    compositionRoot.originalDuration = compositionRoot.clipDuration
                    anchors.right = undefined
                }
                onReleased: {
                    trimOut.opacity = 0
                    anchors.right = parent.right
                    compositionRoot.trimmedOut(compositionRoot)
                    compositionRoot.updateDrag()
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newDuration = Math.round((x + width) / compositionRoot.timeScale)
                        compositionRoot.trimmingOut(compositionRoot, newDuration)
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimOut.opacity = 1
                        compositionRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to resize"))
                    }
                }
                onExited: {
                    if (!pressed) {
                        trimOut.opacity = 0
                    }
                    if (!mouseArea.containsMouse) {
                        compositionRoot.timeline.showToolTip()
                    }
                    if (!trimOutMouseArea.containsMouse) {
                        compositionRoot.timeline.showKeyBinding()
                    }
                }
                Rectangle {
                    id: trimOut
                    anchors.right: trimOutMouseArea.right
                    width: displayRect.handleWidth
                    height: parent.height
                    color: 'red'
                    opacity: 0
                    Drag.active: trimOutMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimOutMouseArea.pressed || (K.Core.activeTool === K.ToolType.SelectTool && !mouseArea.drag.active && trimOutMouseArea.enabled)
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
                visible: K.KdenliveSettings.uiDebugMode
                anchors.left: labelRect.right
                Text {
                    // Composition ID text
                    id: debugCid
                    text: compositionRoot.clipId
                    font: K.UiUtils.smallestReadableFont
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
                visible: compositionRoot.width > K.UiUtils.baseSizeMedium
                width: label.width + 2
                height: label.height
                Text {
                    id: label
                    text: compositionRoot.clipName + (compositionRoot.aTrack > -1 ? ' > ' + compositionRoot.timeline.getTrackNameFromMltIndex(compositionRoot.aTrack) : '')
                    font: K.UiUtils.smallestReadableFont
                    anchors {
                        top: labelRect.top
                        left: labelRect.left
                        topMargin: 1
                        leftMargin: 1
                    }
                    color: 'black'
                }
                states: [
                    State { when: !compositionRoot.hideDecorations
                        PropertyChanges { labelRect.opacity: 1.0 }
                    },
                    State { when: compositionRoot.hideDecorations
                        PropertyChanges { labelRect.opacity: 0.0 }
                    }
                ]
                transitions: Transition {
                    NumberAnimation { property: "opacity"; duration: 250}
                }

            }
        }
        Loader {
            // keyframes container
            id: effectRow
            clip: true
            anchors.fill: parent
            active: compositionRoot.visible
            asynchronous: true
            visible: status == Loader.Ready && compositionRoot.showKeyframes && compositionRoot.keyframeModel && compositionRoot.width > 2 * K.UiUtils.baseSizeMedium
            source: compositionRoot.keyframeModel == undefined ? "" : "KeyframeView.qml"
            Binding {
                    target: effectRow.item
                    property: "kfrModel"
                    value: compositionRoot.keyframeModel
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
