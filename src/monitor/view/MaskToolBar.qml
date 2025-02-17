/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls
import QtQuick

MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    property bool showAutoKeyframe: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * fontMetrics.font.pixelSize
    height: parent.height

    Rectangle {
        id: effecttoolbar
        objectName: "effecttoolbar"
        width: barZone.width
        anchors.verticalCenter: parent.verticalCenter
        height: childrenRect.height
        color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.7)
        radius: 4
        border.color : Qt.rgba(0, 0, 0, 0.3)
        border.width: 1

        OpacityAnimator {
            id: animator
            target: effecttoolbar;
            from: 1;
            to: 0;
            duration: 2500
            running: false
        }

        function fadeBar()
        {
            animator.start()
        }

        Column {
            width: parent.width
            MonitorToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                iconName: "view-fullscreen"
                toolTipText: i18n("Switch Full Screen")
                onClicked: controller.triggerAction('monitor_fullscreen')
            }
            MonitorToolButton {
                objectName: "generateFrames"
                iconName: "media-record"
                toolTipText: i18n("Generate Mask")
                checkable: false
                visible: root.maskMode < 2
                onClicked: {
                    generateLabel.visible = true
                    root.generateMask()
                }
            }
            MonitorToolButton {
                objectName: "switchOpacity"
                iconName: "edit-opacity"
                toolTipText: i18n("Change Opacity (0% - 25% - 50% - 100%)")
                onClicked: {
                    if (controller.maskOpacity == 0) {
                        controller.maskOpacity = 25;
                    } else if (controller.maskOpacity == 100) {
                        controller.maskOpacity = 0;
                    } else {
                        controller.maskOpacity = controller.maskOpacity * 2;
                    }
                }
            }
            MonitorToolButton {
                objectName: "switchColor"
                iconName: "fill-color"
                toolTipText: i18n("Change Color")
                onClicked: {
                    if (controller.maskColor == 4) {
                        controller.maskColor = 0;
                    } else {
                        controller.maskColor++;
                    }
                }
            }
            MonitorToolButton {
                objectName: "invertMask"
                iconName: "edit-select-invert"
                toolTipText: i18n("Invert Mask")
                onClicked: {
                    controller.maskInverted = !controller.maskInverted
                }
            }
            MonitorToolButton {
                objectName: "abortMask"
                iconName: "dialog-close"
                toolTipText: root.maskMode < 2 ? i18n("Exit Mask Creation") : i18n("Exit Preview Mode")
                checkable: false
                onClicked: {
                    root.exitMaskPreview()
                }
            }
            /*MonitorToolButton {
                objectName: "nextKeyframe"
                iconName: "keyframe-next"
                toolTipText: i18n("Go to Next Keyframe")
                onClicked: controller.seekToKeyframe(-1, 1);
            }
            MonitorToolButton {
                objectName: "prevKeyframe"
                iconName: "keyframe-previous"
                toolTipText: i18n("Go to Previous Keyframe")
                onClicked: controller.seekToKeyframe(-1, -1);
            }*/
            MonitorToolButton {
                iconName: "zoom-in"
                toolTipText: i18n("Zoom in")
                onClicked: controller.triggerAction('monitor_zoomin')
            }
            MonitorToolButton {
                iconName: "zoom-out"
                toolTipText: i18n("Zoom out")
                onClicked: controller.triggerAction('monitor_zoomout')
            }

            MonitorToolButton {
                objectName: "moveBar"
                iconName: "transform-move-horizontal"
                toolTipText: i18n("Move Toolbar")
                onClicked: {
                    if (barZone.rightSide) {
                        barZone.anchors.right = undefined
                        barZone.anchors.left = barZone.parent.left
                    } else {
                        barZone.anchors.left = undefined
                        barZone.anchors.right = barZone.parent.right
                    }
                    barZone.rightSide = !barZone.rightSide
                    effecttoolbar.fadeBar()
                }
            }
        }
    }
}
