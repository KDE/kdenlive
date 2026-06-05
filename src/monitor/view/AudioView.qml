/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick.Controls
import QtQuick

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: audioThumb
    required property K.MonitorProxy monitorController
    property bool isAudioClip: false
    property bool stateVisible: false
    property int audioZoomHeightRef: isAudioClip ? height / 5 : height / 3.5
    property bool displayAudioZoom: true
    property bool dragButtonsVisible: false
    property bool dirty: false
    property bool containsMyMouse: thumbMouseArea.containsMouse || audioZoom.containsMouse || clipMonitorRuler.containsMouse || thumbMouseArea.pressed
    property int clipId: monitorController.clipId
    property bool alwaysShowMonitorAudio: K.KdenliveSettings.alwaysShowMonitorAudio
    state: stateVisible ? "showAudio" : "hideAudio"
    onDragButtonsVisibleChanged: {
        if (!K.KdenliveSettings.alwaysShowMonitorAudio) {
            if (dragButtonsVisible) {
                zoomCollapseTimer.stop()
                audioThumb.state = "showAudio"
            } else if (monitorController.clipHasAV) {
                zoomCollapseTimer.start()
            }
        }
    }
    SystemPalette { id: activePalette }

    Timer {
        id: zoomCollapseTimer
        interval: 1000; running: false; repeat: false
        onTriggered: {
            audioThumb.state = "animatedHide"
        }
    }

    onAlwaysShowMonitorAudioChanged:
    {
        refreshView()
    }

    onClipIdChanged:
    {
        refreshView()
    }

    function refreshView()
    {
        audioThumb.isAudioClip = monitorController.clipType === K.ClipType.Audio
        audioThumb.stateVisible = (K.KdenliveSettings.alwaysShowMonitorAudio && monitorController.clipHasAV) || audioThumb.isAudioClip
        checkAudioThumbState()
    }

    onContainsMyMouseChanged: {
        if (containsMyMouse) {
            monitorController.dragType = '-'
        }
        if (!K.KdenliveSettings.alwaysShowMonitorAudio) {
            if (containsMyMouse) {
                zoomCollapseTimer.stop()
                state = "showAudio"
            } else if (monitorController.clipHasAV) {
                zoomCollapseTimer.start()
            }
        }
    }

    function checkAudioThumbState()
    {
        if (!K.KdenliveSettings.alwaysShowMonitorAudio) {
            zoomCollapseTimer.stop()
            monitorController.rulerHeight = root.zoomOffset
            if (audioThumb.stateVisible) {
                audioThumb.state = "showAudio"
            } else {
                audioThumb.state = "hideAudio"
            }
        } else {
            // adjust monitor image size
            if (audioThumb.stateVisible) {
                monitorController.rulerHeight = audioThumb.height + root.zoomOffset
            } else {
                monitorController.rulerHeight = root.zoomOffset
            }
            audioThumb.state = "showAudio"
        }
    }

    Label {
        id: clipStreamLabel
        font: K.UiUtils.fixedFont
        anchors {
            bottom: audioThumb.isAudioClip ? parent.bottom : parent.top
            horizontalCenter: parent.horizontalCenter
        }
        color: "white"
        text: audioThumb.monitorController.clipStream
        background: Rectangle {
            color: "#222277"
        }
        visible: text != ""
        padding: 4
    }

    states: [
        State {
            name: "showAudio"
            PropertyChanges {
                audioThumb.opacity: 1.0
            }
        },
        State {
            name: "animatedHide"
            PropertyChanges {
                audioThumb.opacity: 0.0
            }
        },
        State {
            name: "hideAudio"
            PropertyChanges {
                audioThumb.opacity: 0.0
            }
        }
    ]

    transitions: [Transition {
        from: "showAudio"; to: "animatedHide";
        NumberAnimation {
            property: "opacity";
            duration: audioThumb.isAudioClip ? 0 : 500
        }
    }]
    K.AudioZoomBar {
        id: audioZoom
        monitorController: audioThumb.monitorController
        visible: audioThumb.isAudioClip || audioThumb.monitorController.clipHasAV
        opacity: audioThumb.dirty ? 0.5 : 1
        anchors.top: parent.top
        height: audioThumb.audioZoomHeightRef
    }
    Item {
        id: mainThumbsContainer
        anchors.fill: parent
        anchors.topMargin: audioZoom.height
        opacity: audioThumb.dirty ? 0.5 : 1
        Rectangle {
            // Audio monitor background
            id: audioBg
            color: Utils.mixColors(activePalette.base, K.KdenliveSettings.thumbColor1, 0.3)
            opacity: audioThumb.isAudioClip || K.KdenliveSettings.alwaysShowMonitorAudio ? 1 : 0.6
            anchors.fill: parent
        }
        Repeater {
            id: streamThumb
            model: audioThumb.monitorController.audioStreams.length
            onCountChanged: {
                thumbTimer.start()
            }
            property double streamHeight: mainThumbsContainer.height / streamThumb.count
            Item {
                id: streamContainer
                required property int index
                anchors.left: parent.left
                anchors.right: parent.right
                property int channelsInStream: audioThumb.monitorController.audioChannels[streamContainer.index]
                property double channelHeight: streamThumb.streamHeight / channelsInStream
                y: streamContainer.index * streamThumb.streamHeight
                height: streamThumb.streamHeight
                // Normal color for the audio wave // top channel
                Repeater {
                    model: streamContainer.channelsInStream
                    Rectangle {
                        id: channelBackground
                        required property int index
                        y: streamContainer.channelHeight * channelBackground.index
                        height: streamContainer.channelHeight - 2
                        anchors.right: parent.right
                        anchors.left: parent.left
                        color: index %2 == 0 ? K.KdenliveSettings.thumbColor1 : K.KdenliveSettings.thumbColor2
                    }
                }
                // Highlight color for the selected wave part
                Rectangle {
                    x: audioThumb.monitorController.zoneIn * timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
                    width: (audioThumb.monitorController.zoneOut - audioThumb.monitorController.zoneIn) * timeScale
                    height: streamThumb.streamHeight - 2
                    color:  Utils.mixColors(K.KdenliveSettings.thumbColor1, activePalette.highlight, 0.6)
                    visible: audioThumb.monitorController.zoneOut > audioThumb.monitorController.zoneIn
                }
                K.TimelineWaveform {
                    id: waveform
                    anchors.right: parent.right
                    anchors.left: parent.left
                    isOpaque: false
                    height: streamThumb.streamHeight
                    channels: streamContainer.channelsInStream
                    binId: audioThumb.monitorController.clipId
                    audioStream: audioThumb.monitorController.audioStreams[streamContainer.index]
                    format: K.KdenliveSettings.displayallchannels
                    normalize: K.KdenliveSettings.normalizechannels
                    property int aClipDuration: root.duration + 1
                    scaleFactor: audioThumb.width / aClipDuration / root.zoomFactor
                    waveInPoint: waveform.aClipDuration * root.zoomStart
                    waveOutPoint: waveform.aClipDuration * (root.zoomStart + root.zoomFactor)
                    fgColorEven: "#00000000" //K.KdenliveSettings.thumbColor1
                    fgColorOdd: "#00000000" //K.KdenliveSettings.thumbColor2
                    bgColorEven: audioBg.color //"#00000000"
                    bgColorOdd: audioBg.color //"#00000000"
                    Repeater {
                        id: centerLinesContainer
                        model: streamContainer.channelsInStream
                        Item {
                            id: channelCenterLines
                            required property int index
                            anchors.fill: parent
                            // Channel center line
                            Rectangle {
                                width: parent.width
                                y: K.KdenliveSettings.displayallchannels ? channelCenterLines.index * streamContainer.channelHeight + streamContainer.channelHeight / 2 - 0.5 : streamContainer.height / 2 - 0.5
                                height: 1
                                opacity: 0.4
                                color: audioBg.color
                            }
                            // Channel separator
                            Rectangle {
                                width: parent.width
                                height: 1
                                y: channelCenterLines.index * streamContainer.channelHeight - 0.5
                                visible: channelCenterLines.index > 0 && K.KdenliveSettings.displayallchannels
                                color: activePalette.base
                            }
                        }
                    }
                }
                // Separator line between streams
                Rectangle {
                    width: parent.width
                    y: waveform.y - 1
                    height: 2
                    visible: streamContainer.index > 0 || !audioThumb.isAudioClip
                    color: activePalette.base
                }
            }
        }
        // Playhead position
        Rectangle {
            color: "red"
            width: 2
            height: streamThumb.streamHeight * streamThumb.count
            x: audioThumb.monitorController.position * timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
        }
    }
    MouseArea {
        id: thumbMouseArea
        anchors.fill: parent
        anchors.topMargin: audioZoom.height
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        propagateComposedEvents: true
        onEntered: {
            // Show clip name
            /*if (!pressed && labelContainer.opacity == 0) {
                labelContainer.opacity = 1
                contextMenu.opacity = 1
                if (!clipNameLabel.hovered) {
                    showAnimate.restart()
                }
            }*/
        }
        onPressed: mouse => {
            if (audioThumb.isAudioClip && mouseY < audioZoom.height) {
                mouse.accepted = false
                return
            }
            root.seeking = true
            var pos = Math.max(mouseX, 0)
            root.mouseRulerPos = mouseX
            pos += audioThumb.width / root.zoomFactor * root.zoomStart
            audioThumb.monitorController.setPosition(Math.min(pos / root.timeScale, root.duration));
        }
        onPositionChanged: mouse => {
            if (!(mouse.modifiers & Qt.ShiftModifier) && audioThumb.isAudioClip && mouseY < audioZoom.height) {
                mouse.accepted = false
                return
            }
            if (mouse.modifiers & Qt.ShiftModifier || pressed) {
                var pos = Math.max(mouseX, 0)
                root.mouseRulerPos = mouseX
                pos += audioThumb.width / root.zoomFactor * root.zoomStart
                audioThumb.monitorController.setPosition(Math.min(pos / root.timeScale, root.duration));
            }
        }
        onReleased: {
            root.seeking = false
        }

        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                if (wheel.angleDelta.y == 0) {
                    // Don't trigger zoom if delta is null
                    return
                }
                if (wheel.angleDelta.y < 0) {
                    // zoom out
                    clipMonitorRuler.zoomOutRuler(wheel.x)
                } else {
                    // zoom in
                    clipMonitorRuler.zoomInRuler(wheel.x)
                }
            } else {
                wheel.accepted = false
            }

        }
    }
    Rectangle {
        visible: audioThumb.dirty
        width: K.UiUtils.baseSizeMedium * 3
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 6
        anchors.bottomMargin: 6
        height: width
        radius: 4
        color: audioRefresh.hovered ? activePalette.highlight : Qt.rgba(activePalette.base.r, activePalette.base.g, activePalette.base.b, 0.5)
        K.MonitorToolButton {
            id: audioRefresh
            hoverEnabled: true
            iconName: "view-refresh"
            toolTipText: KI18n.i18n("Click to refresh audio waveform")
            onClicked: audioThumb.monitorController.refreshAudio()
        }
    }
}
