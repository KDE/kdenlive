/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "root"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    required property K.MonitorProxy controller
    property string comment
    property string framenum
    property rect framesize
    property point profile: controller.profile
    property int overlayType: 0
    property point center
    property double scalex
    property double scaley
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double stretch : 1
    property double sourcedar : 1
    property double offsetx : 0
    property double offsety : 0
    property int activeTrack: 0
    property int requestedKeyFrame
    property int duration: 300
    property int mouseRulerPos: 0
    property var tracks: []

    signal activateTrack(int position)

    MouseArea {
        id: barOverArea
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        onWheel: wheel => {
            root.controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
    }

    Item {
        id: frame
        objectName: "referenceframe"
        width: root.profile.x * root.scalex
        height: root.profile.y * root.scaley
        x: root.center.x - width / 2 - root.offsetx
        y: root.center.y - height / 2 - root.offsety
        Repeater {
            id: trackSeparators
            model: root.tracks
            property int rows: trackSeparators.count < 2 ? 1 : trackSeparators.count < 5 ? 2 : 3
            property int columns: trackSeparators.count < 2 ? 1 : trackSeparators.count < 3 ? 1 : trackSeparators.count < 7 ? 2 : 3
            Rectangle {
                id: trackSeperatorFrame
                required property int index
                required property string modelData
                width: parent.width / trackSeparators.rows
                height: parent.height / trackSeparators.columns
                x: width * (index % trackSeparators.rows)
                y: height * (Math.floor(index / trackSeparators.rows))
                color: "transparent"
                border.color: index == root.activeTrack ? "#ff0000" : "#00000000"
                border.width: 2
                Label {
                    text: trackSeperatorFrame.modelData
                    color: "#ffffff"
                    padding :4
                    background: Rectangle {
                        color: trackSeperatorFrame.index == root.activeTrack ? "#990000" : "#000066"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        root.activateTrack(trackSeperatorFrame.index)
                        K.Core.triggerAction('perform_multitrack_mode')
                    }
                }
            }
        }
    }
    MultiScreenToolBar {
        id: sceneToolBar
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
        monitorController: root.controller
    }
    MonitorRuler {
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
}
