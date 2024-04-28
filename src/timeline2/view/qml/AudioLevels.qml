/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: recContainer
    property int trackId: -1
    property int recState: audiorec.recordState
    width: parent.width
    implicitHeight: root.baseUnit * 1.5

    onRecStateChanged: {
        if (recState == 1) {
            // Recording
            recbutton.color = 'orange'
            recbutton.ToolTip.text = i18n("Stop")
        } else if (recState == 2) {
            // Paused
            recbutton.color = 'white'
            recbutton.ToolTip.text = i18n("Paused")
        } else {
            recbutton.color = 'darkred'
            recbutton.ToolTip.text = i18n("Record")
        }
    }
    RowLayout {
        spacing: 4
        Layout.fillWidth: true
        Rectangle {
            id: recbutton
            width: root.baseUnit * 1.5
            height: root.baseUnit * 1.5
            radius: recState == 2 ? 0 : root.baseUnit * .75
            color: trackHeadRoot.isLocked ? 'grey' : 'darkred'
            border.color: 'black'
            ToolTip.visible: buttonArea.containsMouse
            ToolTip.delay: 1000
            ToolTip.timeout: 5000
            ToolTip.text: i18n("Record") + timeline.getActionShortcut("audio_record")
            Rectangle {
                visible: recState == 2
                color: 'black'
                width: recbutton.width / 3 - 1
                height: recbutton.height
                anchors.centerIn: parent
            }
            MouseArea {
                id: buttonArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                enabled: !trackHeadRoot.isLocked
                onEntered:  {
                    parent.border.color = 'red'
                    parent.border.width = 4
                }
                onExited:  {
                    parent.border.color = 'black'
                    parent.border.width = 1
                }
                onClicked: {
                    timeline.switchRecording(trackId, recState == 1 ? 0 : 1)
                }
            }
        }
        Rectangle {
            id: levelsContainer
            width: recContainer.width - recbutton.width - 8
            height: recbutton.height - 1
            x: 1
            y: 1
            ToolTip.text: i18n("Mic level")
            ToolTip.visible: levelArea.containsMouse
            ToolTip.delay: 1000
            ToolTip.timeout: 5000
            color: Qt.lighter(activePalette.base)
            Repeater {
                model: audiorec.levels.length === 0 ? 2 : audiorec.levels.length
                id: bgRepeater
                Rectangle {
                    required property int index
                    width: levelsContainer.width - 2
                    height: Math.max(1, levelsContainer.height / bgRepeater.count - 2)
                    x: 1
                    y: (height + 1) * index
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "darkgreen" }
                        GradientStop { position: 0.69; color: "darkgreen" }
                        GradientStop { position: 0.7; color: "green" }
                        GradientStop { position: 0.84; color: "green" }
                        GradientStop { position: 0.85; color: "yellow" }
                        GradientStop { position: 0.99; color: "yellow" }
                        GradientStop { position: 1.0; color: "red" }
                    }
                }
            }
            Repeater {
                model: audiorec.levels.length === 0 ? 2 : audiorec.levels.length
                id: levelRepeater
                property int lastPos: 0
                property int currentPos: 0
                Item {
                    anchors.fill: parent
                    anchors.margins: 1
                    property double currentLevel: audiorec.levels.length <= 0 ? 0 : audiorec.levels[index]
                    property double peak: 0
                    Rectangle {
                        color: activePalette.base
                        opacity: 0.8
                        width: parent.width * (1.0 - currentLevel)
                        anchors.right: parent.right
                        height: parent.height / levelRepeater.count
                        y: height * index
                    }
                    onCurrentLevelChanged: {
                        if (currentLevel > peak) {
                            peak = currentLevel
                        } else if (peak > 0) {
                            peak -= 0.02
                        }
                    }
                    Rectangle {
                        visible: peak > 0
                        color: activePalette.text
                        width: 2
                        height: parent.height / levelRepeater.count
                        x: parent.width * peak
                        y: height * index
                    }
                }
            }
            Repeater {
                model: 8
                Rectangle {
                    color: "#000000"
                    width: 1
                    y: 1
                    height: parent.height - 2
                    x: parent.width * (index + 1) / 9
                }
            }
            Rectangle {
                // main frame
                anchors.fill: parent
                color: "#00000000"
                border.color: "#000000"
                border.width: 1
            }
            MouseArea {
                id: levelArea
                hoverEnabled: true
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
            }
        }
    }
}
