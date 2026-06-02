/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
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
    property bool rightSide: true
    property bool showAutoKeyframe: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * K.UiUtils.baseSizeMedium
    height: parent.height
    onEntered: {
        animator.stop()
        effecttoolbar.opacity = 1
    }
    onExited: {
        if (!zoomButton.menuVisible) {
            effecttoolbar.opacity = 0
        }
    }

    SystemPalette { id: activePalette }

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
            K.MonitorToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                iconName: "view-fullscreen"
                toolTipText: KI18n.i18n("Switch Full Screen")
                onClicked: K.Core.triggerAction('monitor_fullscreen')
            }
            K.MonitorToolButton {
                objectName: "switchGrid"
                iconName: "snap"
                toolTipText: KI18n.i18n("Show Grid")
                checkable: true
                checked: K.KdenliveSettings.showMonitorGrid
                onClicked: {
                    barZone.monitorController.switchGrid()
                }
            }
            K.MonitorToolButton {
                objectName: "showSafeZone"
                iconName: "select-rectangular"
                toolTipText: KI18n.i18n("Show Safe Areas")
                checkable: true
                checked: false
                onCheckedChanged: {
                    barZone.monitorController.showSafezone = checked
                }
                Component.onCompleted: {
                    checked = barZone.monitorController.showSafezone
                }
            }
            K.MonitorToolButton {
                objectName: "switchOverlay"
                iconName: "view-grid"
                toolTipText: KI18n.i18n("Composition Guides")
                onClicked: {
                    if (barZone.monitorController.overlayType >= 5) {
                        barZone.monitorController.overlayType = 0
                    } else {
                        barZone.monitorController.overlayType = barZone.monitorController.overlayType + 1;
                    }
                    root.overlayType = barZone.monitorController.overlayType
                }
            }
            K.MonitorToolButton {
                objectName: "nextKeyframe"
                iconName: "keyframe-next"
                toolTipText: KI18n.i18n("Go to Next Keyframe")
                onClicked: barZone.monitorController.seekToKeyframe(-1, 1);
            }
            K.MonitorToolButton {
                objectName: "prevKeyframe"
                iconName: "keyframe-previous"
                toolTipText: KI18n.i18n("Go to Previous Keyframe")
                onClicked: barZone.monitorController.seekToKeyframe(-1, -1);
            }
            K.MonitorToolButton {
                objectName: "addKeyframe"
                iconName: "keyframe-add"
                toolTipText: KI18n.i18n("Add/Remove Keyframe")
                onClicked: barZone.monitorController.addRemoveKeyframe()
            }
            K.MonitorToolButton {
                iconName: "keyframe-record"
                toolTipText: KI18n.i18n("Automatic Keyframes")
                onClicked: () => { K.KdenliveSettings.autoKeyframe = K.KdenliveSettings.autoKeyframe }
                checkable: true
                checked: K.KdenliveSettings.autoKeyframe
                visible: barZone.showAutoKeyframe
            }
            K.MonitorZoomButton {
                id: zoomButton
                monitorController: barZone.monitorController
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
                    effecttoolbar.fadeBar()
                }
            }
        }
    }
}
