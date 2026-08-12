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

TreeView {
    // The model needs to be a QAbstractItemModel
    id: treeViewItem
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
            treeViewItem.dopeRootItem.overKeyframe = treeViewItem.dopesheetmodel.isOnKeyframe(treeViewItem.dopeRootItem.consumerPosition, false, treeViewItem.dopeRootItem.getActiveCppParamIndex())
            if (current.valid && current.parent) {
                var activeIndex = treeViewItem.dopeRootItem.getActiveCppParamIndex()
                if (activeIndex.valid) {
                    treeViewItem.keyframeCurve.model = treeViewItem.dopesheetmodel.getKeyframeModel(activeIndex)
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
    }
}