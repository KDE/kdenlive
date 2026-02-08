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
    property int mouseFramePos: -1
    property int hoverKeyframe: -1
    property int offset: 0
    property color hoverColor: "#cc9900"
    // Ruler scaling
    property real timeScale: keyframeContainerWidth / frameDuration
    property var allSelectedKeyframes: []
    // Playhead position
    property int consumerPosition: proxy ? proxy.position - offset: -1
    property int keyframeContainerWidth: width - treeView.headerWidth - (2 * baseUnit)
    focus: true
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

    function deleteSelection() {
        console.log("Triggered deletion...")
        console.log('deleting kfs: ', root.allSelectedKeyframes)
        var indexList = []
        var keyframesList = []
        while (root.allSelectedKeyframes.length > 0) {
            var elem = root.allSelectedKeyframes.pop()
            console.log('Found SELECTION: ', elem.index, ' = ', elem.kfrs)
            indexList.push(elem.index)
            keyframesList.push(elem.kfrs)
        }
        timeline.dopeSheetModel().removeKeyframes(indexList, keyframesList)
    }

    function clearSelection() {
        while (root.allSelectedKeyframes.length > 0) {
            var elem = root.allSelectedKeyframes.pop()
            var item = treeView.itemAtIndex(elem.index)
            item.selectedKeyframes = []
            item.selectedKeyframesChanged()
        }
        root.allSelectedKeyframes = []
        root.allSelectedKeyframesChanged()
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
            /*K.TimelinePlayhead {
                id: playhead
                height: Math.round(root.baseUnit * .8)
                width: Math.round(root.baseUnit * 1.2)
                fillColor: activePalette.windowText
                anchors.bottom: parent.bottom
                anchors.bottomMargin: ruler.zoneHeight - 1
                anchors.horizontalCenter: rulerCursor.horizontalCenter
                // bottom line on zoom
            }*/
        }
    }
    Rectangle {
        anchors.fill: playheadLabel
        radius: 4
        color: activePalette.light
    }
    Label {
        id: playheadLabel
        visible: root.mouseFramePos > -1
        anchors.horizontalCenter: rulerCursor.horizontalCenter
        text: root.consumerPosition
        leftPadding: 6
        rightPadding: 6
    }
    Rectangle {
        // Vertical line over ruler zone
        id: rulerCursor
        x: treeView.headerWidth + root.baseUnit + root.consumerPosition * root.timeScale
        anchors.top: playheadLabel.bottom
        anchors.bottom: parent.bottom
        color: activePalette.text
        width: 1
        Rectangle {
            color: ruler.dimmedColor
            width: Math.max(1, root.timeScale)
            height: 1
            visible: width > root.baseUnit * 1.2
        }
    }
    Rectangle {
        anchors.fill: mouseLabel
        visible: mouseLabel.visible
        radius: 4
        color: root.hoverKeyframe > -1 ? root.hoverColor : activePalette.highlight
    }
    Label {
        id: mouseLabel
        visible: root.mouseFramePos > -1
        anchors.horizontalCenter: mouseLine.horizontalCenter
        text: root.mouseFramePos
        leftPadding: 6
        rightPadding: 6
    }
    Rectangle {
        id: mouseLine
        anchors.top: mouseLabel.bottom
        anchors.bottom: root.bottom
        width: 1
        visible: mouseLabel.visible
        x: treeView.headerWidth + root.baseUnit + root.mouseFramePos * root.keyframeContainerWidth / root.frameDuration
        color: activePalette.highlight
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
            property var selectedKeyframes: []
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
                    visible: current
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
                    anchors.fill: parent
                    hoverEnabled: true
                    anchors.leftMargin: keyframeSlider.anchors.leftMargin
                    anchors.rightMargin: keyframeSlider.anchors.rightMargin
                    onPressed: mouse => {
                        clickPos = currentFrame
                        clickIndex = currentIndex
                        dragStarted = false
                        clickPoint = Qt.point(mouseX, mouseY)
                        mouse.accepted = true
                        if (clickIndex < 0) {
                            // Not on a keyframe
                            root.clearSelection()
                            contentRect.selectedKeyframes = []
                            contentRect.selectedKeyframesChanged()
                            return
                        }
                        var tIndex = contentRect.treeView.index(contentRect.row, contentRect.column)
                        treeView.selectionModel.setCurrentIndex(tIndex, ItemSelectionModel.SelectCurrent)
                        if (mouse.modifiers & Qt.ShiftModifier) {
                            const selectionIndex = contentRect.selectedKeyframes.indexOf(currentIndex);
                            if (selectionIndex > -1) {
                                // Remove from selection
                                contentRect.selectedKeyframes.splice(selectionIndex, 1);
                            } else {
                                // Add to selection
                                contentRect.selectedKeyframes.push(currentIndex)
                            }
                            contentRect.selectedKeyframesChanged()
                        } else {
                            root.clearSelection()
                            contentRect.selectedKeyframes = [currentIndex]
                        }
                        // Remove possible duplicates
                        var ix = 0
                        while (ix < root.allSelectedKeyframes.length) {
                            var elem = root.allSelectedKeyframes[ix]
                            if (elem.index === tIndex) {
                                root.allSelectedKeyframes.splice(ix, 1);
                            } else {
                                ix++;
                            }
                        }
                        if (depth == 0) {
                            // Build index of related kf to move
                            timeline.dopeSheetModel().buildMasterSelection(tIndex, clickIndex)
                        }
                        root.allSelectedKeyframes.push({index: tIndex, kfrs: contentRect.selectedKeyframes})
                    }
                    onReleased: mouse => {
                        if (dragStarted) {
                            if (depth == 0) {
                                var tIndex = contentRect.treeView.index(contentRect.row, contentRect.column)
                                timeline.dopeSheetModel().moveKeyframe(tIndex, clickPos, false)
                                timeline.dopeSheetModel().moveKeyframe(tIndex, root.mouseFramePos, true)
                            } else {
                                dopeModel.moveKeyframeByIndex(clickIndex, clickPos, false)
                                dopeModel.moveKeyframeByIndex(clickIndex, root.mouseFramePos, true)
                            }
                        } else if (clickIndex > -1) {
                            dopeModel.seekToKeyframe(clickIndex)
                        }
                        dragStarted = false
                        mouse.accepted = true
                    }

                    onPositionChanged: mouse => {
                        var mouseFrame = Math.max(0., mouse.x / root.keyframeContainerWidth)
                        mouseFrame = Math.min(1., mouseFrame)
                        root.mouseFramePos = Math.round(mouseFrame * frameDuration)
                        if (!pressed) {
                            return
                        }
                        if (!dragStarted) {
                            if (Math.abs(mouseX - clickPoint.x) + Math.abs(mouseY - clickPoint.y) > Qt.styleHints.startDragDistance) {
                                console.log(' - - - DRAG STARTED -- - ')
                                dragStarted = true
                            }
                        }

                        if (mouse.buttons === Qt.LeftButton && dragStarted && clickIndex > -1) {
                            if (depth == 0) {
                                // Moving a recap keyframe
                                var tIndex = contentRect.treeView.index(contentRect.row, contentRect.column)
                                timeline.dopeSheetModel().moveKeyframe(tIndex, root.mouseFramePos, false)
                            } else {
                                dopeModel.moveKeyframeByIndex(clickIndex, root.mouseFramePos, false)
                            }
                        }
                    }
                    onDoubleClicked: mouse => {
                        var tIndex
                        if (kfMoveArea.currentFrame > -1) {
                            console.log('Removing keyframe')
                            // Double click on a keyframe, remove it
                            if (depth == 0) {
                                tIndex = contentRect.treeView.index(contentRect.row, contentRect.column)
                                timeline.dopeSheetModel().removeKeyframe(tIndex, kfMoveArea.currentFrame)
                            } else {
                                dopeModel.removeKeyframe(kfMoveArea.currentFrame)
                            }
                            kfMoveArea.currentFrame = -1
                            kfMoveArea.currentIndex = -1
                            root.hoverKeyframe = -1
                            return
                        }

                        currentPercentPos = Math.max(0., mouse.x / root.keyframeContainerWidth)
                        currentPercentPos = Math.min(1., currentPercentPos)
                        if (depth == 0) {
                            tIndex = contentRect.treeView.index(contentRect.row, contentRect.column)
                            timeline.dopeSheetModel().addPercentKeyframe(tIndex, currentPercentPos)
                        } else {
                            console.log('Adding keyframe at: ', currentPercentPos)
                            dopeModel.addPercentKeyframe(currentPercentPos)
                        }
                        kfMoveArea.currentFrame = root.mouseFramePos
                        root.hoverKeyframe = root.mouseFramePos
                    }
                }
                Repeater {
                    id: paramModel
                    model: dopeModel
                    Rectangle {
                        id: handle
                        x: percentPosition * kfContainer.width - root.baseUnit/2 - ((kfArea.containsMouse || kfArea.pressed) ? 1 : 0)
                        anchors.verticalCenter: kfContainer.verticalCenter
                        width: root.baseUnit - (kfArea.containsMouse ? 0 : 2)
                        height: width
                        property bool atMousePos: root.mouseFramePos == frame
                        color: contentRect.selectedKeyframes.indexOf(index) > -1 ? activePalette.highlight : activePalette.light
                        radius: Math.round(width/2)
                        border.width: atMousePos ? 2 : 1
                        border.color: (kfArea.containsMouse || kfArea.pressed) ? activePalette.highlight : atMousePos ? root.hoverColor : activePalette.text
                        MouseArea {
                            id: kfArea
                            anchors.fill: handle
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.NoButton
                            onEntered: {
                                console.log("entered kfr: ", index, 'CURRENT SELECTION: ', contentRect.selectedKeyframes)
                                kfMoveArea.currentFrame = frame
                                kfMoveArea.currentIndex = index
                                root.hoverKeyframe = frame
                                root.mouseFramePos = frame
                            }
                            onExited: {
                                console.log("exited kfr: ", index)
                                kfMoveArea.currentFrame = -1
                                kfMoveArea.currentIndex = -1
                                root.hoverKeyframe = -1
                            }
                        }
                    }
                }
            }
        }
    }
}
