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
    property double scalex
    property double scaley
    property double stretch
    property double sourcedar
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
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
      property double darOffset
      width: root.width
      height: root.height
      anchors.centerIn: root
      darOffset: root.sourcedar < root.profile.x * root.stretch / root.profile.y ? (root.profile.x * root.stretch - root.profile.y * root.sourcedar) / (2 * root.profile.x * root.stretch) :(root.profile.y - root.profile.x * root.stretch / root.sourcedar) / (2 * root.profile.y)
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
            var p1 = convertPoint(root.centerPoints[0])
            var p2 = convertPoint(root.centerPoints[1])
            var p3 = convertPoint(root.centerPoints[2])
            var p4 = convertPoint(root.centerPoints[3])

            // Handles
            context.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            context.fillRect(p2.x - handleSize, p2.y - handleSize, 2 * handleSize, 2 * handleSize);
            context.fillRect(p3.x - handleSize, p3.y - handleSize, 2 * handleSize, 2 * handleSize);
            context.fillRect(p4.x - handleSize, p4.y - handleSize, 2 * handleSize, 2 * handleSize);
            // Rect
            context.moveTo(p1.x, p1.y)
            context.lineTo(p2.x, p2.y)
            context.lineTo(p3.x, p3.y)
            context.lineTo(p4.x, p4.y)
            context.lineTo(p1.x, p1.y)

            // Source rect
            if (sourcedar > 0) {
                if (sourcedar < root.profile.x / root.profile.y) {
                    // vertical bars
                    context.moveTo(p1.x + (darOffset * (p2.x - p1.x)), p1.y + (darOffset * (p2.y - p1.y)))
                    context.lineTo(p4.x + (darOffset * (p3.x - p4.x)), p4.y + (darOffset * (p3.y-p4.y)))
                    context.moveTo(p2.x + (darOffset * (p1.x - p2.x)), p2.y + (darOffset * (p1.y - p2.y)))
                    context.lineTo(p3.x + (darOffset * (p4.x - p3.x)), p3.y + (darOffset * (p4.y-p3.y)))
                } else {
                    // horizontal bars
                    context.moveTo(p1.x + (darOffset * (p4.x - p1.x)), p1.y + (darOffset * (p4.y - p1.y)))
                    context.lineTo(p2.x + (darOffset * (p3.x - p2.x)), p2.y + (darOffset * (p3.y-p2.y)))
                    context.moveTo(p4.x + (darOffset * (p1.x - p4.x)), p4.y + (darOffset * (p1.y - p4.y)))
                    context.lineTo(p3.x + (darOffset * (p2.x - p3.x)), p3.y + (darOffset * (p2.y-p3.y)))
                }
            }
            context.stroke()
            context.restore()
        }
    }

    function convertPoint(p)
    {
        var x = frame.x + p.x * root.scalex
        var y = frame.y + p.y * root.scaley
        return Qt.point(x,y);
    }
  }
    Rectangle {
        id: frame
        objectName: "referenceframe"
        property color hoverColor: "#ff0000"
        width: root.profile.x * root.scalex
        height: root.profile.y * root.scaley
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
                  root.centerPoints[root.requestedKeyFrame].x = (mouseX - frame.x) / root.scalex;
                  root.centerPoints[root.requestedKeyFrame].y = (mouseY - frame.y) / root.scaley;
                  root.effectPolygonChanged()
                }
        }
    }
}
