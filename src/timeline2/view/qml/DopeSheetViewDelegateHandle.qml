/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

Item {
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
    visible : x >= -width/2 && x < containerWidth + width/2
    anchors.verticalCenter: parent.verticalCenter
    width: handleWidth - (kfArea.containsMouse ? 0 : 2)
    height: width
    property bool atMousePos: dopeRootItem.mouseFramePos === modelFrame
    property color fillColor: dopeRootItem.keyframeGrabbed(parentScope.getIndex(row, column), index) > -1 ? 'red' : dopeRootItem.keyframeSelected(parentScope.getIndex(row, column), index) > -1 ? activePalette.highlight : activePalette.light

    property int borderWidth: atMousePos ? 2 : 1
    property color borderColor: (kfArea.containsMouse || kfArea.pressed) ? activePalette.highlight : atMousePos ? dopeHoverColor : activePalette.text

    Component {
        id: triangleView
        Shape {
            id: shapeContainer
            anchors.fill: parent
            ShapePath {
                fillColor: handle.fillColor
                strokeColor: handle.borderColor
                strokeWidth: handle.borderWidth
                startX: shapeContainer.width / 2; startY: 0  // Top vertex
                PathLine { x: shapeContainer.width; y: shapeContainer.height } // Bottom right vertex
                PathLine { x: 0; y: shapeContainer.height }  // Bottom left vertex
                PathLine { x: shapeContainer.width / 2; y: 0 }  // Back to top to close
            }
        }
    }
    Component {
        id: bounceInView
        Shape {
            id: shapeContainer
            anchors.fill: parent
            ShapePath {
                fillColor: handle.fillColor
                strokeColor: handle.borderColor
                strokeWidth: handle.borderWidth
                startX: 0; startY: 0  // Top vertex
                PathLine { x: shapeContainer.width / 3; y: shapeContainer.height * 2 / 3 }
                PathLine { x: shapeContainer.width * 2 / 3; y: shapeContainer.height / 3 }
                PathLine { x: shapeContainer.width; y: shapeContainer.height }
                PathLine { x: 0; y: shapeContainer.height }
                PathLine { x: 0; y: 0 }
            }
        }
    }
    Component {
        id: bounceOutView
        Shape {
            id: shapeContainer
            anchors.fill: parent
            ShapePath {
                fillColor: handle.fillColor
                strokeColor: handle.borderColor
                strokeWidth: handle.borderWidth
                startX: 0; startY: shapeContainer.height  // Top vertex
                PathLine { x: shapeContainer.width / 3; y: shapeContainer.height / 3 }
                PathLine { x: shapeContainer.width * 2 / 3; y: shapeContainer.height * 2 / 3 }
                PathLine { x: shapeContainer.width; y: 0 }
                PathLine { x: shapeContainer.width; y: shapeContainer.height }
                PathLine { x: 0; y: shapeContainer.height }
            }
        }
    }
    Component {
        id: rectangleView
        Rectangle {
            anchors.fill: parent
            color: handle.fillColor
            border.color: handle.borderColor
            border.width: handle.borderWidth
            radius: handle.modelType == 0 ? 0 : Math.round(width/2)
        }
    }

    Loader {
        id: shapeLoader
        anchors.fill: parent
        sourceComponent: handle.modelType == 1 ? triangleView : handle.modelType == 32 ? bounceInView : handle.modelType == 33 ? bounceOutView : rectangleView
    }

    MouseArea {
        id: kfArea
        anchors.fill: handle
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton
        onEntered: {
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
