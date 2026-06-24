/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.ki18n

import org.kde.kdenlive as K

Rectangle {
    id: dopeRoot
    anchors.fill: parent
    anchors.margins: 2
    SystemPalette { id: activePalette }
    border.width: 2
    border.color: dopeRoot.viewHasFocus ? activePalette.highlight : activePalette.base
    color: activePalette.base
    property int baseUnit: Math.max(12, fontMetrics.font.pixelSize)
    // Effects duration
    property int inPoint: dopesheetmodel.dopeInPoint
    property int frameDuration: dopesheetmodel.dopeDuration
    property int toolbarHeight: Math.max(28, K.UiUtils.baseSizeMedium * 1.8)
    property int mouseFramePos: -1
    property int hoverKeyframe: -1
    required property var keyframeTypes
    required property K.DopeSheetModel dopesheetmodel
    property K.MonitorProxy proxy
    property var keyframeType
    property int ownerType
    property int ownerId
    property bool viewHasFocus: false
    // The position in frame of the stack owner
    property int offset: dopesheetmodel.dopePosition
    property color hoverColor: "#cc9900"
    // Ruler scaling, 1 means view is fully visible, 2 means zoomed twice
    property real timeScale: 1
    // The maximum timeScale factor, where the full item width is visible
    property real maximumScaleFactor: keyframeContainerWidth / frameDuration
    // Contains a map of item model index / index of selected keyframes
    property var allSelectedKeyframes: []
    // Contains a map of item model index / index of grabbed keyframes, movable with keyboard
    property var grabbedKeyframes: []
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
    readonly property font miniFont: K.UiUtils.smallestReadableFont
    onFrameDurationChanged: {
        console.log('UPDATED DOPE DURATION: ', frameDuration)
        if (frameDuration === 0) {
            keyframeCurve.model = undefined
        }
    }
    onConsumerPositionChanged: {
        rulerCursor.overKeyframe = dopesheetmodel.isOnKeyframe(dopeRoot.consumerPosition, false, getActiveCppParamIndex())
    }

    function updateOverKeyframeFromModel(overKeyframe) {
        rulerCursor.overKeyframe = overKeyframe
    }

    function switchFocus(hasFocus) {
        viewHasFocus = hasFocus
    }

    function setBlockAutoScroll(block) {
        // Placeholder
    }

    function updateOwner(type, id) {
        dopeRoot.ownerType = type
        dopeRoot.ownerId = id
        dopeRoot.contentScroll = 0
        dopeRoot.timeScale = 1
        ruler.adjustStepSize()
    }

    function scrollByWheel(wheel) {
        var proposedPos
        if (wheel.angleDelta.y < 0) {
            proposedPos = Math.max(0, Math.min((horZoomBar.contentPos * dopeRoot.frameDuration - wheel.angleDelta.y) / dopeRoot.frameDuration, 1 - 1 / dopeRoot.timeScale))
        } else {
            proposedPos = Math.max(horZoomBar.contentPos * dopeRoot.frameDuration - wheel.angleDelta.y, 0) / dopeRoot.frameDuration
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
            dopeRoot.wheelAccumulatedDelta += wheel.angleDelta.y;
            // Zoom
            if (dopeRoot.wheelAccumulatedDelta >= defaultDeltasPerStep) {
                dopeRoot.zoom(1.5);
                dopeRoot.wheelAccumulatedDelta = 0;
            } else if (dopeRoot.wheelAccumulatedDelta <= -defaultDeltasPerStep) {
                var factor = 2. / 3
                dopeRoot.zoom(factor);
                dopeRoot.wheelAccumulatedDelta = 0;
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
        dopeRoot.timeScale = Math.max(1, dopeRoot.timeScale * factor)
    }

    function deleteSelection() {
        console.log('deleting kfs: ', dopeRoot.allSelectedKeyframes)
        var indexList = []
        var keyframesList = []
        while (dopeRoot.allSelectedKeyframes.length > 0) {
            var elem = dopeRoot.allSelectedKeyframes.pop()
            indexList.push(elem.index)
            // Sort keyframes in reverse order for deletion
            elem.kfrs.sort(function(a, b) {
              return b - a;
            });
            keyframesList.push(elem.kfrs)
        }
        dopesheetmodel.removeKeyframes(indexList, keyframesList)
    }

    function clearGrabAndSelection() {
        dopeRoot.grabbedKeyframes = []
        clearSelection();
    }

    function grabKeyframes() {
        dopeRoot.grabbedKeyframes = dopeRoot.allSelectedKeyframes
        console.log('+++++++++++++\nGRABBING KEYFRAMES\n++++++++++++++')
    }

    function keyframeSelected(itemIndex, keyframeIndex) {
        var selectedKeyframes = getSelectedKeyframesForIndex(itemIndex)
        return selectedKeyframes.indexOf(keyframeIndex)
    }

    function keyframeGrabbed(itemIndex, keyframeIndex) {
        var grabbedKeyframes = getGrabbedKeyframesForIndex(itemIndex)
        return grabbedKeyframes.indexOf(keyframeIndex)
    }

    function clearSelection() {
        dopeRoot.allSelectedKeyframes = []
        dopeRoot.allSelectedKeyframesChanged()
    }

    function getSelectedKeyframesForIndex(itemIndex) {
        var ix = 0
        while (ix < dopeRoot.allSelectedKeyframes.length) {
            var elem = dopeRoot.allSelectedKeyframes[ix]
            if (elem.index === itemIndex) {
                return elem.kfrs
            } else {
                ix++
            }
        }
        return []
    }

    function getGrabbedKeyframesForIndex(itemIndex) {
        var ix = 0
        while (ix < dopeRoot.grabbedKeyframes.length) {
            var elem = dopeRoot.grabbedKeyframes[ix]
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
            dopeRoot.allSelectedKeyframes = []
        } else {
            while (ix < dopeRoot.allSelectedKeyframes.length) {
                var elem = dopeRoot.allSelectedKeyframes[ix]
                if (elem.index === itemIndex) {
                    dopeRoot.allSelectedKeyframes.splice(ix, 1);
                } else {
                    ix++
                }
            }
        }
        dopeRoot.allSelectedKeyframes.push({index: itemIndex, kfrs: itemKeyframes})
    }

    function frameToView(position) {
        return position * dopeRoot.maximumScaleFactor * dopeRoot.timeScale - (dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
    }

    function viewToFrame(position) {
        return Math.round(position / dopeRoot.maximumScaleFactor / dopeRoot.timeScale)
    }

    function isInView(position) {
        if (position < 0) {
            return false
        }
        var viewPos = frameToView(position)
        return viewPos >= -K.UiUtils.baseSizeMedium/2 && viewPos < dopeRoot.keyframeContainerWidth + K.UiUtils.baseSizeMedium/2
    }

    function selectRubber(addToSelection) {
        // Start frame
        var startFrame = Math.min(dopeRoot.rubberBottomRight.x, dopeRoot.rubberTopLeft.x) - treeView.headerWidth - K.UiUtils.baseSizeMedium + (dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        var endFrame = Math.max(dopeRoot.rubberBottomRight.x, dopeRoot.rubberTopLeft.x) - treeView.headerWidth - K.UiUtils.baseSizeMedium + (dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        startFrame = viewToFrame(startFrame)
        endFrame = viewToFrame(endFrame)
        console.log('SELECTING FRAMES BETWEEN: ', startFrame, '-', endFrame)
        var topPos = mapToItem(treeView, 0, Math.min(dopeRoot.rubberBottomRight.y, dopeRoot.rubberTopLeft.y))
        topPos.y = Math.max(0, topPos.y)
        var bottomPos = mapToItem(treeView, 0, Math.max(dopeRoot.rubberBottomRight.y, dopeRoot.rubberTopLeft.y))
        bottomPos.y = Math.min(treeView.contentHeight - 1, bottomPos.y)
        var topRow = treeView.cellAtPosition(topPos)
        var bottomRow = treeView.cellAtPosition(bottomPos)
        var result = dopesheetmodel.selectKeyframeRange(treeView.modelIndex(topRow), treeView.modelIndex(bottomRow), startFrame, endFrame)
        updateSelectedKeyframesFromModel(result, addToSelection)
    }

    function updateSelectedKeyframesFromModel(result, addToSelection) {
        if (!addToSelection) {
            dopeRoot.allSelectedKeyframes = []
        }
        var indexes = dopesheetmodel.selectedIndexes()
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
        dopeRoot.allSelectedKeyframesChanged()
    }

    function updateGrabbedKeyframesFromModel() {
        dopeRoot.grabbedKeyframes = dopeRoot.allSelectedKeyframes
        var indexes = dopesheetmodel.grabbedIndexes()
        dopeRoot.grabbedKeyframesChanged()
    }

    function moveGrab(left) {
        dopesheetmodel.moveKeyframe(dopeRoot.grabbedKeyframes, dopeRoot.mouseFramePos, dopeRoot.mouseFramePos + (left ? -1 : 1), true)
    }

    function getActiveCppParamIndex() {
        console.log('Current SELECTION INDEX: ', treeView.selectionModel.currentIndex)
        var item = treeView.itemAtIndex(treeView.selectionModel.currentIndex)
        if (item) {
            return treeView.model.index(treeView.selectionModel.currentIndex.row, treeView.selectionModel.currentIndex.column, treeView.selectionModel.currentIndex.parent)
        }
        console.log('NO ACTIVE ITEM FOUND IN DOPESHEET...')
        return treeView.index(-1, -1)
    }

    function setActiveIndexFromModel(index) {
        console.log('READY TO SET MODEINDEX FROM C++: ', index)
        // Ensure item is visible
        let modelIndex = treeView.model.index(index, 0)
        var currentIx = treeView.selectionModel.currentIndex
        if (currentIx.parent && currentIx.parent.valid) {
            if (currentIx.parent.row === modelIndex.row) {
                return
            }
        } else if (currentIx.row === modelIndex.row) {
            return
        }
        treeView.selectionModel.setCurrentIndex(modelIndex, ItemSelectionModel.SelectCurrent);
    }

    function setActiveIndex(index) {
        if (index.parent && index.parent.valid) {
            dopesheetmodel.setActiveIndex(index.parent.row)
        } else {
            dopesheetmodel.setActiveIndex(index.row)
        }
    }

    function pasteKeyframes(position=undefined) {
        if (position === undefined) {
            position = dopeRoot.consumerPosition
        }
        console.log('pasting to pos: ', position)
        dopesheetmodel.slotPasteKeyframeFromClipBoard(position)
    }

    function copyKeyframes() {
        dopesheetmodel.copySelectedKeyframes(getActiveCppParamIndex(), dopeRoot.allSelectedKeyframes)
    }

    Keys.onDownPressed: {
        if (treeView.selectionModel.currentIndex.parent != treeView.rootIndex) {
            // Child Item
            var parentIndex = treeView.selectionModel.currentIndex.parent
            if (treeView.selectionModel.currentIndex.row < treeView.model.rowCount(parentIndex) - 1) {
                treeView.selectionModel.setCurrentIndex(treeView.model.index(treeView.selectionModel.currentIndex.row + 1, treeView.selectionModel.currentIndex.column, parentIndex), ItemSelectionModel.SelectCurrent)
                return
            }
            // Move to next top level item
            if (parentIndex.row < treeView.model.rowCount(treeView.rootIndex) - 1) {
                treeView.selectionModel.setCurrentIndex(treeView.model.index(parentIndex.row + 1, treeView.selectionModel.currentIndex.column), ItemSelectionModel.SelectCurrent)
                return
            }
        }
        // Move to next child or top level item
        if (treeView.model.rowCount(treeView.selectionModel.currentIndex) > 0) {
            treeView.selectionModel.setCurrentIndex(treeView.model.index(0, 0, treeView.selectionModel.currentIndex), ItemSelectionModel.SelectCurrent)
        } else {
            if (treeView.selectionModel.currentIndex.row < treeView.model.rowCount(treeView.rootIndex) - 1) {
                treeView.selectionModel.setCurrentIndex(treeView.model.index(treeView.selectionModel.currentIndex.row + 1, treeView.selectionModel.currentIndex.column), ItemSelectionModel.SelectCurrent)
            }
        }
    }

    Keys.onUpPressed: {
        if (treeView.selectionModel.currentIndex.parent != treeView.rootIndex) {
            // Child Item
            var parentIndex = treeView.selectionModel.currentIndex.parent
            if (treeView.selectionModel.currentIndex.row > 0) {
                treeView.selectionModel.setCurrentIndex(treeView.model.index(treeView.selectionModel.currentIndex.row - 1, treeView.selectionModel.currentIndex.column, parentIndex), ItemSelectionModel.SelectCurrent)
                return
            }
            // Move to top level item
            treeView.selectionModel.setCurrentIndex(parentIndex, ItemSelectionModel.SelectCurrent)
            return
        }
        // Move to next child or top level item
        if (treeView.selectionModel.currentIndex.row > 0) {
            var upperItem = treeView.model.index(treeView.selectionModel.currentIndex.row - 1, treeView.selectionModel.currentIndex.column)
            if (treeView.model.rowCount(upperItem) > 0) {
                treeView.selectionModel.setCurrentIndex(treeView.model.index(treeView.model.rowCount(upperItem) - 1, treeView.selectionModel.currentIndex.column, upperItem), ItemSelectionModel.SelectCurrent)
            } else {
                treeView.selectionModel.setCurrentIndex(upperItem, ItemSelectionModel.SelectCurrent)
            }
        }
    }

    Menu {
        id: defaultTypeMenu
        ActionGroup {
            id: defTypeActions
            exclusive: true
        }
        MenuItem {
            text: KI18n.i18n("Use same type as previous keyframe")
            checkable: true
            checked: K.KdenliveSettings.usepreviouskeyframeinterp
            onTriggered: {
                K.KdenliveSettings.usepreviouskeyframeinterp = checked
            }
        }
        Menu {
            title: KI18n.i18n("Default type for new keyframes")
            Repeater {
                model: keyframeTypes
                MenuItem {
                    required property var modelData
                    text: modelData.text
                    checkable: true
                    action: Action {
                        text: modelData.text
                        checkable: true
                        checked: modelData.value === K.KdenliveSettings.defaultkeyframeinterp
                        ActionGroup.group: defTypeActions
                        onTriggered: {
                            console.log('changing default kf type to: ', modelData.value)
                            K.KdenliveSettings.defaultkeyframeinterp = modelData.value
                        }
                    }
                }
            }
        }
    }
    Item {
        id: dopeBar
        height: dopeRoot.toolbarHeight + 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        RowLayout {
            //anchors.fill: parent
            ToolButton {
                id: kfTypeButton
                icon.name: "application-menu"
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                onClicked: {
                    // Check required kfr type
                    defaultTypeMenu.popup()
                }
            }
            ToolButton {
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                icon.name: rulerCursor.overKeyframe ? "keyframe-remove" : "keyframe-add"
                ToolTip.text: KI18n.i18n("Add/Remove Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: K.Core.triggerAction('keyframe_add')
            }
            ToolButton {
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                icon.name: "align-horizontal-center"
                enabled: dopeRoot.allSelectedKeyframes.length > 0
                ToolTip.text: KI18n.i18n("Align Keyframe to Playhead")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, -1, dopeRoot.consumerPosition, true)
            }
            ToolButton {
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                icon.name: "edit-copy"
                enabled: dopeRoot.allSelectedKeyframes.length > 0
                ToolTip.text: KI18n.i18n("Copy Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: copyKeyframes()
            }
            ToolButton {
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                icon.name: "edit-paste"
                ToolTip.text: KI18n.i18n("Paste Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: dopeRoot.pasteKeyframes()
            }
            ToolButton {
                icon.name: "arrow-left"
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                enabled: dopeRoot.consumerPosition > 0
                ToolTip.text: KI18n.i18n("Add/Remove Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: K.Core.triggerAction('monitor_seek_snap_backward')
            }
            ToolButton {
                icon.name: "arrow-right"
                implicitWidth: dopeRoot.toolbarHeight
                implicitHeight: width
                enabled: dopeRoot.consumerPosition < dopeRoot.frameDuration - 1
                onClicked: {
                    console.log('FWD CLICK: ', dopeRoot.consumerPosition, ' < ', dopeRoot.frameDuration)
                    K.Core.triggerAction('monitor_seek_snap_forward')
                }
            }
            ComboBox {
                id: keyframeTypeCombo
                model: keyframeTypes
                textRole: "text"
                valueRole: "value"
                currentValue: dopeRoot.keyframeType
                enabled: dopeRoot.allSelectedKeyframes.length > 0
                ToolTip.text: KI18n.i18n("Type for selected keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onActivated: {
                    console.log('changing kf type to: ', currentValue, ' current: ', dopeRoot.keyframeType)
                    dopesheetmodel.changeKeyframeType(dopeRoot.allSelectedKeyframes, currentValue)
                    dopeRoot.keyframeType = currentValue
                }
            }
        }
    }

    Flickable {
        // scroll area for the Ruler.
        id: rulercontainer
        anchors.top: dopeBar.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.leftMargin: K.UiUtils.baseSizeMedium + treeView.headerWidth
        anchors.rightMargin: K.UiUtils.baseSizeMedium
        height: Math.round(K.UiUtils.baseSizeMedium * 2.5)
        contentWidth: Math.min(parent.width, dopeRoot.frameDuration * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        contentX: Math.min(dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor, dopeRoot.frameDuration * dopeRoot.timeScale - width)
        interactive: false
        clip: true
        onWidthChanged: {
            ruler.adjustStepSize()
        }
        DopeRuler {
            id: ruler
            width: rulercontainer.width
            height: rulercontainer.height
            rulerOffset: dopeRoot.offset
            scalingFactor: dopeRoot.timeScale * dopeRoot.maximumScaleFactor
        }
    }
    Rectangle {
        anchors.fill: playheadLabel
        visible: playheadLabel.visible
        radius: 4
        color: rulerCursor.overKeyframe ? dopeRoot.hoverColor : activePalette.light
    }
    Label {
        id: playheadLabel
        visible: dopeRoot.isInView(dopeRoot.consumerPosition)
        anchors.top: rulercontainer.top
        anchors.horizontalCenter: rulerCursor.horizontalCenter
        text: dopeRoot.consumerPosition
        leftPadding: 6
        rightPadding: 6
    }
    Rectangle {
        // Vertical line over ruler zone
        id: rulerCursor
        anchors.top: playheadLabel.bottom
        anchors.bottom: parent.bottom
        visible: playheadLabel.visible
        property bool overKeyframe: false
        z: 4
        x: treeView.headerWidth + K.UiUtils.baseSizeMedium + frameToView(dopeRoot.consumerPosition)
        color: activePalette.text
        width: 1
        Rectangle {
            color: ruler.dimmedColor
            width: Math.max(1, 1 / dopeRoot.timeScale)
            height: 1
            visible: width > K.UiUtils.baseSizeMedium * 1.2
        }
    }
    Rectangle {
        anchors.fill: mouseLabel
        visible: mouseLabel.visible
        radius: 4
        color: dopeRoot.hoverKeyframe > -1 ? dopeRoot.hoverColor : activePalette.highlight
    }
    Label {
        id: mouseLabel
        visible: !ruler.pressed && (backgroundArea.containsMouse || treeView.hoveredParam > -1)
        anchors.top: rulercontainer.top
        anchors.horizontalCenter: mouseLine.horizontalCenter
        text: dopeRoot.mouseFramePos
        leftPadding: 6
        rightPadding: 6
    }
    Rectangle {
        id: mouseLine
        anchors.top: mouseLabel.bottom
        anchors.bottom: dopeRoot.bottom
        z: 5
        width: 1
        visible: mouseLabel.visible
        x: treeView.headerWidth + K.UiUtils.baseSizeMedium + frameToView(dopeRoot.mouseFramePos)
        color: activePalette.highlight
    }
    MouseArea {
        id: backgroundArea
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        anchors.topMargin: dopeBar.height
        anchors.leftMargin: treeView.headerWidth
        hoverEnabled: true
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                dopeRoot.zoomByWheel(wheel)
            } else {
                // Scroll
                dopeRoot.scrollByWheel(wheel)
            }
        }
        onEntered: {
            treeView.hoveredParam = -1
        }

        onPositionChanged: mouse => {
            var mousePos = Math.max(0., (mouse.x - K.UiUtils.baseSizeMedium + dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor))
            if (mousePos <= 0 && dopeRoot.mouseFramePos == 0) {
                // In the header zone, ignore
                return
            }
            dopeRoot.mouseFramePos = viewToFrame(mousePos)
            console.log('MOUSE FRAME TO: ', dopeRoot.mouseFramePos, ', NEW: ', mousePos)
        }
    }

    Rectangle {
        // Param name background
        id: headerBg
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.bottom: parent.bottom
        anchors.top: rulercontainer.bottom
        width: treeView.headerWidth
        color: activePalette.alternateBase
    }

    Menu {
        id: keyframeMenu
        MenuItem {
            text: KI18n.i18n("Cut")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: {
            }
        }
        MenuItem {
            text: KI18n.i18n("Copy")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: dopeRoot.copyKeyframes()
        }
        MenuItem {
            text: KI18n.i18n("Paste")
            onTriggered: dopeRoot.pasteKeyframes(dopeRoot.mouseFramePos)
        }
        MenuItem {
            text: KI18n.i18n("Move to Playhead")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: {
                dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, dopeRoot.mouseFramePos, dopeRoot.consumerPosition, true)
            }
        }
        MenuItem {
            text: KI18n.i18n("Align Left")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: {
                dopesheetmodel.alignKeyframe(dopeRoot.allSelectedKeyframes, false)
            }
        }
        MenuItem {
            text: KI18n.i18n("Align Right")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: {
                dopesheetmodel.alignKeyframe(dopeRoot.allSelectedKeyframes, true)
            }
        }
        Menu {
            id: typeMenu
            title: KI18n.i18n("Type")
            ActionGroup {
                id: typeActions
                exclusive: true
            }
            Repeater {
                model: keyframeTypes
                MenuItem {
                    required property var modelData
                    text: modelData.text
                    checkable: true
                    action: Action {
                        text: modelData.text
                        checkable: true
                        ActionGroup.group: typeActions
                        onTriggered: {
                            console.log('changing kf type to: ', modelData.value, ' current: ', dopeRoot.keyframeType)
                            dopesheetmodel.changeKeyframeType(dopeRoot.allSelectedKeyframes, modelData.value)
                            dopeRoot.keyframeType = modelData.value
                        }
                    }
                }
            }
        }
        MenuItem {
            text: KI18n.i18n("Remove Keyframe")
            onTriggered: {
                if (treeView.selectedKeyframe > -1) {
                    console.log('Removing keyframe')
                    // Double click on a keyframe, remove it
                    dopesheetmodel.removeKeyframe(treeView.activeIndex, treeView.selectedKeyframe)
                    treeView.selectedKeyframe = -1
                    treeView.activeIndex = -1
                    dopeRoot.hoverKeyframe = -1
                }
            }
        }
    }

    Menu {
        id: otherMenu
        MenuItem {
            text: KI18n.i18n("Paste")
            onTriggered: dopeRoot.pasteKeyframes(dopeRoot.mouseFramePos)
        }
        MenuItem {
            text: KI18n.i18n("Add keyframe")
            onTriggered: {
                if (treeView.selectedKeyframe > -1) {
                    console.log('Adding keyframe')
                    dopesheetmodel.addKeyframe(treeView.activeIndex, dopeRoot.mouseFramePos)
                    dopeRoot.hoverKeyframe = dopeRoot.mouseFramePos
                }
            }
        }
    }

    TreeView {
        // The model needs to be a QAbstractItemModel
        id: treeView
        model: dopesheetmodel
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: keyframeContainer.top
        anchors.top: rulercontainer.bottom
        property int headerWidth: 100
        property int hoveredParam: -1
        property var activeIndex
        property int selectedKeyframe
        clip: true
        // Disable flicking
        acceptedButtons: Qt.NoButton
        selectionModel: ItemSelectionModel {
            model: dopesheetmodel
            onCurrentChanged: (current, previous) => {
                rulerCursor.overKeyframe = dopesheetmodel.isOnKeyframe(dopeRoot.consumerPosition, false, getActiveCppParamIndex())
                if (current.valid && current.parent) {
                    var activeIndex = getActiveCppParamIndex()
                    keyframeCurve.model = dopesheetmodel.getKeyframeModel(activeIndex)
                    let cppEffectIndex = treeView.index(activeIndex.parent === treeView.rootIndex ? activeIndex.row : activeIndex.parent.row, 0)
                    dopesheetmodel.setActiveIndex(cppEffectIndex)
                }
            }
        }
        // You can set a custom delegate or use a built-in TreeViewDelegate
        delegate: Item {
            id: contentRect
            implicitWidth: dopeRoot.width
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
                color: 'darkgoldenrod'
                opacity: 0.3
                visible: depth == 0
                x: 4
                anchors.fill: parent
                //height: parent.height
                //width: treeView.headerWidth
            }
            Rectangle {
                color: Qt.rgba(activePalette.highlight.r * 0.6, activePalette.highlight.g * 0.6, activePalette.highlight.b * 0.6, 1)
                radius: 4
                visible: current
                x: 4
                height: parent.height
                width: treeView.headerWidth
            }
            property Animation indicatorAnimation: NumberAnimation {
                target: indicator
                property: "rotation"
                from: expanded ? 0 : 90
                to: expanded ? 90 : 0
                duration: 200
                easing.type: Easing.OutQuart
            }
            TableView.onPooled: indicatorAnimation.complete()
            TableView.onReused: if (current) indicatorAnimation.start()
            onExpandedChanged: indicator.rotation = expanded ? 90 : 0

            ToolButton {
                id: indicator
                icon.name: "arrow-right"
                visible: contentRect.hasChildren
                onClicked: {
                    treeView.toggleExpanded(row)
                }
                x: contentRect.padding
                height: paramLabel.height
                width: height
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                id: paramLabel
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: indicator.right
                //anchors.leftMargin: contentRect.padding
                text: dopeName
                rightPadding: 4
                leftPadding: 4
                font.bold: depth == 0
                Component.onCompleted: {
                    if (treeView.headerWidth < (paramLabel.width + indicator.width + contentRect.padding)) {
                        treeView.headerWidth = paramLabel.width + indicator.width + contentRect.padding
                    }
                }
            }
            Item {
                id: kfContainer
                anchors.left: contentRect.left
                anchors.right: contentRect.right
                anchors.top: contentRect.top
                anchors.bottom: contentRect.bottom
                anchors.leftMargin: K.UiUtils.baseSizeMedium + treeView.headerWidth
                anchors.rightMargin: K.UiUtils.baseSizeMedium / 2 + 2
                Rectangle {
                    // keyframe slider
                    id: keyframeSlider
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: Math.max(4, K.UiUtils.baseSizeMedium / 4)
                    radius: height/3
                    color: contentRect.current ? activePalette.highlight : contentRect.row == treeView.hoveredParam ? Qt.rgba(activePalette.highlight.r * 0.6, activePalette.highlight.g * 0.6, activePalette.highlight.b * 0.6, 1) : activePalette.light
                    border.width: 1
                    border.color: activePalette.shadow
                }
                MouseArea {
                    id: kfMoveArea
                    anchors.fill: parent
                    anchors.leftMargin: -K.UiUtils.baseSizeMedium
                    anchors.rightMargin: -K.UiUtils.baseSizeMedium
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
                    property bool ctrlClick: false
                    property var buttonClicked
                    hoverEnabled: true
                    onHoveredChanged: {
                        if (containsMouse) {
                            treeView.hoveredParam = contentRect.row
                            console.log('COmparing slider width: ', keyframeSlider.width, ' == ', keyframeContainer.width)
                        }
                    }

                    onPressed: mouse => {
                        clickFrame = currentFrame
                        clickIndex = currentIndex
                        movePosition = -1
                        dragStarted = false
                        clickPoint = Qt.point(mouseX, mouseY)
                        rubberTopLeft = mapToItem(dopeRoot, mouseX, mouseY)
                        mouse.accepted = true
                        shiftClick = mouse.modifiers & Qt.ShiftModifier
                        ctrlClick = mouse.modifiers & Qt.ControlModifier
                        buttonClicked = mouse.buttons
                        // Select parameter
                        var parameterIndex = treeView.index(row, column)
                        treeView.selectionModel.setCurrentIndex(parameterIndex, ItemSelectionModel.SelectCurrent);

                        if (clickIndex < 0) {
                            // Not on a keyframe
                            dopeRoot.keyframeType = -1
                            if (mouse.buttons === Qt.RightButton) {
                                treeView.activeIndex = parameterIndex
                                otherMenu.popup()
                            } else {
                                dopeRoot.clearSelection()
                            }
                            return
                        }
                        dopeRoot.keyframeType = dopeModel.getKeyframeTypeAtFrame(clickFrame)
                        var selectedKeyframes = getSelectedKeyframesForIndex(parameterIndex)
                        var alreadySelected = selectedKeyframes.indexOf(currentIndex) > -1
                        var actionList = typeActions.actions
                        if (mouse.buttons === Qt.RightButton) {
                            if (alreadySelected) {
                                // keyframe already selected, just show menu
                                treeView.selectedKeyframe = kfMoveArea.currentFrame
                                treeView.activeIndex = parameterIndex
                                var matchingText
                                for (var i = 0; i < keyframeTypes.length; i++) {
                                    if (keyframeTypes[i].value === dopeRoot.keyframeType) {
                                        matchingText = keyframeTypes[i].text
                                        break
                                    }
                                }

                                for (var j = 0; j < actionList.length; j++) {
                                    if (actionList[j].text === matchingText) {
                                        console.log('CHECK ACTION: ', j)
                                        actionList[j].checked = true
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
                            if (contentRect.hasChildren) {
                                // Top level item, build index of related kf to select
                                dopesheetmodel.buildMasterSelection(parameterIndex, clickIndex)
                                var result = dopesheetmodel.selectKeyframeAtPos(parameterIndex, clickFrame)
                                updateSelectedKeyframesFromModel(result, shiftClick)
                                return
                            }

                            updateSelectedKeyframesForIndex(parameterIndex, selectedKeyframes, shiftClick)
                            dopeRoot.allSelectedKeyframesChanged()
                        }
                        if (ctrlClick) {
                            dopesheetmodel.setScaledInfo(dopeRoot.allSelectedKeyframes, clickFrame)
                        }
                        if (mouse.buttons === Qt.RightButton) {
                            // Show context menu
                            treeView.selectedKeyframe = kfMoveArea.currentFrame
                            treeView.activeIndex = parameterIndex
                            for (i = 0; i < keyframeTypes.length; i++) {
                                if (keyframeTypes[i].value === dopeRoot.keyframeType) {
                                    matchingText = keyframeTypes[i].text
                                    break
                               }
                            }

                            for (j = 0; j < actionList.length; j++) {
                                if (actionList[j].text === matchingText) {
                                    console.log('CHECK ACTION: ', j)
                                    actionList[j].checked = true
                                    break
                                }
                            }
                            keyframeMenu.popup()
                        }
                    }
                    onReleased: mouse => {
                        if (dopeRoot.rubberSelect) {
                            // Select all keyframes inside our rectangle
                            selectRubber(false)
                            dopeRoot.rubberSelect = false
                            dragStarted = false
                            return
                        }
                        if (dragStarted) {
                            if (ctrlClick) {
                                dopesheetmodel.moveScaledKeyframe(clickFrame, false, false)
                                dopesheetmodel.moveScaledKeyframe(movePosition, true, true)
                            } else if (clickIndex > -1) {
                                dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, movePosition, clickFrame, false)
                                dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, clickFrame, movePosition, true)
                                kfMoveArea.currentFrame = movePosition
                            }
                        } else if (clickIndex > -1 && buttonClicked === Qt.LeftButton && !shiftClick) {
                            dopeModel.seekToKeyframe(clickIndex)
                        }
                        if (ctrlClick) {
                            dopesheetmodel.resetScaledInfo()
                        }
                        dragStarted = false
                        mouse.accepted = true
                    }

                    onWheel: wheel => {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            dopeRoot.zoomByWheel(wheel)
                        } else {
                            // Scroll
                            dopeRoot.scrollByWheel(wheel)
                        }
                    }

                    onPositionChanged: mouse => {
                        var mousePos = Math.max(0., (mouse.x - K.UiUtils.baseSizeMedium + dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor))
                        dopeRoot.mouseFramePos = viewToFrame(mousePos)
                        if (!pressed) {
                            kfMoveArea.currentFrame = -1
                            kfMoveArea.currentIndex = -1
                            dopeRoot.hoverKeyframe = -1
                            return
                        }
                        if (!dragStarted && mouse.buttons === Qt.LeftButton) {
                            if (Math.abs(mouseX - clickPoint.x) + Math.abs(mouseY - clickPoint.y) > Application.styleHints.startDragDistance) {
                                console.log(' - - - DRAG STARTED -- - ')
                                dragStarted = true
                                if (shiftClick) {
                                    // Start rectangle selection
                                    dopeRoot.rubberSelect = true
                                    dopeRoot.rubberBottomRight = mapToItem(dopeRoot, mouseX, mouseY)
                                    return
                                }
                            }
                        } else if (shiftClick) {
                            // Update rectangle selection
                            dopeRoot.rubberBottomRight = mapToItem(dopeRoot, mouseX, mouseY)
                            selectRubber(false)
                            return
                        }
                        if (mouse.buttons === Qt.LeftButton && dragStarted && clickIndex > -1) {
                            if (movePosition == dopeRoot.mouseFramePos) {
                                // No move, abort
                                return
                            }
                            if (ctrlClick) {
                                dopesheetmodel.moveScaledKeyframe(dopeRoot.mouseFramePos, false, true)
                            } else {
                                dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, movePosition < 0 ? clickFrame : movePosition, dopeRoot.mouseFramePos, false)
                            }
                            movePosition = dopeRoot.mouseFramePos
                        }
                    }
                    onDoubleClicked: mouse => {
                        var parameterIndex = treeView.index(row, column)
                        if (kfMoveArea.currentFrame > -1) {
                            console.log('Removing keyframe')
                            // Double click on a keyframe, remove it
                            if (contentRect.hasChildren) {
                                dopesheetmodel.removeKeyframe(parameterIndex, kfMoveArea.currentFrame)
                            } else {
                                dopeModel.removeKeyframe(kfMoveArea.currentFrame)
                            }
                            kfMoveArea.currentFrame = -1
                            kfMoveArea.currentIndex = -1
                            dopeRoot.hoverKeyframe = -1
                            dopeRoot.keyframeType = -1
                            return
                        }
                        if (contentRect.hasChildren) {
                            dopesheetmodel.addKeyframe(parameterIndex, dopeRoot.mouseFramePos)
                        } else {
                            console.log('Adding keyframe at: ', dopeRoot.mouseFramePos)
                            dopeModel.addKeyframe(dopeRoot.mouseFramePos)
                        }
                        kfMoveArea.currentFrame = dopeRoot.mouseFramePos
                        dopeRoot.keyframeType = dopeModel.getKeyframeTypeAtFrame(kfMoveArea.currentFrame)
                        dopeRoot.hoverKeyframe = dopeRoot.mouseFramePos
                    }
                }
                Repeater {
                    id: paramModel
                    model: dopeModel
                    property int handleWidth: Math.round(K.UiUtils.baseSizeMedium * 0.8)
                    Rectangle {
                        id: handle
                        z: 10
                        x: percentPosition * kfContainer.width * dopeRoot.timeScale - dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor - width / 2
                        visible : x >= -width/2 && x < dopeRoot.keyframeContainerWidth + width/2
                        anchors.verticalCenter: kfContainer.verticalCenter
                        width: paramModel.handleWidth - (kfArea.containsMouse ? 0 : 2)
                        height: width
                        property bool atMousePos: dopeRoot.mouseFramePos === model.frame
                        color: keyframeGrabbed(treeView.index(contentRect.row, contentRect.column), index) > -1 ? 'red' : keyframeSelected(treeView.index(contentRect.row, contentRect.column), index) > -1 ? activePalette.highlight : activePalette.light
                        radius: type == 1 ? 0 : Math.round(width/2)
                        border.width: atMousePos ? 2 : 1
                        border.color: (kfArea.containsMouse || kfArea.pressed) ? activePalette.highlight : atMousePos ? dopeRoot.hoverColor : activePalette.text

                        MouseArea {
                            id: kfArea
                            anchors.fill: handle
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.NoButton
                            onEntered: {
                                console.log("entered kfr: ", index)
                                kfMoveArea.currentFrame = model.frame
                                kfMoveArea.currentIndex = index
                                //dopeRoot.keyframeType = type
                                dopeRoot.hoverKeyframe = model.frame
                                dopeRoot.mouseFramePos = model.frame
                            }
                            ToolTip.text: model.description
                            ToolTip.delay: 1000
                            ToolTip.visible: containsMouse && !kfMoveArea.pressed
                        }
                    }
                }
                Component.onCompleted: {
                    console.log('Loaded TREEVIEW COMPONENT ID: ', treeView.model.index(contentRect.row, contentRect.column))
                    console.log('CONTAINER WIDTH CHANGED ON COMPLETION: ', keyframeContainer.width, ' / RULER: ', rulercontainer.width, 'CONTEINER CONTENR: ', keyframeContainer.contentWidth, 'TREEVIEW: ', treeView.width, ' - ', treeView.headerWidth,'\n.....................')
                }
            }
        }
    }
    Rectangle {
        color: "#33FFFFFF"
        border.color: activePalette.highlight
        border.width: 1
        visible: dopeRoot.rubberSelect
        x: Math.min(dopeRoot.rubberTopLeft.x, dopeRoot.rubberBottomRight.x)
        y: Math.min(dopeRoot.rubberTopLeft.y, dopeRoot.rubberBottomRight.y)
        width: Math.abs(dopeRoot.rubberBottomRight.x - dopeRoot.rubberTopLeft.x)
        height: Math.abs(dopeRoot.rubberBottomRight.y - dopeRoot.rubberTopLeft.y)
    }

    Flickable {
        id: keyframeContainer
        anchors.left: dopeRoot.left
        anchors.leftMargin: K.UiUtils.baseSizeMedium + treeView.headerWidth
        anchors.right: dopeRoot.right
        anchors.rightMargin: K.UiUtils.baseSizeMedium
        anchors.bottom: horZoomBar.top
        anchors.bottomMargin: 2
        height: keyframeCurve.model === undefined ? 0 : K.UiUtils.baseSizeMedium * 4
        contentWidth: Math.max(dopeRoot.keyframeContainerWidth, dopeRoot.frameDuration * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        contentHeight: height
        contentX: Math.min(dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor, dopeRoot.frameDuration * dopeRoot.timeScale * dopeRoot.maximumScaleFactor - width)
        interactive: false

        clip: true
        Loader {
            // Keyframe curve
            id: keyframeCurve
            anchors.fill: parent
            property var model: undefined
            property bool hasKeyframes: false
            property bool isPanning: false
            asynchronous: true
            visible: status == Loader.Ready
            active: true
            source: keyframeCurve.model === undefined ? "" : "KeyframeView.qml"
            Binding {
                target: keyframeCurve.item
                property: "kfrModel"
                value: keyframeCurve.model
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "selected"
                value: true
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "isPanning"
                value: false
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "timescale"
                value: dopeRoot.maximumScaleFactor * dopeRoot.timeScale
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "containerWidth"
                value: dopeRoot.keyframeContainerWidth
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "ownerId"
                value: dopeRoot.ownerId
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "ownerType"
                value: dopeRoot.ownerType
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }

            Binding {
                target: keyframeCurve.item
                property: "inPoint"
                value: dopesheetmodel.dopeInPoint
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "outPoint"
                value: dopesheetmodel.dopeInPoint + dopesheetmodel.dopeDuration - 1
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "modelStart"
                value: dopesheetmodel.dopePosition
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "scrollStart"
                value: dopeRoot.contentScroll
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
        }
    }
    K.ZoomBar {
        id: horZoomBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: dopeRoot.bottom
        }
        height: Math.round(K.UiUtils.baseSizeMedium * 0.7)
        barMinWidth: K.UiUtils.baseSizeMedium
        fitsZoom: dopeRoot.timeScale === 1 && dopeRoot.contentScroll === 0
        zoomFactor: 1 / dopeRoot.timeScale
        onProposeZoomFactor: (proposedValue) => {
            dopeRoot.timeScale = 1. / proposedValue
            zoomOnBar = true
        }
        contentPos: 0
        onProposeContentPos: (proposedValue) => {
            // The corresponding pixel offset
            dopeRoot.contentScroll = Math.max(0, proposedValue * dopeRoot.frameDuration)
            console.log('proposing scroll: ', proposedValue, ', CONTENT SCROLL: ', dopeRoot.contentScroll, ', SCLAE: ', dopeRoot.timeScale, '\n\n***********************************\n\n')
        }
        onZoomByWheel: wheel => dopeRoot.zoomByWheel(wheel)
        onFitZoom: {
            dopeRoot.timeScale = 1
            //scrollView.contentX = 0
            //zoomOnBar = true
        }
    }
}
