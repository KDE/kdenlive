/*
 * Copyright (c) 2013-2016 Meltytech, LLC
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

Rectangle {
    id: trackHeadRoot
    property string trackName
    property string effectNames
    property bool isStackEnabled
    property bool isDisabled
    property bool collapsed: false
    property int isComposite
    property bool isLocked: false
    property bool isActive: false
    property bool isAudio
    property bool showAudioRecord: false
    property bool current: false
    property int trackId : -42
    property string trackTag
    property int thumbsFormat: 0
    border.width: 1
    border.color: root.frameColor

    function pulseLockButton() {
        flashLock.restart();
    }

    color: getTrackColor(isAudio, true)
    //border.color: selected? 'red' : 'transparent'
    //border.width: selected? 1 : 0
    clip: true
    state: 'normal'
    states: [
        State {
            name: 'current'
            when: trackHeadRoot.current
            PropertyChanges {
                target: trackHeadRoot
                color: selectedTrackColor
            }
        },
        State {
            when: !trackHeadRoot.current
            name: 'normal'
            PropertyChanges {
                target: trackHeadRoot
                color: getTrackColor(isAudio, true)
            }
        }
    ]

    Keys.onDownPressed: {
        root.moveSelectedTrack(1)
    }
    Keys.onUpPressed: {
        root.moveSelectedTrack(-1)
    }

    MouseArea {
        id: headerMouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            timeline.activeTrack = trackId
            if (mouse.button == Qt.RightButton) {
                root.showHeaderMenu()
            }
        }
        onClicked: {
            console.log('TRACK ID: ', trackId)
            parent.forceActiveFocus()
            nameEdit.visible = false
            if (mouse.button == Qt.LeftButton) {
                timeline.showTrackAsset(trackId)
            }
        }
    }
    Label {
        id: trackTarget
        property color bgColor: 'grey'
        font: miniFont
        color: timeline.targetTextColor
        background: Rectangle {
            color: trackTarget.bgColor
        }
        width: 2 * fontMetrics.boundingRect("M").width
        height: trackHeadRoot.height
        verticalAlignment: Text.AlignTop
        horizontalAlignment: Text.AlignHCenter
        visible: trackHeadRoot.isAudio ? timeline.hasAudioTarget > 0 : timeline.hasVideoTarget
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 1

        MouseArea {
            id: targetArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (mouse.button == Qt.RightButton) {
                    if (trackHeadRoot.isAudio) {
                        root.showTargetMenu(trackId)
                    } else {
                        root.showHeaderMenu()
                    }
                }
                else {
                    if (trackHeadRoot.isAudio) {
                        timeline.switchAudioTarget(trackHeadRoot.trackId);
                    } else {
                        if (trackHeadRoot.trackId == timeline.videoTarget) {
                            timeline.videoTarget = -1;
                        } else if (timeline.hasVideoTarget) {
                            timeline.videoTarget = trackHeadRoot.trackId;
                        }
                    }
                }
            }
            ToolButton {
                id: targetMouse
                focusPolicy: Qt.NoFocus
                visible: trackHeadRoot.isAudio && timeline.clipTargets > 1 && trackHeadRoot.height > (2 * expandButton.height)
                background: Rectangle {
                    color: Qt.darker(trackTarget.bgColor, 1.5)
                    border.color: activePalette.light
                }
                anchors.bottom: parent.bottom
                width: parent.width
                height: width
                contentItem: Item {
                    Image {
                        source: "image://icon/go-down"
                        anchors.fill: parent
                    }
                }
                onClicked: {
                    root.showTargetMenu(trackId)
                }
                ToolTip {
                    visible: targetMouse.hovered
                    font: miniFont
                    delay: 1500
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        text: timeline.actionText("switch_target_stream")
                    }
                }
            }
        }
        ToolTip {
            visible: targetArea.containsMouse && !targetMouse.hovered
            font: miniFont
            delay: 1500
            timeout: 5000
            background: Rectangle {
                color: activePalette.alternateBase
                border.color: activePalette.light
            }
            contentItem: Label {
                color: activePalette.text
                text: i18n("Click to toggle track as target. Target tracks will receive the inserted clips")
            }
        }
    state:  'normalTarget'
    states: [
        State {
            name: 'target'
            when: (trackHeadRoot.isAudio && timeline.audioTarget.indexOf(trackHeadRoot.trackId) > -1) || (!trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.videoTarget)
            PropertyChanges {
                target: trackTarget
                bgColor: timeline.targetColor
                text: trackHeadRoot.isAudio ? timeline.audioTargetName(trackHeadRoot.trackId) : ''
            }
        },
        State {
            name: 'inactiveTarget'
            when: (trackHeadRoot.isAudio && timeline.lastAudioTarget.indexOf(trackHeadRoot.trackId) > -1) || (!trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.lastVideoTarget)
            PropertyChanges {
                target: trackTarget
                opacity: 0.3
                bgColor: activePalette.text
                text: trackHeadRoot.isAudio ? timeline.audioTargetName(trackHeadRoot.trackId) : ''
            }
        },
        State {
            name: 'noTarget'
            when: !trackHeadRoot.isLocked && !trackHeadRoot.isDisabled
            PropertyChanges {
                target: trackTarget
                bgColor: activePalette.base
                text: ''
            }
        }
    ]
    transitions: [
        Transition {
            to: '*'
            ColorAnimation { target: trackTarget; duration: 300 }
        }
    ]
    }
    Item {
        id: trackHeadColumn
        anchors.fill: parent
        anchors.leftMargin: trackTarget.width
        anchors.topMargin: 0

        ToolButton {
            id: expandButton
            focusPolicy: Qt.NoFocus
            property var modifier: 0
            contentItem: Item {
                Image {
                    source: trackHeadRoot.collapsed ? "image://icon/go-next" : "image://icon/go-down"
                    anchors.centerIn: parent
                    width: root.collapsedHeight - 4
                    height: root.collapsedHeight - 4
                    cache: root.paletteUnchanged
                }
            }
            onClicked: {
                if (modifier & Qt.ShiftModifier) {
                    // Collapse / expand all tracks
                    timeline.collapseAllTrackHeight(trackId, !trackHeadRoot.collapsed, root.collapsedHeight)
                } else {
                    if (trackHeadRoot.collapsed) {
                        var newHeight = Math.max(root.collapsedHeight * 1.5, controller.getTrackProperty(trackId, "kdenlive:trackheight"))
                        controller.setTrackProperty(trackId, "kdenlive:trackheight", newHeight)
                        controller.setTrackProperty(trackId, "kdenlive:collapsed", "0")
                    } else {
                        controller.setTrackProperty(trackId, "kdenlive:collapsed", root.collapsedHeight)
                    }
                }
            }
            MouseArea {
                // Used to pass modifier state to expand button
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: {
                    expandButton.modifier = mouse.modifiers
                    mouse.accepted = false
                }
            }
            anchors.left: parent.left
            width: root.collapsedHeight
            height: root.collapsedHeight
            ToolTip {
                visible: expandButton.hovered
                font: miniFont
                delay: 1500
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    text: trackLabel.visible? i18n("Minimize") : i18n("Expand")
                }
            }
        }
        Label {
            id: trackLed
            property color bgColor: Qt.darker(trackHeadRoot.color, 0.55)
            anchors.left: expandButton.right
            font: miniFont
            text: trackHeadRoot.trackTag
            color: activePalette.text
            background: Rectangle {
                color: trackLed.bgColor
            }
            width: fontMetrics.boundingRect("M").width * trackHeadRoot.trackTag.length
            height: root.collapsedHeight - 2
            y: 1
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            MouseArea {
                id: tagMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    timeline.switchTrackActive(trackHeadRoot.trackId)
                }
            }
            ToolTip {
                visible: tagMouseArea.containsMouse
                font: miniFont
                delay: 1500
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    text: i18n("Click to make track active/inactive. Active tracks will react to editing operations")
                }
                }
        state:  'normalled'
            states: [
                State {
                    name: 'locked'
                    when: trackHeadRoot.isLocked
                    PropertyChanges {
                        target: trackLed
                        bgColor: 'red'
                    }
                },
                State {
                    name: 'active'
                    when: trackHeadRoot.isActive
                    PropertyChanges {
                        target: trackLed
                        bgColor: timeline.targetColor
                        color: timeline.targetTextColor
                    }
                },
                State {
                    name: 'inactive'
                    when: !trackHeadRoot.isLocked && !trackHeadRoot.isActive
                    PropertyChanges {
                        target: trackLed
                        bgColor: Qt.darker(trackHeadRoot.color, 0.55)
                    }
                }
            ]
            transitions: [
                Transition {
                    to: '*'
                    ColorAnimation { target: trackLed; duration: 300 }
                }
            ]
        }
        Label {
            anchors.left: trackLed.right
            anchors.top: parent.top
            anchors.leftMargin: 2
            height: trackLed.height
            width: buttonsRow.x - x
            text: trackHeadRoot.trackName
            elide: Text.ElideRight
            font: miniFont
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
            visible: !trackLabel.visible && trackHeadRoot.width > (trackTarget.width + expandButton.width + trackLed.width + (4 * muteButton.width) + 4)
        }
        Row {
            id: buttonsRow
            width: childrenRect.width
            x: Math.max(2 * root.collapsedHeight + 2, parent.width - width - 4)
            spacing: 0
            ToolButton {
                id: effectButton
                focusPolicy: Qt.NoFocus
                contentItem: Item {
                    Image {
                        source: "image://icon/tools-wizard"
                        anchors.centerIn: parent
                        width: root.collapsedHeight - 4
                        height: root.collapsedHeight - 4
                        cache: root.paletteUnchanged
                        opacity: effectButton.enabled ? 1 : 0.5
                    }
                }
                enabled: trackHeadRoot.effectNames != ''
                checkable: true
                checked: enabled && trackHeadRoot.isStackEnabled
                onClicked: {
                    timeline.showTrackAsset(trackId)
                    controller.setTrackStackEnabled(trackId, !isStackEnabled)
                }
                width: root.collapsedHeight
                height: root.collapsedHeight
            }
            ToolButton {
                id: muteButton
                focusPolicy: Qt.NoFocus
                contentItem: Item {
                    Image {
                        source: isAudio ? (isDisabled ? "image://icon/kdenlive-hide-audio" : "image://icon/kdenlive-show-audio") : (isDisabled ? "image://icon/kdenlive-hide-video" : "image://icon/kdenlive-show-video")
                        anchors.centerIn: parent
                        width: root.collapsedHeight - 4
                        height: root.collapsedHeight - 4
                        cache: root.paletteUnchanged
                    }
                }
                width: root.collapsedHeight
                height: root.collapsedHeight
                onClicked: controller.hideTrack(trackId, isDisabled ? (isAudio ? '1' : '2') : '3')
                ToolTip {
                    visible: muteButton.hovered
                    font: miniFont
                    delay: 1500
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        text: isAudio ? (isDisabled? i18n("Unmute") : i18n("Mute")) : (isDisabled? i18n("Show") : i18n("Hide"))
                    }
                }
            }

            ToolButton {
                id: lockButton
                width: root.collapsedHeight
                height: root.collapsedHeight
                focusPolicy: Qt.NoFocus
                contentItem: Item {
                    Image {
                        source: trackHeadRoot.isLocked ? "image://icon/kdenlive-lock" : "image://icon/kdenlive-unlock"
                        anchors.centerIn: parent
                        width: root.collapsedHeight - 4
                        height: root.collapsedHeight - 4
                        cache: root.paletteUnchanged
                    }
                }
                onClicked: controller.setTrackLockedState(trackId, !isLocked)
                ToolTip {
                    visible: lockButton.hovered
                    font: miniFont
                    delay: 1500
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        text: isLocked? i18n("Unlock track") : i18n("Lock track")
                    }
                }

                SequentialAnimation {
                    id: flashLock
                    loops: 1
                    ScaleAnimator {
                        target: lockButton
                        from: 1
                        to: 2
                        duration: 500
                    }
                    ScaleAnimator {
                        target: lockButton
                        from: 2
                        to: 1
                        duration: 500
                    }
                 }
            }
        }
        Item {
            id: recLayout
            y: root.collapsedHeight + 4
            anchors.left: trackHeadColumn.left
            anchors.right: trackHeadColumn.right
            anchors.margins: 2
            height: showAudioRecord ? root.collapsedHeight : 0
            Loader {
                id: audioVuMeter
                anchors.fill: parent
                visible: showAudioRecord && (trackHeadRoot.height >= 2 * muteButton.height + resizer.height)
                source: isAudio && showAudioRecord ? "AudioLevels.qml" : ""
                onLoaded: item.trackId = trackId
            }
        }
        Item {
            anchors.bottom: trackHeadColumn.bottom
            anchors.left: trackHeadColumn.left
            anchors.right: trackHeadColumn.right
            anchors.margins: 2
            height: nameEdit.height
            Rectangle {
                id: trackLabel
                color: 'transparent'
                radius: 2
                anchors.fill: parent
                border.color: trackNameMouseArea.containsMouse ? activePalette.highlight : 'transparent'
                visible: (trackHeadRoot.height >= trackLabel.height + muteButton.height + resizer.height + recLayout.height)
                MouseArea {
                    id: trackNameMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    cursorShape: Qt.IBeamCursor
                    onDoubleClicked: {
                        nameEdit.visible = true
                        nameEdit.focus = true
                        nameEdit.selectAll()
                    }
                    onClicked: {
                        timeline.showTrackAsset(trackId)
                        timeline.activeTrack = trackId
                        trackHeadRoot.focus = true
                    }
                }
                Label {
                    text: trackName
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                    font: miniFont
                }
                Label {
                    id: placeHolder
                    visible: trackName == '' && (trackNameMouseArea.containsMouse || headerMouseArea.containsMouse)
                    enabled: false
                    text: i18n("Edit track name")
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                    font: miniFont
                }
                TextField {
                    id: nameEdit
                    visible: false
                    width: parent.width
                    text: trackName
                    font: miniFont
                    background: Rectangle {
                        radius: 2
                        color: activePalette.window
                        anchors.fill: parent
                    }
                    /*style: TextFieldStyle {
                        padding.top:0
                        padding.bottom: 0
                        background: Rectangle {
                            radius: 2
                            color: activePalette.window
                            anchors.fill: parent
                        }
                    }*/
                    onEditingFinished: {
                        controller.setTrackName(trackId, text)
                        visible = false
                    }
                }
            }
        }
    }
    Rectangle {
            id: resizer
            height: 4
            color: 'red'
            opacity: 0
            Drag.active: trimInMouseArea.drag.active
            Drag.proposedAction: Qt.MoveAction
            width: trackHeadRoot.width
            y: trackHeadRoot.height - height

            MouseArea {
                id: trimInMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor
                drag.target: parent
                drag.axis: Drag.YAxis
                drag.minimumY: root.collapsedHeight - resizer.height
                property double startY
                property double originalY
                drag.smoothed: false
                property bool dragStarted: false

                onPressed: {
                    root.autoScrolling = false
                    dragStarted = false
                    startY = mapToItem(null, x, y).y
                    originalY = trackHeadRoot.height // reusing originalX to accumulate delta for bubble help
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    if (!trimInMouseArea.containsMouse) {
                        parent.opacity = 0
                    }
                    if (mouse.modifiers & Qt.ShiftModifier && dragStarted) {
                        timeline.adjustAllTrackHeight(trackHeadRoot.trackId, trackHeadRoot.height)
                    }
                }
                onEntered: parent.opacity = 0.3
                onExited: parent.opacity = 0
                onDoubleClicked: {
                    timeline.defaultTrackHeight(mouse.modifiers & Qt.ShiftModifier ? -1 : trackHeadRoot.trackId)
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        parent.opacity = 0.5
                        if (!dragStarted && Math.abs(mapToItem(null, x, y).y - startY) > 2) {
                            dragStarted = true
                        }
                        var newHeight = originalY + (mapToItem(null, x, y).y - startY)
                        newHeight =  Math.max(root.collapsedHeight, newHeight)
                        if (newHeight == root.collapsedHeight) {
                            controller.setTrackProperty(trackId, "kdenlive:collapsed", root.collapsedHeight)
                        } else {
                            controller.setTrackProperty(trackId, "kdenlive:trackheight", newHeight)
                            controller.setTrackProperty(trackId, "kdenlive:collapsed", "0")
                        }
                    }
                }
            }
        }
    DropArea { //Drop area for tracks
        anchors.fill: trackHeadRoot
        keys: 'kdenlive/effect'
        property string dropData
        property string dropSource
        property int dropRow: -1
        onEntered: {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
        }
        onDropped: {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                controller.addTrackEffect(trackHeadRoot.trackId, dropData);
            } else {
                controller.copyTrackEffect(trackHeadRoot.trackId, dropSource);
            }
            dropSource = ''
            dropRow = -1
            drag.acceptProposedAction
        }
    }
}

