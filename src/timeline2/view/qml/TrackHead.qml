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

import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.2
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

Rectangle {
    id: trackHeadRoot
    property string trackName
    property bool isMute
    property bool collapsed: false
    property bool isHidden
    property int isComposite
    property bool isLocked
    property bool isAudio
    property bool selected: false
    property bool current: false
    property int myTrackHeight
    property int trackId : -42
    property int collapsedHeight: nameEdit.height + 2
    border.width: 1
    border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.1)
    signal clicked()

    function pulseLockButton() {
        flashLock.restart();
    }

    SystemPalette { id: activePalette }
    color: selected ? selectedTrackColor : getTrackColor(isAudio, true)
    //border.color: selected? 'red' : 'transparent'
    //border.width: selected? 1 : 0
    clip: true
    state: 'normal'
    states: [
        State {
            name: 'selected'
            when: trackHeadRoot.selected
            PropertyChanges {
                target: trackHeadRoot
                //color: isAudio? root.shotcutBlue : 'darkseagreen'
            }
        },
        State {
            name: 'current'
            when: trackHeadRoot.current
            PropertyChanges {
                target: trackHeadRoot
                color: selectedTrackColor
            }
        },
        State {
            when: !trackHeadRoot.selected && !trackHeadRoot.current
            name: 'normal'
            PropertyChanges {
                target: trackHeadRoot
                color: getTrackColor(isAudio, true)
            }
        }
    ]
    transitions: [
        Transition {
            to: '*'
            ColorAnimation { target: trackHeadRoot; duration: 150 }
        }
    ]

    Keys.onDownPressed: {
        root.moveSelectedTrack(1)
    }
    Keys.onUpPressed: {
        root.moveSelectedTrack(-1)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: {
            parent.clicked()
            parent.forceActiveFocus()
            nameEdit.visible = false
            if (mouse.button == Qt.LeftButton) {
                timeline.showTrackAsset(trackId)
            }
            else if (mouse.button == Qt.RightButton) {
                headerMenu.popup()
            }
        }
    }
    ColumnLayout {
        id: trackHeadColumn
        spacing: 0
        anchors.fill: parent
        anchors.leftMargin: 0
        anchors.topMargin: 0

        RowLayout {
            spacing: 0
            Layout.leftMargin: 2
            ToolButton {
                id: expandButton
                iconName: trackHeadRoot.collapsed ? 'arrow-right' : 'arrow-down'
                onClicked: {
                    trackHeadRoot.myTrackHeight = trackHeadRoot.collapsed ? Math.max(collapsedHeight * 1.5, controller.getTrackProperty(trackId, "kdenlive:trackheight")) : collapsedHeight
                }
                tooltip: buttonBar.visible? i18n('Minimize') : i18n('Expand')
            }
            Item {
                width: nameEdit.height / 3
                height: width
                Rectangle {
                    id: trackLed
                    color: 'grey'
                    anchors.fill: parent
                    width: height
                    radius: height
                    border.width: 1
                    MouseArea {
                        anchors.fill: parent
                        onDoubleClicked: {
                            if (trackHeadRoot.isAudio) {
                                if (trackHeadRoot.trackId == timeline.audioTarget) {
                                    timeline.audioTarget = -1;
                                } else {
                                    timeline.audioTarget = trackHeadRoot.trackId;
                                }
                            } else {
                                if (trackHeadRoot.trackId == timeline.videoTarget) {
                                    timeline.videoTarget = -1;
                                } else {
                                    timeline.videoTarget = trackHeadRoot.trackId;
                                }
                            }
                        }
                    }
                    state:  'normalled'
                    states: [
                        State {
                            name: 'locked'
                            when: trackHeadRoot.isLocked
                            PropertyChanges {
                                target: trackLed
                                color: 'red'
                            }
                        },
                        State {
                            name: 'target'
                            when: (trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.audioTarget) || (!trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.videoTarget)
                            PropertyChanges {
                                target: trackLed
                                color: 'yellow'
                            }
                        },
                        State {
                            name: 'mute'
                            when: trackHeadRoot.isMute || trackHeadRoot.isHidden
                            PropertyChanges {
                                target: trackLed
                                color: 'orange'
                            }
                        },
                        State {
                            name: 'normalled'
                            when: !trackHeadRoot.isLocked && !trackHeadRoot.isMute && !trackHeadRoot.isHidden
                            PropertyChanges {
                                target: trackLed
                                color: trackHeadRoot.selected ? 'green' : 'grey'
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
                InnerShadow {
                    // Led shadow
                    anchors.fill: trackLed
                    cached: false
                    horizontalOffset: -2
                    verticalOffset: -2
                    radius: trackLed.height
                    color: Qt.darker(trackLed.color)
                    fast: true
                    source: trackLed
                }
            }
            Rectangle {
                id: trackLabel
                color: 'transparent'
                Layout.fillWidth: true
                radius: 2
                border.color: trackNameMouseArea.containsMouse ? activePalette.highlight : 'transparent'
                height: nameEdit.height
                MouseArea {
                    id: trackNameMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onDoubleClicked: {
                        nameEdit.visible = true
                        nameEdit.focus = true
                        nameEdit.selectAll()
                    }
                    onClicked: {
                        mouse.accepted = false
                    }
                }
                Label {
                    text: trackName
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                }
                TextField {
                    id: nameEdit
                    visible: false
                    width: parent.width
                    text: trackName
                    onEditingFinished: {
                        controller.setTrackProperty(trackId, "kdenlive:track_name", text)
                        visible = false
                    }
                }
            }
        }
        RowLayout {
            spacing: root.baseUnit / 2
            id: buttonBar
            visible: (trackHeadRoot.height >= trackLabel.height + muteButton.height + resizer.height)
            Layout.leftMargin: root.baseUnit
            ToolButton {
                id: muteButton
                implicitHeight: root.baseUnit * 2
                implicitWidth: root.baseUnit * 2
                iconName: isMute ? 'kdenlive-hide-audio' : 'kdenlive-show-audio'
                iconSource: isMute ? 'qrc:///pics/kdenlive-hide-audio.svgz' : 'qrc:///pics/kdenlive-show-audio.svgz'
                onClicked: controller.setTrackProperty(trackId, "hide", isMute ? isHidden ? '1' : '0' : isHidden ? '3' : '2')
                tooltip: isMute? i18n('Unmute') : i18n('Mute')
            }

            ToolButton {
                id: hideButton
                implicitHeight: root.baseUnit * 2
                implicitWidth: root.baseUnit * 2
                visible: !isAudio
                iconName: isHidden ? 'kdenlive-hide-video' : 'kdenlive-show-video'
                iconSource: isHidden? 'qrc:///pics/kdenlive-hide-video.svgz' : 'qrc:///pics/kdenlive-show-video.svgz'
                onClicked: {
                    controller.setTrackProperty(trackId, "hide", isHidden ? isMute ? '2' : '0' : isMute ? '3' : '1')
                    timeline.requestRefresh()
                }
                tooltip: isHidden? i18n('Show') : i18n('Hide')
            }

            ToolButton {
                id: lockButton
                implicitHeight: root.baseUnit * 2
                implicitWidth: root.baseUnit * 2
                iconName: isLocked ? 'kdenlive-lock' : 'kdenlive-unlock'
                iconSource: isLocked ? 'qrc:///pics/kdenlive-lock.svg' : 'qrc:///pics/kdenlive-unlock.svg'
                onClicked: controller.setTrackProperty(trackId, "kdenlive:locked_track", isLocked ? '0' : '1')
                tooltip: isLocked? i18n('Unlock track') : i18n('Lock track')

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
        Rectangle {
            // Spacer
            color: "transparent"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        Rectangle {
            id: resizer
            Layout.fillWidth: true
            height: 4
            color: 'red'
            opacity: 0
            Drag.active: trimInMouseArea.drag.active
            Drag.proposedAction: Qt.MoveAction

            MouseArea {
                id: trimInMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor
                drag.target: parent
                drag.axis: Drag.YAxis
                property double startY
                property double originalY
                drag.smoothed: false

                onPressed: {
                    root.stopScrolling = true
                    startY = mapToItem(null, x, y).y
                    originalY = trackHeadRoot.height // reusing originalX to accumulate delta for bubble help
                }
                onReleased: {
                    root.stopScrolling = false
                    parent.opacity = 0
                }
                onEntered: parent.opacity = 0.5
                onExited: parent.opacity = 0
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        parent.opacity = 0.5
                        var newHeight = originalY + (mapToItem(null, x, y).y - startY)
                        newHeight =  Math.max(collapsedHeight, newHeight)
                        trackHeadRoot.myTrackHeight = newHeight
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

