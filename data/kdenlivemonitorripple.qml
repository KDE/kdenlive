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
            text: 'Rolling Mode'
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
            root.doAcceptRipple(true)
        }
    }
    Rectangle {
        id: accept
        anchors {
            left: root.left
            top: info.bottom
        }
        width: root.width
        height: root.height / 2 - info.height
        color: "darkGreen"
        Text {
            text: 'close'
            color: "white"
            anchors.centerIn: parent
        }
    }
}
