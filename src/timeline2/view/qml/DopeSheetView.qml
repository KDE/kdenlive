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
    // Contains a map of item model index / index of selected keyframes
    property var allSelectedKeyframes: []
    // Rubber selection
    property bool rubberSelect: false
    property point rubberTopLeft
    property point rubberBottomRight
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
            indexList.push(elem.index)
            // Sort keyframes in reverse order for deletion
            elem.kfrs.sort(function(a, b) {
              return b - a;
            });
            keyframesList.push(elem.kfrs)
        }
        timeline.dopeSheetModel().removeKeyframes(indexList, keyframesList)
    }

    function keyframeSelected(itemIndex, keyframeIndex) {
        var selectedKeyframes = getSelectedKeyframesForIndex(itemIndex)
        console.log('Looking for KF: ', keyframeIndex, ' in: ', selectedKeyframes)
        return selectedKeyframes.indexOf(keyframeIndex)
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

    function getSelectedKeyframesForIndex(itemIndex) {
        var ix = 0
        while (ix < root.allSelectedKeyframes.length) {
            var elem = root.allSelectedKeyframes[ix]
            if (elem.index === itemIndex) {
                return elem.kfrs
            } else {
                ix++
            }
        }
        return []
    }

    function updateSelectedKeyframesForIndex(itemIndex, itemKeyframes, addToSelection) {
        var ix = 0
        // First delete existing
        if (!addToSelection) {
            // Clear all selection
            root.allSelectedKeyframes = []
        } else {
            while (ix < root.allSelectedKeyframes.length) {
                var elem = root.allSelectedKeyframes[ix]
                if (elem.index === itemIndex) {
                    root.allSelectedKeyframes.splice(ix, 1);
                } else {
                    ix++
                }
            }
        }
        root.allSelectedKeyframes.push({index: itemIndex, kfrs: itemKeyframes})
        console.log('SETTING KFS FOR IX: ', itemIndex, ' = ', itemKeyframes,'\n\n___________________')
        root.allSelectedKeyframesChanged()
    }

    function selectRubber() {
        // Start frame
        var startFrame = Math.min(root.rubberBottomRight.x, root.rubberTopLeft.x) - treeView.headerWidth - root.baseUnit
        var endFrame = Math.max(root.rubberBottomRight.x, root.rubberTopLeft.x) - treeView.headerWidth - root.baseUnit
        startFrame = Math.round(startFrame / root.timeScale)
        endFrame = Math.round(endFrame / root.timeScale)
        var topPos = mapToItem(treeView, 0, Math.min(root.rubberBottomRight.y, root.rubberTopLeft.y))
        topPos.y = Math.max(0, topPos.y)
        var bottomPos = mapToItem(treeView, 0, Math.max(root.rubberBottomRight.y, root.rubberTopLeft.y))
        bottomPos.y = Math.min(treeView.contentHeight - 1, bottomPos.y)
        var topRow = treeView.cellAtPosition(topPos)
        var bottomRow = treeView.cellAtPosition(bottomPos)
        console.log('Selecting keyframes: ', startFrame, ' - ', endFrame, ' IN ROWS: ', topRow, ' / ', bottomRow)
        console.log('Requesting keyframes between Model Indexes: ', treeView.modelIndex(topRow), ' - ', treeView.modelIndex(bottomRow))
        var result = timeline.dopeSheetModel().selectKeyframeRange(treeView.modelIndex(topRow), treeView.modelIndex(bottomRow), startFrame, endFrame)
        root.allSelectedKeyframes = []
        var indexes = timeline.dopeSheetModel().selectedIndexes()
        console.log('FOUND KEYFRAMES ON PARAMS: ', indexes.length)
        var ix = 0
        while (indexes.length > 0) {
            var id = indexes.pop()
            if (!ix.toString() in result) {
                console.log('key ', ix, ' not found in MAP...')
                break;
            }
            var kf = result[ix.toString()]
            console.log('FOUND KEYFRAMES AT INDEX: ', id, ', position: ', ix, ' = ', kf)
            root.allSelectedKeyframes.push({index: id, kfrs: [kf]})
            var item = treeView.itemAtIndex(id)
            if (!item || item === undefined) {
                console.log('Undefined parameter at index: ', id)
            } else {
                item.selectedKeyframes = []
                item.selectedKeyframes = kf
                item.selectedKeyframesChanged()
            }
            console.log('GOT IX: ', id, ' = ', kf)
            ix++
        }
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
        anchors.top: playheadLabel.bottom
        anchors.bottom: parent.bottom
        z: 4
        x: treeView.headerWidth + root.baseUnit + root.consumerPosition * root.timeScale
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
        z: 5
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
            implicitHeight: fontMetrics.lineSpacing * 1.3
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
            Rectangle {
                color: Qt.rgba(activePalette.highlight.r * 0.6, activePalette.highlight.g * 0.6, activePalette.highlight.b * 0.6, 1)
                radius: 4
                visible: current
                height: parent.height
                width: treeView.headerWidth
            }
            ToolButton {
                icon.name: expanded ? "arrow-down" : "arrow-right"
                visible: depth == 0
                onClicked: {
                    treeView.toggleExpanded(row)
                }
                height: paramLabel.height
                width: height
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                id: paramLabel
                anchors.verticalCenter: parent.verticalCenter
                x: indentation
                text: dopeName
                rightPadding: 4
                leftPadding: 4
                font.bold: depth == 0
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
                property var itemIndex: contentRect.treeView.index(contentRect.row, contentRect.column)
                Rectangle {
                    // keyframe slider
                    id: keyframeSlider
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 6
                    radius: 2
                    color: kfMoveArea.containsMouse || kfMoveArea.currentFrame > -1 ? Qt.rgba(activePalette.highlight.r * 0.6, activePalette.highlight.g * 0.6, activePalette.highlight.b * 0.6, 1) : activePalette.light
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
                    property bool shiftClick: false
                    anchors.fill: parent
                    hoverEnabled: true
                    anchors.leftMargin: keyframeSlider.anchors.leftMargin
                    anchors.rightMargin: keyframeSlider.anchors.rightMargin
                    onPressed: mouse => {
                        clickPos = currentFrame
                        clickIndex = currentIndex
                        dragStarted = false
                        clickPoint = Qt.point(mouseX, mouseY)
                        rubberTopLeft = mapToItem(root, mouseX, mouseY)
                        mouse.accepted = true
                        if (mouse.modifiers & Qt.ShiftModifier) {
                            shiftClick = true
                        } else {
                            shiftClick = false
                        }
                        // Select parameter
                        treeView.selectionModel.setCurrentIndex(kfContainer.itemIndex, ItemSelectionModel.SelectCurrent)

                        if (clickIndex < 0) {
                            // Not on a keyframe
                            root.clearSelectiselectedKon()
                            return
                        }
                        var selectedKeyframes = getSelectedKeyframesForIndex(kfContainer.itemIndex)

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            const selectionIndex = selectedKeyframes.indexOf(currentIndex);
                            if (selectionIndex > -1) {
                                // Remove from selection
                                selectedKeyframes.splice(selectionIndex, 1);
                            } else {
                                // Add to selection
                                selectedKeyframes.push(currentIndex)
                            }
                        } else {
                            selectedKeyframes = [currentIndex]
                        }
                        if (depth == 0) {
                            // Build index of related kf to move
                            timeline.dopeSheetModel().buildMasterSelection(kfContainer.itemIndex, clickIndex)
                        }
                        updateSelectedKeyframesForIndex(kfContainer.itemIndex, selectedKeyframes, shiftClick)
                    }
                    onReleased: mouse => {
                        if (root.rubberSelect) {
                            // Select all keyframes inside our rectangle
                            selectRubber()
                            root.rubberSelect = false
                            dragStarted = false
                            return
                        }
                        if (dragStarted) {
                            if (clickIndex > -1) {
                                if (depth == 0) {
                                    timeline.dopeSheetModel().moveKeyframe(kfContainer.itemIndex, clickPos, false)
                                    timeline.dopeSheetModel().moveKeyframe(kfContainer.itemIndex, root.mouseFramePos, true)
                                } else {
                                    dopeModel.moveKeyframeByIndex(clickIndex, clickPos, false)
                                    dopeModel.moveKeyframeByIndex(clickIndex, root.mouseFramePos, true)
                                }
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
                            kfMoveArea.currentFrame = -1
                            kfMoveArea.currentIndex = -1
                            root.hoverKeyframe = -1
                            return
                        }
                        if (!dragStarted) {
                            if (Math.abs(mouseX - clickPoint.x) + Math.abs(mouseY - clickPoint.y) > Qt.styleHints.startDragDistance) {
                                console.log(' - - - DRAG STARTED -- - ')
                                dragStarted = true
                                if (shiftClick) {
                                    // Start rectangle selection
                                    root.rubberSelect = true
                                    root.rubberBottomRight = mapToItem(root, mouseX, mouseY)
                                    return
                                }
                            }
                        } else if (shiftClick) {
                            // Update rectangle selection
                            root.rubberBottomRight = mapToItem(root, mouseX, mouseY)
                            return
                        }

                        if (mouse.buttons === Qt.LeftButton && dragStarted && clickIndex > -1) {
                            if (depth == 0) {
                                // Moving a recap keyframe
                                timeline.dopeSheetModel().moveKeyframe(kfContainer.itemIndex, root.mouseFramePos, false)
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
                                timeline.dopeSheetModel().removeKeyframe(kfContainer.itemIndex, kfMoveArea.currentFrame)
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
                            timeline.dopeSheetModel().addPercentKeyframe(kfContainer.itemIndex, currentPercentPos)
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
                        z: 10
                        x: percentPosition * kfContainer.width - root.baseUnit/2 - ((kfArea.containsMouse || kfArea.pressed) ? 1 : 0)
                        anchors.verticalCenter: kfContainer.verticalCenter
                        width: root.baseUnit - (kfArea.containsMouse ? 0 : 2)
                        height: width
                        property bool atMousePos: root.mouseFramePos == frame
                        color: keyframeSelected(kfContainer.itemIndex, index) > -1 ? activePalette.highlight : activePalette.light
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
                                console.log("entered kfr: ", index)
                                kfMoveArea.currentFrame = frame
                                kfMoveArea.currentIndex = index
                                root.hoverKeyframe = frame
                                root.mouseFramePos = frame
                            }
                        }
                    }
                }
            }
        }
    }
    Rectangle {
        color: "#33FFFFFF"
        border.color: activePalette.highlight
        border.width: 1
        visible: root.rubberSelect
        x: Math.min(root.rubberTopLeft.x, root.rubberBottomRight.x)
        y: Math.min(root.rubberTopLeft.y, root.rubberBottomRight.y)
        width: Math.abs(root.rubberBottomRight.x - root.rubberTopLeft.x)
        height: Math.abs(root.rubberBottomRight.y - root.rubberTopLeft.y)
    }
}
