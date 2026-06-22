/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.ki18n

import org.kde.kdenlive as K


Rectangle {
    id: trackHeadRoot
    required property K.TimelineController timeline
    required property K.TimelineItemModel controller
    required property string trackName
    required property string effectNames
    required property bool isStackEnabled
    required property bool isDisabled
    required property bool collapsed
    required property int isComposite
    required property bool isLocked
    required property bool isActive
    required property bool isAudio
    required property bool showAudioRecord
    required property bool current
    required property int trackId
    required property string trackTag
    required property int thumbsFormat
    required property int collapsedHeight
    border.width: 1

    SystemPalette { id: activePalette }

    function editName() {
        nameEdit.visible = true
        nameEdit.focus = true
        nameEdit.selectAll()
    }

    function animateLock() {
        flashLock.restart();
    }
    
    onShowAudioRecordChanged: {
        if (showAudioRecord && trackHeadRoot.height < 2 * trackHeadRoot.collapsedHeight + Math.ceil(K.UiUtils.baseSizeMedium/3)) {
            // Ensure trackheight is large enough to have the vu-meter visible
            timeline.adjustTrackHeight(trackHeadRoot.trackId, 2 * trackHeadRoot.collapsedHeight + Math.ceil(K.UiUtils.baseSizeMedium/3))
        }
    }

    //border.color: selected? 'red' : 'transparent'
    //border.width: selected? 1 : 0
    clip: true
    state: 'normal'
    states: [
        State {
            name: 'current'
            when: trackHeadRoot.current
            PropertyChanges {
                trackHeadRoot.color: showAudioRecord ? Qt.tint(getTrackColor(isAudio, true), Qt.rgba(1, 0, 0, 0.16)) : selectedTrackColor
            }
        },
        State {
            when: !trackHeadRoot.current
            name: 'normal'
            PropertyChanges {
                trackHeadRoot.color: showAudioRecord ? Qt.tint(getTrackColor(isAudio, true), Qt.rgba(1, 0, 0, 0.16)) : getTrackColor(isAudio, true)
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
        onPressed: mouse => {
            trackHeadRoot.timeline.activeTrack = trackHeadRoot.trackId
            if (mouse.button == Qt.RightButton) {
                root.showHeaderMenu()
            }
        }
        onClicked: mouse => {
            console.log('TRACK ID: ', trackHeadRoot.trackId)
            parent.forceActiveFocus()
            nameEdit.visible = false
            if (mouse.button == Qt.LeftButton) {
                trackHeadRoot.timeline.showTrackAsset(trackHeadRoot.trackId)
            }
        }
    }
    Label {
        id: trackTarget
        property color bgColor: 'grey'
        font: K.UiUtils.smallestReadableFont
        color: trackHeadRoot.timeline.targetTextColor
        background: Rectangle {
            color: trackTarget.bgColor
        }
        width: 1.5 * K.UiUtils.baseSizeMedium
        height: trackHeadRoot.height
        verticalAlignment: Text.AlignTop
        horizontalAlignment: Text.AlignHCenter
        visible: trackHeadRoot.isAudio ? trackHeadRoot.timeline.hasAudioTarget > 0 : trackHeadRoot.timeline.hasVideoTarget
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 1

        MouseArea {
            id: targetArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: mouse => {
                if (mouse.button == Qt.RightButton) {
                    if (trackHeadRoot.isAudio) {
                        root.showTargetMenu(trackHeadRoot.trackId)
                    } else {
                        root.showHeaderMenu()
                    }
                }
                else {
                    if (trackHeadRoot.isAudio) {
                        trackHeadRoot.timeline.switchAudioTarget(trackHeadRoot.trackId);
                    } else {
                        if (trackHeadRoot.trackId === trackHeadRoot.timeline.videoTarget) {
                            trackHeadRoot.timeline.videoTarget = -1;
                        } else if (trackHeadRoot.timeline.hasVideoTarget) {
                            trackHeadRoot.timeline.videoTarget = trackHeadRoot.trackId;
                        }
                    }
                }
            }
            ToolButton {
                id: targetMouse
                focusPolicy: Qt.NoFocus
                visible: trackHeadRoot.isAudio && trackHeadRoot.timeline.clipTargets > 1 && trackHeadRoot.height > (2 * expandButton.height)
                background: Rectangle {
                    color: Qt.darker(trackTarget.bgColor, 1.5)
                    border.color: activePalette.light
                }
                anchors.bottom: parent.bottom
                width: parent.width
                height: width
                icon.name: "go-down"
                onClicked: {
                    root.showTargetMenu(trackHeadRoot.trackId)
                }
                ToolTip {
                    visible: targetMouse.hovered
                    font: K.UiUtils.smallestReadableFont
                    text: K.Core.actionText("switch_target_stream")
                }
            }
        }
        ToolTip {
            visible: targetArea.containsMouse && !targetMouse.hovered
            font: K.UiUtils.smallestReadableFont
            text: KI18n.i18n("Click to toggle track as target. Target tracks will receive the inserted clips")
        }
    state:  'normalTarget'
    states: [
        State {
            name: 'target'
            when: (trackHeadRoot.isAudio && trackHeadRoot.timeline.audioTarget.indexOf(trackHeadRoot.trackId) > -1)
                  || (!trackHeadRoot.isAudio && trackHeadRoot.trackId === trackHeadRoot.timeline.videoTarget)
            PropertyChanges {
                trackTarget.bgColor: timeline.targetColor
                trackTarget.text: trackHeadRoot.isAudio ? timeline.audioTargetName(trackHeadRoot.trackId) : ''
            }
        },
        State {
            name: 'inactiveTarget'
            when: (trackHeadRoot.isAudio && trackHeadRoot.timeline.lastAudioTarget.indexOf(trackHeadRoot.trackId) > -1)
                  || (!trackHeadRoot.isAudio && trackHeadRoot.trackId == trackHeadRoot.timeline.lastVideoTarget)
            PropertyChanges {
                trackTarget.opacity: 0.3
                trackTarget.bgColor: activePalette.text
                trackTarget.text: trackHeadRoot.isAudio ? timeline.audioTargetName(trackHeadRoot.trackId) : ''
            }
        },
        State {
            name: 'noTarget'
            when: !trackHeadRoot.isLocked && !trackHeadRoot.isDisabled
            PropertyChanges {
                trackTarget.bgColor: activePalette.base
                trackTarget.text: ''
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
            property int modifier: 0
            icon.name: trackHeadRoot.collapsed ? "go-next" : "go-down"
            onClicked: {
                if (modifier & Qt.ShiftModifier) {
                    // Collapse / expand all tracks
                    trackHeadRoot.timeline.collapseAllTrackHeight(trackHeadRoot.trackId, !trackHeadRoot.collapsed, trackHeadRoot.collapsedHeight)
                } else {
                    if (trackHeadRoot.collapsed) {
                        var newHeight = Math.max(trackHeadRoot.collapsedHeight * 1.5, trackHeadRoot.controller.getTrackProperty(trackHeadRoot.trackId, "kdenlive:trackheight"))
                        trackHeadRoot.controller.setTrackProperty(trackHeadRoot.trackId, "kdenlive:trackheight", newHeight)
                        trackHeadRoot.controller.setTrackProperty(trackHeadRoot.trackId, "kdenlive:collapsed", "0")
                    } else {
                        trackHeadRoot.controller.setTrackProperty(trackHeadRoot.trackId, "kdenlive:collapsed", trackHeadRoot.collapsedHeight)
                    }
                }
                if (trackHeadRoot.timeline.autotrackHeight) {
                    trackHeadRoot.timeline.autofitTrackHeight(scrollView.height - subtitleTrack.height, trackHeadRoot.collapsedHeight)
                }
            }
            MouseArea {
                // Used to pass modifier state to expand button
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true
                onPressed: mouse => {
                    expandButton.modifier = mouse.modifiers
                    mouse.accepted = false
                }
                onEntered: {
                    trackHeadRoot.timeline.showKeyBinding(KI18n.i18n("<b>Shift</b> to collapse/expand all tracks of the same type (audio/video)"))
                }
                onExited: {
                    trackHeadRoot.timeline.showKeyBinding()
                }
            }
            anchors.left: parent.left
            width: trackHeadRoot.collapsedHeight
            height: trackHeadRoot.collapsedHeight
            ToolTip {
                visible: expandButton.hovered
                font: K.UiUtils.smallestReadableFont
                text: trackLabel.visible? KI18n.i18n("Minimize") : KI18n.i18n("Expand")
            }
        }
        Label {
            id: trackLed
            property color bgColor: Qt.darker(trackHeadRoot.color, 0.55)
            anchors.left: expandButton.right
            font: K.UiUtils.smallestReadableFont
            text: trackHeadRoot.trackTag
            color: activePalette.text
            background: Rectangle {
                color: trackLed.bgColor
            }
            width: root.trackTagWidth
            height: trackHeadRoot.collapsedHeight - 2
            y: 1
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            MouseArea {
                id: tagMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    trackHeadRoot.timeline.switchTrackActive(trackHeadRoot.trackId)
                }
            }
            ToolTip {
                visible: tagMouseArea.containsMouse
                font: K.UiUtils.smallestReadableFont
                text: (trackHeadRoot.trackName.length == 0 || miniTrackLabel.visible || trackLabel.visible) ? KI18n.i18n("Click to make track active/inactive. Active tracks will react to editing operations") : trackHeadRoot.trackName
            }
        state:  'normalled'
            states: [
                State {
                    name: 'locked'
                    when: trackHeadRoot.isLocked
                    PropertyChanges {
                        trackLed.bgColor: 'red'
                    }
                },
                State {
                    name: 'active'
                    when: trackHeadRoot.isActive
                    PropertyChanges {
                        trackLed.bgColor: timeline.targetColor
                        trackLed.color: timeline.targetTextColor
                    }
                },
                State {
                    name: 'inactive'
                    when: !trackHeadRoot.isLocked && !trackHeadRoot.isActive
                    PropertyChanges {
                        trackLed.bgColor: Qt.darker(trackHeadRoot.color, 0.55)
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
            // Debug: trackId
            anchors.left: trackLed.right
            background: Rectangle {
                color: 'magenta'
            }
            anchors.leftMargin: 2
            width: root.trackTagWidth * 2
            height: trackHeadRoot.collapsedHeight - 2
            y: 1
            text: trackHeadRoot.trackId
            elide: Text.ElideRight
            font: K.UiUtils.smallestReadableFont
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            visible: K.KdenliveSettings.uiDebugMode
        }
        Label {
            id: miniTrackLabel
            anchors.left: trackLed.right
            anchors.top: parent.top
            anchors.leftMargin: 2
            height: trackLed.height
            width: buttonsRow.x - x
            text: trackHeadRoot.trackName
            elide: Text.ElideRight
            font: K.UiUtils.smallestReadableFont
            color: activePalette.text
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
            visible: !trackLabel.visible && trackHeadRoot.width > (trackTarget.width + expandButton.width + trackLed.width + (4 * muteButton.width) + 4)
        }
        Row {
            id: buttonsRow
            width: childrenRect.width
            x: Math.max(2 * trackHeadRoot.collapsedHeight + 2, parent.width - width - 4)
            spacing: 0
            ToolButton {
                id: effectButton
                focusPolicy: Qt.NoFocus
                icon.name: "tools-wizard"
                enabled: trackHeadRoot.effectNames != ''
                checkable: true
                checked: enabled && trackHeadRoot.isStackEnabled
                onClicked: {
                    trackHeadRoot.timeline.showTrackAsset(trackHeadRoot.trackId)
                    trackHeadRoot.controller.setTrackStackEnabled(trackHeadRoot.trackId, !trackHeadRoot.isStackEnabled)
                }
                width: trackHeadRoot.collapsedHeight
                height: trackHeadRoot.collapsedHeight
                ToolTip {
                    visible: effectButton.hovered
                    font: K.UiUtils.smallestReadableFont
                    text: enabled ? (trackHeadRoot.isStackEnabled ? KI18n.i18n("Disable track effects"): KI18n.i18n("Enable track effects"))
                                  : KI18n.i18n("Toggle track effects");
                }
            }
            ToolButton {
                id: muteButton
                property int modifier: 0
                focusPolicy: Qt.NoFocus
                icon.name: trackHeadRoot.isAudio ? (trackHeadRoot.isDisabled ? "audio-off" : "audio-volume-high")
                                   : (trackHeadRoot.isDisabled ? "kdenlive-hide-video" : "kdenlive-show-video")
                width: trackHeadRoot.collapsedHeight
                height: trackHeadRoot.collapsedHeight
                onClicked: trackHeadRoot.timeline.hideTrack(trackHeadRoot.trackId, trackHeadRoot.isDisabled, modifier & Qt.ShiftModifier)
                MouseArea {
                    // Used to pass modifier state to expand button
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: true
                    onPressed: mouse => {
                        muteButton.modifier = mouse.modifiers
                        mouse.accepted = false
                    }
                    onEntered: {
                        trackHeadRoot.timeline.showKeyBinding(KI18n.i18n("<b>Shift</b> to show/hide all tracks of the same type (audio/video)"))
                    }
                    onExited: {
                        trackHeadRoot.timeline.showKeyBinding()
                    }
                }
                ToolTip {
                    visible: muteButton.hovered
                    font: K.UiUtils.smallestReadableFont
                    text: trackHeadRoot.isAudio ? (trackHeadRoot.isDisabled? KI18n.i18n("Unmute") : KI18n.i18n("Mute"))
                                  : (trackHeadRoot.isDisabled? KI18n.i18n("Show") : KI18n.i18n("Hide"))
                }
            }

            ToolButton {
                id: lockButton
                width: trackHeadRoot.collapsedHeight
                height: trackHeadRoot.collapsedHeight
                focusPolicy: Qt.NoFocus
                icon.name: trackHeadRoot.isLocked ? "lock" : "unlock"
                onClicked: trackHeadRoot.controller.setTrackLockedState(trackHeadRoot.trackId, !trackHeadRoot.isLocked)
                ToolTip {
                    visible: lockButton.hovered
                    font: K.UiUtils.smallestReadableFont
                    text: trackHeadRoot.isLocked ? KI18n.i18n("Unlock track") : KI18n.i18n("Lock track")
                }
                SequentialAnimation {
                    id: flashLock
                    loops: 3
                    ParallelAnimation {
                        ScaleAnimator {target: lockButton; from: 1; to: 1.2; duration: 120}
                    }
                    ParallelAnimation {
                        ScaleAnimator {target: lockButton; from: 1.6; to: 1; duration: 120}
                    }
                }
            }
        }
        Item {
            id: recLayout
            y: trackHeadRoot.collapsedHeight + 4
            anchors.left: trackHeadColumn.left
            anchors.right: trackHeadColumn.right
            anchors.margins: 2
            height: trackHeadRoot.showAudioRecord ? trackHeadRoot.collapsedHeight - 4 : 0
            Loader {
                id: audioVuMeter
                asynchronous: true 
                anchors.fill: parent
                visible: trackHeadRoot.showAudioRecord && (trackHeadRoot.height >= 2 * trackHeadRoot.collapsedHeight + Math.ceil(K.UiUtils.baseSizeMedium/3))
                active: trackHeadRoot.isAudio && trackHeadRoot.showAudioRecord
                sourceComponent: AudioRecordingControls {
                    trackId: trackHeadRoot.trackId
                    timeline: trackHeadRoot.timeline
                    audiorec: root.audiorec
                }
            }
        }
        Item {
            id: nameEditContainer
            anchors.bottom: trackHeadColumn.bottom
            anchors.left: trackHeadColumn.left
            anchors.right: trackHeadColumn.right
            anchors.margins: 2
            anchors.rightMargin: 2
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
                    onDoubleClicked: editName()
                    onClicked: {
                        trackHeadRoot.timeline.showTrackAsset(trackHeadRoot.trackId)
                        trackHeadRoot.timeline.activeTrack = trackHeadRoot.trackId
                        trackHeadRoot.focus = true
                    }
                }
                Label {
                    text: trackHeadRoot.trackName
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                    font: K.UiUtils.smallestReadableFont
                    color: activePalette.text
                }
                Label {
                    id: placeHolder
                    visible: trackHeadRoot.trackName == '' && (trackNameMouseArea.containsMouse || headerMouseArea.containsMouse)
                    enabled: false
                    text: KI18n.i18n("Edit track name")
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                    font: K.UiUtils.smallestReadableFont
                    color: activePalette.text
                }
                TextField {
                    id: nameEdit
                    visible: false
                    width: parent.width
                    text: trackHeadRoot.trackName
                    font: K.UiUtils.smallestReadableFont
                    color: activePalette.text
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
                        trackHeadRoot.controller.setTrackName(trackHeadRoot.trackId, text)
                        tracksArea.focus = true
                        visible = false
                    }
                }
            }
        }
    }
    Rectangle {
            id: resizer
            height: Math.round(K.UiUtils.baseSizeMedium/3)
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
                drag.minimumY: trackHeadRoot.collapsedHeight - resizer.height
                property double startY
                property double originalY
                drag.smoothed: false
                property bool dragStarted: false

                onPressed: {
                    root.blockAutoScroll = true
                    dragStarted = false
                    startY = mapToItem(null, x, y).y
                    originalY = trackHeadRoot.height // reusing originalX to accumulate delta for bubble help
                }
                onReleased: mouse => {
                    root.blockAutoScroll = false
                    if (!trimInMouseArea.containsMouse) {
                        parent.opacity = 0
                    }
                    if (mouse.modifiers & Qt.ShiftModifier && dragStarted) {
                        trackHeadRoot.timeline.adjustAllTrackHeight(trackHeadRoot.trackId, trackHeadRoot.height)
                    }
                }
                onEntered: parent.opacity = 0.3
                onExited: parent.opacity = 0
                onDoubleClicked: mouse => {
                    trackHeadRoot.timeline.defaultTrackHeight(mouse.modifiers & Qt.ShiftModifier ? -1 : trackHeadRoot.trackId)
                }
                onPositionChanged: mouse=> {
                    if (mouse.buttons === Qt.LeftButton) {
                        parent.opacity = 0.5
                        if (!dragStarted && Math.abs(mapToItem(null, x, y).y - startY) > 2) {
                            dragStarted = true
                        }
                        var newHeight = Math.round(originalY + (mapToItem(null, x, y).y - startY))
                        newHeight =  Math.max(trackHeadRoot.collapsedHeight, newHeight)
                        if (newHeight == trackHeadRoot.collapsedHeight) {
                            trackHeadRoot.controller.setTrackProperty(trackHeadRoot.trackId, "kdenlive:collapsed", trackHeadRoot.collapsedHeight)
                        } else {
                            trackHeadRoot.controller.setTrackProperty(trackHeadRoot.trackId, "kdenlive:trackheight", newHeight)
                            trackHeadRoot.controller.setTrackProperty(trackHeadRoot.trackId, "kdenlive:collapsed", "0")
                        }
                    }
                }
            }
        }
    DropArea { //Drop area for tracks
        anchors.fill: trackHeadRoot
        anchors.bottomMargin: resizer.height
        keys: ['kdenlive/effect']
        property string dropData
        property string dropSource
        onEntered: drag => {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
            drag.acceptProposedAction()
        }
        onDropped: drag => {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                trackHeadRoot.controller.addTrackEffect(trackHeadRoot.trackId, dropData);
            } else {
                trackHeadRoot.controller.copyTrackEffect(trackHeadRoot.trackId, dropSource);
            }
            drag.acceptProposedAction()
        }
    }
}

