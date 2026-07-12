/*
    SPDX-FileCopyrightText: 2016-2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

MouseArea {
    id: barZone
    hoverEnabled: true
    required property K.MonitorProxy monitorController
    required property bool isClipMonitor
    property bool rightSide: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * fontMetrics.font.pixelSize
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

    FontMetrics {
        id: fontMetrics
        font: K.UiUtils.fixedFont
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
            K.MonitorToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                iconName: "view-fullscreen"
                toolTipText: KI18n.i18n("Switch Full Screen")
                onClicked: {
                    barZone.monitorController.activateClipMonitor(barZone.isClipMonitor)
                    K.Core.triggerAction('monitor_fullscreen')
                }
            }
            K.MonitorZoomButton {
                id: zoomButton
                monitorController: barZone.monitorController
                isClipMonitor: barZone.isClipMonitor
            }
            K.MonitorToolButton {
                objectName: "moveBar"
                iconName: "transform-move-horizontal"
                toolTipText: KI18n.i18n("Move Toolbar")
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
