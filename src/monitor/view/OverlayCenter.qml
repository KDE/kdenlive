import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.0

Item {
    id: overlay
    Rectangle {
        color: root.overlayColor
        width: parent.width
        height: 1
        anchors.centerIn: parent
    }
    Rectangle {
        color: root.overlayColor
        height: parent.height
        width: 1
        anchors.centerIn: parent
    }
}
