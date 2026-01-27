/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kdenlive as K

Rectangle {
    id: dopeRoot
    anchors.fill: parent
    SystemPalette { id: activePalette }
    property int keyframeSize: Math.max(12, fontMetrics.font.pixelSize)
    color: activePalette.base
    FontMetrics {
        id: fontMetrics
        font: miniFont
    }
    readonly property font miniFont: ({
        pixelSize: miniFontSize
    })
    TreeView {
        // The model needs to be a QAbstractItemModel
        id: treeView
        model: timeline.dopeSheetModel()
        anchors.fill: parent
        selectionModel: ItemSelectionModel {}
        // You can set a custom delegate or use a built-in TreeViewDelegate
        delegate: Item {
            id: contentRect
            implicitWidth: dopeRoot.width
            implicitHeight: fontMetrics.lineSpacing
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
            ToolButton {
                icon.name: "arrow-right"
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
                width: 160
                background: Rectangle {
                    color: activePalette.highlight
                    radius: 4
                    visible: row == treeView.currentRow
                }
            }
            Item {
                id: kfContainer
                anchors.left: paramLabel.right
                anchors.right: contentRect.right
                anchors.top: contentRect.top
                anchors.bottom: contentRect.bottom
                anchors.leftMargin: dopeRoot.keyframeSize
                anchors.rightMargin: dopeRoot.keyframeSize
                // visible: depth > 0
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 6
                    radius: 2
                    color: activePalette.light
                    border.width: 1
                    border.color: activePalette.shadow
                }
                Repeater {
                    model: dopeModel
                    Rectangle {
                        id: handle
                        property int clickPos
                        property double currentPercentPos
                        x: percentPosition * kfContainer.width - dopeRoot.keyframeSize/2 - ((kfArea.containsMouse || kfArea.pressed) ? 1 : 0)
                        anchors.verticalCenter: kfContainer.verticalCenter
                        width: dopeRoot.keyframeSize - (kfArea.containsMouse ? 0 : 2)
                        height: width
                        color: activePalette.light
                        radius: Math.round(width/2)
                        border.width: 1
                        border.color: (kfArea.containsMouse || kfArea.pressed) ? activePalette.highlight : activePalette.text
                        MouseArea {
                            id: kfArea
                            anchors.fill: handle
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPositionChanged: mouse => {
                                if (mouse.buttons === Qt.LeftButton) {
                                    currentPercentPos = Math.max(0., (parent.x + mouse.x) / kfContainer.width)
                                    currentPercentPos = Math.min(1., currentPercentPos)
                                    dopeModel.movePercentKeyframe(index, currentPercentPos)
                                    handle.x = currentPercentPos * kfContainer.width - dopeRoot.keyframeSize/2 - (kfArea.containsMouse ? 1 : 0)
                                }
                            }
                            onClicked: {
                                clickPos = frame
                            }
                            onReleased: {
                                if (depth == 0) {
                                    console.log("====================== MOVED RECAP EFFECT KF ===============")
                                    // TODO: Move all param keyframes at pos clickPos
                                    //timeline.moveAssetKeyframes(index, clickPos, currentPercentPos);
                                } else {
                                    dopeModel.movePercentKeyframeWithUndo(index, clickPos, currentPercentPos)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
