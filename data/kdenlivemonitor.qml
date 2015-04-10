import QtQuick 2.0

Item {
    id: root
    objectName: "root"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property point center
    property double scale

    Rectangle {
        anchors {
            right: parent.right
            //left: parent.left
            bottom: parent.bottom
        }
        width: label.width + 10
        height: label.height + 10
        //height: root.height * 0.1
        color: "#99ff0000"

    Text {
        id: label
        objectName: "overlaytext"
        anchors.centerIn: parent
        text: root.framenum
    }
    }

    Rectangle {
        objectName: "marker"
        anchors {
            right: parent.right
            top: parent.top
        }
        width: marker.width + 10
        height: marker.height + 5
        color: "#99ff0000"
        border.color: "#33ff0000"
        border.width: 3
        radius: 5
        visible: root.comment != ""
         Text {
        id: marker
        objectName: "markertext"
        anchors.centerIn: parent
        color: "white"
        text: root.comment
    }
    }
}
