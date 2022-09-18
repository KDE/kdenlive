/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick 2.15

Rectangle {
    id: counter
    property int countdown: 3
    property int subcount: 0
    anchors.fill: parent
    property int size: Math.min(width, height)
    color: Qt.rgba(0, 0, 0, 0.5)
    Timer {
        id: countdownTimer
        interval: 100
        running: counter.countdown > 0
        repeat: true
        onTriggered: {
            counter.subcount += 1
            if (counter.subcount % 10 == 0) {
                counter.subcount = 0
                counter.countdown--
                if (counter.countdown == 0) {
                    root.stopCountdown()
                }
            }
        }
    }
    Rectangle {
        width: counter.size * 0.6
        height: width
        color: "black"
        border.color: "white"
        border.width: 4
        radius: width*0.5
        opacity: 0.5
        anchors.centerIn: parent
    }
    Repeater {
        model: 4
        anchors.fill: parent
        delegate: Label {
            anchors.centerIn: parent
            visible: counter.countdown <= index
            opacity: counter.countdown == index ? 1 : 0.4
            scale: counter.countdown >= index ? 1.0 : 0.0
            text: modelData
            color: index < 2 ? 'red' : 'white'
            font.pixelSize: counter.size * 0.5
            Behavior on opacity { NumberAnimation {} }
            //horizontalAlignment: Text.AlignHCenter
        }
    }
    Rectangle {
        color: 'white'
        height: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: parent.width - (counter.subcount * parent.width / 10)
    }
}
