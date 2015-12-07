import QtQuick 2.0

Item {
    id: root
    objectName: "root"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property point center
    property double scale

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
