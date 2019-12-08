import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick 2.11

Item {
    id: overlay
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
}
