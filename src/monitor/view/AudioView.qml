/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls
import QtQuick

import org.kde.kdenlive as K

Item {
    id: audioThumb
    property bool isAudioClip: false
    property bool stateVisible: false
    property int audioZoomHeightRef: isAudioClip ? height / 5 : height / 3.5
    property bool displayAudioZoom: true
    property bool containsMouse: thumbMouseArea.containsMouse || audioZoom.containsMouse || clipMonitorRuler.containsMouse || thumbMouseArea.pressed
    property int clipId: controller.clipId
    state: stateVisible ? "showAudio" : "hideAudio"

    Timer {
        id: zoomCollapseTimer
        interval: 1000; running: false; repeat: false
        onTriggered: {
            state = "animatedHide"
        }
    }

    onClipIdChanged:
    {
        audioThumb.isAudioClip = controller.clipType === K.ClipType.Audio
        audioThumb.stateVisible = (K.KdenliveSettings.alwaysShowMonitorAudio && controller.clipHasAV) || audioThumb.isAudioClip
        checkAudioThumbState()
    }

    onContainsMouseChanged: {
        if (!K.KdenliveSettings.alwaysShowMonitorAudio) {
            if (containsMouse) {
               zoomCollapseTimer.stop()
                state = "showAudio"
            } else if (controller.clipHasAV) {
                zoomCollapseTimer.start()
            }
        }
    }

    function checkAudioThumbState()
    {
        if (!K.KdenliveSettings.alwaysShowMonitorAudio) {
            zoomCollapseTimer.stop()
            if (audioThumb.stateVisible) {
                state = "showAudio"
            } else {
                state = "hideAudio"
            }
        } else {
            // adjust monitor image size
            if (audioThumb.stateVisible) {
                controller.rulerHeight = audioThumb.height + root.zoomOffset
            } else {
                controller.rulerHeight = root.zoomOffset
            }
            state = "showAudio"
        }
    }

    Label {
        id: clipStreamLabel
        font: fixedFont
        anchors {
            bottom: audioThumb.isAudioClip ? parent.bottom : parent.top
            horizontalCenter: parent.horizontalCenter
        }
        color: "white"
        text: controller.clipStream
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
        visible: audioThumb.isAudioClip || controller.clipHasAV
        anchors.top: parent.top
        height: audioThumb.audioZoomHeightRef
    }
    Item {
        id: mainThumbsContainer
        anchors.fill: parent
        anchors.topMargin: audioZoom.height
        Rectangle {
            // Audio monitor background
            id: audioBg
            color: Utils.mixColors(activePalette.base, K.KdenliveSettings.thumbColor1, 0.3)
            opacity: audioThumb.isAudioClip || K.KdenliveSettings.alwaysShowMonitorAudio ? 1 : 0.6
            anchors.fill: parent
        }
        Repeater {
            id: streamThumb
            model: controller.audioStreams.length
            onCountChanged: {
                thumbTimer.start()
            }
            property double streamHeight: mainThumbsContainer.height / streamThumb.count
            Item {
                id: streamContainer
                anchors.left: parent.left
                anchors.right: parent.right
                property int channelsInStream: controller.audioChannels[model.index]
                property double channelHeight: streamThumb.streamHeight / channelsInStream
                y: model.index * streamThumb.streamHeight
                height: streamThumb.streamHeight
                // Normal color for the audio wave // top channel
                Repeater {
                    model: channelsInStream
                    Rectangle {
                        y: channelHeight * model.index
                        height: channelHeight - 2
                        anchors.right: parent.right
                        anchors.left: parent.left
                        color: model.index %2 == 0 ? K.KdenliveSettings.thumbColor1 : K.KdenliveSettings.thumbColor2
                    }
                }
                // Highlight color for the selected wave part
                Rectangle {
                    x: controller.zoneIn * timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
                    width: (controller.zoneOut - controller.zoneIn) * timeScale
                    height: streamThumb.streamHeight - 2
                    color:  Utils.mixColors(K.KdenliveSettings.thumbColor1, activePalette.highlight, 0.6)
                    visible: controller.zoneOut > controller.zoneIn
                }
                K.TimelineWaveform {
                    id: waveform
                    anchors.right: parent.right
                    anchors.left: parent.left
                    isOpaque: false
                    height: streamThumb.streamHeight
                    channels: streamContainer.channelsInStream
                    binId: controller.clipId
                    audioStream: controller.audioStreams[model.index]
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
                            anchors.fill: parent
                            // Channel center line
                            Rectangle {
                                width: parent.width
                                y: K.KdenliveSettings.displayallchannels ? model.index * streamContainer.channelHeight + streamContainer.channelHeight / 2 - 0.5 : streamContainer.height / 2 - 0.5
                                height: 1
                                opacity: 0.4
                                color: audioBg.color
                            }
                            // Channel separator
                            Rectangle {
                                width: parent.width
                                height: 1
                                y: model.index * streamContainer.channelHeight - 0.5
                                visible: model.index > 0 && K.KdenliveSettings.displayallchannels
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
                    visible: model.index > 0 || !audioThumb.isAudioClip
                    color: activePalette.base
                }
            }
        }
        // Playhead position
        Rectangle {
            color: "red"
            width: 2
            height: streamThumb.streamHeight * streamThumb.count
            x: controller.position * timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
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
            controller.setPosition(Math.min(pos / root.timeScale, root.duration));
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
                controller.setPosition(Math.min(pos / root.timeScale, root.duration));
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
}
