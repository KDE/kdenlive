import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick 2.11

Item {
    id: overlay
    //property color: 'cyan'
    Rectangle {
        id: safezone
        objectName: "safezone"
        color: "transparent"
        border.color: root.overlayColor
        width: parent.width * 0.9
        height: parent.height * 0.9
        anchors.centerIn: parent
        Rectangle {
            id: safetext
            objectName: "safetext"
            color: "transparent"
            border.color: root.overlayColor
            width: frame.width * 0.8
            height: frame.height * 0.8
            anchors.centerIn: parent
        }
        Rectangle {
            color: root.overlayColor
            width: frame.width / 20
            height: 1
            anchors.centerIn: parent
        }
        Rectangle {
            color: root.overlayColor
            height: frame.width / 20
            width: 1
            anchors.centerIn: parent
        }
        Rectangle {
            color: root.overlayColor
            height: frame.height / 11
            width: 1
            y: 0
            x: parent.width / 2
        }
        Rectangle {
            color: root.overlayColor
            height: frame.height / 11
            width: 1
            y: parent.height -height
            x: parent.width / 2
        }
        Rectangle {
            color: root.overlayColor
            width: frame.width / 11
            height: 1
            y: parent.height / 2
            x: 0
        }
        Rectangle {
            color: root.overlayColor
            width: frame.width / 11
            height: 1
            y: parent.height / 2
            x: parent.width -width
        }
    }
}
