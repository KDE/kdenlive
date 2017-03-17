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

import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.2
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
//import Shotcut.Controls 1.0 as Shotcut

Rectangle {
    id: trackHeadRoot
    property string trackName
    property bool isMute
    property bool isHidden
    property int isComposite
    property bool isLocked
    property bool isAudio
    property bool selected: false
    property bool current: false
    property int myTrackHeight
    property int trackId : -42
    border.width: 1
    border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.1)
    signal clicked()

    function pulseLockButton() {
        lockButtonAnim.restart();
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


    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: {
            parent.clicked()
            parent.forceActiveFocus()
            nameEdit.visible = false
            if (mouse.button == Qt.RightButton)
                headerMenu.popup()
        }
    }
    ColumnLayout {
        id: trackHeadColumn
        spacing: 0
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.topMargin: 2

        RowLayout {
            spacing: 0
            Layout.leftMargin: 2
            ToolButton {
                id: expandButton
                iconName: buttonBar.visible ? 'arrow-down' : 'arrow-right'
                onClicked: {
                    trackHeadRoot.myTrackHeight = buttonBar.visible ? nameEdit.height : '100'
                }
                tooltip: buttonBar.visible? i18n('Minimize') : i18n('Expand')
            }
            Rectangle {
                id: trackLabel
                color: 'transparent'
                Layout.fillWidth: true
                radius: 2
                border.color: trackNameMouseArea.containsMouse? (trackHeadRoot.current? activePalette.shadow : activePalette.highlight) : 'transparent'
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
            spacing: 0
            id: buttonBar
            visible: (trackHeadRoot.height > trackLabel.height + muteButton.height + resizer.height + 4)
            Layout.leftMargin: 2
            Rectangle {
                id: trackLed
                color: 'grey'
                width: 14
                height: 14
                radius: 14
                border.width: 1
                InnerShadow {
                    anchors.fill: parent
                    cached: false
                    horizontalOffset: -1
                    verticalOffset: -1
                    radius: 12
                    samples: 10
                    color: Qt.darker(parent.color)
                    smooth: true
                    source: parent
                }
                state: 'normalled'
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
                        name: 'mute'
                        when: trackHeadRoot.isMute || trackHeadRoot.isHidden
                        PropertyChanges {
                            target: trackLed
                            color: 'orange'
                        }
                    },
                    State {
                        when: !trackHeadRoot.isLocked && !trackHeadRoot.isMute && !trackHeadRoot.isHidden
                        name: 'normalled'
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
            ToolButton {
                id: muteButton
                iconName: isMute ? 'kdenlive-hide-audio' : 'kdenlive-show-audio'
                iconSource: isMute ? 'qrc:///pics/kdenlive-hide-audio.svgz' : 'qrc:///pics/kdenlive-show-audio.svgz'
                onClicked: controller.setTrackProperty(trackId, "hide", isMute ? isHidden ? '1' : '0' : isHidden ? '3' : '2')
                tooltip: isMute? i18n('Unmute') : i18n('Mute')
            }

            ToolButton {
                id: hideButton
                visible: !isAudio
                iconName: isHidden ? 'kdenlive-hide-video' : 'kdenlive-show-video'
                iconSource: isHidden? 'qrc:///pics/kdenlive-hide-video.svgz' : 'qrc:///pics/kdenlive-show-video.svgz'
                onClicked: controller.setTrackProperty(trackId, "hide", isHidden ? isMute ? '2' : '0' : isMute ? '3' : '1')
                tooltip: isHidden? i18n('Show') : i18n('Hide')
            }

            ToolButton {
                id: lockButton
                iconName: isLocked ? 'kdenlive-lock' : 'kdenlive-unlock'
                iconSource: isLocked ? 'qrc:///pics/kdenlive-lock.svgz' : 'qrc:///pics/kdenlive-unlock.svgz'
                onClicked: controller.setTrackProperty(trackId, "kdenlive:locked_track", isLocked ? '0' : '1')
                tooltip: isLocked? i18n('Unlock track') : i18n('Lock track')
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
            height: 5
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
                        newHeight =  Math.max(trackLabel.height + resizer.height + 4, newHeight)
                        trackHeadRoot.myTrackHeight = newHeight
                    }
                }
            }
        }
    }
}

