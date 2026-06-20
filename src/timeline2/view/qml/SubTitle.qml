/*
    SPDX-FileCopyrightText: 2020-2021 Jean-Baptiste Mardelle
    SPDX-FileCopyrightText: 2020 Sashmita Raghav

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: subtitleRoot
    visible : true
    z: selected ? 30 : 20

    required property K.TimelineController timeline
    required property K.TimelineItemModel controller
    required property bool isPanning
    required property double timeScale

    readonly property bool isUserInteracting: subtitleClipArea.pressed || startMouseArea.pressed || endMouseArea.pressed

    property int oldStartX
    property int startFrame
    property int fakeStartFrame
    property int endFrame
    property int subId
    property int duration : endFrame - startFrame
    property string subtitle
    property bool selected
    property bool isGrabbed: false
    property int subLayer
    height: subtitleTrack.height / (timeline.maxSubLayer + 1)
    property int handleWidth: Math.max(2, Math.ceil(K.UiUtils.baseSizeMedium / 4))
    y: height * subLayer

    function editText()
    {
        subtitleBase.textEditBegin = true
    }

    onSubLayerChanged: {
        y = height * subLayer
    }
    
    onHeightChanged: {
        y = height * subLayer
    }

    onFakeStartFrameChanged: {
        if (subtitleRoot.fakeStartFrame == -1) {
            // Restore binding
            subtitleBase.x = Qt.binding(function () {
                return subtitleRoot.startFrame * subtitleRoot.timeScale
            })
        } else {
            subtitleBase.x = subtitleRoot.fakeStartFrame * subtitleRoot.timeScale
        }
    }

    onStartFrameChanged: {
        if (!subtitleClipArea.pressed) {
            subtitleClipArea.x = startFrame * subtitleRoot.timeScale
        }
    }

    onTimeScaleChanged: {
        subtitleClipArea.x = startFrame * subtitleRoot.timeScale;
    }

    onIsGrabbedChanged: {
        if (subtitleRoot.isGrabbed) {
            grabItem()
        } else {
            timeline.showToolTip()
            subtitleClipArea.focus = false
        }
    }

    onSelectedChanged: {
        if (!selected && isGrabbed) {
            //timeline.grabCurrent()
        }
        if (subtitleBase.textEditBegin) {
            // End editing on focus change
            subtitleBase.textEditBegin = false
        }
    }

    function grabItem() {
        subtitleClipArea.forceActiveFocus()
        subtitleClipArea.focus = true
    }

    MouseArea {
            // Clip shifting
            id: subtitleClipArea
            x: subtitleRoot.startFrame * subtitleRoot.timeScale
            height: parent.height
            width: subtitleBase.width
            hoverEnabled: !subtitleRoot.isPanning
            enabled: !subtitleRoot.isPanning
            property int newStart: -1
            property int diff: -1
            property int oldLayer
            property int oldStartFrame
            property int snappedFrame
            property int snappedLayer
            // Used for continuous scrolling
            property int incrementalOffset
            property double delta: -1
            property double oldDelta: 0
            property bool startMove: false
            visible: K.Core.activeTool === K.ToolType.SelectTool
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: (pressed ? Qt.ClosedHandCursor : ((startMouseArea.drag.active || endMouseArea.drag.active)? Qt.SizeHorCursor: Qt.PointingHandCursor));
            drag.axis: Drag.XAxis | Drag.YAxis
            drag.smoothed: false
            drag.minimumX: 0
            onEntered: {
                if (subtitleRoot.isPanning) return
                console.log('ENTERED SUBTITLE MOUSE AREA')
                subtitleRoot.timeline.showKeyBinding(KI18n.i18n("<b>Double click</b> to edit text"))
            }
            onExited: {
                if (subtitleRoot.isPanning) return
                subtitleRoot.timeline.showKeyBinding()
            }
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                console.log('ENTERED ITEM CLICKED:', subtitleRoot.subtitle, ' ID: ', subtitleRoot.subId, 'START FRAME: ', subtitleRoot.startFrame)
                oldStartX = scrollView.contentX + mapToItem(scrollView, mouseX, 0).x
                oldStartFrame = subtitleRoot.startFrame
                oldLayer = subtitleRoot.subLayer
                snappedFrame = oldStartFrame
                snappedLayer = oldLayer
                x = subtitleBase.x
                startMove = mouse.button & Qt.LeftButton
                if (startMove) {
                    root.subtitleMoving = true
                    root.subtitleItem = subtitleClipArea
                    incrementalOffset = 0
                }
                if (subtitleRoot.timeline.selection.indexOf(subtitleRoot.subId) === -1) {
                    subtitleRoot.controller.requestAddToSelection(subtitleRoot.subId, !(mouse.modifiers & Qt.ShiftModifier))
                    subtitleRoot.timeline.showAsset(subtitleRoot.subId);
                } else if (mouse.modifiers & Qt.ShiftModifier) {
                    console.log('REMOVE FROM SELECTION!!!!')
                    subtitleRoot.controller.requestRemoveFromSelection(subtitleRoot.subId)
                } else {
                    subtitleRoot.timeline.showAsset(subtitleRoot.subId)
                }
                subtitleRoot.timeline.activeTrack = -2
                subtitleRoot.timeline.activeSubLayer = subtitleRoot.subLayer
            }
            function checkOffset(offset) {
                if (pressed && !subtitleBase.textEditBegin && startMove) {
                    incrementalOffset += offset
                    newStart = Math.max(0, oldStartFrame + (scrollView.contentX + mapToItem(scrollView,mouseX, 0).x + incrementalOffset - subtitleRoot.oldStartX)/ subtitleRoot.timeScale)
                    snappedFrame = subtitleRoot.controller.suggestSubtitleMove(subtitleRoot.subId, subtitleRoot.subLayer, newStart, root.consumerPosition, root.snapping)
                    root.continuousScrolling(scrollView.contentX + mapToItem(scrollView, mouseX, 0).x + incrementalOffset, subtitleRoot.timeScale)
                }
            }
            onPositionChanged: (mouse) => {
                incrementalOffset = 0
                checkOffset(0)

                var layerOffset = mouse.y / subtitleRoot.height
                if (layerOffset >= 1 || layerOffset <= 0) {
                    var newLayer = Math.min(subtitleRoot.subLayer + layerOffset, subtitleRoot.timeline.maxSubLayer)
                    newLayer = Math.max(0, newLayer)
                    snappedLayer = Math.floor(newLayer)
                    subtitleRoot.controller.requestSubtitleMove(subtitleRoot.subId, snappedLayer, snappedFrame, true, false)
                }
            }
            onReleased: mouse => {
                root.subtitleMoving = false
                root.subtitleItem = undefined
                if (subtitleBase.textEditBegin) {
                    mouse.accepted = false
                    return
                }
                if (startMove) {
                    startMove = false
                    if (subtitleBase.x < 0)
                        subtitleBase.x = 0
                    // if mouse out of the bottom of the SubtitleTrack with shift pressed, snappedLayer++
                    if (mouse.y > subtitleRoot.height && mouse.modifiers & Qt.ShiftModifier) {
                        snappedLayer++
                    }
                    console.log("old start frame", oldStartFrame/subtitleRoot.timeline.scaleFactor, "new frame after shifting ", oldStartFrame/subtitleRoot.timeline.scaleFactor + delta)
                    subtitleRoot.controller.requestSubtitleMove(subtitleRoot.subId, oldLayer, oldStartFrame, false, false)
                    subtitleRoot.controller.requestSubtitleMove(subtitleRoot.subId, snappedLayer, snappedFrame, true, true, true)
                    x = snappedFrame * subtitleRoot.timeScale
                }
                console.log('RELEASED DONE\n\n_______________')
            }
            onClicked: mouse => {
                if (mouse.button == Qt.RightButton) {
                    //console.log('RIGHT BUTTON CLICKED')
                    root.showSubtitleClipMenu()
                }
            }
            onDoubleClicked: {
                subtitleBase.textEditBegin = true
            }
            Keys.onShortcutOverride: event => {
                event.accepted = subtitleRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)
            }
            Keys.onLeftPressed: event => {
                var offset = event.modifiers === Qt.ShiftModifier ? K.Core.getCurrentFps() : 1
                if (subtitleRoot.controller.requestSubtitleMove(subtitleRoot.subId, subtitleRoot.subLayer, subtitleRoot.startFrame - offset, true, true, true)) {
                    subtitleRoot.timeline.showToolTip(KI18n.i18n("Position: %1", subtitleRoot.timeline.simplifiedTC(subtitleRoot.startFrame)));
                }
            }
            Keys.onRightPressed: event => {
                var offset = event.modifiers === Qt.ShiftModifier ? K.Core.getCurrentFps() : 1
                if (subtitleRoot.controller.requestSubtitleMove(subtitleRoot.subId, subtitleRoot.subLayer, subtitleRoot.startFrame + offset, true, true, true)) {
                    subtitleRoot.timeline.showToolTip(KI18n.i18n("Position: %1", subtitleRoot.timeline.simplifiedTC(subtitleRoot.startFrame)));
                }
            }
            /*Keys.onUpPressed: {
                controller.requestClipMove(subtitleRoot.subId, controller.getNextTrackId(subtitleRoot.trackId), subtitleRoot.startFrame, true, true, true);
            }
            Keys.onDownPressed: {
                controller.requestClipMove(subtitleRoot.subId, controller.getPreviousTrackId(subtitleRoot.trackId), subtitleRoot.startFrame, true, true, true);
            }*/
            Keys.onEscapePressed: {
                subtitleRoot.timeline.grabCurrent()
                //focus = false
            }
        }
    Item {
        id: subtitleBase
        property bool textEditBegin: false
        height: subtitleRoot.height
        width: subtitleRoot.duration * subtitleRoot.timeScale // to make width change wrt timeline scale factor
        x: subtitleRoot.startFrame * subtitleRoot.timeScale;
        clip: true
        TextField {
            id: subtitleEdit
            font: K.UiUtils.smallestReadableFont
            activeFocusOnPress: true
            selectByMouse: true
            onEditingFinished: {
                subtitleEdit.focus = false
                parent.textEditBegin = false
                if (subtitleRoot.subtitle != subtitleEdit.text) {
                    subtitleModel.editSubtitle(subtitleRoot.subId, subtitleEdit.text, subtitleRoot.subtitle)
                }
            }
            anchors.fill: parent
            //visible: timeScale >= 6
            enabled: parent.textEditBegin
            opacity: root.subtitlesDisabled ? 0.5 : 1
            onEnabledChanged: {
                if (enabled) {
                    selectAll()
                    focus = true
                    forceActiveFocus()
                }
            }
            text: subtitleRoot.subtitle
            height: subtitleBase.height
            width: subtitleBase.width
            wrapMode: TextField.WordWrap
            horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
            background: Rectangle {
                color: root.subtitlesLocked ? "#ff6666" : enabled ? "#fff" : '#ccccff'
                border {
                    color: subtitleRoot.selected ? subtitleRoot.timeline.selectionColor : "#000"
                    width: subtitleRoot.isGrabbed ? 8 : 2
                }
            }
            color: 'black'
            padding: 0
        }
    }
    Item {
        // start position resize handle
        id: leftstart
        width: K.UiUtils.baseSizeMedium
        height: subtitleBase.height
        anchors.top: subtitleBase.top
        anchors.left: subtitleBase.left
        visible: true
        MouseArea {
            // Left resize handle to change start timing
            id: startMouseArea
            anchors.fill: parent
            hoverEnabled: !subtitleRoot.isPanning
            enabled: !subtitleRoot.isPanning
            visible: K.Core.activeTool === K.ToolType.SelectTool
            property int newStart: subtitleRoot.startFrame
            property int newDuration: subtitleRoot.duration
            property int originalDuration: subtitleRoot.duration
            property int oldMouseX
            property int oldStartFrame: 0
            property bool shiftTrim: false
            acceptedButtons: Qt.LeftButton
            drag.axis: Drag.XAxis
            drag.smoothed: false
            cursorShape: containsMouse || pressed ? Qt.SizeHorCursor : Qt.ClosedHandCursor;
            drag.target: leftstart
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                oldMouseX = mouseX
                leftstart.anchors.left = undefined
                oldStartFrame = subtitleRoot.startFrame // the original start frame of subtitle
                originalDuration = subtitleRoot.duration
                newDuration = subtitleRoot.duration
                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                if (!shiftTrim && (subtitleRoot.controller.isInGroup(subtitleRoot.subId) || subtitleRoot.controller.hasMultipleSelection())) {
                    root.groupTrimData = subtitleRoot.controller.getGroupData(subtitleRoot.subId)
                }
            }
            onPositionChanged: {
                if (pressed) {
                    newDuration = subtitleRoot.endFrame - Math.round(leftstart.x / subtitleRoot.timeScale)
                    if (newDuration != originalDuration && subtitleBase.x >= 0) {
                        var frame = subtitleRoot.controller.requestItemResize(subtitleRoot.subId, newDuration , false, false, root.snapping, shiftTrim);
                        if (frame > 0) {
                            newStart = subtitleRoot.endFrame - frame
                        }
                    }
                }
            }
            onReleased: {
                //console.log('its RELEASED')
                trimIn.opacity = 0
                leftstart.anchors.left = subtitleBase.left
                if (oldStartFrame != newStart) {
                    if (shiftTrim || (root.groupTrimData == undefined || K.Core.activeTool === K.ToolType.RippleTool)) {
                        subtitleRoot.controller.requestItemResize(subtitleRoot.subId, subtitleRoot.endFrame - oldStartFrame, false, false);
                        subtitleRoot.controller.requestItemResize(subtitleRoot.subId, subtitleRoot.endFrame - newStart, false, true, -1, shiftTrim);
                    } else {
                        var updatedGroupData = subtitleRoot.controller.getGroupData(subtitleRoot.subId)
                        subtitleRoot.controller.processGroupResize(root.groupTrimData, updatedGroupData, false)
                    }
                }
                root.groupTrimData = undefined
            }
            onEntered: {
                if (!pressed) {
                    trimIn.opacity = 1
                    subtitleRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to resize"))
                }
            }
            onExited: {
                if (!pressed) {
                    trimIn.opacity = 0
                }
                if (!subtitleClipArea.containsMouse) {
                    subtitleRoot.timeline.showKeyBinding()
                }
            }

            Rectangle {
                id: trimIn
                anchors.left: parent.left
                width: subtitleRoot.handleWidth
                height: parent.height
                color: 'lawngreen'
                opacity: 0
                Drag.active: startMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction
                //visible: startMouseArea.pressed
            }
        }
    }
    
    Item {
        // end position resize handle
        id: rightend
        width: K.UiUtils.baseSizeMedium
        height: subtitleBase.height
        //x: subtitleRoot.endFrame * timeScale
        anchors.right: subtitleBase.right
        anchors.top: subtitleBase.top
        //Drag.active: endMouseArea.drag.active
        //Drag.proposedAction: Qt.MoveAction
        visible: true
        MouseArea {
            // Right resize handle to change end timing
            id: endMouseArea
            anchors.fill: parent
            hoverEnabled: !subtitleRoot.isPanning
            enabled: !subtitleRoot.isPanning
            visible: K.Core.activeTool === K.ToolType.SelectTool
            property bool sizeChanged: false
            property int oldMouseX
            acceptedButtons: Qt.LeftButton
            property int newDuration: subtitleRoot.duration
            property int originalDuration
            property bool shiftTrim: false
            cursorShape: containsMouse || pressed ? Qt.SizeHorCursor : Qt.ClosedHandCursor;
            drag.target: rightend
            drag.axis: Drag.XAxis
            drag.smoothed: false

            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                newDuration = subtitleRoot.duration
                originalDuration = subtitleRoot.duration
                //rightend.anchors.right = undefined
                oldMouseX = mouseX
                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                if (!shiftTrim && (subtitleRoot.controller.isInGroup(subtitleRoot.subId) || subtitleRoot.controller.hasMultipleSelection())) {
                    root.groupTrimData = subtitleRoot.controller.getGroupData(subtitleRoot.subId)
                }
            }
            onPositionChanged: {
                if (pressed) {
                    if ((mouseX != oldMouseX && subtitleRoot.duration > 1) || (subtitleRoot.duration <= 1 && mouseX > oldMouseX)) {
                        sizeChanged = true
                        //duration = subtitleBase.width + (mouseX - oldMouseX)/ timeline.scaleFactor
                        newDuration = Math.round((subtitleBase.width + mouseX - oldMouseX)/timeScale)
                        // Perform resize without changing model
                        var frame = subtitleRoot.controller.requestItemResize(subtitleRoot.subId, newDuration , true, false, root.snapping, shiftTrim);
                        if (frame > 0) {
                            newDuration = frame
                        }
                    }
                }
            }
            onReleased: {
                trimOut.opacity = 0
                rightend.anchors.right = subtitleBase.right
                console.log(' GOT RESIZE: ', newDuration, ' > ', originalDuration)
                if (mouseX != oldMouseX || sizeChanged) {
                    if (shiftTrim || (root.groupTrimData == undefined || K.Core.activeTool === K.ToolType.RippleTool)) {
                        // Restore original size
                        subtitleRoot.controller.requestItemResize(subtitleRoot.subId, originalDuration , true, false);
                        // Perform real resize
                        subtitleRoot.controller.requestItemResize(subtitleRoot.subId, newDuration , true, true, -1, shiftTrim)
                    } else {
                        var updatedGroupData = subtitleRoot.controller.getGroupData(subtitleRoot.subId)
                        subtitleRoot.controller.processGroupResize(root.groupTrimData, updatedGroupData, true)
                    }
                    sizeChanged = false
                }
                root.groupTrimData = undefined
            }
            onEntered: {
                console.log('ENTER MOUSE END AREA')
                if (!pressed) {
                    trimOut.opacity = 1
                    subtitleRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to resize"))
                }
            }
            onExited: {
                if (!pressed) {
                    trimOut.opacity = 0
                }
                if (!subtitleClipArea.containsMouse) {
                    subtitleRoot.timeline.showKeyBinding()
                }
            }

            Rectangle {
                id: trimOut
                anchors.right: parent.right
                width: subtitleRoot.handleWidth
                height: parent.height
                color: 'red'
                opacity: 0
                Drag.active: endMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction
                //visible: endMouseArea.pressed
            }
        }
    }
}
