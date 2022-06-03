/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtGraphicalEffects 1.0
import QtQuick.Layouts 1.11

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
            recordInfo.text = i18n("Paused")
        } else {
            recbutton.color = 'darkred'
            recbutton.ToolTip.text = i18n("Record")
            recordInfo.text = i18n("Record")
        }
    }
    RowLayout {
        spacing: 4
        Layout.fillWidth: true
        Rectangle {
            id: recbutton
            width: root.baseUnit * 1.5
            height: root.baseUnit * 1.5
            radius: root.baseUnit * .75
            color: trackHeadRoot.isLocked ? 'grey' : 'darkred'
            border.color: 'black'
            ToolTip.visible: buttonArea.containsMouse
            ToolTip.delay: 1000
            ToolTip.timeout: 5000
            ToolTip.text: i18n("Record")
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
            width: recContainer.width - recbutton.width - 6
            height: recbutton.height
            ToolTip.text: i18n("Mic level")
            ToolTip.visible: levelArea.containsMouse
            ToolTip.delay: 1000
            ToolTip.timeout: 5000
            color: Qt.lighter(activePalette.base)
            Repeater {
                model: audiorec.levels.length === 0 ? 2 : audiorec.levels.length
                id: bgRepeater
                Rectangle {
                    color: 'transparent'
                    LinearGradient {
                        anchors.fill: parent
                        anchors.margins: 0.5
                        start: Qt.point(0,0)
                        end: Qt.point(levelsContainer.width, 0)
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "darkgreen" }
                            GradientStop { position: 0.69; color: "darkgreen" }
                            GradientStop { position: 0.7; color: "green" }
                            GradientStop { position: 0.84; color: "green" }
                            GradientStop { position: 0.85; color: "yellow" }
                            GradientStop { position: 0.99; color: "yellow" }
                            GradientStop { position: 1.0; color: "red" }
                        }
                    }
                    width: parent.width - 1
                    height: parent.height / bgRepeater.count
                    y: height * index
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
                        opacity: 0.9
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
