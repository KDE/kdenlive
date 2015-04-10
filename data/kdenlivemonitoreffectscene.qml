import QtQuick 2.0

Item {
    id: root
    objectName: "rooteffectscene"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize
    property rect framebase
    property point center
    property double scale
    property bool iskeyframe
    signal effectChanged()
    signal addKeyframe()

    Item {
      id: effectscene
      objectName: "effectscene"
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
    MouseArea {
        id: global
        objectName: "global"
        width: root.width; height: root.height
        anchors.centerIn: root
        onClicked: {

        }
        onDoubleClicked: {
            root.addKeyframe()
        }
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
              width: effectsize.height; height: effectsize.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeFDiagCursor
              onEntered: { tlhandle.color = '#ffff00'}
              onExited: { tlhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
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
                  root.effectChanged()
                  effectsize.visible = false
              }
            }
            Text {
                id: effectpos
                objectName: "effectpos"
                color: "red"
                visible: false
                anchors {
                    top: parent.bottom
                    left: parent.right
                }
                text: framesize.x.toFixed(0) + "x" + framesize.y.toFixed(0)
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
              width: effectsize.height; height: effectsize.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeBDiagCursor
              onEntered: { trhandle.color = '#ffff00'}
              onExited: { trhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.width = (framerect.width + (mouseX - oldMouseX)) / root.scale;
                  framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scale;
                  framesize.height = (framerect.height - (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.effectChanged()
                  effectsize.visible = false
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
              width: effectsize.height; height: effectsize.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeBDiagCursor
              onEntered: { blhandle.color = '#ffff00'}
              onExited: { blhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scale;
                  framesize.width = (framerect.width - (mouseX - oldMouseX)) / root.scale;
                  framesize.height = (framerect.height + (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.effectChanged()
                  effectsize.visible = false
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
              width: effectsize.height; height: effectsize.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeFDiagCursor
              onEntered: { brhandle.color = '#ffff00'}
              onExited: { brhandle.color = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
              }
              onPositionChanged: {
                if (pressed) {
                  framesize.width = (framerect.width + (mouseX - oldMouseX)) / root.scale;
                  framesize.height = (framerect.height + (mouseY - oldMouseY)) / root.scale;
                }
              }
              onReleased: {
                  root.effectChanged()
                  effectsize.visible = false
              }
            }
            Text {
                id: effectsize
                objectName: "effectsize"
                color: "red"
                visible: false
                anchors {
                    bottom: parent.top
                    right: parent.left
                }
                text: framesize.width.toFixed(0) + "x" + framesize.height.toFixed(0)
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: effectsize.height * 1.5
            color: framerect.hoverColor
            visible: root.iskeyframe
            MouseArea {
              width: effectsize.height * 1.5; height: effectsize.height * 1.5
              anchors.centerIn: parent
              property int oldMouseX
              property int oldMouseY
              hoverEnabled: true
              cursorShape: Qt.SizeAllCursor
              onEntered: { framerect.hoverColor = '#ffff00'}
              onExited: { framerect.hoverColor = '#ff0000'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectpos.visible = true
              }
              onPositionChanged: {
                  if (pressed) {
                      framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scale;
                      framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scale;
                  }
              }
              onReleased: {
                  root.effectChanged()
                  effectpos.visible = false
              }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: effectsize.height * 1.5
            height: 1
            color: framerect.hoverColor
            visible: root.iskeyframe
        }
    }
    }
}
