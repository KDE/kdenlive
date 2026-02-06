/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kdenlive as K

Item {
    id: root
    anchors.fill: parent
    SystemPalette { id: activePalette }
    property int baseUnit: Math.max(12, fontMetrics.font.pixelSize)
    // Effects duration
    property int frameDuration: 100
    property int offset: 0
    // Ruler scaling
    property real timeScale: keyframeContainerWidth / frameDuration
    // Playhead position
    property int consumerPosition: proxy ? proxy.position - offset: -1
    property int keyframeContainerWidth: width - treeView.headerWidth - (2 * baseUnit)
    FontMetrics {
        id: fontMetrics
        font: miniFont
    }
    readonly property font miniFont: ({
        pixelSize: miniFontSize
    })
    onFrameDurationChanged: {
        console.log('UPDATED DOPE DURATION: ', frameDuration)
    }
    onConsumerPositionChanged: {
        console.log('UPDATED DOPE POSITION: ', consumerPosition)
    }

    Flickable {
        // scroll area for the Ruler.
        id: rulercontainer
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.leftMargin: root.baseUnit + treeView.headerWidth
        anchors.rightMargin: root.baseUnit
        height: Math.round(root.baseUnit * 2.5)
        contentWidth: Math.max(parent.width, root.frameDuration * root.timeScale)
        interactive: false
        clip: true
        onWidthChanged: {
            ruler.adjustStepSize()
        }
        Ruler {
            id: ruler
            width: parent.width
            height: parent.height
            rulerOffset: root.offset
            scalingFactor: root.timeScale
            K.TimelinePlayhead {
                id: playhead
                height: Math.round(root.baseUnit * .8)
                width: Math.round(root.baseUnit * 1.2)
                fillColor: activePalette.windowText
                anchors.bottom: parent.bottom
                anchors.bottomMargin: ruler.zoneHeight - 1
                anchors.horizontalCenter: rulerCursor.horizontalCenter
                // bottom line on zoom
            }
            Rectangle {
                // Vertical line over ruler zone
                id: rulerCursor
                color: activePalette.text
                width: 1
                height: ruler.zoneHeight - 1
                x: root.consumerPosition * root.timeScale
                onXChanged: {
                    console.log("CURSOR X SET TO: ", x)
                }

                anchors.bottom: parent.bottom
                Rectangle {
                    color: ruler.dimmedColor
                    width: Math.max(1, root.timeScale)
                    height: 1
                    visible: width > playhead.width
                }
            }
        }
    }
    TreeView {
        // The model needs to be a QAbstractItemModel
        id: treeView
        model: timeline.dopeSheetModel()
        anchors.fill: parent
        anchors.topMargin: rulercontainer.height
        property int headerWidth: 100
        // Disable flicking
        acceptedButtons: Qt.NoButton
        selectionModel: ItemSelectionModel {
            model: timeline.dopeSheetModel()
        }
        // You can set a custom delegate or use a built-in TreeViewDelegate
        delegate: Item {
            id: contentRect
            implicitWidth: root.width
            implicitHeight: fontMetrics.lineSpacing
            readonly property real indentation: 20
            readonly property real padding: 5

            // Assigned to by TreeView:
            required property TreeView treeView
            required property bool isTreeNode
            required property bool expanded
            required property bool hasChildren
            required property int depth
            required property int row
            required property int column
            required property bool current
            required property bool selected
            ToolButton {
                icon.name: treeView.isExpanded(row) ? "arrow-down" : "arrow-right"
                visible: depth == 0
                onClicked: treeView.toggleExpanded(row)
                width: parent.height
                height: width
            }

            Label {
                id: paramLabel
                text: dopeName
                x: indentation
                font.bold: depth == 0
                background: Rectangle {
                    color: activePalette.highlight
                    radius: 4
                    visible: current //row == treeView.currentRow
                }
                Component.onCompleted: {
                    if (treeView.headerWidth < (paramLabel.width + indentation)) {
                        treeView.headerWidth = paramLabel.width + indentation
                    }
                }
            }
            Item {
                id: kfContainer
                anchors.left: contentRect.left
                anchors.right: contentRect.right
                anchors.top: contentRect.top
                anchors.bottom: contentRect.bottom
                anchors.leftMargin: root.baseUnit + treeView.headerWidth
                anchors.rightMargin: root.baseUnit
                // visible: depth > 0
                Rectangle {
                    // keyframe scroll
                    id: keyframeSlider
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 6
                    radius: 2
                    color: activePalette.light
                    border.width: 1
                    border.color: activePalette.shadow
                }
                MouseArea {
                    id: kfMoveArea
                    property int clickPos
                    property int clickIndex
                    property int currentFrame: -1
                    property int currentIndex: -1
                    property double currentPercentPos
                    property bool dragStarted: false
                    property point clickPoint
                    propagateComposedEvents: true
                    anchors.fill: parent
                    anchors.leftMargin: keyframeSlider.anchors.leftMargin
                    anchors.rightMargin: keyframeSlider.anchors.rightMargin
                    onPressed: mouse => {
                        console.log('==============  MS PRESSED, IX: ', currentIndex, ' / FRM: ', currentFrame, ' ==========')
                        clickPos = currentFrame
                        clickIndex = currentIndex
                        dragStarted = false
                        clickPoint = Qt.point(mouseX, mouseY)
                        var tIndex = contentRect.treeView.index(row, column)
                        treeView.selectionModel.setCurrentIndex(tIndex, ItemSelectionModel.SelectCurrent)
                        mouse.accepted = true
                    }
                    onReleased: mouse => {
                        console.log("============== MOUSE RELEASED ===========")
                        if (depth > 0 && clickIndex > -1 && dragStarted) {
                            dopeModel.movePercentKeyframeWithUndo(clickIndex, clickPos, currentPercentPos)
                        } else if (clickIndex > -1) {
                            dopeModel.seekToKeyframe(clickIndex)
                        }
                        dragStarted = false
                        mouse.accepted = true
                    }
                    onPositionChanged: mouse => {
                        if (!dragStarted) {
                            if (Math.abs(mouseX - clickPoint.x) + Math.abs(mouseY - clickPoint.y) > Qt.styleHints.startDragDistance) {
                                console.log(' - - - DRAG STARTED -- - ')
                                dragStarted = true
                            }
                        }

                        if (mouse.buttons === Qt.LeftButton && dragStarted && clickIndex > -1) {
                            currentPercentPos = Math.max(0., mouse.x / kfContainer.width)
                            currentPercentPos = Math.min(1., currentPercentPos)
                            dopeModel.movePercentKeyframe(clickIndex, currentPercentPos)
                        }
                    }
                    onDoubleClicked: mouse => {
                        currentPercentPos = Math.max(0., mouse.x / kfContainer.width)
                        currentPercentPos = Math.min(1., currentPercentPos)
                        dopeModel.addPercentKeyframe(currentPercentPos)
                    }
                }
                Repeater {
                    model: dopeModel
                    Rectangle {
                        id: handle
                        x: percentPosition * kfContainer.width - root.baseUnit/2 - ((kfArea.containsMouse || kfArea.pressed) ? 1 : 0)
                        anchors.verticalCenter: kfContainer.verticalCenter
                        width: root.baseUnit - (kfArea.containsMouse ? 0 : 2)
                        height: width
                        color: activePalette.light
                        radius: Math.round(width/2)
                        border.width: 1
                        border.color: (kfArea.containsMouse || kfArea.pressed) ? activePalette.highlight : activePalette.text
                        MouseArea {
                            id: kfArea
                            anchors.fill: handle
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.NoButton
                            onEntered: {
                                console.log("entered kfr: ", index)
                                kfMoveArea.currentFrame = frame
                                kfMoveArea.currentIndex = index
                            }
                            onExited: {
                                console.log("exited kfr: ", index)
                                kfMoveArea.currentFrame = -1
                                kfMoveArea.currentIndex = -1
                            }
                        }
                    }
                }
            }
        }
    }
}
