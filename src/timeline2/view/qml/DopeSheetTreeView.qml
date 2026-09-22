/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick

TreeView {
    // The model needs to be a QAbstractItemModel
    id: treeViewItem
    anchors.fill: parent
    required property var dopesheetmodel
    required property var dopesheetfiltermodel
    required property var keyframeCurve
    model: dopesheetfiltermodel
    property var activeIndex
    property int selectedKeyframe
    clip: true

    // Disable flicking
    acceptedButtons: Qt.NoButton
    selectionModel: ItemSelectionModel {
        model: treeViewItem.dopesheetfiltermodel
        onCurrentChanged: (current, previous) => {
            treeViewItem.dopeRootItem.overKeyframe = treeViewItem.dopesheetmodel.isOnKeyframe(treeViewItem.dopeRootItem.consumerPosition + treeViewItem.dopeRootItem.inPoint, false, treeViewItem.dopeRootItem.getActiveCppParamIndex())
            if (current.valid && current.parent) {
                let currentIndex = treeViewItem.dopeRootItem.getActiveCppParamIndex()
                if (currentIndex.valid) {
                    treeViewItem.keyframeCurve.model = treeViewItem.dopesheetmodel.getKeyframeModel(currentIndex)
                }
            }
        }
    }

    required property int hoveredParam
    required property var fontMetrics
    required property var activePalette
    required property var dopeRootItem
    required property int dopeWidth
    required property point rubberTopLeft
    required property int keyframeContainerWidth
    required property color hoverColor

    MouseArea {
        id: bgArea
        // This mousearea will handle all clicks outside parameters slider,
        // mostly handling rubber selection from outside
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.leftMargin: treeViewItem.dopeRootItem.headerWidth
        height: treeViewItem.dopeRootItem.keyframeContainerTop
        acceptedButtons: Qt.LeftButton
        property point clickPoint
        property bool shiftClick: false
        property bool dragStarted: false
        hoverEnabled: true
        onWheel: wheel => {
            if (treeViewItem.dopeRootItem.showRuler) {
                treeViewItem.dopeRootItem.directScrollByWheel(wheel)
            } else {
                treeViewItem.dopeRootItem.scrollByWheel(wheel)
            }
        }
        onEntered: {
            console.log('ENTERED TREEVIEW BACKGROUND!!!')
            treeViewItem.hoveredParam = -1
        }

        onPositionChanged: mouse => {
            let mousePos = Math.max(0., mouse.x + treeViewItem.dopeRootItem.contentScroll)
            if (mousePos <= 0 && treeViewItem.dopeRootItem.mouseFramePos == 0) {
                // In the header zone, ignore
            } else {
                treeViewItem.dopeRootItem.mouseFramePos = treeViewItem.dopeRootItem.viewToFrame(mousePos)
            }
            if (!pressed) {
                // Only process further on left click
                console.log('not pressed, aborting')
                mouse.accepted = false
                return
            }

            if (shiftClick) {
                mouse.accepted = true
                if (!dragStarted) {
                    if (Math.abs(mouse.x - clickPoint.x) + Math.abs(mouse.y - clickPoint.y) > Application.styleHints.startDragDistance) {
                        console.log(' - - - DRAG STARTED -- - ')
                        dragStarted = true
                        // Start rectangle selection
                        treeViewItem.dopeRootItem.rubberSelect = true
                        treeViewItem.dopeRootItem.rubberTopLeft = Qt.point(clickPoint.x + treeViewItem.dopeRootItem.headerWidth, clickPoint.y)
                        treeViewItem.dopeRootItem.rubberBottomRight = Qt.point(mouse.x + treeViewItem.dopeRootItem.headerWidth, mouse.y)
                        return
                    }
                } else {
                    treeViewItem.dopeRootItem.rubberBottomRight = Qt.point(mouse.x + treeViewItem.dopeRootItem.headerWidth, mouse.y)
                    treeViewItem.dopeRootItem.selectRubber(false)
                }
            } else {
                mouse.accepted = false
            }
        }
        onPressed: mouse => {
            clickPoint = Qt.point(mouse.x, mouse.y)
            shiftClick = mouse.modifiers & Qt.ShiftModifier
            dragStarted = false
            mouse.accepted = shiftClick
        }
        onReleased: {
            dragStarted = false
            shiftClick = false
            treeViewItem.dopeRootItem.rubberSelect = false
        }
    }

    // You can set a custom delegate or use a built-in TreeViewDelegate
    delegate: DopeSheetViewDelegate {
        hoveredParam: treeViewItem.hoveredParam
        dopesheetmodel: treeViewItem.dopesheetmodel
        keyframeContainerWidth: treeViewItem.keyframeContainerWidth
        rubberTopLeft: treeViewItem.rubberTopLeft
        dopeRootItem: treeViewItem.dopeRootItem
        fontMetrics: treeViewItem.fontMetrics
        activePalette: treeViewItem.activePalette
        dopeWidth: treeViewItem.dopeWidth
        hoverColor: treeViewItem.hoverColor
        onSelectKeyframe: kfIndex => treeViewItem.selectedKeyframe = kfIndex
        onActiveParamChanged: paramIndex => treeViewItem.activeIndex = paramIndex
        onParamHovered: paramRow => treeViewItem.hoveredParam = paramRow
    }
}
