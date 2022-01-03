/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.4
import QtQuick 2.11

MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    property bool showAutoKeyframe: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * fontMetrics.font.pixelSize
    height: parent.height
    onEntered: {
        animator.stop()
        effecttoolbar.opacity = 1
    }
    onExited: {
        effecttoolbar.opacity = 0
    }

    Rectangle {
        id: effecttoolbar
        objectName: "effecttoolbar"
        width: barZone.width
        anchors.verticalCenter: parent.verticalCenter
        height: childrenRect.height
        color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.7)
        opacity: 0
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
                objectName: "switchOverlay"
                iconName: "view-grid"
                toolTipText: i18n("Change Overlay")
                onClicked: {
                    if (controller.overlayType >= 5) {
                        controller.overlayType = 0
                    } else {
                        controller.overlayType = controller.overlayType + 1;
                    }
                    root.overlayType = controller.overlayType
                }
            }
            MonitorToolButton {
                objectName: "nextKeyframe"
                iconName: "keyframe-next"
                toolTipText: i18n("Go to Next Keyframe")
                onClicked: controller.seekNextKeyframe()
            }
            MonitorToolButton {
                objectName: "prevKeyframe"
                iconName: "keyframe-previous"
                toolTipText: i18n("Go to Previous Keyframe")
                onClicked: controller.seekPreviousKeyframe()
            }
            MonitorToolButton {
                objectName: "addKeyframe"
                iconName: "keyframe-add"
                toolTipText: i18n("Add/Remove Keyframe")
                onClicked: controller.addRemoveKeyframe()
            }
            MonitorToolButton {
                iconName: "keyframe-record"
                toolTipText: i18n("Automatic Keyframes")
                onClicked: controller.switchAutoKeyframe()
                checkable: true
                checked: controller.autoKeyframe
                visible: barZone.showAutoKeyframe
            }
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
