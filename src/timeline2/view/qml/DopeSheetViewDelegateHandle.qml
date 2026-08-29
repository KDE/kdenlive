/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: handle
    required property var model
    //required property var modelData
    required property int modelFrame
    required property int modelType
    required property string modelDescription
    required property real modelPercentPosition

    required property int index
    required property int row
    required property int column
    required property int handleWidth
    required property int containerWidth
    required property int keyframeContainerWidth
    required property double contentScroll
    required property real timeScale
    required property var parentScope
    required property var dopeRootItem
    required property var activePalette
    required property real maximumScaleFactor
    required property color dopeHoverColor
    required property bool kfPressed
    required property var delegateProperties

    z: 10
    x: modelPercentPosition * containerWidth * timeScale - contentScroll * timeScale * maximumScaleFactor - width / 2
    visible : x >= -width/2 && x < keyframeContainerWidth + width/2
    anchors.verticalCenter: parent.verticalCenter
    width: handleWidth - (kfArea.containsMouse ? 0 : 2)
    height: width
    property bool atMousePos: dopeRootItem.mouseFramePos === modelFrame
    color: dopeRootItem.keyframeGrabbed(parentScope.getIndex(row, column), index) > -1 ? 'red' : dopeRootItem.keyframeSelected(parentScope.getIndex(row, column), index) > -1 ? activePalette.highlight : activePalette.light
    radius: modelType == 1 ? 0 : Math.round(width/2)
    border.width: atMousePos ? 2 : 1
    border.color: (kfArea.containsMouse || kfArea.pressed) ? activePalette.highlight : atMousePos ? dopeHoverColor : activePalette.text

    MouseArea {
        id: kfArea
        anchors.fill: handle
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton
        onEntered: {
            console.log("entered kfr: ", handle.index,' PRESSED: ', handle.delegateProperties.currentKFFrame)
            handle.delegateProperties.currentKFFrame = handle.modelFrame + handle.dopeRootItem.inPoint
            handle.delegateProperties.currentKFIndex = handle.index
            //dopeRoot.keyframeType = type
            handle.dopeRootItem.hoverKeyframe = handle.modelFrame
            handle.dopeRootItem.mouseFramePos = handle.modelFrame
        }
        onExited: {
            handle.delegateProperties.currentKFFrame = -1
            handle.delegateProperties.currentKFIndex = -1
            handle.dopeRootItem.hoverKeyframe = -1
        }

        ToolTip.text: handle.modelDescription
        ToolTip.delay: 1000
        ToolTip.visible: containsMouse && !handle.kfPressed
    }
}
