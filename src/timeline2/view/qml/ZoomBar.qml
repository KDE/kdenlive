/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.0
import QtQml.Models 2.11
import QtQuick.Controls 2.4

Rectangle {
    id: zoomContainer
    SystemPalette { id: barPalette; colorGroup: SystemPalette.Disabled }
    property int barMinWidth: 1
    property bool hoveredBar: barArea.containsMouse || barArea.pressed || zoomStart.isActive || zoomEnd.isActive
    color: hoveredBar || containerArea.containsMouse ? barPalette.text : activePalette.window
    radius: height / 2
    property Flickable flickable: undefined
    MouseArea {
        id: containerArea
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
                flickable.contentX = newPos / zoomHandleContainer.width * flickable.contentWidth

            }
        }
        onPressed: {
            if (mouse.buttons === Qt.LeftButton) {
                if (mouseX > zoomEnd.x + zoomEnd.width) {
                    flickable.contentX = (zoomBar.x + zoomBar.width) / zoomHandleContainer.width * flickable.contentWidth
                } else if (mouseX < zoomBar.x) {
                    flickable.contentX = Math.max(0, (zoomBar.x - zoomBar.width) / zoomHandleContainer.width * flickable.contentWidth)
                }
            }
        }
    }
    Item {
        id: zoomHandleContainer
        property int previousX: 0
        property double previousScale: -1
        anchors.fill: parent
        Item {
            id: zoomBar
            height: parent.height
            property int minWidth: barMinWidth + zoomEnd.width + zoomStart.width
            property int preferedWidth: scrollView.visibleArea.widthRatio * zoomHandleContainer.width
            width: !zoomStart.pressed && !zoomEnd.pressed && preferedWidth < minWidth ? minWidth : preferedWidth
            x: scrollView.visibleArea.xPosition * zoomHandleContainer.width
            MouseArea {
                id: barArea
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
                        flickable.contentX = zoomBar.x / zoomHandleContainer.width * flickable.contentWidth
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
                        zoomHandleContainer.previousX = Math.round(zoomBar.x / (zoomHandleContainer.width) * flickable.contentWidth / timeline.scaleFactor)
                        zoomHandleContainer.previousScale = timeline.scaleFactor
                        root.zoomOnBar = 0
                        timeline.scaleFactor = root.fitZoom()
                    }
                }
                Rectangle {
                    color: hoveredBar ? activePalette.highlight : containerArea.containsMouse ? activePalette.text : barPalette.text
                    opacity: hoveredBar || containerArea.containsMouse ? 0.6 : 1
                    x: parent.x + parent.height
                    height: parent.height
                    width: parent.width - parent.height * 2
                }
            }
        }
        //Start drag handle
        MouseArea {
            id: zoomStart
            property bool isActive: zoomStart.containsMouse || zoomStart.pressed
            anchors {
                right: pressed ? undefined : zoomBar.left
                rightMargin: -width
                bottom: zoomBar.bottom
            }
            width: zoomBar.height
            height: zoomBar.height
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.max(0, x + mouseX)
                    updatedPos = Math.min(updatedPos, zoomEnd.x - width - 1)
                    var firstFrame = Math.round(updatedPos / (zoomHandleContainer.width) * flickable.contentWidth / timeline.scaleFactor)
                    var lastFrame = Math.round((zoomBar.x + zoomBar.width + 0.5) / (zoomHandleContainer.width) * flickable.contentWidth / timeline.scaleFactor)
                    root.zoomOnBar = firstFrame
                    timeline.scaleFactor = flickable.width / (lastFrame - firstFrame)
                    startHandleRect.x = updatedPos - x
                }
            }
            Rectangle {
                id: startHandleRect
                anchors.fill: parent.pressed ? undefined : parent
                radius: height / 2
                color: zoomStart.isActive ? activePalette.highlight : hoveredBar || containerArea.containsMouse ? activePalette.text : barPalette.text
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
            anchors {
                left: pressed ? undefined : zoomBar.right
                leftMargin: -width
                bottom: zoomBar.bottom
            }
            width: zoomBar.height
            height: zoomBar.height
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.min(zoomHandleContainer.width, x + mouseX)
                    updatedPos = Math.max(updatedPos, zoomBar.x + width * 2 + 1)
                    var lastFrame = Math.round(updatedPos / (zoomHandleContainer.width) * flickable.contentWidth / timeline.scaleFactor)
                    var firstFrame = Math.round((zoomBar.x) / (zoomHandleContainer.width) * flickable.contentWidth / timeline.scaleFactor)
                    root.zoomOnBar = firstFrame
                    timeline.scaleFactor = flickable.width / (lastFrame - firstFrame)
                    endHandleRect.x = updatedPos - x - width
                }
            }
            Rectangle {
                id: endHandleRect
                anchors.fill: parent.pressed ? undefined : parent
                radius: height / 2
                color: zoomEnd.isActive ? activePalette.highlight : hoveredBar || containerArea.containsMouse ? activePalette.text : barPalette.text
                Rectangle {
                    anchors.fill: parent
                    anchors.rightMargin: height / 2
                    color: parent.color
                }
            }
        }
    }
}

