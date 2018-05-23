import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.4

    // Monitor ruler
Rectangle {
    id: ruler
    color: activePalette.window

    Timer {
        id: zoneToolTipTimer
        interval: 3000; running: false;
    }


    function updateRuler()
    {
        root.timeScale = width / root.duration
        if (root.duration < 200) {
            root.frameSize = 5 * root.timeScale
        } else if (duration < 2500) {
            frameSize = 25 * root.timeScale
        } else if (duration < 10000) {
            root.frameSize = 50 * root.timeScale
        } else {
            root.frameSize = 100 * root.timeScale
        }
    }

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
                width: mlabel.contentWidth
                height: mlabel.contentHeight
                anchors {
                    bottom: parent.top
                }
                color: model.color
                Text {
                    id: mlabel
                    visible: true //mouseOverRuler && Math.abs(root.mouseRulerPos - markerBase.x) < 4
                    text: model.comment
                    font.pixelSize: root.baseUnit
                    anchors {
                        fill: parent
                        //topMargin: 2
                        //leftMargin: 2
                    }
                    color: 'white'
                }
                MouseArea {
                    z: 10
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    //onDoubleClicked: timeline.editMarker(clipRoot.binId, model.frame)
                    onClicked: {
                        controller.requestSeekPosition(markerBase.x / root.timeScale)
                    }
                }
            }
        }
    }
    MouseArea {
        id: rulerMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onEntered: root.mouseOverRuler = true;
        onExited: root.mouseOverRuler = false;
        onPressed: {
            if (mouse.buttons === Qt.LeftButton) {
                controller.requestSeekPosition(Math.min(mouseX / root.timeScale, root.duration));
            }
        }
        onPositionChanged: {
            if (mouse.buttons === Qt.LeftButton) {
                root.mouseRulerPos = mouseX
                if (pressed) {
                    controller.requestSeekPosition(Math.min(mouseX / root.timeScale, root.duration));
                }
            }
        }
    }
    // Zone duration indicator
    Rectangle {
        visible: zoneToolTipTimer.running || rulerMouseArea.containsMouse || trimInMouseArea.containsMouse || trimInMouseArea.pressed || trimOutMouseArea.containsMouse || trimOutMouseArea.pressed
        width: inLabel.contentWidth
        height: inLabel.contentHeight
        property int centerPos: zone.x + zone.width / 2 - inLabel.contentWidth / 2
        x: centerPos < 0 ? 0 : centerPos > ruler.width - inLabel.contentWidth ? ruler.width - inLabel.contentWidth : centerPos
        color: activePalette.highlight
        anchors.bottom: ruler.top
        Label {
            id: inLabel
            anchors.fill: parent
            text: trimInMouseArea.containsMouse || trimInMouseArea.pressed ? controller.toTimecode(controller.zoneIn) + '>' + controller.toTimecode(controller.zoneOut - controller.zoneIn) : trimOutMouseArea.containsMouse || trimOutMouseArea.pressed ? controller.toTimecode(controller.zoneOut - controller.zoneIn) + '<' + controller.toTimecode(controller.zoneOut) : controller.toTimecode(controller.zoneOut - controller.zoneIn)
            font.pixelSize: root.baseUnit
            color: activePalette.highlightedText
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
        onXChanged: zoneToolTipTimer.start()
        onWidthChanged: zoneToolTipTimer.start()
    }
    Rectangle {
        id: trimIn
        x: zone.x
        y: zone.y
        height: zone.height
        width: 5
        color: 'lawngreen'
        opacity: trimInMouseArea.containsMouse || trimInMouseArea.drag.active ? 0.5 : 0
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
            drag.minimumX: 0
            drag.maximumX: ruler.width
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    controller.zoneIn = Math.round(trimIn.x / root.timeScale)
                }
            }
        }
    }
    Rectangle {
        id: trimOut
        x: zone.x + zone.width - width
        y: zone.y
        height: zone.height
        width: 5
        color: 'darkred'
        opacity: trimOutMouseArea.containsMouse || trimOutMouseArea.drag.active ? 0.5 : 0
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
            drag.minimumX: 0
            drag.maximumX: ruler.width
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    controller.zoneOut = Math.round((trimOut.x + trimOut.width) / root.timeScale)
                }
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
