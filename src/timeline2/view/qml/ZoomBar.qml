import QtQuick 2.0
import QtQml.Models 2.11
import QtQuick.Controls 2.4

Rectangle {
    id: zoomContainer
    SystemPalette { id: barPalette; colorGroup: SystemPalette.Disabled }
    height: Math.round(root.baseUnit * 0.7)
    property bool hoveredBar: barArea.containsMouse || zoomArea.containsMouse || zoomArea.pressed || zoomStart.isActive || zoomEnd.isActive
    color: hoveredBar ? barPalette.text : activePalette.window
    //border.color: activePalette.dark
    //border.width: 1
    radius: height / 2
    visible: root.zoomBarWidth < 1
    MouseArea {
        id: barArea
        anchors.fill: parent
        hoverEnabled: true
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
        onPressed: {
            if (mouse.buttons === Qt.LeftButton) {
                if (mouseX > zoomEnd.x + zoomEnd.width) {
                    scrollView.contentX = (zoomBar.x + zoomBar.width) / zoomHandleContainer.width * scrollView.contentWidth
                } else if (mouseX < zoomBar.x) {
                    scrollView.contentX = Math.max(0, (zoomBar.x - zoomBar.width) / zoomHandleContainer.width * scrollView.contentWidth)
                }
            }
        }
    }
    Item {
        id: zoomHandleContainer
        property int previousX: 0
        property double previousScale: -1
        anchors.fill: parent
        //anchors.margins: 1
        Rectangle {
            id: zoomBar
            color: (zoomArea.containsMouse || zoomArea.pressed || zoomStart.isActive || zoomEnd.isActive) ? activePalette.highlight : hoveredBar ? activePalette.text : barPalette.text
            opacity: hoveredBar ? 0.6 : 1
            height: parent.height
            property bool placeHolderBar: !zoomStart.pressed && !zoomEnd.pressed && root.zoomBarWidth * zoomHandleContainer.width < root.baseUnit
            width: placeHolderBar ? root.baseUnit : root.zoomBarWidth * zoomHandleContainer.width
            x: root.zoomStart * zoomHandleContainer.width
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
                onDoubleClicked: {
                    // Switch between current zoom and fit all project
                    if (zoomBar.x == 0 && timeline.scaleFactor === root.fitZoom()) {
                        // Restore previous pos
                        if (zoomHandleContainer.previousScale > -1) {
                            root.zoomOnBar = zoomHandleContainer.previousX
                            timeline.scaleFactor = zoomHandleContainer.previousScale
                        }
                    } else {
                        zoomHandleContainer.previousX = Math.round(zoomBar.x / (zoomHandleContainer.width) * scrollView.contentWidth / timeline.scaleFactor)
                        zoomHandleContainer.previousScale = timeline.scaleFactor
                        root.zoomOnBar = 0
                        timeline.scaleFactor = root.fitZoom()
                    }
                }
            }
        }
        //Start drag handle
        MouseArea {
            id: zoomStart
            property bool isActive: zoomStart.containsMouse || zoomStart.pressed
            anchors.right: pressed ? undefined : zoomBar.left
            anchors.rightMargin: zoomBar.x < width ? -width : 0
            anchors.bottom: zoomBar.bottom
            width: zoomBar.height
            height: zoomBar.height
            hoverEnabled: true
            drag.axis: Drag.XAxis
            drag.smoothed: false
            drag.maximumX: zoomEnd.x - width
            cursorShape: Qt.SizeHorCursor
            onPressed: {
                startHandleRect.anchors.fill = undefined
            }
            onReleased: {
                startHandleRect.anchors.fill = zoomStart
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.max(0, x + mouseX + width)
                    updatedPos = Math.min(updatedPos, zoomEnd.x - 1)
                    var firstFrame = Math.round(updatedPos / (zoomHandleContainer.width) * scrollView.contentWidth / timeline.scaleFactor)
                    var lastFrame = Math.round((zoomBar.x + zoomBar.width) / (zoomHandleContainer.width) * scrollView.contentWidth / timeline.scaleFactor)
                    root.zoomOnBar = firstFrame
                    timeline.scaleFactor = scrollView.width / (lastFrame - firstFrame)
                    startHandleRect.x = updatedPos - x - width
                }
            }
            Rectangle {
                id: startHandleRect
                anchors.fill: parent
                radius: height / 2
                color: zoomStart.isActive ? activePalette.highlight : hoveredBar ? activePalette.text : barPalette.text
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
            anchors.bottom: zoomBar.bottom
            width: zoomBar.height
            height: zoomBar.height
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.minimumX: zoomStart.x + width
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
                    var updatedPos = Math.min(zoomHandleContainer.width, x + mouseX)
                    updatedPos = Math.max(updatedPos, zoomBar.x + 1)
                    var lastFrame = Math.round(updatedPos / (zoomHandleContainer.width) * scrollView.contentWidth / timeline.scaleFactor)
                    var firstFrame = Math.round((zoomBar.x) / (zoomHandleContainer.width) * scrollView.contentWidth / timeline.scaleFactor)
                    root.zoomOnBar = firstFrame
                    timeline.scaleFactor = scrollView.width / (lastFrame - firstFrame)
                    endHandleRect.x = updatedPos - x
                }
            }
            Rectangle {
                id: endHandleRect
                anchors.fill: parent
                radius: height / 2
                color: zoomEnd.isActive ? activePalette.highlight : hoveredBar ? activePalette.text : barPalette.text
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

