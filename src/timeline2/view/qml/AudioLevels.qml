import QtQuick 2.11
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
        } else if (recState == 2) {
            // Paused
            recbutton.color = 'white'
        } else {
            recbutton.color = 'darkred'
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
            MouseArea {
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
                    timeline.switchRecording(trackId)
                }
            }
        }
        Rectangle {
            id: levelsContainer
            width: recContainer.width - recbutton.width - 6
            height: recbutton.height
            color: Qt.lighter(activePalette.base)
            Repeater {
                model: audiorec.levels.length == 0 ? 2 : audiorec.levels.length
                id: bgRepeater
                Rectangle {
                    color: 'transparent'
                    LinearGradient {
                        anchors.fill: parent
                        anchors.margins: 0.5
                        start: Qt.point(0,0)
                        end: Qt.point(recContainer.width, 0)
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "green" }
                            GradientStop { position: 0.6; color: "greenyellow" }
                            GradientStop { position: 0.7; color: "yellow" }
                            GradientStop { position: 0.9; color: "gold" }
                            GradientStop { position: 1.0; color: "red" }
                        }
                    }
                    width: parent.width - 1
                    height: parent.height / bgRepeater.count
                    y: height * index
                }
            }
            Repeater {
                model: audiorec.levels.length == 0 ? 2 : audiorec.levels.length
                id: levelRepeater
                Item {
                    anchors.fill: parent
                    anchors.margins: 1
                    property double currentLevel: audiorec.levels.length <= 0 ? 0 : audiorec.levels[index]
                    property double peak: 0
                    Rectangle {
                        color: "#cc000000"
                        width: parent.width * (1.0 - currentLevel)
                        anchors.right: parent.right
                        height: parent.height / levelRepeater.count
                        y: height * index
                    }
                    onCurrentLevelChanged: {
                        if (currentLevel > peak) {
                            peak = currentLevel
                        } else if (peak > 0) {
                            peak -= 0.003
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
        }
    }
}
