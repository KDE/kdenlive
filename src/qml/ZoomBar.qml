/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: zoomContainer
    SystemPalette { id: activePalette }
    SystemPalette { id: barPalette; colorGroup: SystemPalette.Disabled }
    property bool hoveredBar: barArea.containsMouse || barArea.pressed || zoomStart.isActive || zoomEnd.isActive
    property int barMinWidth: 1
    required property real contentPos
    required property real zoomFactor
    required property bool fitsZoom
    property string toolTipText
    signal proposeContentPos(real proposedValue)
    signal proposeZoomFactor(real proposedValue)
    signal zoomByWheel(var wheel)
    signal fitZoom()
    color: hoveredBar || containerArea.containsMouse ? barPalette.text : activePalette.window
    radius: height / 2
    border {
        color: activePalette.window
        width: 1
    }
    MouseArea {
        id: containerArea
        anchors.fill: parent
        hoverEnabled: true
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                zoomContainer.zoomByWheel(wheel)
            } else {
                var newPos = zoomBar.x
                if (wheel.angleDelta.y < 0) {
                    newPos = Math.min(zoomHandleContainer.width - zoomBar.width, newPos + 10)
                } else {
                    newPos = Math.max(0, newPos - 10)
                }
                zoomContainer.proposeContentPos(newPos / zoomHandleContainer.width)
            }
        }
        onPressed: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                if (mouseX > zoomEnd.x + zoomEnd.width) {
                    zoomContainer.proposeContentPos((zoomBar.x + zoomBar.width) / zoomHandleContainer.width)
                } else if (mouseX < zoomBar.x) {
                    zoomContainer.proposeContentPos(Math.max(0, (zoomBar.x - zoomBar.width) / zoomHandleContainer.width))
                }
            }
        }
    }
    Item {
        id: zoomHandleContainer
        anchors.fill: parent
        Item {
            id: zoomBar
            height: parent.height
            property int minWidth: zoomContainer.barMinWidth + zoomEnd.width + zoomStart.width
            property int preferedWidth: zoomContainer.zoomFactor * zoomHandleContainer.width
            width: !zoomStart.pressed && !zoomEnd.pressed && preferedWidth < minWidth ? minWidth : preferedWidth
            x: zoomContainer.contentPos * zoomHandleContainer.width
            MouseArea {
                id: barArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                property real previousPos: 0
                property real previousFactor: -1
                drag {
                    target: zoomBar
                    axis: Drag.XAxis
                    smoothed: false
                    minimumX: 0
                    maximumX: zoomHandleContainer.width - zoomBar.width
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        zoomContainer.proposeContentPos(zoomBar.x / zoomHandleContainer.width)
                    }
                }
                onDoubleClicked: {
                    // Switch between current zoom and fit whole content
                    if (zoomBar.x == 0 && zoomContainer.fitsZoom) {
                        // Restore previous pos
                        if (previousFactor > -1) {
                            zoomContainer.proposeZoomFactor(previousFactor)
                            zoomContainer.proposeContentPos(previousPos)
                        }
                    } else {
                        previousPos = zoomContainer.contentPos
                        previousFactor = zoomContainer.zoomFactor
                        zoomContainer.fitZoom()
                    }
                }
                Rectangle {
                    color: zoomContainer.hoveredBar ? activePalette.highlight : containerArea.containsMouse ? activePalette.text : barPalette.text
                    opacity: zoomContainer.hoveredBar || containerArea.containsMouse ? 0.6 : 1
                    x: parent.x + zoomStart.width
                    height: parent.height
                    width: parent.width - zoomStart.width - zoomEnd.width
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
            onPositionChanged: mouse => {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.max(0, x + mouseX)
                    updatedPos = Math.min(updatedPos, zoomEnd.x - width - 1)
                    zoomContainer.proposeZoomFactor((zoomBar.x + zoomBar.width + 0.5 - updatedPos) / zoomHandleContainer.width)
                    zoomContainer.proposeContentPos(updatedPos / zoomHandleContainer.width)
                    startHandleRect.x = updatedPos - x
                }
            }
            Rectangle {
                id: startHandleRect
                anchors.fill: parent.pressed ? undefined : parent
                radius: height / 2
                color: zoomStart.isActive ? activePalette.highlight : zoomContainer.hoveredBar || containerArea.containsMouse ? activePalette.text : barPalette.text
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
            onPositionChanged: mouse => {
                if (mouse.buttons === Qt.LeftButton) {
                    var updatedPos = Math.min(zoomHandleContainer.width, x + mouseX)
                    updatedPos = Math.max(updatedPos, zoomBar.x + width * 2 + 1)
                    var zoomBarX = zoomBar.x // we need to save the value before we change zoomFactor, but apply it afterwards
                    zoomContainer.proposeZoomFactor((updatedPos - zoomBar.x) / zoomHandleContainer.width)
                    zoomContainer.proposeContentPos(zoomBarX / zoomHandleContainer.width)
                    endHandleRect.x = updatedPos - x - width
                }
            }
            Rectangle {
                id: endHandleRect
                anchors.fill: parent.pressed ? undefined : parent
                radius: height / 2
                color: zoomEnd.isActive ? activePalette.highlight : zoomContainer.hoveredBar || containerArea.containsMouse ? activePalette.text : barPalette.text
                Rectangle {
                    anchors {
                        fill: parent
                        rightMargin: height / 2
                    }
                    color: parent.color
                }
            }
        }
    }
    ToolTip {
        visible: barArea.containsMouse && zoomContainer.toolTipText
        delay: 1000
        timeout: 5000
        background: Rectangle {
            color: activePalette.alternateBase
            border.color: activePalette.light
        }
        contentItem: Label {
            color: activePalette.text
            //font: fixedFont
            text: zoomContainer.toolTipText
        }
    }
}
