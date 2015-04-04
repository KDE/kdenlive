import QtQuick 2.0

Item {
    id: root
    objectName: "root"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize
    property rect framebase
    property point center
    property double scale
    property bool iskeyframe
    signal qmlSignal()
    
    Item {
      id: effectscene
      objectName: "effectscene"
      visible: false
    Rectangle {
        id: frame
        objectName: "frame"
        property color hoverColor: "#ff0000"
        //anchors.centerIn: root.center
        //x: root.framebase.x
        //y: root.framebase.y
        width: root.framebase.width * root.scale
        height: root.framebase.height * root.scale
        x: root.center.x - width / 2
        y: root.center.y - height / 2
        //height: root.height * 0.1
        color: "transparent"
        border.color: "#ffffff00"
    }
    
    Rectangle {
        id: framerect
        objectName: "framerect"
        property color hoverColor: "#ff0000"
        x: frame.x + root.framesize.x * root.scale
        y: frame.y + root.framesize.y * root.scale
        width: root.framesize.width * root.scale
        height: root.framesize.height * root.scale
        //height: root.height * 0.1
        color: "transparent"
        border.color: "#ffff0000"
        Rectangle {
            id: "tlhandle"
            anchors {
            top: parent.top
            left: parent.left
            }
            visible: root.iskeyframe
            width: 10
            height: 10
            color: "red"
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: 10; height: 10
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: "PointingHandCursor"
              onEntered: { tlhandle.color = '#ffff00'}
              onExited: { tlhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scale;
                  framesize.width = (framerect.width - (mouseX - oldMouseX)) / root.scale;
                  framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scale;
                  framesize.height = (framerect.height - (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.qmlSignal()
              }
            }
        }
        Rectangle {
            id: "trhandle"
            anchors {
            top: parent.top
            right: parent.right
            }
            width: 10
            height: 10
            color: "red"
            visible: root.iskeyframe
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: 10; height: 10
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: "PointingHandCursor"
              onEntered: { trhandle.color = '#ffff00'}
              onExited: { trhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.width = (framerect.width + (mouseX - oldMouseX)) / root.scale;
                  framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scale;
                  framesize.height = (framerect.height - (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.qmlSignal()
              }
            }
        }
        Rectangle {
            id: "blhandle"
            anchors {
            bottom: parent.bottom
            left: parent.left
            }
            width: 10
            height: 10
            color: "red"
            visible: root.iskeyframe
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: 10; height: 10
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: "PointingHandCursor"
              onEntered: { blhandle.color = '#ffff00'}
              onExited: { blhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scale;
                  framesize.width = (framerect.width - (mouseX - oldMouseX)) / root.scale;
                  framesize.height = (framerect.height + (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.qmlSignal()
              }
            }
        }
        Rectangle {
            id: "brhandle"
            anchors {
            bottom: parent.bottom
            right: parent.right
            }
            width: 10
            height: 10
            color: "red"
            visible: root.iskeyframe
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: 10; height: 10
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: "PointingHandCursor"
              onEntered: { brhandle.color = '#ffff00'}
              onExited: { brhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.width = (framerect.width + (mouseX - oldMouseX)) / root.scale;
                  framesize.height = (framerect.height + (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.qmlSignal()
              }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: 10
            color: framerect.hoverColor
            visible: root.iskeyframe
            MouseArea {
              width: 12; height: 12
              anchors.centerIn: parent
              property int oldMouseX
              property int oldMouseY
              hoverEnabled: true
              cursorShape: "PointingHandCursor"
              onEntered: { framerect.hoverColor = '#ffff00'}
              onExited: { framerect.hoverColor = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
              }
              onPositionChanged: {
                  if (pressed) {
                      framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scale;
                      framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scale;
                  }
              }
              onReleased: {
                  root.qmlSignal()
              }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 1
            color: framerect.hoverColor
            visible: root.iskeyframe
        }
    }
    }
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
