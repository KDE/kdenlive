import QtQuick 2.0

Item {
    id: root
    objectName: "rootripple"

    // default size, but scalable by user
    height: 300; width: 400
    signal doAcceptRipple(bool doAccept)
    property int displayFontSize

    Rectangle {
        id: info
        x: root.x
        y: root.height / 2
        width: root.width
        height: root.displayFontSize * 2
        color: "darkBlue"
        Text {
            text: 'Ripple Mode'
            color: "white"
            anchors.centerIn: parent
        }
    }

    MouseArea {
        width: root.width; height: root.height / 2 - info.height
        anchors {
            left: root.left
            top: info.bottom
        }
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.doAcceptRipple(mouseX > width / 2)
        }
    }

    Rectangle {
        id: cancel
        anchors {
            left: root.left
            top: info.bottom
        }
        width: root.width / 2
        height: root.height / 2 - info.height
        color: "darkRed"
        Text {
            text: 'Cancel'
            color: "white"
            anchors.centerIn: parent
        }
    }
    Rectangle {
        id: accept
        anchors {
            right: root.right
            top: info.bottom
        }
        width: root.width / 2
        height: root.height / 2 - info.height
        color: "darkGreen"
        Text {
            text: 'Accept'
            color: "white"
            anchors.centerIn: parent
        }
    }
}
