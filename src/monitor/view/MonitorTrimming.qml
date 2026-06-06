/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    required property K.MonitorProxy controller
    property string markerText
    property point profile: controller.profile
    property double zoom
    property double scalex
    property double scaley
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showSafezone: false
    property bool showAudiothumb: false
    property int duration: 300
    property int overlayType: controller.overlayType
    property bool isClipMonitor: false

    FontMetrics {
        id: fontMetrics
        font: K.UiUtils.fixedFont
    }

    signal editCurrentMarker()

    function switchOverlay() {
        if (controller.overlayType >= 5) {
            controller.overlayType = 0
        } else {
            controller.overlayType = controller.overlayType + 1;
        }
        root.overlayType = controller.overlayType
    }
    MouseArea {
        id: barOverArea
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        onWheel: wheel => {
            root.controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        /*onEntered: {
            controller.setWidgetKeyBinding(KI18n.i18n("TODO: This is the wonderful new trimming monitor overlay!"));
        }*/
        onExited: {
            root.controller.setWidgetKeyBinding();
        }
    }
    K.SceneToolBar {
        id: sceneToolBar
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
        monitorController: root.controller
        isClipMonitor: root.isClipMonitor
    }

    Item {
        height: root.height - root.controller.rulerHeight
        width: root.width
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            anchors.centerIn: parent
            Rectangle {
                width: parent.width / 2
                height: parent.height / 2
                x: 0
                y: parent.height / 4
                color: "transparent"
                //border.color: "#ff0000"
                //border.width: 2
                Label {
                    text: KI18n.i18n("In")
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "#990000"
                    }
                }
                Label {
                    id: trimmingTC1
                    font.family: fontMetrics.font.family
                    objectName: "trimmingTC1"
                    text: root.controller.trimmingTC1
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "black"
                        opacity: 0.5
                    }
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 8
                    }
                }
            }
            Rectangle {
                width: parent.width / 2
                height: parent.height / 2
                x: parent.width / 2
                y: parent.height / 4
                color: "transparent"
                //border.color: "#ff0000"
                //border.width: 2
                Label {
                    text: KI18n.i18n("Out")
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "#990000"
                    }
                }
                Label {
                    id: trimmingTC2
                    font.family: fontMetrics.font.family
                    objectName: "trimmingTC2"
                    text: root.controller.trimmingTC2
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "black"
                        opacity: 0.5
                    }
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 8
                    }
                }
            }
        }
    }
}
