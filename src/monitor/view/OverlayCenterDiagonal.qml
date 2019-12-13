import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick 2.11

Item {
    id: overlay
    property double diagonalLenght: Math.sqrt(Math.pow(parent.height, 2) + Math.pow(parent.width, 2))

    function degreesRotation(width, height) {
        var a = height/width;
        var b = Math.sqrt(1 + Math.pow(a, 2));
        var angle = Math.acos(Math.pow(a,2) / (a * b));
        return angle * (180 / Math.PI);
    }

    Rectangle {
        color: root.overlayColor
        width: overlay.diagonalLenght
        height: 1
        rotation: degreesRotation(parent.height, parent.width)
        anchors.centerIn: parent
        antialiasing: true
    }

    Rectangle {
        color: root.overlayColor
        height: overlay.diagonalLenght
        width: 1
        rotation: degreesRotation(parent.width, parent.height)
        anchors.centerIn: parent
        antialiasing: true
    }
}
