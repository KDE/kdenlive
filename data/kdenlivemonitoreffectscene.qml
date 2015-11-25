import QtQuick 2.0

Item {
    id: root
    objectName: "rooteffectscene"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize
    property point profile
    property point center
    property double scale
    onScaleChanged: canvas.requestPaint()
    property bool iskeyframe
    property int requestedKeyFrame
    property var centerPoints: []
    onCenterPointsChanged: canvas.requestPaint()
    signal effectChanged()
    signal addKeyframe()
    signal seekToKeyframe()

    Text {
        id: fontReference
        property int fontSize
        fontSize: font.pointSize
    }

    Canvas {
      id: canvas
      property double handleSize
      width: root.width
      height: root.height
      anchors.centerIn: root
      contextType: "2d";
      handleSize: fontReference.fontSize / 2
      renderStrategy: Canvas.Threaded;
      onPaint:
      {
        if (context) {
            context.clearRect(0,0, width, height);
            context.beginPath()
            context.strokeStyle = Qt.rgba(1, 0, 0, 0.5)
            context.fillStyle = Qt.rgba(1, 0, 0, 0.5)
            context.lineWidth = 2

            for(var i = 0; i < root.centerPoints.length; i++)
            {
                var p1 = convertPoint(root.centerPoints[i])
                if(i == 0)
                {
                    context.moveTo(p1.x, p1.y)
                    context.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                    continue
                }
                context.lineTo(p1.x, p1.y)
                context.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            }
            context.stroke()
            context.restore()
        }
    }

    function convertPoint(p)
    {
        var x = frame.x + p.x * root.scale
        var y = frame.y + p.y * root.scale
        return Qt.point(x,y);
    }
  }
    Rectangle {
        id: frame
        objectName: "referenceframe"
        property color hoverColor: "#ff0000"
        width: root.profile.x * root.scale
        height: root.profile.y * root.scale
        x: root.center.x - width / 2
        y: root.center.y - height / 2
        color: "transparent"
        border.color: "#ffffff00"
    }
    MouseArea {
        id: global
        objectName: "global"
        width: root.width; height: root.height
        anchors.centerIn: root
        hoverEnabled: true
        cursorShape: containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor

        readonly property bool containsMouse: {
              for(var i = 0; i < root.centerPoints.length; i++)
              {
                var p1 = canvas.convertPoint(root.centerPoints[i])
                if (Math.abs(p1.x - mouseX) <= canvas.handleSize && Math.abs(p1.y - mouseY) <= canvas.handleSize) {
                    root.requestedKeyFrame = i
                    return true
                }
              }
              root.requestedKeyFrame = -1
              return false
        }

        onClicked: {
            if (root.requestedKeyFrame >= 0) {
                root.seekToKeyframe();
            }

        }
        onDoubleClicked: {
            root.addKeyframe()
        }
    }
    Rectangle {
        id: framerect
        property color hoverColor: "#ffffff"
        x: frame.x + root.framesize.x * root.scale
        y: frame.y + root.framesize.y * root.scale
        width: root.framesize.width * root.scale
        height: root.framesize.height * root.scale
        color: "transparent"
        border.color: "#ffff0000"
        Rectangle {
            id: "tlhandle"
            anchors {
            top: parent.top
            left: parent.left
            }
            visible: root.iskeyframe
            width: effectsize.height * 0.7
            height: this.width
            color: "red"
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: parent.width; height: parent.height
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
                  root.effectChanged()
                }
              }
              onReleased: {
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
            width: effectsize.height * 0.7
            height: this.width
            color: "red"
            visible: root.iskeyframe
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: parent.width; height: parent.height
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
                  root.effectChanged()
                }
              }
              onReleased: {
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
            width: effectsize.height * 0.7
            height: this.width
            color: "red"
            visible: root.iskeyframe
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: parent.width; height: parent.height
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
                  root.effectChanged()
                }
              }
              onReleased: {
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
            width: effectsize.height * 0.7
            height: this.width
            color: "red"
            visible: root.iskeyframe
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              width: parent.width; height: parent.height
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
                  root.effectChanged()
                }
              }
              onReleased: {
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
            height: root.iskeyframe ? effectsize.height * 1.5 : effectsize.height / 2
            color: framerect.hoverColor
            MouseArea {
              width: effectsize.height * 1.5; height: effectsize.height * 1.5
              anchors.centerIn: parent
              property int oldMouseX
              property int oldMouseY
              hoverEnabled: true
              enabled: root.iskeyframe
              cursorShape: root.iskeyframe ? Qt.SizeAllCursor : Qt.ArrowCursor
              onEntered: { framerect.hoverColor = '#ffff00'}
              onExited: { framerect.hoverColor = '#ffffff'}
              onPressed: {
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectpos.visible = true
              }
              onPositionChanged: {
                  if (pressed) {
                      framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scale;
                      framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scale;
                      root.effectChanged()
                  }
              }
              onReleased: {
                  effectpos.visible = false
              }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: root.iskeyframe ? effectsize.height * 1.5 : effectsize.height / 2
            height: 1
            color: framerect.hoverColor
        }
    }
}
