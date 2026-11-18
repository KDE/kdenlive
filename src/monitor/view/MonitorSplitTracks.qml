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
    property bool isClipMonitor: false
    property int viewType: K.SceneType.MonitorSplitTrack
    property string comment
    property string framenum
    property rect framesize
    property point profile: controller.profile
    property point center
    property double scalex
    property double scaley
    property double stretch : 1
    property double sourcedar : 1
    property double offsetx : 0
    property double offsety : 0
    property int activeTrack: 0
    property int requestedKeyFrame
    property int duration: 300
    property var tracks: []

    signal activateTrack(int position)

    function scheduleLayoutUpdate() {
        if (root.tracks.length > 1) {
            layoutUpdateTimer.restart();
	}
    }

    Timer {
       id: layoutUpdateTimer
       interval: 120
       repeat: false
       onTriggered: {
           const availableHeight = Math.round(root.height - root.controller.rulerHeight);
	   root.controller.updateMultiTrackLayout(Math.round(root.width), availableHeight);
        }
    }
    onWidthChanged: scheduleLayoutUpdate()
    onHeightChanged: scheduleLayoutUpdate()
    onTracksChanged: scheduleLayoutUpdate()
    onProfileChanged: scheduleLayoutUpdate()
    Component.onCompleted: scheduleLayoutUpdate()

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
	    property real viewportHeight: Math.max(1, root.height - root.controller.rulerHeight)
	    property real profileAspect: root.profile.x > 0 && root.profile.y > 0 ? root.profile.x / root.profile.y : (16 / 9)
	    property real viewportAspect: root.width > 0 && root.height > root.controller.rulerHeight ? root.width / viewportHeight : profileAspect
	    property int columns: trackSeparators.count > 0 ? Math.min(trackSeparators.count, Math.max(1, Math.ceil(Math.sqrt(trackSeparators.count) * Math.sqrt(viewportAspect / profileAspect)))) : 1
	    property int rows: trackSeparators.count > 0 ? Math.ceil(trackSeparators.count / columns) : 1
	    
            Rectangle {
                id: trackSeperatorFrame
                required property int index
                required property string modelData
		property int rowIndex: Math.floor(index / trackSeparators.columns)
		property int columnIndex: index % trackSeparators.columns
		property int itemsInRow: Math.min(trackSeparators.columns, trackSeparators.count - rowIndex * trackSeparators.columns)

		width: parent.width / itemsInRow
		height: parent.height / trackSeparators.rows
		x: width * columnIndex
		y: height * rowIndex
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
        isClipMonitor: root.isClipMonitor
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
