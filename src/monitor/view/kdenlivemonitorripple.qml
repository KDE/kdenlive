import QtQuick 2.11

Item {
    id: root
    objectName: "rootripple"

    // default size, but scalable by user
    height: 300; width: 400
    signal doAcceptRipple(bool doAccept)
    signal switchTrimMode(int mode)
    property int displayFontSize
    property int trimmode

    Rectangle {
        id: info
        x: root.x
        y: root.height / 2
        width: root.width / 4
        height: root.displayFontSize * 2
        border.width: 1
        color: trimmode == 1 ? "darkRed" : "darkBlue"
        Text {
            text: i18n("Ripple")
            color: "white"
            anchors.centerIn: parent
        }
        MouseArea {
          hoverEnabled: true
          anchors.fill: info
          cursorShape: Qt.PointingHandCursor
          onClicked: { root.switchTrimMode(1);}
        }
    }
    Rectangle {
        id: info2
        x: info.width
        y: info.y
        width: info.width
        height: info.height
        border.width: 1
        color: trimmode == 2 ? "darkRed" : "darkBlue"
        Text {
            text: i18n("Rolling")
            color: "white"
            anchors.centerIn: parent
        }
        MouseArea {
          hoverEnabled: true
          anchors.fill: info2
          cursorShape: Qt.PointingHandCursor
          onClicked: { root.switchTrimMode(2);}
        }
    }
    Rectangle {
        id: info3
        x: info.width * 2
        y: info.y
        width: info.width
        height: info.height
        border.width: 1
        color: trimmode == 3 ? "darkRed" : "darkBlue"
        Text {
            text: i18n("Slip")
            color: "white"
            anchors.centerIn: parent
        }
        MouseArea {
          hoverEnabled: true
          anchors.fill: info3
          cursorShape: Qt.PointingHandCursor
          onClicked: { root.switchTrimMode(3);}
        }
    }
    Rectangle {
        id: info4
        x: info.width * 3
        y: info.y
        width: info.width
        height: info.height
        border.width: 1
        color: trimmode == 4 ? "darkRed" : "darkBlue"
        Text {
            text: i18n("Slide")
            color: "white"
            anchors.centerIn: parent
        }
        MouseArea {
          hoverEnabled: true
          anchors.fill: info4
          cursorShape: Qt.PointingHandCursor
          onClicked: { root.switchTrimMode(4);}
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
            text: i18n("close")
            color: "white"
            anchors.centerIn: parent
        }
    }
}
