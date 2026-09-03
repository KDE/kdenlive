/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

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
    property int toolbarHeight: K.UiUtils.baseSizeMedium * 1.5
    property int mouseFramePos: -1
    property int hoverKeyframe: -1
    property bool overKeyframe: false
    required property var keyframeTypes
    required property K.DopeSheetModel dopesheetmodel
    required property K.DopeFilter dopesheetFilterModel
    property K.MonitorProxy proxy
    property var keyframeType
    property int ownerType: -1
    property int ownerId: -1
    property bool viewHasFocus: false
    property bool showTimelineTime: false
    property bool blockUpdate: false
    property var rubberStartIndex
    property var rubberEndIndex
    property int rubberStartFrame: 0
    property int rubberEndFrame: 0
    property bool rubberAddToSelection: false
    property bool rubberSelectPending: false
    property bool paramUpdatePending: false
    // Whether the zoom operation was performed through zoombar
    property bool zoomOnBar: false
    property int zoomOnMouse: -1

    // The position in frame of the stack owner
    property int offset: dopesheetmodel.dopePosition
    property color hoverColor: "#bb8800"
    // Ruler scaling, 1 means view is fully visible, 2 means zoomed twice
    property real timeScale: 1
    // The maximum timeScale factor, where the full item width is visible
    property real maximumScaleFactor: frameDuration <= 0 ? 0 : keyframeContainerWidth / frameDuration
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
    property var typeActionsList: typeActions.actions
    // the X offset for the keyframes view
    property double contentScroll: 0
    // Playhead position
    property int headerWidth: 100
    property int consumerPosition: proxy && ownerType > -1 ? proxy.position - offset: -1
    property bool insideOwner: dopeRoot.consumerPosition > 0 && dopeRoot.consumerPosition < frameDuration
    property int keyframeContainerWidth: keyframeContainer.width
    property int snapping: (K.KdenliveSettings.snaptopoints && (dopeRoot.timeScale < 2 * K.UiUtils.baseSizeMedium)) ?
                               Math.floor(K.UiUtils.baseSizeMedium / (dopeRoot.timeScale > 3 ? dopeRoot.timeScale / 2 : dopeRoot.timeScale)) : -1
    focus: true

    function showContextMenu() {
        keyframeMenu.popup()
    }

    function getPositionForKeyframe() {
        return dopeRoot.mouseFramePos + dopeRoot.inPoint
    }

    onOwnerIdChanged: {
        dopeRoot.paramUpdatePending = false
    }

    onOverKeyframeChanged: {
        dopesheetmodel.onKeyframeChanged(overKeyframe, false)
    }

    FontMetrics {
        id: fontMetrics
        font: K.UiUtils.smallestReadableFont
    }
    readonly property font miniFont: K.UiUtils.smallestReadableFont
    onFrameDurationChanged: {
        console.log('UPDATED DOPE DURATION: ', frameDuration)
        if (frameDuration === 0) {
            keyframeCurve.model = undefined
        }
    }

    onConsumerPositionChanged: {
        console.log(' - - - updated dope cpos: ', dopeRoot.consumerPosition)
        dopeRoot.scheduleParamUpdate()
    }

    function processParamUpdate() {
        dopeRoot.overKeyframe = dopesheetmodel.isOnKeyframe(dopeRoot.consumerPosition + dopeRoot.inPoint, false, getActiveCppParamIndex())
        dopeRoot.paramUpdatePending = false
        if (dopeRoot.insideOwner && dopeRoot.timeScale > 1) {
            scrollToFrame(dopeRoot.consumerPosition)
        }
    }

    function scheduleParamUpdate() {
        if (dopeRoot.paramUpdatePending) {
            return
        }
        dopeRoot.paramUpdatePending = true
        Qt.callLater(processParamUpdate)
    }

    onInsideOwnerChanged: {
        if (insideOwner) {
            if (!dopeRoot.overKeyframe) {
                dopesheetmodel.onKeyframeChanged(false, false)
            }
        } else {
            dopesheetmodel.onKeyframeChanged(false, true)
        }
    }

    signal filterDopeView(var searchText)

    function expandAll()
    {
        treeViewItem.expandRecursively()
    }

    function updateOverKeyframeFromModel(overKeyframe) {
        dopeRoot.overKeyframe = overKeyframe
    }

    function switchFocus(hasFocus) {
        viewHasFocus = hasFocus
    }

    function blockModelUpdate(doBlock) {
        blockUpdate = doBlock
        if (!doBlock) {
            // Trigger refresh
            dopeRoot.overKeyframe = dopeRoot.dopesheetmodel.isOnKeyframe(dopeRoot.consumerPosition + dopeRoot.inPoint, false, getActiveCppParamIndex())
        }
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
        treeViewItem.expand(0)
    }

    function scrollToFrame(pos) {
        let proposedPos = Math.max(0, Math.min(pos / dopeRoot.frameDuration, 1 - 1 / dopeRoot.timeScale))
        console.log('Checked scrolling for pos: ', pos, ' = ', proposedPos)
        horZoomBar.ensureVisible(proposedPos, false)
    }

    function scrollByWheel(wheel) {
        let proposedPos
        if (wheel.angleDelta.y < 0) {
            proposedPos = Math.max(0, Math.min((horZoomBar.contentPos * dopeRoot.frameDuration - wheel.angleDelta.y) / dopeRoot.frameDuration, 1 - 1 / dopeRoot.timeScale))
        } else {
            proposedPos = Math.max(horZoomBar.contentPos * dopeRoot.frameDuration - wheel.angleDelta.y, 0) / dopeRoot.frameDuration
        }
        horZoomBar.proposeContentPos(proposedPos)
    }

    function zoomByWheel(wheel) {
        if (wheel.modifiers & Qt.AltModifier) {
            // Seek to next snap
            if (wheel.angleDelta.x > 0) {
                K.Core.triggerAction('monitor_seek_snap_backward')
            } else {
                K.Core.triggerAction('monitor_seek_snap_forward')
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
        let previousPos = dopeRoot.mouseFramePos
        dopeRoot.timeScale = 1. / (Math.min(1, 1 / (dopeRoot.timeScale * factor)))
        if (dopeRoot.zoomOnBar) {
            dopeRoot.zoomOnBar = false
        } else {
            let mouseOffset = backgroundArea.mouseX - K.UiUtils.baseSizeMedium
            let position = (previousPos - viewToFrame(mouseOffset)) / dopeRoot.frameDuration
            position = Math.max(0, position)
            position = Math.min(1 - horZoomBar.zoomFactor, position)
            horZoomBar.proposeContentPos(position)
        }
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
            if (elem.index === treeViewItem.model.mapToSource(itemIndex)) {
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
            if (elem.index === treeViewItem.model.mapToSource(itemIndex)) {
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
                    break
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
        var startFrame = Math.min(dopeRoot.rubberBottomRight.x, dopeRoot.rubberTopLeft.x) - dopeRoot.headerWidth - K.UiUtils.baseSizeMedium + (dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        var endFrame = Math.max(dopeRoot.rubberBottomRight.x, dopeRoot.rubberTopLeft.x) - dopeRoot.headerWidth - K.UiUtils.baseSizeMedium + (dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        dopeRoot.rubberStartFrame = viewToFrame(startFrame) + dopeRoot.inPoint
        dopeRoot.rubberEndFrame = viewToFrame(endFrame) + dopeRoot.inPoint
        console.log('SELECTING FRAMES BETWEEN: ', startFrame, '-', endFrame)
        var topPos = mapToItem(treeViewItem, 0, Math.min(dopeRoot.rubberBottomRight.y, dopeRoot.rubberTopLeft.y))
        topPos.y = Math.max(0, topPos.y)
        var bottomPos = mapToItem(treeViewItem, 0, Math.max(dopeRoot.rubberBottomRight.y, dopeRoot.rubberTopLeft.y))
        bottomPos.y = Math.min(treeViewItem.contentHeight - 1, bottomPos.y)
        var topRow = treeViewItem.cellAtPosition(topPos)
        var bottomRow = treeViewItem.cellAtPosition(bottomPos)
        dopeRoot.rubberStartIndex = treeViewItem.model.mapToSource(treeViewItem.modelIndex(topRow))
        dopeRoot.rubberEndIndex = treeViewItem.model.mapToSource(treeViewItem.modelIndex(bottomRow))
        dopeRoot.rubberAddToSelection = addToSelection
        scheduleRubberSelect()
    }

    function processRubberSelect() {
        var result = dopesheetmodel.selectKeyframeRange(dopeRoot.rubberStartIndex, dopeRoot.rubberEndIndex, dopeRoot.rubberStartFrame, dopeRoot.rubberEndFrame)
        updateSelectedKeyframesFromModel(result, dopeRoot.rubberAddToSelection, false)
        dopeRoot.rubberSelectPending = false
    }

    function scheduleRubberSelect() {
        if (dopeRoot.rubberSelectPending)
            return

        dopeRoot.rubberSelectPending = true
        Qt.callLater(dopeRoot.processRubberSelect)
    }

    function updateSelectedKeyframesFromModel(result, addToSelection, removeFromSelection) {
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
            // insertedKF is the list of new keyframes to add/remove
            var insertedKF = result[id.internalId.toString()]
            if (addToSelection) {
                var existingKF = getSelectedKeyframesForIndex(treeViewItem.model.mapFromSource(id))
                let currentPos = existingKF.indexOf(insertedKF[0])
                if (removeFromSelection) {
                    console.log('remove op, found at: ', currentPos)
                    if (currentPos > -1) {
                        existingKF.splice(currentPos, 1)
                        insertedKF = existingKF
                    } else {
                        // Nothing to to
                        continue
                    }
                } else if (existingKF.length > 0) {
                    if (currentPos === -1) {
                        // Keyframe was not previously selected
                        insertedKF = insertedKF.concat(existingKF)
                    } else {
                        // Nothing to to
                        continue
                    }
                }
            }
            // Remove duplicates
            let s = new Set(insertedKF);
            let deduplicated = [...s]
            updateSelectedKeyframesForIndex(id, deduplicated, true)
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
        if (treeViewItem.selectionModel.currentIndex.valid) {
            return treeViewItem.model.mapToSource(treeViewItem.model.index(treeViewItem.selectionModel.currentIndex.row, treeViewItem.selectionModel.currentIndex.column, treeViewItem.selectionModel.currentIndex.parent))
        }
        console.log('NO ACTIVE ITEM FOUND IN DOPESHEET...')
        return {}//treeViewItem.model.index(0, 0, treeViewItem.model.index(0, 0, treeViewItem.rootIndex))
    }

    function activateParamFromModel(paramIndex) {
        console.log('Setting active from model: ', paramIndex)
        treeViewItem.selectionModel.setCurrentIndex(paramIndex, ItemSelectionModel.SelectCurrent);
        treeViewItem.expand(paramIndex)
    }

    function setActiveIndexFromModel(row, paramRow) {
        console.log('READY TO SET MODEINDEX FROM C++: ', row, ' / ', paramRow)
        // Ensure item is visible
        let modelIndex = treeViewItem.model.index(row, 0, treeViewItem.model.index(0, 0, treeViewItem.rootIndex))
        if (!modelIndex.valid) {
            console.log('invalid ix from C++')
            return
        }
        var currentIx = treeViewItem.selectionModel.currentIndex
        if (currentIx.parent && currentIx.parent.parent && currentIx.parent.parent != treeViewItem.rootIndex) {
            // We are on an effect param with a recap
            if (currentIx.parent.row === modelIndex.row && (paramRow < 0)) {
                return
            }
        } else if (currentIx.parent.parent === treeViewItem.rootIndex) {
            // We are on an effect recap
            if (currentIx.row === modelIndex.row && paramRow < 0) {
                return
            }
        }
        console.log('Setting index from C++: ', modelIndex, ' on PARAMM: ', paramRow)
        if (paramRow < 0 || treeViewItem.model.rowCount(modelIndex) === 0) {
            treeViewItem.selectionModel.setCurrentIndex(modelIndex, ItemSelectionModel.SelectCurrent);
        } else {
            let paramIndex = dopesheetmodel.getQmlSelectionIndex(treeViewItem.model, row, paramRow)
            if (currentIx === paramIndex) {
                return
            }
            treeViewItem.expandToIndex(paramIndex)
            treeViewItem.selectionModel.setCurrentIndex(paramIndex, ItemSelectionModel.SelectCurrent);
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
        if (treeViewItem.selectionModel.currentIndex.parent != treeViewItem.rootIndex) {
            // Child Item
            var parentIndex = treeViewItem.selectionModel.currentIndex.parent
            if (treeViewItem.selectionModel.currentIndex.row < treeViewItem.model.rowCount(parentIndex) - 1) {
                treeViewItem.selectionModel.setCurrentIndex(treeViewItem.model.index(treeViewItem.selectionModel.currentIndex.row + 1, treeViewItem.selectionModel.currentIndex.column, parentIndex), ItemSelectionModel.SelectCurrent)
                return
            }
            // Move to next top level item
            if (parentIndex.row < treeViewItem.model.rowCount(treeViewItem.rootIndex) - 1) {
                treeViewItem.selectionModel.setCurrentIndex(treeViewItem.model.index(parentIndex.row + 1, treeViewItem.selectionModel.currentIndex.column), ItemSelectionModel.SelectCurrent)
                return
            }
        }
        // Move to next child or top level item
        if (treeViewItem.model.rowCount(treeViewItem.selectionModel.currentIndex) > 0) {
            treeViewItem.selectionModel.setCurrentIndex(treeViewItem.model.index(0, 0, treeViewItem.selectionModel.currentIndex), ItemSelectionModel.SelectCurrent)
        } else {
            if (treeViewItem.selectionModel.currentIndex.row < treeViewItem.model.rowCount(treeViewItem.rootIndex) - 1) {
                treeViewItem.selectionModel.setCurrentIndex(treeViewItem.model.index(treeViewItem.selectionModel.currentIndex.row + 1, treeViewItem.selectionModel.currentIndex.column), ItemSelectionModel.SelectCurrent)
            }
        }
    }

    Keys.onUpPressed: {
        if (treeViewItem.selectionModel.currentIndex.parent != treeViewItem.rootIndex) {
            // Child Item
            var parentIndex = treeViewItem.selectionModel.currentIndex.parent
            if (treeViewItem.selectionModel.currentIndex.row > 0) {
                treeViewItem.selectionModel.setCurrentIndex(treeViewItem.model.index(treeViewItem.selectionModel.currentIndex.row - 1, treeViewItem.selectionModel.currentIndex.column, parentIndex), ItemSelectionModel.SelectCurrent)
                return
            }
            // Move to top level item
            treeViewItem.selectionModel.setCurrentIndex(parentIndex, ItemSelectionModel.SelectCurrent)
            return
        }
        // Move to next child or top level item
        if (treeViewItem.selectionModel.currentIndex.row > 0) {
            var upperItem = treeViewItem.model.index(treeViewItem.selectionModel.currentIndex.row - 1, treeViewItem.selectionModel.currentIndex.column)
            if (treeViewItem.model.rowCount(upperItem) > 0) {
                treeViewItem.selectionModel.setCurrentIndex(treeViewItem.model.index(treeViewItem.model.rowCount(upperItem) - 1, treeViewItem.selectionModel.currentIndex.column, upperItem), ItemSelectionModel.SelectCurrent)
            } else {
                treeViewItem.selectionModel.setCurrentIndex(upperItem, ItemSelectionModel.SelectCurrent)
            }
        }
    }

    Menu {
        id: defaultTypeMenu
        MenuItem {
            text: KI18n.i18n("Use same type as previous keyframe")
            checkable: true
            checked: K.KdenliveSettings.usepreviouskeyframeinterp
            onTriggered: {
                K.KdenliveSettings.usepreviouskeyframeinterp = checked
            }
        }

        Menu {
            id: kfMenu
            title: KI18n.i18n("Default type for new keyframes")
            ActionGroup {
                id: defTypeActions
                exclusive: true
            }
            Repeater {
                model: dopeRoot.keyframeTypes
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.text
                    checkable: true
                    checked: modelData.value === K.KdenliveSettings.defaultkeyframeinterp
                    onTriggered: {
                        console.log('changing default kf type to: ', modelData.value)
                        K.KdenliveSettings.defaultkeyframeinterp = modelData.value
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
        property int buttonHeight: dopeRoot.toolbarHeight
        property int iconHeight: buttonHeight - 4
        RowLayout {
            anchors.fill: parent
            ToolButton {
                id: kfTypeButton
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "application-menu"
                onClicked: {
                    // Check required kfr type
                    defaultTypeMenu.popup()
                }
            }
            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "smallclock"
                ToolTip.text: KI18n.i18n("Show absolute timecode")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                checkable: true
                checked: dopeRoot.showTimelineTime
                onClicked: dopeRoot.showTimelineTime = !dopeRoot.showTimelineTime
            }

            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: dopeRoot.overKeyframe ? "keyframe-remove" : "keyframe-add"
                ToolTip.text: KI18n.i18n("Add/Remove Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: K.Core.triggerAction('keyframe_add')
            }
            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "align-horizontal-center"
                enabled: dopeRoot.allSelectedKeyframes.length > 0
                ToolTip.text: KI18n.i18n("Align Keyframe to Playhead")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: dopeRoot.dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, -1, dopeRoot.consumerPosition, true)
            }
            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "edit-copy"
                enabled: dopeRoot.allSelectedKeyframes.length > 0
                ToolTip.text: KI18n.i18n("Copy Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: dopeRoot.copyKeyframes()
            }
            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "edit-paste"
                ToolTip.text: KI18n.i18n("Paste Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: dopeRoot.pasteKeyframes()
            }
            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "arrow-left"
                enabled: dopeRoot.consumerPosition > 0
                ToolTip.text: KI18n.i18n("Go to Previous Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: K.Core.triggerAction('monitor_seek_kf_backward')
            }
            ToolButton {
                implicitWidth: dopeBar.buttonHeight
                implicitHeight: dopeBar.buttonHeight
                icon.width: dopeBar.iconHeight
                icon.height: dopeBar.iconHeight
                icon.name: "arrow-right"
                enabled: dopeRoot.consumerPosition < dopeRoot.frameDuration - 1
                ToolTip.text: KI18n.i18n("Go to Next Keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onClicked: K.Core.triggerAction('monitor_seek_kf_forward')
            }
            ComboBox {
                id: keyframeTypeCombo
                model: dopeRoot.keyframeTypes
                textRole: "text"
                valueRole: "value"
                implicitHeight: dopeBar.buttonHeight
                currentValue: dopeRoot.keyframeType
                enabled: dopeRoot.allSelectedKeyframes.length > 0
                ToolTip.text: KI18n.i18n("Type for selected keyframe")
                ToolTip.delay: 1000
                ToolTip.visible: hovered
                onActivated: {
                    console.log('changing kf type to: ', currentValue, ' current: ', dopeRoot.keyframeType)
                    dopeRoot.dopesheetmodel.changeKeyframeType(dopeRoot.allSelectedKeyframes, currentValue)
                    dopeRoot.keyframeType = currentValue
                }
            }
            SearchField {
                id: dopeSearch
                implicitWidth: dopeRoot.baseUnit * 10
                implicitHeight: dopeBar.buttonHeight
                onTextChanged: {
                    dopeRoot.filterDopeView(text)
                }
            }
        }
    }


    DopeRuler {
        id: ruler
        anchors.top: dopeBar.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.leftMargin: K.UiUtils.baseSizeMedium + dopeRoot.headerWidth
        anchors.rightMargin: K.UiUtils.baseSizeMedium
        height: Math.round(K.UiUtils.baseSizeMedium * 2.5)
        rulerOffset: dopeRoot.offset
        monitorController: dopeRoot.proxy
        timecodeOffset: dopeRoot.dopesheetmodel ? dopeRoot.dopesheetmodel.timecodeOffset : 0
        scalingFactor: dopeRoot.timeScale * dopeRoot.maximumScaleFactor
        rulercontainerWidth: Math.max(width, dopeRoot.frameDuration * dopeRoot.timeScale * dopeRoot.maximumScaleFactor)
        scrollViewContentX: dopeRoot.contentScroll
        snapping: dopeRoot.snapping
        fontMetrics: fontMetrics
        onZoomByWheel: (wheel) => { dopeRoot.zoomByWheel(wheel) }
        onWidthChanged: {
            ruler.adjustStepSize()
        }
    }
    Rectangle {
        anchors.fill: playheadLabel
        visible: playheadLabel.visible
        radius: 4
        color: dopeRoot.overKeyframe ? dopeRoot.hoverColor : activePalette.light
    }
    Label {
        id: playheadLabel
        visible: rulerCursor.visible
        anchors.top: ruler.top
        anchors.horizontalCenter: rulerCursor.horizontalCenter
        text: K.Core.timecodeString(dopeRoot.consumerPosition + (dopeRoot.showTimelineTime ? dopeRoot.offset : 0))
        leftPadding: 6
        rightPadding: 6
    }
    Rectangle {
        // Vertical line over ruler zone
        id: rulerCursor
        anchors.top: playheadLabel.bottom
        anchors.bottom: parent.bottom
        visible: dopeRoot.ownerType > -1 && x >= dopeRoot.headerWidth + K.UiUtils.baseSizeMedium && x < parent.width
        z: 4
        x: dopeRoot.headerWidth + K.UiUtils.baseSizeMedium + dopeRoot.frameToView(dopeRoot.consumerPosition)
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
        visible: !ruler.pressed && (backgroundArea.containsMouse || treeViewItem.hoveredParam > -1)
        anchors.top: ruler.top
        anchors.horizontalCenter: mouseLine.horizontalCenter
        text: K.Core.timecodeString(dopeRoot.mouseFramePos + (dopeRoot.showTimelineTime ? dopeRoot.offset : 0))
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
        x: dopeRoot.headerWidth + K.UiUtils.baseSizeMedium + dopeRoot.frameToView(dopeRoot.mouseFramePos)
        color: activePalette.highlight
    }
    MouseArea {
        id: backgroundArea
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        anchors.topMargin: dopeBar.height
        anchors.leftMargin: dopeRoot.headerWidth
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
            treeViewItem.hoveredParam = -1
        }

        onPositionChanged: mouse => {
            var mousePos = Math.max(0., (mouse.x - K.UiUtils.baseSizeMedium + dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor))
            if (mousePos <= 0 && dopeRoot.mouseFramePos == 0) {
                // In the header zone, ignore
                return
            }
            dopeRoot.mouseFramePos = dopeRoot.viewToFrame(mousePos)
            console.log('MOUSE FRAME TO: ', dopeRoot.mouseFramePos, ', NEW: ', mousePos)
        }
    }

    Rectangle {
        // Param name background
        id: headerBg
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.bottom: parent.bottom
        anchors.top: ruler.bottom
        width: dopeRoot.headerWidth
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
                dopeRoot.dopesheetmodel.moveKeyframe(dopeRoot.allSelectedKeyframes, dopeRoot.mouseFramePos, dopeRoot.consumerPosition, true)
            }
        }
        MenuItem {
            text: KI18n.i18n("Align Left")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: {
                dopeRoot.dopesheetmodel.alignKeyframe(dopeRoot.allSelectedKeyframes, false)
            }
        }
        MenuItem {
            text: KI18n.i18n("Align Right")
            enabled: dopeRoot.hoverKeyframe > -1
            onTriggered: {
                dopeRoot.dopesheetmodel.alignKeyframe(dopeRoot.allSelectedKeyframes, true)
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
                id: kfCtxMenu
                model: dopeRoot.keyframeTypes
                property var currentType: dopeRoot.keyframeType
                function changeKeyframe(type) {
                    console.log('changing kf type to: ', type, ' current: ', dopeRoot.keyframeType)
                    dopeRoot.dopesheetmodel.changeKeyframeType(dopeRoot.allSelectedKeyframes, type)
                    dopeRoot.keyframeType = type
                }

                MenuItem {
                    id: kfMenuItem
                    required property var modelData
                    text: modelData.text
                    property bool matches: modelData.value == kfCtxMenu.currentType
                    checkable: true
                    function changeKeyframe() {
                        kfCtxMenu.changeKeyframe(modelData.value)
                    }
                    action: Action {
                        ActionGroup.group: typeActions
                        checkable: true
                        checked: kfMenuItem.matches
                        onTriggered: {
                            kfMenuItem.changeKeyframe()
                        }
                    }
                }
            }
        }
        MenuItem {
            text: KI18n.i18n("Remove Keyframe")
            onTriggered: {
                if (treeViewItem.selectedKeyframe > -1) {
                    console.log('Removing keyframe')
                    // Double click on a keyframe, remove it
                    dopeRoot.dopesheetmodel.removeKeyframe(treeViewItem.activeIndex, treeViewItem.selectedKeyframe)
                    treeViewItem.selectedKeyframe = -1
                    treeViewItem.activeIndex = -1
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
                if (treeViewItem.selectedKeyframe > -1) {
                    console.log('Adding keyframe')
                    dopeRoot.dopesheetmodel.addKeyframe(treeViewItem.activeIndex, dopeRoot.getPositionForKeyframe())
                    dopeRoot.hoverKeyframe = dopeRoot.mouseFramePos
                }
            }
        }
    }

    DopeSheetTreeView {
        // The model needs to be a QAbstractItemModel
        id: treeViewItem
        dopesheetmodel: dopeRoot.dopesheetmodel
        dopesheetfiltermodel: dopeRoot.dopesheetFilterModel
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: keyframeContainer.top
        anchors.top: ruler.bottom
        // Disable flicking
        acceptedButtons: Qt.NoButton
        selectionModel: ItemSelectionModel {
            model: dopeRoot.dopesheetFilterModel
            onCurrentChanged: (current, previous) => {
                if (current.valid) {
                    var activeIndex = dopeRoot.getActiveCppParamIndex()
                    if (activeIndex.valid) {
                        dopeRoot.overKeyframe = dopeRoot.dopesheetmodel.isOnKeyframe(dopeRoot.consumerPosition + dopeRoot.inPoint, false, activeIndex)
                        if (current.parent) {
                            keyframeCurve.model = dopeRoot.dopesheetmodel.getKeyframeModel(activeIndex)
                            dopeRoot.dopesheetmodel.activateParam(activeIndex)
                        }
                    }
                }
            }
        }

        hoveredParam: -1
        fontMetrics: fontMetrics
        activePalette: activePalette
        dopeRootItem: dopeRoot
        keyframeCurve: keyframeCurve
        dopeWidth: dopeRoot.width
        keyframeContainerWidth: dopeRoot.keyframeContainerWidth
        rubberTopLeft: dopeRoot.rubberTopLeft
        hoverColor: dopeRoot.hoverColor
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
        anchors.leftMargin: K.UiUtils.baseSizeMedium + dopeRoot.headerWidth
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
        MouseArea {
            id: keyframeMouseArea
            anchors.fill: parent
            onDoubleClicked: mouse =>{
                if (keyframeCurve.model) {
                    var newVal = (height - mouse.y) / height
                    keyframeCurve.model.addKeyframe(dopeRoot.getPositionForKeyframe(), newVal)
                }
            }
        }
        Loader {
            // Keyframe curve
            id: keyframeCurve
            anchors.fill: parent
            property var model: undefined
            property bool hasKeyframes:  status == Loader.Ready ? keyframeCurve.model === undefined ? false : (keyframeCurve.item as KeyframeView).kfrCount > 1 : 0
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
                value: dopeRoot.dopesheetmodel.dopeInPoint
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "outPoint"
                value: dopeRoot.dopesheetmodel.dopeInPoint + dopeRoot.dopesheetmodel.dopeDuration - 1
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "modelStart"
                value: dopeRoot.dopesheetmodel.dopePosition
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "scrollStart"
                value: dopeRoot.contentScroll * dopeRoot.timeScale * dopeRoot.maximumScaleFactor //dopeRoot.contentScroll
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "timeScale"
                value: dopeRoot.maximumScaleFactor * dopeRoot.timeScale
                when: keyframeCurve.status === Loader.Ready && keyframeCurve.item
                restoreMode: Binding.RestoreBindingOrValue
            }
            Binding {
                target: keyframeCurve.item
                property: "timelineScrollViewWidth"
                value: keyframeContainer.width
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
            leftMargin: dopeRoot.headerWidth
        }
        height: Math.round(K.UiUtils.baseSizeMedium * 0.7)
        barMinWidth: K.UiUtils.baseSizeMedium
        fitsZoom: dopeRoot.timeScale === 1 && dopeRoot.contentScroll === 0
        zoomFactor: 1 / dopeRoot.timeScale
        onProposeZoomFactor: (proposedValue) => {
            dopeRoot.timeScale = 1. / proposedValue
            dopeRoot.zoomOnBar = true
        }
        contentPos: dopeRoot.contentScroll / dopeRoot.frameDuration
        onProposeContentPos: (proposedValue) => {
            // The corresponding pixel offset
            console.log('proposing scroll: ', (proposedValue * dopeRoot.frameDuration), ', PREVIOUS CONTENT SCROLL: ', dopeRoot.contentScroll)
            dopeRoot.contentScroll = Math.max(0, proposedValue * dopeRoot.frameDuration)
        }
        onZoomByWheel: wheel => dopeRoot.zoomByWheel(wheel)
        onFitZoom: {
            dopeRoot.timeScale = 1
            //scrollView.contentX = 0
            dopeRoot.zoomOnBar = true
        }
    }
}
