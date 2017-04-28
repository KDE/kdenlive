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
    property double zoom
    property double scalex
    property double scaley
    property double stretch : 1
    property double sourcedar : 1
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    property double offsetx : 0
    property double offsety : 0
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    onSourcedarChanged: refreshdar()
    property bool iskeyframe
    property int requestedKeyFrame
    property var centerPoints: []
    property bool showToolbar: false
    onCenterPointsChanged: canvas.requestPaint()
    signal effectPolygonChanged()
    signal addKeyframe()
    signal seekToKeyframe()
    signal toolBarChanged(bool doAccept)
    onZoomChanged: {
        effectToolBar.setZoom(root.zoom)
    }

    function refreshdar() {
        canvas.darOffset = root.sourcedar < root.profile.x * root.stretch / root.profile.y ? (root.profile.x * root.stretch - root.profile.y * root.sourcedar) / (2 * root.profile.x * root.stretch) :(root.profile.y - root.profile.x * root.stretch / root.sourcedar) / (2 * root.profile.y);
        canvas.requestPaint()
    }

    Text {
        id: fontReference
        property int fontSize
        fontSize: font.pointSize
    }

    Canvas {
      id: canvas
      property double handleSize
      property double darOffset : 0
      width: root.width
      height: root.height
      anchors.centerIn: root
      contextType: "2d";
      handleSize: fontReference.fontSize / 2
      renderTarget: Canvas.FramebufferObject
      renderStrategy: Canvas.Cooperative
      onPaint:
      {
        var ctx = getContext('2d')
        //if (context) {
            ctx.clearRect(0,0, width, height);
            ctx.beginPath()
            ctx.strokeStyle = Qt.rgba(1, 0, 0, 0.5)
            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
            ctx.lineWidth = 2
            var p1 = convertPoint(root.centerPoints[0])
            var p2 = convertPoint(root.centerPoints[1])
            var p3 = convertPoint(root.centerPoints[2])
            var p4 = convertPoint(root.centerPoints[3])
            //console.log('paint' + p1);

          // Handles
          if (root.iskeyframe == true) {
            if (root.requestedKeyFrame == 0) {
                ctx.fillStyle = Qt.rgba(1, 1, 0, 1)
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
            }
            else ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            if (root.requestedKeyFrame == 1) {
                ctx.fillStyle = Qt.rgba(1, 1, 0, 1)
                ctx.fillRect(p2.x - handleSize, p2.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
            }
            else ctx.fillRect(p2.x - handleSize, p2.y - handleSize, 2 * handleSize, 2 * handleSize);
            if (root.requestedKeyFrame == 2) {
                ctx.fillStyle = Qt.rgba(1, 1, 0, 1)
                ctx.fillRect(p3.x - handleSize, p3.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
            }
            else ctx.fillRect(p3.x - handleSize, p3.y - handleSize, 2 * handleSize, 2 * handleSize);
            if (root.requestedKeyFrame == 3) {
                ctx.fillStyle = Qt.rgba(1, 1, 0, 1)
                ctx.fillRect(p4.x - handleSize, p4.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
            }
            else ctx.fillRect(p4.x - handleSize, p4.y - handleSize, 2 * handleSize, 2 * handleSize);
          }
          // Rect
          ctx.moveTo(p1.x, p1.y)
          ctx.lineTo(p2.x, p2.y)
          ctx.lineTo(p3.x, p3.y)
          ctx.lineTo(p4.x, p4.y)
          ctx.lineTo(p1.x, p1.y)

          // Source rect
          if (canvas.darOffset != 0 && root.sourcedar > 0) {
              if (root.sourcedar < root.profile.x / root.profile.y) {
                  // vertical bars
                  ctx.moveTo(p1.x + (darOffset * (p2.x - p1.x)), p1.y + (darOffset * (p2.y - p1.y)))
                  ctx.lineTo(p4.x + (darOffset * (p3.x - p4.x)), p4.y + (darOffset * (p3.y-p4.y)))
                  ctx.moveTo(p2.x + (darOffset * (p1.x - p2.x)), p2.y + (darOffset * (p1.y - p2.y)))
                  ctx.lineTo(p3.x + (darOffset * (p4.x - p3.x)), p3.y + (darOffset * (p4.y-p3.y)))
              } else {
                  // horizontal bars
                  ctx.moveTo(p1.x + (darOffset * (p4.x - p1.x)), p1.y + (darOffset * (p4.y - p1.y)))
                  ctx.lineTo(p2.x + (darOffset * (p3.x - p2.x)), p2.y + (darOffset * (p3.y-p2.y)))
                  ctx.moveTo(p4.x + (darOffset * (p1.x - p4.x)), p4.y + (darOffset * (p1.y - p4.y)))
                  ctx.lineTo(p3.x + (darOffset * (p2.x - p3.x)), p3.y + (darOffset * (p2.y-p3.y)))
              }
          }
          ctx.stroke()
            //ctx.restore()
        //}
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
        x: root.center.x - width / 2 - root.offsetx
        y: root.center.y - height / 2 - root.offsety
        color: "transparent"
        border.color: "#ffffff00"
    }
    MouseArea {
        id: global
        objectName: "global"
        width: root.width; height: root.height
        property bool containsMouse
        anchors.centerIn: root
        hoverEnabled: true
        cursorShape: containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor

        onDoubleClicked: {
            root.addKeyframe()
        }

        onPositionChanged: {
            if (root.iskeyframe == false) return;
            if (pressed && root.requestedKeyFrame >= 0) {
                root.centerPoints[root.requestedKeyFrame].x = (mouseX - frame.x) / root.scalex;
                root.centerPoints[root.requestedKeyFrame].y = (mouseY - frame.y) / root.scaley;
                canvas.requestPaint()
                root.effectPolygonChanged()
            } else {
              for(var i = 0; i < root.centerPoints.length; i++)
              {
                var p1 = canvas.convertPoint(root.centerPoints[i])
                if (Math.abs(p1.x - mouseX) <= canvas.handleSize && Math.abs(p1.y - mouseY) <= canvas.handleSize) {
                    if (i == root.requestedKeyFrame) {
                        containsMouse = true;
                        return;
                    }
                    root.requestedKeyFrame = i
                    canvas.requestPaint()
                    containsMouse = true;
                    return;
                }
              }
              if (root.requestedKeyFrame == -1) {
                  return;
              }
              root.requestedKeyFrame = -1
              containsMouse = false;
              canvas.requestPaint()
            }
        }
    }
    EffectToolBar {
        id: effectToolBar
        anchors {
            left: parent.left
            top: parent.top
            topMargin: 10
            leftMargin: 10
        }
        visible: root.showToolbar
    }
}
