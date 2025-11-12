/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls

import org.kde.kdenlive as Kdenlive

MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    width: 2.4 * fontMetrics.font.pixelSize
    acceptedButtons: Qt.NoButton
    height: parent.height
    onEntered: {
        animator.stop()
        scenetoolbar.opacity = 1
    }
    onExited: {
        if (!zoomButton.menuVisible) {
            scenetoolbar.opacity = 0
        }
    }

    Rectangle {
        id: scenetoolbar
        objectName: "scenetoolbar"
        width: barZone.width
        height: childrenRect.height
        SystemPalette { id: myPalette; colorGroup: SystemPalette.Active }
        color: Qt.rgba(myPalette.window.r, myPalette.window.g, myPalette.window.b, 0.7)
        opacity: 0
        radius: 4
        border.color : Qt.rgba(0, 0, 0, 0.3)
        border.width: 1
        OpacityAnimator {
            id: animator
            target: scenetoolbar;
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
            Kdenlive.MonitorToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                iconName: "view-fullscreen"
                toolTipText: i18n("Switch Full Screen")
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_fullscreen')
                }
            }
            Kdenlive.MonitorToolButton {
                objectName: "showSafeZone"
                iconName: "select-rectangular"
                toolTipText: i18n("Show Safe Zone")
                checkable: true
                checked: false
                onCheckedChanged: {
                    controller.showSafezone = checked
                }
                Component.onCompleted: {
                    checked = controller.showSafezone
                }
            }
            Kdenlive.MonitorToolButton {
                id: switchOverlay
                iconName: "view-grid"
                toolTipText: i18n("Composition Guides")
                onClicked: {
                    if (controller.overlayType >= 5) {
                        controller.overlayType = 0
                    } else {
                        controller.overlayType = controller.overlayType + 1;
                    }
                    root.overlayType = controller.overlayType
                }
            }
            Kdenlive.MonitorZoomButton {
                id: zoomButton
            }
            Kdenlive.MonitorToolButton {
                objectName: "addMarker"
                iconName: "bookmark-new"
                toolTipText: i18n("Add/Remove Marker")
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('add_marker_guide_quickly')
                }
            }
            Kdenlive.MonitorToolButton {
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
                    scenetoolbar.fadeBar()
                }
            }
        }
    }
}
