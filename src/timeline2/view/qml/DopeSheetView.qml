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
    property var keyframeType
    // The position in frame of the stack owner
    property int offset: 0
    property color hoverColor: "#cc9900"
    // Ruler scaling, 1 means view is fully visible, 2 means zoomed twice
    property real timeScale: 1
    // The maximum timeScale factor, where the full item width is visible
    property real maximumScaleFactor: keyframeContainerWidth / frameDuration
    // Contains a map of item model index / index of selected keyframes
    property var allSelectedKeyframes: []
    property int collapsedHeight: Math.max(28, baseUnit * 1.8)
    // Rubber selection
    property bool rubberSelect: false
    property point rubberTopLeft
    property point rubberBottomRight
    property int wheelAccumulatedDelta: 0
    readonly property int defaultDeltasPerStep: 120
    // the X offset for the keyframes view
    property double contentScroll: 0
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

    function updateOwner(totalDuration, itemStartPos) {
        root.frameDuration = totalDuration
        root.offset = itemStartPos
        root.contentScroll = 0
        root.timeScale = 1
    }

    function scrollByWheel(wheel) {
        var proposedPos
        if (wheel.angleDelta.y < 0) {
            proposedPos = Math.max(0, Math.min((horZoomBar.contentPos * root.frameDuration - wheel.angleDelta.y) / root.frameDuration, 1 - 1 / root.timeScale))
        } else {
            proposedPos = Math.max(horZoomBar.contentPos * root.frameDuration - wheel.angleDelta.y, 0) / root.frameDuration
        }
        horZoomBar.contentPos = proposedPos
        horZoomBar.proposeContentPos(proposedPos)
    }

    function zoomByWheel(wheel) {
        if (wheel.modifiers & Qt.AltModifier) {
            // Seek to next snap
            if (wheel.angleDelta.x > 0) {
                timeline.triggerAction('monitor_seek_snap_backward')
            } else {
                timeline.triggerAction('monitor_seek_snap_forward')
            }
        } else if (wheel.modifiers & Qt.ControlModifier) {
            root.wheelAccumulatedDelta += wheel.angleDelta.y;
            // Zoom
            if (root.wheelAccumulatedDelta >= defaultDeltasPerStep) {
                root.zoom(1.5);
                root.wheelAccumulatedDelta = 0;
            } else if (root.wheelAccumulatedDelta <= -defaultDeltasPerStep) {
                var factor = 2. / 3
                root.zoom(factor);
                root.wheelAccumulatedDelta = 0;
            }
        }/* else if (wheel.modifiers & Qt.ShiftModifier) {
            if (K.KdenliveSettings.scrollvertically || rubberSelect.visible) {
                horizontalScroll(wheel)
            } else {
                verticalScroll(wheel)
            }
        } else {
            if (K.KdenliveSettings.scrollvertically) {
                verticalScroll(wheel)
            } else {
                horizontalScroll(wheel)
            }
        }*/
        wheel.accepted = true
    }

    function zoom(factor) {
        root.timeScale = Math.max(1, root.timeScale * factor)
    }

    function deleteSelection() {
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
    }

    function frameToView(position) {
        return position * root.maximumScaleFactor * root.timeScale - (root.contentScroll * root.timeScale * root.maximumScaleFactor)
    }

    function viewToFrame(position) {
        return Math.round(position / root.maximumScaleFactor / root.timeScale)
    }

    function isInView(position) {
        if (position < 0) {
            return false
        }
        var viewPos = frameToView(position)
        return viewPos >= -root.baseUnit/2 && viewPos < root.keyframeContainerWidth + root.baseUnit/2
    }

    function selectRubber(addToSelection) {
        // Start frame
        var startFrame = Math.min(root.rubberBottomRight.x, root.rubberTopLeft.x) - treeView.headerWidth - root.baseUnit + (root.contentScroll * root.timeScale * root.maximumScaleFactor)
        var endFrame = Math.max(root.rubberBottomRight.x, root.rubberTopLeft.x) - treeView.headerWidth - root.baseUnit + (root.contentScroll * root.timeScale * root.maximumScaleFactor)
        startFrame = viewToFrame(startFrame)
        endFrame = viewToFrame(endFrame)
        console.log('SELECTING FRAMES BETWEEN: ', startFrame, '-', endFrame)
        var topPos = mapToItem(treeView, 0, Math.min(root.rubberBottomRight.y, root.rubberTopLeft.y))
        topPos.y = Math.max(0, topPos.y)
        var bottomPos = mapToItem(treeView, 0, Math.max(root.rubberBottomRight.y, root.rubberTopLeft.y))
        bottomPos.y = Math.min(treeView.contentHeight - 1, bottomPos.y)
        var topRow = treeView.cellAtPosition(topPos)
        var bottomRow = treeView.cellAtPosition(bottomPos)
        var result = timeline.dopeSheetModel().selectKeyframeRange(treeView.modelIndex(topRow), treeView.modelIndex(bottomRow), startFrame, endFrame)
        if (!addToSelection) {
            root.allSelectedKeyframes = []
        }
        var indexes = timeline.dopeSheetModel().selectedIndexes()
        while (indexes.length > 0) {
            var id = indexes.pop()
            if (!id.valid) {
                continue
            }
            if (!id.internalId.toString() in result) {
                console.log('ERROR, ABORTING: key ', id, ' not found in MAP...')
                break;
            }
            var kf = result[id.internalId.toString()]
            if (addToSelection) {
                var existingKF = getSelectedKeyframesForIndex(id)
                if (existingKF.length > 0) {
                    kf = kf.concat(existingKF)
                }
            }
            updateSelectedKeyframesForIndex(id, kf, true)
        }
        root.allSelectedKeyframesChanged()
    }

    Menu {
        id: defaultTypeMenu
        ActionGroup {
            id: defTypeActions
            exclusive: true
        }
        MenuItem {
            text: i18n("Use same type as previous keyframe")
            checkable: true
            checked: K.KdenliveSettings.usepreviouskeyframeinterp
            onTriggered: {
                K.KdenliveSettings.usepreviouskeyframeinterp = checked
            }
        }
        ActionGroup {
            id: currTypeActions
            exclusive: true
        }
        Menu {
            title: i18n("Type for selected keyframes")
            Repeater {
                model: keyframeTypeNames
                MenuItem {
                    required property string modelData
                    text: modelData
                    checkable: true
                    action: Action {
                        text: modelData
                        checkable: true
                        ActionGroup.group: currTypeActions
                        onTriggered: {
                            timeline.dopeSheetModel().changeKeyframeType(root.allSelectedKeyframes, keyframeTypes[text])
                            root.keyframeType = keyframeTypes[text]
                        }
                    }
                }
            }
        }
        Menu {
            title: i18n("Default type for new keyframes")
            Repeater {
                model: keyframeTypeNames
                MenuItem {
                    required property string modelData
                    text: modelData
                    checkable: true
                    action: Action {
                        text: modelData
                        checkable: true
                        checked: keyframeTypes[text] == K.KdenliveSettings.defaultkeyframeinterp
                        ActionGroup.group: defTypeActions
                        onTriggered: {
                            console.log('changing default kf type to: ', keyframeTypes[text])
                            K.KdenliveSettings.defaultkeyframeinterp = keyframeTypes[text]
                        }
                    }
                }
            }
        }
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
        contentWidth: Math.min(parent.width, root.frameDuration * root.timeScale)
        contentX: Math.min(root.contentScroll * root.timeScale * root.maximumScaleFactor, root.frameDuration * root.timeScale - width)
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
            scalingFactor: root.timeScale * root.maximumScaleFactor
            maxDuration: root.frameDuration
            hideZone: true
        }
    }
    Rectangle {
        anchors.fill: playheadLabel
        visible: playheadLabel.visible
        radius: 4
        color: activePalette.light
    }
    Label {
        id: playheadLabel
        visible: root.isInView(root.consumerPosition)
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
        visible: playheadLabel.visible
        z: 4
        x: treeView.headerWidth + root.baseUnit + frameToView(root.consumerPosition)
        color: activePalette.text
        width: 1
        Rectangle {
            color: ruler.dimmedColor
            width: Math.max(1, 1 / root.timeScale)
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
        visible: (backgroundArea.containsMouse || treeView.hoveredParam > -1)// && root.isInView(root.mouseFramePos)
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
        x: treeView.headerWidth + root.baseUnit + frameToView(root.mouseFramePos)
        color: activePalette.highlight
    }
    MouseArea {
        id: backgroundArea
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        anchors.leftMargin: treeView.headerWidth
        hoverEnabled: true
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                root.zoomByWheel(wheel)
            } else {
                // Scroll
                root.scrollByWheel(wheel)
            }
        }
        onEntered: {
            treeView.hoveredParam = -1
        }

        onPositionChanged: mouse => {
            var mousePos = Math.max(0., (mouse.x - treeView.headerWidth - root.baseUnit + root.contentScroll * root.timeScale * root.maximumScaleFactor))
            if (mousePos <= 0 && root.mouseFramePos == 0) {
                // In the header zone, ignore
                return
            }
            console.log('MOUSE FRAME TO: ', root.mouseFramePos, ', NEW: ', mousePos)
            root.mouseFramePos = viewToFrame(mousePos)
        }
    }

    Rectangle {
        // Param name background
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: rulercontainer.height
        width: treeView.headerWidth
        color: activePalette.alternateBase
    }

    Menu {
        id: keyframeMenu
        MenuItem {
            text: i18n("Cut")
            enabled: root.hoverKeyframe > -1
            onTriggered: {
            }
        }
        MenuItem {
            text: i18n("Copy")
            enabled: root.hoverKeyframe > -1
            onTriggered: {
                timeline.dopeSheetModel().copyKeyframes(root.allSelectedKeyframes)
            }
        }
        Menu {
            id: typeMenu
            title: i18n("Type")
            ActionGroup {
                id: typeActions
                exclusive: true
            }
            Repeater {
                model: keyframeTypeNames
                MenuItem {
                    required property string modelData
                    text: modelData
                    checkable: true
                    action: Action {
                        text: modelData
                        checkable: true
                        ActionGroup.group: typeActions
                        onTriggered: {
                            console.log('changing kf type to: ', keyframeTypes[text], ' current: ', root.keyframeType)
                            timeline.dopeSheetModel().changeKeyframeType(root.allSelectedKeyframes, keyframeTypes[text])
                            root.keyframeType = keyframeTypes[text]
                        }
                    }
                }
            }
        }
        MenuItem {
            text: i18n("Remove Keyframe")
            onTriggered: {
                if (treeView.selectedKeyframe > -1) {
                    console.log('Removing keyframe')
                    // Double click on a keyframe, remove it
                    timeline.dopeSheetModel().removeKeyframe(treeView.activeIndex, treeView.selectedKeyframe)
                    treeView.selectedKeyframe = -1
                    treeView.activeIndex = -1
                    root.hoverKeyframe = -1
                }
            }
        }
    }

    Menu {
        id: otherMenu
        MenuItem {
            text: i18n("Paste")
        }
        MenuItem {
            text: i18n("Add keyframe")
            onTriggered: {
                if (treeView.selectedKeyframe > -1) {
                    console.log('Adding keyframe')
                    timeline.dopeSheetModel().addKeyframe(treeView.activeIndex, root.mouseFramePos)
                    root.hoverKeyframe = root.mouseFramePos
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
        property int hoveredParam: -1
        property var activeIndex
        property int selectedKeyframe
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
                    color: contentRect.row == treeView.hoveredParam ? Qt.rgba(activePalette.highlight.r * 0.6, activePalette.highlight.g * 0.6, activePalette.highlight.b * 0.6, 1) : activePalette.light
                    border.width: 1
                    border.color: activePalette.shadow
                }
                MouseArea {
                    id: kfMoveArea
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    // The frame position of the clicked keyframe, -1 if none
                    property int clickFrame: -1
                    // The index of the clicked keyframe, -1 if none
                    property int clickIndex: -1
                    // The frame position of the hovered keyframe, -1 if none
                    property int currentFrame: -1
                    // The index of the hovered keyframe, -1 if none
                    property int currentIndex: -1
                    // The last position of a moved keyframe, -1 if none
                    property int movePosition: -1
                    property bool dragStarted: false
                    property point clickPoint
                    property bool shiftClick: false
                    property var buttonClicked
                    anchors.fill: parent
                    anchors.leftMargin: -root.baseUnit
                    anchors.rightMargin: -root.baseUnit
                    hoverEnabled: true
                    onHoveredChanged: {
                        if (containsMouse) {
                            treeView.hoveredParam = contentRect.row
                        }
                    }

                    onPressed: mouse => {
                        clickFrame = currentFrame
                        clickIndex = currentIndex
                        movePosition = -1
                        dragStarted = false
                        clickPoint = Qt.point(mouseX, mouseY)
                        rubberTopLeft = mapToItem(root, mouseX, mouseY)
                        mouse.accepted = true
                        if (mouse.modifiers & Qt.ShiftModifier) {
                            shiftClick = true
                        } else {
                            shiftClick = false
                        }
                        buttonClicked = mouse.buttons
                        // Select parameter
                        treeView.selectionModel.setCurrentIndex(kfContainer.itemIndex, ItemSelectionModel.SelectCurrent)

                        if (clickIndex < 0) {
                            // Not on a keyframe
                            root.keyframeType = -1
                            if (mouse.buttons === Qt.RightButton) {
                                treeView.activeIndex = kfContainer.itemIndex
                                otherMenu.popup()
                            } else {
                                root.clearSelection()
                            }
                            return
                        }
                        root.keyframeType = dopeModel.getKeyframeTypeAtFrame(clickFrame)
                        var selectedKeyframes = getSelectedKeyframesForIndex(kfContainer.itemIndex)
                        var alreadySelected = selectedKeyframes.indexOf(currentIndex) > -1
                        if (mouse.buttons === Qt.RightButton) {
                            if (alreadySelected) {
                                // keyframe already selected, just show menu
                                treeView.selectedKeyframe = kfMoveArea.currentFrame
                                treeView.activeIndex = kfContainer.itemIndex
                                var actionList = typeActions.actions
                                for (var i = 0; i < actionList.length; i++) {
                                    console.log('CHECKING ACTION: ', actionList[i].text, ', TYPE: ', keyframeTypes[actionList[i].text],' == ', root.keyframeType)
                                    if (keyframeTypes[actionList[i].text] == root.keyframeType) {
                                        console.log('CHECK ACTION: ', i)
                                        actionList[i].checked = true
                                        break
                                   }
                                }
                                keyframeMenu.popup()
                                return
                            }
                        }
                        if (!alreadySelected) {
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
                            root.allSelectedKeyframesChanged()
                        }
                        if (mouse.buttons === Qt.RightButton) {
                            treeView.selectedKeyframe = kfMoveArea.currentFrame
                            treeView.activeIndex = kfContainer.itemIndex
                            var actionList = typeActions.actions
                            for (var i = 0; i < actionList.length; i++) {
                                console.log('CHECKING ACTION: ', actionList[i].text, ', TYPE: ', keyframeTypes[actionList[i].text],' == ', root.keyframeType)
                                if (keyframeTypes[actionList[i].text] == root.keyframeType) {
                                    console.log('CHECK ACTION: ', i)
                                    actionList[i].checked = true
                                    break
                                }
                            }
                            keyframeMenu.popup()
                        }
                    }
                    onReleased: mouse => {
                        if (root.rubberSelect) {
                            // Select all keyframes inside our rectangle
                            selectRubber(false)
                            root.rubberSelect = false
                            dragStarted = false
                            return
                        }
                        if (dragStarted) {
                            if (clickIndex > -1) {
                                timeline.dopeSheetModel().moveKeyframe(root.allSelectedKeyframes, movePosition, clickFrame, false)
                                timeline.dopeSheetModel().moveKeyframe(root.allSelectedKeyframes, clickFrame, movePosition, true)
                            }
                        } else if (clickIndex > -1 && buttonClicked === Qt.LeftButton && !shiftClick) {
                            dopeModel.seekToKeyframe(clickIndex)
                        }
                        dragStarted = false
                        mouse.accepted = true
                    }

                    onWheel: wheel => {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            root.zoomByWheel(wheel)
                        } else {
                            // Scroll
                            root.scrollByWheel(wheel)
                        }
                    }

                    onPositionChanged: mouse => {
                        var mousePos = Math.max(0., (mouse.x - root.baseUnit + root.contentScroll * root.timeScale * root.maximumScaleFactor))
                        root.mouseFramePos = viewToFrame(mousePos)
                        if (!pressed) {
                            kfMoveArea.currentFrame = -1
                            kfMoveArea.currentIndex = -1
                            root.hoverKeyframe = -1
                            return
                        }
                        if (!dragStarted && mouse.buttons === Qt.LeftButton) {
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
                            selectRubber(false)
                            return
                        }
                        if (mouse.buttons === Qt.LeftButton && dragStarted && clickIndex > -1) {
                            if (movePosition == root.mouseFramePos) {
                                // No move, abort
                                return
                            }

                            timeline.dopeSheetModel().moveKeyframe(root.allSelectedKeyframes, movePosition < 0 ? clickFrame : movePosition, root.mouseFramePos, false)
                            movePosition = root.mouseFramePos
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
                            root.keyframeType = -1
                            return
                        }
                        if (depth == 0) {
                            timeline.dopeSheetModel().addKeyframe(kfContainer.itemIndex, root.mouseFramePos)
                        } else {
                            console.log('Adding keyframe at: ', root.mouseFramePos)
                            dopeModel.addKeyframe(root.mouseFramePos)
                        }
                        kfMoveArea.currentFrame = root.mouseFramePos
                        root.keyframeType = dopeModel.getKeyframeTypeAtFrame(kfMoveArea.currentFrame)
                        root.hoverKeyframe = root.mouseFramePos
                    }
                }
                Repeater {
                    id: paramModel
                    model: dopeModel
                    Rectangle {
                        id: handle
                        z: 10
                        x: percentPosition * kfContainer.width * root.timeScale - root.contentScroll * root.timeScale * root.maximumScaleFactor - root.baseUnit/2 - ((kfArea.containsMouse || kfArea.pressed) ? 1 : 0)
                        visible : x >= -root.baseUnit/2 - 1 && x < root.keyframeContainerWidth + root.baseUnit/2
                        anchors.verticalCenter: kfContainer.verticalCenter
                        width: root.baseUnit - (kfArea.containsMouse ? 0 : 2)
                        height: width
                        property bool atMousePos: root.mouseFramePos == frame
                        color: keyframeSelected(kfContainer.itemIndex, index) > -1 ? activePalette.highlight : activePalette.light
                        radius: type == 1 ? 0 : Math.round(width/2)
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
                                root.keyframeType = type
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
    K.ZoomBar {
        id: horZoomBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: root.bottom
        }
        height: Math.round(root.baseUnit * 0.7)
        barMinWidth: root.baseUnit
        fitsZoom: root.timeScale === 1 && root.contentScroll === 0
        zoomFactor: 1 / root.timeScale
        onProposeZoomFactor: (proposedValue) => {
            root.timeScale = 1. / proposedValue
            zoomOnBar = true
        }
        contentPos: 0
        onProposeContentPos: (proposedValue) => {
            // The corresponding pixel offset
            root.contentScroll = Math.max(0, proposedValue * root.frameDuration)
            console.log('proposing scroll: ', proposedValue, ', CONTENT SCROLL: ', root.contentScroll, ', SCLAE: ', root.timeScale)
        }
        onZoomByWheel: wheel => root.zoomByWheel(wheel)
        onFitZoom: {
            root.timeScale = 1
            //scrollView.contentX = 0
            //zoomOnBar = true
        }
    }
    ToolButton {
        id: kfTypeButton
        anchors.top: root.top
        anchors.left: root.left
        anchors.topMargin: 2
        anchors.leftMargin: 2
        icon.name: "application-menu"
        width: root.collapsedHeight
        height: width
        onClicked: {
            // Check requiered kfr type
            var actionList = currTypeActions.actions
            for (var i = 0; i < actionList.length; i++) {
                console.log('CHECKING ACTION: ', actionList[i].text, ', TYPE: ', keyframeTypes[actionList[i].text],' == ', root.keyframeType)
                if (keyframeTypes[actionList[i].text] == root.keyframeType) {
                    console.log('CHECK ACTION: ', i)
                    actionList[i].checked = true
                    break
                }
            }
            defaultTypeMenu.popup()
        }
    }
}
