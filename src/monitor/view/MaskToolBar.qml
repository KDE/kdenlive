/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick

import org.kde.ki18n

import org.kde.kdenlive as K

MouseArea {
    id: barZone
    hoverEnabled: true
    required property K.MonitorProxy monitorController
    required property bool isClipMonitor
    required property int maskMode
    property bool rightSide: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * K.UiUtils.baseSizeMedium
    height: parent.height
    SystemPalette { id: activePalette }

    signal generateMask()
    signal exitMaskPreview()

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
            K.MonitorToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                iconName: "view-fullscreen"
                toolTipText: KI18n.i18n("Switch Full Screen")
                onClicked: K.Core.triggerAction('monitor_fullscreen')
            }
            K.MonitorToolButton {
                objectName: "generateFrames"
                iconName: "media-record"
                toolTipText: KI18n.i18n("Generate Mask")
                checkable: false
                visible: barZone.maskMode != MaskModeType.MaskPreview
                onClicked: {
                    barZone.generateMask()
                }
            }
            K.MonitorToolButton {
                objectName: "switchOpacity"
                iconName: "edit-opacity"
                toolTipText: KI18n.i18n("Change Opacity (0% - 25% - 50% - 100%)")
                onClicked: {
                    if (barZone.monitorController.maskOpacity == 0) {
                        barZone.monitorController.maskOpacity = 25;
                    } else if (barZone.monitorController.maskOpacity == 100) {
                        barZone.monitorController.maskOpacity = 0;
                    } else {
                        barZone.monitorController.maskOpacity = barZone.monitorController.maskOpacity * 2;
                    }
                }
            }
            K.MonitorToolButton {
                objectName: "invertMask"
                iconName: "edit-select-invert"
                toolTipText: KI18n.i18n("Invert Mask")
                onClicked: {
                    barZone.monitorController.maskInverted = !barZone.monitorController.maskInverted
                }
            }
            K.MonitorToolButton {
                objectName: "abortMask"
                iconName: "dialog-close"
                toolTipText: barZone.maskMode != MaskModeType.MaskPreview ? KI18n.i18n("Exit Mask Creation") : KI18n.i18n("Exit Preview Mode")
                checkable: false
                onClicked: {
                    barZone.exitMaskPreview()
                }
            }
            K.MonitorZoomButton {
                id: zoomButton
                monitorController: barZone.monitorController
                isClipMonitor: barZone.isClipMonitor
                onPopupAboutToHide: {
                    if (!barZone.containsMouse) {
                        effecttoolbar.opacity = 0
                    }
                }
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
