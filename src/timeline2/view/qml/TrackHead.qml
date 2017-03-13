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
//import Shotcut.Controls 1.0 as Shotcut

Rectangle {
    id: trackHeadRoot
    property string trackName
    property bool isMute
    property bool isHidden
    property int isComposite
    property bool isLocked
    property bool isVideo
    property bool selected: false
    property bool current: false
    property int myTrackHeight
    property int trackId : -42

    signal clicked()

    function pulseLockButton() {
        lockButtonAnim.restart();
    }

    SystemPalette { id: activePalette }
    color: selected ? selectedTrackColor : (index % 2)? activePalette.alternateBase : activePalette.base
    border.color: selected? 'red' : 'transparent'
    border.width: selected? 1 : 0
    clip: true
    state: 'normal'
    states: [
        State {
            name: 'selected'
            when: trackHeadRoot.selected
            PropertyChanges {
                target: trackHeadRoot
                //color: isVideo? root.shotcutBlue : 'darkseagreen'
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
                color: (index % 2)? activePalette.alternateBase : activePalette.base
            }
        }
    ]
    transitions: [
        Transition {
            to: '*'
            ColorAnimation { target: trackHeadRoot; duration: 100 }
        }
    ]

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: {
            parent.clicked()
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

        Rectangle {
            id: trackLabel
            color: 'transparent'
            width: trackHeadRoot.width - 4
            radius: 2
            border.color: trackNameMouseArea.containsMouse? (trackHeadRoot.current? activePalette.shadow : activePalette.highlight) : 'transparent'
            height: nameEdit.height
            MouseArea {
                id: trackNameMouseArea
                height: parent.height
                width: nameEdit.width
                hoverEnabled: true
                onClicked: {
                    nameEdit.visible = true
                    nameEdit.selectAll()
                }
            }
            Label {
                text: trackName
                anchors.left: parent.left
                anchors.leftMargin: 4
                color: activePalette.windowText
                elide: Qt.ElideRight
                antialiasing: true
                font.pixelSize: root.baseUnit * 1.5
                //width: trackLabel.width - 8
            }
            TextField {
                id: nameEdit
                visible: false
                width: parent.width
                text: trackName
                style: TextFieldStyle {
                    font.pixelSize: root.baseUnit * 1.5
                }
                onAccepted: {
                    multitrack.setTrackProperty(trackId, "kdenlive:track_name", text)
                    visible = false
                }
                onFocusChanged: visible = focus
            }
        }
        RowLayout {
            spacing: 6
            visible: (trackHeadRoot.height > trackLabel.height + muteButton.height + resizer.height + 4)
            Layout.leftMargin: 2
            ToolButton {
                id: muteButton
                implicitWidth: 20
                implicitHeight: 20
                iconName: isMute ? 'kdenlive-hide-audio' : 'kdenlive-show-audio'
                iconSource: isMute ? 'qrc:///pics/kdenlive-hide-audio.svgz' : 'qrc:///pics/kdenlive-show-audio.svgz'
                onClicked: multitrack.setTrackProperty(trackId, "hide", isMute ? isHidden ? '1' : '0' : isHidden ? '3' : '2')
                tooltip: isMute? qsTr('Unmute') : qsTr('Mute')
            }

            ToolButton {
                id: hideButton
                visible: isVideo
                implicitWidth: 20
                implicitHeight: 20
                iconName: isHidden ? 'kdenlive-hide-video' : 'kdenlive-show-video'
                iconSource: isHidden? 'qrc:///pics/kdenlive-hide-video.svgz' : 'qrc:///pics/kdenlive-show-video.svgz'
                onClicked: multitrack.setTrackProperty(trackId, "hide", isHidden ? isMute ? '2' : '0' : isMute ? '3' : '1')
                tooltip: isHidden? qsTr('Show') : qsTr('Hide')
            }

            ToolButton {
                id: lockButton
                implicitWidth: 20
                implicitHeight: 20
                iconName: isLocked ? 'kdenlive-lock' : 'kdenlive-unlock'
                iconSource: isLocked ? 'qrc:///pics/kdenlive-lock.svgz' : 'qrc:///pics/kdenlive-unlock.svgz'
                onClicked: multitrack.setTrackProperty(trackId, "kdenlive:locked_track", isLocked ? '0' : '1')
                tooltip: isLocked? qsTr('Unlock track') : qsTr('Lock track')
            }
        }
        Rectangle {
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

