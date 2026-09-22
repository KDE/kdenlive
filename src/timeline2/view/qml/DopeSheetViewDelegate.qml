/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import org.kde.ki18n
import org.kde.kdenlive as K

Item {
    id: delegateRect
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
    required property int dopeWidth
    required property var fontMetrics
    required property var activePalette

    required property int hoveredParam
    required property var dopesheetmodel
    required property int keyframeContainerWidth
    required property var dopeRootItem
    required property point rubberTopLeft
    required property color hoverColor

    property bool modelExpanded: model && model.expandedRole ? model.expandedRole : false
    property bool isBlankRecap: model && model.rowCount > 0 && model.dopeRecap && !hasChildren

    property double contentScroll: delegateRect.dopeRootItem.contentScroll

    implicitWidth: dopeWidth
    property int handleWidth: Math.round(K.UiUtils.baseSizeMedium * 0.8)
    implicitHeight: fontMetrics.lineSpacing * 1.3
    readonly property real indentation: 20
    readonly property real padding: 5
    required property var model
    property int containerWidth: width - delegateRect.dopeRootItem.headerWidth
    // The frame position of the hovered keyframe, -1 if none
    property int currentKFFrame: -1
    // The index of the hovered keyframe, -1 if none
    property int currentKFIndex: -1
    property bool kfPressed: kfMoveArea.pressed

    signal selectKeyframe(int kfIndex)
    signal activeParamChanged(var paramIndex)
    signal paramHovered(int paramRow)
    clip: true

    Menu {
        id: paramMenu
        property bool paramHasKeyframes
        property var paramIndex
        MenuItem {
            text: KI18n.i18n("Copy Keyframes")
            icon.name: "edit-copy"
            enabled: paramMenu.paramHasKeyframes
            onTriggered: {
                delegateRect.dopesheetmodel.removeAllKeyframes(paramMenu.paramIndex)
                delegateRect.selectKeyframe(-1)
                delegateRect.dopeRootItem.hoverKeyframe = -1
            }
        }
        MenuItem {
            text: KI18n.i18n("Load Curve")
            icon.name: "document-open"
            onTriggered: {
                // TODO
            }
        }
        MenuItem {
            text: KI18n.i18n("Save Curve")
            icon.name: "document-save-as"
            enabled: paramMenu.paramHasKeyframes
            onTriggered: {
                // TODO
            }
        }
        MenuItem {
            text: KI18n.i18n("Remove Keyframes")
            icon.name: "edit-delete"
            enabled: paramMenu.paramHasKeyframes
            onTriggered: {
                delegateRect.dopesheetmodel.removeAllKeyframes(paramMenu.paramIndex)
                delegateRect.selectKeyframe(-1)
                delegateRect.dopeRootItem.hoverKeyframe = -1
            }
        }
    }

    onContentScrollChanged: {
        if (kfMoveArea.containsMouse) {
            //let mousePos = Math.max(0., (kfMoveArea.mouseX + delegateRect.dopeRootItem.contentScroll))
            let mousePos = Math.max(0., (kfMoveArea.mouseX + delegateRect.dopeRootItem.mouseOffset))
            delegateRect.dopeRootItem.mouseFramePos = delegateRect.dopeRootItem.viewToFrame(mousePos)
        }
    }

    function restoreExpandedState() {
        if (!delegateRect.treeView) {
            console.log('UNAVAILABLE TREEVIEW; ABORTING...')
            return
        }

        if (delegateRect.dopeRootItem.ownerId == -1) {
            if (delegateRect.depth > 0) {
                delegateRect.treeView.collapse(delegateRect.row)
            } else if (!delegateRect.expanded) {
                delegateRect.treeView.collapse(delegateRect.row)
            }
            return
        }

        if (delegateRect.depth == 0) {
            if (!delegateRect.expanded) {
                delegateRect.treeView.expand(delegateRect.row)
            }
            return
        }

        if (delegateRect.depth == 1) {
            console.log('CHECKING ITEM: ', delegateRect.model.dopeName, ' IS EXPANDED: ', delegateRect.modelExpanded, ' / ', delegateRect.expanded)
            if (delegateRect.modelExpanded != delegateRect.expanded) {
                delegateRect.treeView.expandToIndex(delegateRect.treeView.index(delegateRect.row, delegateRect.column))
                if (delegateRect.modelExpanded) {
                    delegateRect.treeView.expand(delegateRect.row)
                } else {
                    delegateRect.treeView.collapse(delegateRect.row)
                }
            }
        }
        if (delegateRect.depth > 1 && delegateRect.expanded) {
            delegateRect.treeView.collapse(delegateRect.row)
        }
    }

    onModelExpandedChanged: {
        if (depth == 1 && modelExpanded != expanded) {
            if (modelExpanded) {
                delegateRect.treeView.expand(delegateRect.row)
            } else {
                delegateRect.treeView.collapse(delegateRect.row)
            }
        }
    }
    Item  {
        anchors.fill: parent
        anchors.leftMargin: delegateRect.dopeRootItem.headerWidth
        clip: true

    Rectangle {
        // keyframes background
        color: delegateRect.depth == 0 ? 'darkorange' : 'darkgoldenrod'
        opacity: 0.15
        visible: delegateRect.depth < 2
        radius: 8
        anchors.fill: kfContainer
        anchors.topMargin: 2
        anchors.bottomMargin: 2
    }


    Item {
        id: kfContainer
        //anchors.left: delegateRect.left
        //anchors.right: delegateRect.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: delegateRect.dopeRootItem.kfOffset
        width: delegateRect.dopeRootItem.frameDuration * delegateRect.dopeRootItem.timeScale
        visible: !delegateRect.isBlankRecap
        Rectangle {
            // keyframe slider
            id: keyframeSlider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: Math.max(4, K.UiUtils.baseSizeMedium / 4)
            radius: height/3
            color: delegateRect.current ? delegateRect.activePalette.highlight : delegateRect.row == delegateRect.hoveredParam ? Qt.rgba(delegateRect.activePalette.highlight.r * 0.6, delegateRect.activePalette.highlight.g * 0.6, delegateRect.activePalette.highlight.b * 0.6, 1) : delegateRect.activePalette.light
            border.width: 1
            border.color: delegateRect.activePalette.shadow
        }
        MouseArea {
            id: kfMoveArea
            anchors.fill: parent
            anchors.leftMargin: - delegateRect.handleWidth / 2
            anchors.rightMargin: - delegateRect.handleWidth / 2
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            // The frame position of the clicked keyframe, -1 if none
            property int clickFrame: -1
            // The index of the clicked keyframe, -1 if none
            property int clickIndex: -1
            // The last position of a moved keyframe, -1 if none
            property int movePosition: -1
            property bool dragStarted: false
            property point clickPoint
            property bool shiftClick: false
            property bool ctrlClick: false
            property var buttonClicked
            hoverEnabled: true

            onPressed: mouse => {
                clickFrame = delegateRect.currentKFFrame
                clickIndex = delegateRect.currentKFIndex
                movePosition = -1
                dragStarted = false
                clickPoint = Qt.point(mouseX, mouseY)
                delegateRect.dopeRootItem.rubberTopLeft = mapToItem(delegateRect.dopeRootItem, mouseX, mouseY)
                mouse.accepted = true
                shiftClick = mouse.modifiers & Qt.ShiftModifier
                ctrlClick = mouse.modifiers & Qt.ControlModifier
                buttonClicked = mouse.buttons
                let i
                let j
                // Select parameter
                let parameterIndex = delegateRect.treeView.index(delegateRect.row, delegateRect.column)
                console.log(' GOT CLICK PARAM INDEX: ', parameterIndex)
                delegateRect.treeView.selectionModel.setCurrentIndex(parameterIndex, ItemSelectionModel.SelectCurrent);

                if (clickIndex < 0) {
                    // Not on a keyframe
                    delegateRect.dopeRootItem.keyframeType = -1
                    if (mouse.buttons === Qt.RightButton) {
                        delegateRect.activeParamChanged(parameterIndex)
                        delegateRect.dopeRootItem.showOtherMenu()
                    } else {
                        delegateRect.dopeRootItem.clearSelection()
                    }
                    return
                }
                delegateRect.dopeRootItem.keyframeType = delegateRect.model.dopeModel.getKeyframeTypeAtFrame(clickFrame)
                let selectedKeyframes = delegateRect.dopeRootItem.getSelectedKeyframesForIndex(parameterIndex)
                let alreadySelected = selectedKeyframes.indexOf(delegateRect.currentKFIndex) > -1
                let actionList = delegateRect.dopeRootItem.typeActionsList
                let matchingText
                if (mouse.buttons === Qt.RightButton) {
                    if (alreadySelected) {
                        // keyframe already selected, just show menu
                        delegateRect.selectKeyframe(delegateRect.currentKFFrame)
                        delegateRect.activeParamChanged(parameterIndex)
                        for (i = 0; i < delegateRect.dopeRootItem.keyframeTypes.length; i++) {
                            if (delegateRect.dopeRootItem.keyframeTypes[i].value === delegateRect.dopeRootItem.keyframeType) {
                                matchingText = delegateRect.dopeRootItem.keyframeTypes[i].text
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
                        delegateRect.dopeRootItem.showContextMenu()
                        return
                    }
                }
                let updateKeyframeSelection = false
                let removeFromSelection = false
                if (mouse.modifiers & Qt.ShiftModifier) {
                    if (alreadySelected) {
                        // Remove from selection
                        let selectionIndex = selectedKeyframes.indexOf(delegateRect.currentKFIndex);
                        selectedKeyframes.splice(selectionIndex, 1);
                        removeFromSelection = true
                    } else {
                        // Add to selection
                        selectedKeyframes.push(delegateRect.currentKFIndex)
                    }
                    updateKeyframeSelection = true
                } else if (!alreadySelected) {
                    selectedKeyframes = [delegateRect.currentKFIndex]
                    updateKeyframeSelection = true
                }
                if (updateKeyframeSelection) {
                    if (delegateRect.hasChildren) {
                        // Top level item, build index of related kf to select
                        let mapped = delegateRect.treeView.model.mapToSource(parameterIndex)
                        let result = delegateRect.dopesheetmodel.selectKeyframeAtPos(mapped, clickFrame)
                        delegateRect.dopeRootItem.updateSelectedKeyframesFromModel(result, shiftClick, removeFromSelection)
                        return
                    }
                    console.log('UPDATING SELECTED KEYFRAMES: ', selectedKeyframes)
                    delegateRect.dopeRootItem.updateSelectedKeyframesForIndex(delegateRect.treeView.model.mapToSource(parameterIndex), selectedKeyframes, shiftClick)
                    delegateRect.dopeRootItem.allSelectedKeyframesChanged()
                }
                if (ctrlClick) {
                    delegateRect.dopesheetmodel.setScaledInfo(delegateRect.dopeRootItem.allSelectedKeyframes, clickFrame)
                }
                if (mouse.buttons === Qt.RightButton) {
                    // Show context menu
                    delegateRect.selectKeyframe(delegateRect.currentKFFrame)
                    delegateRect.activeParamChanged(parameterIndex)
                    for (i = 0; i < delegateRect.dopeRootItem.keyframeTypes.length; i++) {
                        if (delegateRect.dopeRootItem.keyframeTypes[i].value === delegateRect.dopeRootItem.keyframeType) {
                            matchingText = delegateRect.dopeRootItem.keyframeTypes[i].text
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
                    delegateRect.dopeRootItem.showContextMenu()
                }
            }
            onReleased: mouse => {
                if (delegateRect.dopeRootItem.rubberSelect) {
                    // Select all keyframes inside our rectangle
                    delegateRect.dopeRootItem.selectRubber(false)
                    delegateRect.dopeRootItem.rubberSelect = false
                    dragStarted = false
                    return
                }
                if (dragStarted) {
                    if (ctrlClick) {
                        delegateRect.dopesheetmodel.moveScaledKeyframe(clickFrame, false, false)
                        delegateRect.dopesheetmodel.moveScaledKeyframe(movePosition, true, true)
                    } else if (clickIndex > -1) {
                        delegateRect.dopesheetmodel.moveKeyframe(delegateRect.dopeRootItem.allSelectedKeyframes, movePosition, clickFrame, false)
                        delegateRect.dopesheetmodel.moveKeyframe(delegateRect.dopeRootItem.allSelectedKeyframes, clickFrame, movePosition, true)
                        delegateRect.currentKFFrame = movePosition
                    }
                } else if (clickIndex > -1 && buttonClicked === Qt.LeftButton && !shiftClick) {
                    delegateRect.model.dopeModel.seekToKeyframe(clickIndex)
                }
                if (ctrlClick) {
                    delegateRect.dopesheetmodel.resetScaledInfo()
                }
                dragStarted = false
                mouse.accepted = true
            }

            onWheel: wheel => delegateRect.dopeRootItem.scrollByWheel(wheel)

            onPositionChanged: mouse => {
                let mousePos = Math.max(0., (mouse.x + delegateRect.dopeRootItem.mouseOffset))
                delegateRect.dopeRootItem.mouseFramePos = delegateRect.dopeRootItem.viewToFrame(mousePos)
                if (!pressed) {
                    return
                }
                if (!dragStarted && mouse.buttons === Qt.LeftButton) {
                    if (Math.abs(mouseX - clickPoint.x) + Math.abs(mouseY - clickPoint.y) > Application.styleHints.startDragDistance) {
                        dragStarted = true
                        if (shiftClick) {
                            // Start rectangle selection
                            delegateRect.dopeRootItem.rubberSelect = true
                            delegateRect.dopeRootItem.rubberBottomRight = mapToItem(delegateRect.dopeRootItem, mouseX, mouseY)
                            return
                        }
                    }
                } else if (shiftClick) {
                    // Update rectangle selection
                    delegateRect.dopeRootItem.rubberBottomRight = mapToItem(delegateRect.dopeRootItem, mouseX, mouseY)
                    delegateRect.dopeRootItem.selectRubber(false)
                    console.log('GOT bottom POS: ', delegateRect.dopeRootItem.rubberBottomRight.y, ' MOUSEY: ', mouseY, ' / ', mouse.y)
                    return
                }
                if (mouse.buttons === Qt.LeftButton && dragStarted && clickIndex > -1) {
                    let updatedKfPosition = delegateRect.dopeRootItem.getPositionForKeyframe()
                    if (movePosition == updatedKfPosition) {
                        // No move, abort
                        // return
                    }
                    if (ctrlClick) {
                        delegateRect.dopesheetmodel.moveScaledKeyframe(updatedKfPosition, false, true)
                    } else {
                        console.log(' . .. MOVING KF TO: ', updatedKfPosition, '\n___________')
                        delegateRect.dopesheetmodel.moveKeyframe(delegateRect.dopeRootItem.allSelectedKeyframes, movePosition < 0 ? clickFrame : movePosition, updatedKfPosition, false)
                    }
                    movePosition = updatedKfPosition
                }
            }
            onDoubleClicked: mouse => {
                let parameterIndex = delegateRect.treeView.index(delegateRect.row, delegateRect.column)
                console.log('Double ckick at: ', delegateRect.currentKFFrame)
                if (delegateRect.isBlankRecap) {
                    // Clicked on a recap without children, abort
                    console.log('Clicked on a childless recap, abort')
                    return
                }
                let currentMouseFrame = delegateRect.dopeRootItem.getPositionForKeyframe()
                if (delegateRect.currentKFFrame > -1 && delegateRect.currentKFFrame == currentMouseFrame) {
                    console.log('Removing keyframe at: ', delegateRect.currentKFFrame)
                    // Double click on a keyframe, remove it
                    delegateRect.dopesheetmodel.removeKeyframe(delegateRect.treeView.model.mapToSource(parameterIndex), delegateRect.currentKFFrame)
                    delegateRect.currentKFFrame = -1
                    delegateRect.currentKFIndex = -1
                    delegateRect.dopeRootItem.hoverKeyframe = -1
                    delegateRect.dopeRootItem.keyframeType = -1
                    return
                }
                delegateRect.currentKFFrame = currentMouseFrame
                if (delegateRect.hasChildren) {
                    delegateRect.dopeRootItem.dopesheetmodel.addKeyframe(delegateRect.treeView.model.mapToSource(parameterIndex), delegateRect.currentKFFrame)
                } else {
                    delegateRect.model.dopeModel.addKeyframe(delegateRect.currentKFFrame)
                }
                delegateRect.model.dopeModel.seekToPos(delegateRect.currentKFFrame - delegateRect.dopeRootItem.inPoint)
                delegateRect.dopeRootItem.keyframeType = delegateRect.model.dopeModel.getKeyframeTypeAtFrame(delegateRect.currentKFFrame)
                delegateRect.dopeRootItem.hoverKeyframe = delegateRect.dopeRootItem.mouseFramePos
            }
        }
        HoverHandler {
            onHoveredChanged: {
                if (hovered) {
                    delegateRect.paramHovered(delegateRect.row)
                    K.Core.showKeyBinding(KI18n.i18n("<b>Drag</b> to move, <b>Ctrl+Drag</b> to scale selection, <b>Double Click</b> to add/remove keyframe"))
                } else {
                    K.Core.showKeyBinding()
                }
            }
        }
        Repeater {
            id: paramModel
            model: delegateRect.model.dopeModel
            property int ownerInPoint: delegateRect.dopeRootItem.inPoint
            onCountChanged: {
                // A keyframe was added/removed, check if playhead position is over a keyframe
                console.log('&&&&&&&&&&&&&&\n\n', delegateRect.dopeRootItem.getActiveCppParamIndex(),'\n\n&&&&&&&&&&&&&')
                if (count > 0 && !delegateRect.dopeRootItem.blockUpdate) {
                    let index = delegateRect.dopeRootItem.getActiveCppParamIndex()
                    if (index.valid) {
                        delegateRect.dopeRootItem.overKeyframe = delegateRect.dopesheetmodel.isOnKeyframe(delegateRect.dopeRootItem.consumerPosition + ownerInPoint, false, index)
                    }
                }
            }
            function getIndex(row, column) {
                return delegateRect.treeView.index(row, column)
            }

            DopeSheetViewDelegateHandle {
                id: handle
                row: delegateRect.row
                column: delegateRect.column
                handleWidth: delegateRect.handleWidth
                containerWidth: delegateRect.containerWidth
                timeScale: delegateRect.dopeRootItem.timeScale
                contentScroll: delegateRect.dopeRootItem.contentScroll / delegateRect.dopeRootItem.timeScale
                //maximumScaleFactor: delegateRect.dopeRootItem.maximumScaleFactor
                keyframeContainerWidth: delegateRect.keyframeContainerWidth
                activePalette: delegateRect.activePalette
                kfPressed: delegateRect.kfPressed
                dopeHoverColor: delegateRect.hoverColor
                dopeRootItem: delegateRect.dopeRootItem
                parentScope: paramModel
                modelFrame: model.frame - paramModel.ownerInPoint
                modelInPos: delegateRect.dopeRootItem.itemPosition
                modelDuration: delegateRect.dopeRootItem.frameDuration
                modelDescription: model.description
                modelPercentPosition: model.percentPosition
                modelType: model.type
                delegateProperties: delegateRect
            }
        }
        Component.onCompleted: {
            delegateRect.treeView.expand(0)
            console.log('Loaded TREEVIEW COMPONENT ID: ', delegateRect.treeView.model.index(delegateRect.row, delegateRect.column))
        }
    }
    }
    Rectangle {
        // param name background
        color: delegateRect.depth == 0 ? 'darkorange' : 'darkgoldenrod'
        opacity: 0.25
        visible: delegateRect.depth < 2
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
        width: delegateRect.dopeRootItem.headerWidth
    }

    Rectangle {
        color: Qt.rgba(delegateRect.activePalette.highlight.r * 0.6, delegateRect.activePalette.highlight.g * 0.6, delegateRect.activePalette.highlight.b * 0.6, 1)
        radius: 4
        visible: delegateRect.current
        height: parent.height
        width: delegateRect.dopeRootItem.headerWidth
    }
    property Animation indicatorAnimation: NumberAnimation {
        target: indicator
        property: "rotation"
        from: delegateRect.expanded ? 0 : 90
        to: delegateRect.expanded ? 90 : 0
        duration: 200
        easing.type: Easing.OutQuart
    }

    TableView.onPooled: indicatorAnimation.complete()
    TableView.onReused: {
        if (current) indicatorAnimation.start()
            restoreExpandedState()
    }
    onExpandedChanged: {
        indicator.rotation = expanded ? 90 : 0
        if (delegateRect.dopeRootItem.headerWidth < (paramLabel.width + indicator.width + delegateRect.padding)) {
            delegateRect.dopeRootItem.headerWidth = paramLabel.width + indicator.width + delegateRect.padding
        }
    }

    ToolButton {
        id: indicator
        icon.name: "arrow-right"
        visible: delegateRect.hasChildren
        onClicked: {
            let previouslyExpanded = delegateRect.expanded
            delegateRect.treeView.toggleExpanded(delegateRect.row)
            if (delegateRect.depth == 1) {
                delegateRect.model.expandedRole = !previouslyExpanded
            }
        }
        x: delegateRect.padding
        height: paramLabel.height
        width: height
        anchors.verticalCenter: parent.verticalCenter
    }
    MouseArea {
        id: paramNameArea
        height: parent.height
        width: delegateRect.dopeRootItem.headerWidth
        acceptedButtons: Qt.RightButton
        onPressed: {
            //paramMenu.paramIndex = delegateRect.treeView.model.index(delegateRect.row, delegateRect.column)
            paramMenu.paramIndex = paramModel.getIndex(delegateRect.row, delegateRect.column)
            // Select parameter
            delegateRect.treeView.selectionModel.setCurrentIndex(paramMenu.paramIndex, ItemSelectionModel.SelectCurrent);
            paramMenu.paramHasKeyframes = paramModel.count > 0
            paramMenu.popup()
        }
    }

    Label {
        id: paramLabel
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: indicator.right
        text: delegateRect.model && delegateRect.model.dopeName ? delegateRect.model.dopeName : ""
        elide: Label.ElideRight
        readonly property bool isElided: implicitWidth > width
        width: Math.min(implicitWidth, delegateRect.fontMetrics.averageCharacterWidth * 20)
        ToolTip {
            visible: hoverHandler.hovered && paramLabel.isElided
            text: paramLabel.text
            delay: 1000
            timeout: 5000
        }
        HoverHandler {
            id: hoverHandler
        }
        rightPadding: 4
        leftPadding: 4
        font.bold: delegateRect.depth < 2

        Component.onCompleted: {
            if (delegateRect.dopeRootItem.headerWidth < (paramLabel.width + indicator.width + delegateRect.padding)) {
                delegateRect.dopeRootItem.headerWidth = paramLabel.width + indicator.width + delegateRect.padding
            }
        }
    }
    Component.onCompleted: {
        delegateRect.restoreExpandedState()
    }
}
