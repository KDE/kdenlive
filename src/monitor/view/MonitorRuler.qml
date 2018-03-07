import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.4

    // Monitor ruler
    Rectangle {
        id: ruler
        color: activePalette.window

        // frame ticks
        Repeater {
            model: ruler.width / frameSize + 2
            Rectangle {
                property int realPos: index
                x: realPos * frameSize
                anchors.bottom: ruler.bottom
                height: (realPos % 4)? ((realPos % 2) ? 3 : 7) : 12
                width: 1
                color: activePalette.windowText
                opacity: 0.5
            }
        }

        // markers
        Repeater {
            model: markersModel
            delegate:
            Item {
                anchors.fill: parent
                Rectangle {
                    id: markerBase
                    width: 1
                    height: parent.height
                    x: (model.frame) * root.timeScale;
                    color: model.color
                }
                Rectangle {
                    visible: mlabel.visible
                    opacity: 0.7
                    x: markerBase.x
                    radius: 2
                    width: mlabel.width + 4
                    height: mlabel.height
                    anchors {
                        bottom: parent.verticalCenter
                    }
                    color: model.color
                    MouseArea {
                        z: 10
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        //onDoubleClicked: timeline.editMarker(clipRoot.binId, model.frame)
                        //onClicked: timeline.position = (clipRoot.x + markerBase.x) / timeline.scaleFactor
                    }
                }
                Text {
                    id: mlabel
                    visible: mouseOverRuler && Math.abs(root.mouseRulerPos - markerBase.x) < 4
                    text: model.comment
                    font.pixelSize: root.baseUnit
                    x: markerBase.x
                    anchors {
                        bottom: parent.verticalCenter
                        topMargin: 2
                        leftMargin: 2
                    }
                    color: 'white'
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: root.mouseOverRuler = true;
            onExited: root.mouseOverRuler = false;
            onPressed: {
                if (mouse.buttons === Qt.LeftButton) {
                    controller.requestSeekPosition(mouseX / root.timeScale);
                }
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    root.mouseRulerPos = mouseX
                    if (pressed) {
                        controller.requestSeekPosition(mouseX / root.timeScale);
                    }
                }
            }
        }
        // monitor zone
        Rectangle {
            id: zone
            visible: controller.zoneOut > controller.zoneIn
            color: activePalette.highlight
            x: controller.zoneIn * root.timeScale
            width: (controller.zoneOut - controller.zoneIn) * root.timeScale
            anchors.bottom: parent.bottom
            height: ruler.height / 2
            opacity: 0.4

            Rectangle {
                id: trimIn
                anchors.left: parent.left
                anchors.leftMargin: 0
                height: parent.height
                width: 5
                color: 'lawngreen'
                opacity: 0
                Drag.active: trimInMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction

                MouseArea {
                    id: trimInMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.smoothed: false

                    onPressed: {
                        parent.anchors.left = undefined
                    }
                    onReleased: {
                        parent.anchors.left = zone.left
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            controller.zoneIn = controller.zoneIn + Math.round(trimIn.x / root.timeScale)
                        }
                    }
                    onEntered: parent.opacity = 0.5
                    onExited: parent.opacity = 0
                }
            }
            Rectangle {
                id: trimOut
                anchors.right: parent.right
                anchors.rightMargin: 0
                height: parent.height
                width: 5
                color: 'darkred'
                opacity: 0
                Drag.active: trimOutMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction

                MouseArea {
                    id: trimOutMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.smoothed: false

                    onPressed: {
                        parent.anchors.right = undefined
                    }
                    onReleased: {
                        parent.anchors.right = zone.right
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            controller.zoneOut = controller.zoneIn + Math.round((trimOut.x + trimOut.width) / root.timeScale)
                        }
                    }
                    onEntered: parent.opacity = 0.5
                    onExited: parent.opacity = 0
                }
            }
        }

        Rectangle {
            id: seekCursor
            visible: controller.seekPosition > -1
            color: activePalette.highlight
            width: 4
            height: ruler.height
            opacity: 0.5
            x: controller.seekPosition * root.timeScale
            y: 0
        }

        TimelinePlayhead {
            id: playhead
            visible: controller.position > -1
            height: ruler.height * 0.5
            width: ruler.height * 1
            anchors.top: ruler.top
            x: controller.position * root.timeScale - (width / 2)
        }
    }
