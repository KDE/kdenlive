import QtQuick 2.6
import QtGraphicalEffects 1.0
import QtQuick.Layouts 1.3

Item {
    id: recContainer
    property int trackId: -1
    property int recState: audiorec.recordState
    width: parent.width
    
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
        spacing: 2
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
            Layout.fillHeight: true
            width: recContainer.width - recbutton.width - 6
            border.color: root.frameColor
            color: Qt.lighter(activePalette.base)
            Repeater {
                model: audiorec.levels.length
                id: levelRepeater
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
                    width: parent.width * audiorec.levels[index]
                    height: parent.height / levelRepeater.count
                    y: height * index
                }
            }
            Repeater {
                model: 8
                Rectangle {
                    color: levelsContainer.color
                    width: 1
                    height: parent.height
                    x: parent.width * (index + 1) / 9
                }
            }
        }
    }
}
