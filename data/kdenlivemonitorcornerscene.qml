import QtQuick 2.0

Item {
    id: root
    objectName: "rootcornerscene"

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
    signal effectPolygonChanged()
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
            var p1 = convertPoint(root.centerPoints[0])
            context.lineTo(p1.x, p1.y)
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
              if (pressed && root.requestedKeyFrame >= 0) return true;
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

        onDoubleClicked: {
            root.addKeyframe()
        }

        onPositionChanged: {
            if (pressed && root.requestedKeyFrame >= 0) {
                  root.centerPoints[root.requestedKeyFrame].x = (mouseX - frame.x) / root.scale;
                  root.centerPoints[root.requestedKeyFrame].y = (mouseY - frame.y) / root.scale;
                  canvas.requestPaint()
                }
        }

        onReleased: {
            root.effectPolygonChanged()
        }
    }
}
