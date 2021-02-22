import QtQuick 2.0
import QtQml.Models 2.11
import QtQuick.Controls 2.4

Rectangle {
    id: zoomContainer
    SystemPalette { id: barPalette; colorGroup: SystemPalette.Disabled }
    height: root.baseUnit
    property bool hoveredBar: zoomArea.containsMouse || zoomArea.pressed || zoomStart.isActive || zoomEnd.isActive
    // The width of the visible part
    //property double rulerZoomWidth: root.zoomFactor * width
    // The pixel offset
    //property double rulerZoomOffset: root.zoomStart * width / root.zoomFactor
    color: hoveredBar ? barPalette.text : activePalette.dark
    border.color: activePalette.dark
    border.width: 1
    visible: root.showZoomBar
    onVisibleChanged: {
        root.zoomOffset = visible ? height : 0
    }
    MouseArea {
        anchors.fill: parent
        onWheel: {
            if (wheel.modifiers & Qt.ControlModifier) {
                zoomByWheel(wheel)
            } else {
                if (wheel.angleDelta.y < 0) {
                    var newPos = Math.min(zoomHandleContainer.width - zoomBar.width, zoomBar.x + 10)
                } else {
                    var newPos = Math.max(0, zoomBar.x - 10)
                }
                scrollView.contentX = newPos / zoomHandleContainer.width * scrollView.contentWidth

            }
        }
    }
    Item {
        id: zoomHandleContainer
        property int previousX: 0
        property int previousWidth: zoomHandleContainer.width
        anchors.fill: parent
        anchors.margins: 1
        Rectangle {
            id: zoomBar
            color: zoomContainer.hoveredBar ? activePalette.highlight : barPalette.text
            height: parent.height
            width: root.zoomBarWidth //parent.width
            x: root.zoomStart
            MouseArea {
                id: zoomArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                drag.target: zoomBar
                drag.axis: Drag.XAxis
                drag.smoothed: false
                drag.minimumX: 0
                drag.maximumX: zoomHandleContainer.width - zoomBar.width
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        scrollView.contentX = zoomBar.x / zoomHandleContainer.width * scrollView.contentWidth
                    }
                }
                /*onDoubleClicked: {
                    if (zoomBar.x == 0 && zoomBar.width == zoomHandleContainer.width) {
                        // Restore previous pos
                        zoomBar.width = zoomHandleContainer.previousWidth
                        zoomBar.x = zoomHandleContainer.previousX
                        root.zoomStart = zoomBar.x / zoomHandleContainer.width
                        root.zoomFactor = zoomBar.width / zoomHandleContainer.width
                    } else {
                        zoomHandleContainer.previousWidth = zoomBar.width
                        zoomHandleContainer.previousX = zoomBar.x
                        zoomBar.x = 0
                        zoomBar.width = zoomHandleContainer.width
                        root.zoomStart = 0
                        root.zoomFactor = 1
                    }
                }*/
            }
        }
        //Start drag handle
        MouseArea {
            id: zoomStart
            property bool isActive: zoomStart.containsMouse || zoomStart.pressed
            anchors.left: zoomBar.left
            anchors.leftMargin: - root.baseUnit / 2
            anchors.bottom: zoomBar.bottom
            width: root.baseUnit
            height: zoomBar.height
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            onPressed: {
                anchors.left = undefined
                startHandleRect.anchors.fill = undefined
            }
            onReleased: {
                anchors.left = zoomBar.left
                startHandleRect.anchors.fill = zoomStart
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.max(0, x + mouseX + root.baseUnit / 2)
                    updatedPos = Math.min(updatedPos, zoomBar.x + zoomBar.width - root.baseUnit / 2)
                    timeline.scaleFactor = zoomHandleContainer.width /  (zoomBar.x + zoomBar.width - updatedPos) / (root.baseUnit)
                    startHandleRect.x = mouseX
                }
            }
            Rectangle {
                id: startHandleRect
                anchors.fill: parent
                radius: height / 2
                color: zoomStart.isActive ? activePalette.text : barPalette.light
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: height / 2
                    color: parent.color
                }
            }
        }
        //End drag handle
        MouseArea {
            id: zoomEnd
            property bool isActive: zoomEnd.containsMouse || zoomEnd.pressed
            anchors.left: zoomBar.right
            anchors.leftMargin: - root.baseUnit / 2
            anchors.bottom: zoomBar.bottom
            width: root.baseUnit
            height: zoomBar.height
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            onPressed: {
                anchors.left = undefined
                endHandleRect.anchors.fill = undefined
            }
            onReleased: {
                anchors.left = zoomBar.right
                endHandleRect.anchors.fill = zoomEnd
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.min(zoomHandleContainer.width, x + mouseX + root.baseUnit / 2)
                    updatedPos = Math.max(updatedPos, zoomBar.x + root.baseUnit / 2)
                    timeline.scaleFactor = zoomHandleContainer.width /  (updatedPos - zoomBar.x) / (root.baseUnit)
                    endHandleRect.x = mouseX
                }
            }
            Rectangle {
                id: endHandleRect
                anchors.fill: parent
                radius: height / 2
                color: zoomEnd.isActive ? activePalette.text : barPalette.light
                Rectangle {
                    anchors.fill: parent
                    anchors.rightMargin: height / 2
                    color: parent.color
                }
            }
        }
    }
    /*ToolTip {
        visible: zoomArea.containsMouse
        delay: 1000
        timeout: 5000
        background: Rectangle {
            color: activePalette.alternateBase
            border.color: activePalette.light
        }
        contentItem: Label {
            color: activePalette.text
            font: fixedFont
            text: controller.toTimecode((root.duration + 1 )* root.zoomFactor)
        }
    }*/
}

