/*
    SPDX-FileCopyrightText: 2020-2021 Jean-Baptiste Mardelle
    SPDX-FileCopyrightText: 2020 Sashmita Raghav

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.15
import QtQuick.Controls 2.15
import com.enums 1.0

Item {
    id: subtitleRoot
    visible : true
    z: selected ? 30 : 20
    property int oldStartX
    property int startFrame
    property int endFrame
    property int subId
    property int duration : endFrame - startFrame
    property double tScale: root.timeScale
    property string subtitle
    property bool selected
    property bool isGrabbed: false
    height: subtitleTrack.height
    
    function editText()
    {
        subtitleBase.textEditBegin = true
    }

    onStartFrameChanged: {
        if (!subtitleClipArea.pressed) {
            subtitleClipArea.x = startFrame * root.timeScale
        }
    }

    onTScaleChanged: {
        subtitleClipArea.x = startFrame * root.timeScale;
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
            x: startFrame * root.timeScale
            height: parent.height
            width: subtitleBase.width
            hoverEnabled: true
            enabled: true
            property int newStart: -1
            property int diff: -1
            property int oldStartFrame
            property int snappedFrame
            // Used for continuous scrolling
            property int incrementalOffset
            property double delta: -1
            property double oldDelta: 0
            property bool startMove: false
            visible: root.activeTool === ProjectTool.SelectTool
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: (pressed ? Qt.ClosedHandCursor : ((startMouseArea.drag.active || endMouseArea.drag.active)? Qt.SizeHorCursor: Qt.PointingHandCursor));
            drag.axis: Drag.XAxis
            drag.smoothed: false
            drag.minimumX: 0
            onEntered: {
                console.log('ENTERED SUBTITLE MOUSE AREA')
                timeline.showKeyBinding(i18n("<b>Double click</b> to edit text"))
            }
            onExited: {
                timeline.showKeyBinding()
            }
            onPressed: mouse => {
                console.log('ENTERED ITEM CLCKD:', subtitleRoot.subtitle, ' ID: ', subtitleRoot.subId, 'START FRM: ', subtitleRoot.startFrame)
                root.autoScrolling = false
                oldStartX = scrollView.contentX + mapToItem(scrollView, mouseX, 0).x
                oldStartFrame = subtitleRoot.startFrame
                snappedFrame = oldStartFrame
                x = subtitleBase.x
                startMove = mouse.button & Qt.LeftButton
                if (startMove) {
                    root.subtitleMoving = true
                    root.subtitleItem = subtitleClipArea
                    incrementalOffset = 0
                }
                if (timeline.selection.indexOf(subtitleRoot.subId) === -1) {
                    controller.requestAddToSelection(subtitleRoot.subId, !(mouse.modifiers & Qt.ShiftModifier))
                    timeline.showAsset(subtitleRoot.subId);
                } else if (mouse.modifiers & Qt.ShiftModifier) {
                    console.log('REMOVE FROM SELECTION!!!!')
                    controller.requestRemoveFromSelection(subtitleRoot.subId)
                } else {
                    timeline.showAsset(subtitleRoot.subId)
                }
                timeline.activeTrack = -2
            }
            function checkOffset(offset) {
                if (pressed && !subtitleBase.textEditBegin && startMove) {
                    incrementalOffset += offset
                    newStart = Math.max(0, oldStartFrame + (scrollView.contentX + mapToItem(scrollView,mouseX, 0).x + incrementalOffset - oldStartX)/ root.timeScale)
                    snappedFrame = controller.suggestSubtitleMove(subtitleRoot.subId, newStart, root.consumerPosition, root.snapping)
                    root.continuousScrolling(scrollView.contentX + mapToItem(scrollView, mouseX, 0).x + incrementalOffset, 0)
                }
            }
            onPositionChanged: {
                incrementalOffset = 0
                checkOffset(0)
            }
            onReleased: mouse => {
                root.autoScrolling = timeline.autoScroll
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
                    if (oldStartFrame != snappedFrame) {
                        console.log("old start frame",oldStartFrame/timeline.scaleFactor, "new frame after shifting ",oldStartFrame/timeline.scaleFactor + delta)
                        controller.requestSubtitleMove(subtitleRoot.subId, oldStartFrame, false, false);
                        controller.requestSubtitleMove(subtitleRoot.subId, snappedFrame, true, true, true);
                        x = snappedFrame * root.timeScale
                    }
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
                var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
                if (controller.requestSubtitleMove(subtitleRoot.subId, subtitleRoot.startFrame - offset, true, true)) {
                    timeline.showToolTip(i18n("Position: %1", timeline.simplifiedTC(subtitleRoot.startFrame)));
                }
            }
            Keys.onRightPressed: event => {
                var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
                if (controller.requestSubtitleMove(subtitleRoot.subId, subtitleRoot.startFrame + offset, true, true)) {
                    timeline.showToolTip(i18n("Position: %1", timeline.simplifiedTC(subtitleRoot.startFrame)));
                }
            }
            /*Keys.onUpPressed: {
                controller.requestClipMove(subtitleRoot.subId, controller.getNextTrackId(subtitleRoot.trackId), subtitleRoot.startFrame, true, true, true);
            }
            Keys.onDownPressed: {
                controller.requestClipMove(subtitleRoot.subId, controller.getPreviousTrackId(subtitleRoot.trackId), subtitleRoot.startFrame, true, true, true);
            }*/
            Keys.onEscapePressed: {
                timeline.grabCurrent()
                //focus = false
            }
        }
    Item {
        id: subtitleBase
        property bool textEditBegin: false
        height: subtitleTrack.height
        width: duration * root.timeScale // to make width change wrt timeline scale factor
        x: startFrame * root.timeScale;
        clip: true
        TextField {
            id: subtitleEdit
            font: miniFont
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
                    color: subtitleRoot.selected ? root.selectionColor : "#000"
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
        width: root.baseUnit / 2
        height: subtitleBase.height
        anchors.top: subtitleBase.top
        anchors.left: subtitleBase.left
        visible: true
        MouseArea {
            // Left resize handle to change start timing
            id: startMouseArea
            anchors.fill: parent
            hoverEnabled: true
            enabled: true
            visible: root.activeTool === ProjectTool.SelectTool
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
                root.autoScrolling = false
                oldMouseX = mouseX
                leftstart.anchors.left = undefined
                oldStartFrame = subtitleRoot.startFrame // the original start frame of subtitle
                originalDuration = subtitleRoot.duration
                newDuration = subtitleRoot.duration
                trimIn.opacity = 0
                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                if (!shiftTrim && (controller.isInGroup(subtitleRoot.subId) || controller.hasMultipleSelection())) {
                    root.groupTrimData = controller.getGroupData(subtitleRoot.subId)
                }
            }
            onPositionChanged: {
                if (pressed) {
                    newDuration = subtitleRoot.endFrame - Math.round(leftstart.x / root.timeScale)
                    if (newDuration != originalDuration && subtitleBase.x >= 0) {
                        var frame = controller.requestItemResize(subtitleRoot.subId, newDuration , false, false, root.snapping, shiftTrim);
                        if (frame > 0) {
                            newStart = subtitleRoot.endFrame - frame
                        }
                    }
                }
            }
            onReleased: {
                //console.log('its RELEASED')
                root.autoScrolling = timeline.autoScroll
                leftstart.anchors.left = subtitleBase.left
                if (oldStartFrame != newStart) {
                    if (shiftTrim || (root.groupTrimData == undefined || root.activeTool === ProjectTool.RippleTool)) {
                        controller.requestItemResize(subtitleRoot.subId, subtitleRoot.endFrame - oldStartFrame, false, false);
                        controller.requestItemResize(subtitleRoot.subId, subtitleRoot.endFrame - newStart, false, true, -1, shiftTrim);
                    } else {
                        var updatedGroupData = controller.getGroupData(subtitleRoot.subId)
                        controller.processGroupResize(root.groupTrimData, updatedGroupData, false)
                    }
                }
                root.groupTrimData = undefined
            }
            onEntered: {
                if (!pressed) {
                    trimIn.opacity = 1
                    timeline.showKeyBinding(i18n("<b>Drag</b> to resize"))
                }
            }
            onExited: {
                trimIn.opacity = 0
                if (!subtitleClipArea.containsMouse) {
                    timeline.showKeyBinding()
                }
            }

            Rectangle {
                id: trimIn
                anchors.left: parent.left
                width: 2
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
        width: root.baseUnit / 2
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
            hoverEnabled: true
            enabled: true
            visible: root.activeTool === ProjectTool.SelectTool
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
                root.autoScrolling = false
                newDuration = subtitleRoot.duration
                originalDuration = subtitleRoot.duration
                //rightend.anchors.right = undefined
                oldMouseX = mouseX
                trimOut.opacity = 0
                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                if (!shiftTrim && (controller.isInGroup(subtitleRoot.subId) || controller.hasMultipleSelection())) {
                    root.groupTrimData = controller.getGroupData(subtitleRoot.subId)
                }
            }
            onPositionChanged: {
                if (pressed) {
                    if ((mouseX != oldMouseX && duration > 1) || (duration <= 1 && mouseX > oldMouseX)) {
                        sizeChanged = true
                        //duration = subtitleBase.width + (mouseX - oldMouseX)/ timeline.scaleFactor
                        newDuration = Math.round((subtitleBase.width + mouseX - oldMouseX)/timeScale)
                        // Perform resize without changing model
                        var frame = controller.requestItemResize(subtitleRoot.subId, newDuration , true, false, root.snapping, shiftTrim);
                        if (frame > 0) {
                            newDuration = frame
                        }
                    }
                }
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
                rightend.anchors.right = subtitleBase.right
                console.log(' GOT RESIZE: ', newDuration, ' > ', originalDuration)
                if (mouseX != oldMouseX || sizeChanged) {
                    if (shiftTrim || (root.groupTrimData == undefined || root.activeTool === ProjectTool.RippleTool)) {
                        // Restore original size
                        controller.requestItemResize(subtitleRoot.subId, originalDuration , true, false);
                        // Perform real resize
                        controller.requestItemResize(subtitleRoot.subId, newDuration , true, true, -1, shiftTrim)
                    } else {
                        var updatedGroupData = controller.getGroupData(subtitleRoot.subId)
                        controller.processGroupResize(root.groupTrimData, updatedGroupData, true)
                    }
                    sizeChanged = false
                }
                root.groupTrimData = undefined
            }
            onEntered: {
                console.log('ENTER MOUSE END AREA')
                if (!pressed) {
                    trimOut.opacity = 1
                    timeline.showKeyBinding(i18n("<b>Drag</b> to resize"))
                }
            }
            onExited: {
                trimOut.opacity = 0
                if (!subtitleClipArea.containsMouse) {
                    timeline.showKeyBinding()
                }
            }

            Rectangle {
                id: trimOut
                anchors.right: parent.right
                width: 2
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
