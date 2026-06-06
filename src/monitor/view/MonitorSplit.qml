/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "rootsplit"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    required property K.MonitorProxy controller
    property int duration: 300
    property int splitterPos
    property rect framesize
    // percentage holds splitter pos relative to the scene percentage
    property double percentage
    property point profile: controller.profile
    property point center
    property double offsetx
    property double offsety
    property double scalex
    property double scaley
    Component.onCompleted: {
        controller.rulerHeight = 0
    }

    signal qmlMoveSplit()

    percentage: 0.5
    splitterPos: this.width / 2

    MouseArea {
        width: root.width; height: root.height
        anchors.centerIn: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton
        onWheel: wheel => {
            root.controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onPressed: {
            root.percentage = (mouseX - (root.width - (root.profile.x * root.scalex)) / 2) / (root.profile.x * root.scalex)
            root.splitterPos = mouseX
            root.qmlMoveSplit()
        }
        onPositionChanged: {
            if (pressed) {
                root.percentage = (mouseX - (root.width - (root.profile.x * root.scalex)) / 2) / (root.profile.x * root.scalex)
                root.splitterPos = mouseX
                root.qmlMoveSplit()
            }
            timer.restart()
            splitter.visible = true
        }
        //onEntered: { splitter.visible = true }
        onExited: { splitter.visible = false }
    }

    Rectangle {
        id: splitter
        x: root.splitterPos
        y: 0
        width: 1
        height: root.height
        color: "red"
        visible: false
        Text {
            text: KI18n.i18n("Effect")
            color: "red"
            anchors {
                right: parent.left
                top: parent.top
                topMargin: 10
                rightMargin: 10
            }
        }
    }
    K.MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        height: root.controller.rulerHeight
        monitorController: root.controller
        duration: root.duration
    }

    Timer {
        id: timer

        interval: 1000; running: false; repeat: false
        onTriggered:  {
            splitter.visible = false
        }
    }
}
