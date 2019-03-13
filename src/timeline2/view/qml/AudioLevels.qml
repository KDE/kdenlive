import QtQuick 2.6
import QtGraphicalEffects 1.12

Rectangle {
    id: recContainer
    width: parent.width
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
}
