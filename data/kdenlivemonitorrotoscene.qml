import QtQuick 2.0

Item {
    id: root
    objectName: "rootrotoscene"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize: Qt.rect(5, 5, 200, 200)
    property point profile
    profile: Qt.point(1920, 1080)
    property point center: Qt.point(960, 540)
    property double scalex : 1
    property double scaley : 1
    property double stretch : 1
    property double sourcedar : 1
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onSourcedarChanged: refreshdar()
    property bool iskeyframe : true
    property int requestedKeyFrame
    property var centerPoints: [Qt.point(120, 100), Qt.point(160, 100), Qt.point(180, 200), Qt.point(120, 200)]
    property var centerPointsTypes: [Qt.point(120, 100), Qt.point(160, 100), Qt.point(180, 200), Qt.point(120, 200)]
    onCenterPointsChanged: canvas.requestPaint()
    signal effectPolygonChanged()
    signal addKeyframe()
    signal seekToKeyframe()

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
            var startP = p1;
            ctx.moveTo(p1.x, p1.y)
            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            for (var i = 0; i < root.centerPoints.length - 1; i++) {
                p1 = convertPoint(root.centerPoints[i + 1])
                // Control points
                var c1 = convertPoint(root.centerPointsTypes[i])
                var c2 = convertPoint(root.centerPointsTypes[i + 1])
                ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, p1.x, p1.y);
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            }
            if (root.centerPoints.length > 2) {
                var c1 = convertPoint(root.centerPointsTypes[root.centerPoints.length - 1])
                var c2 = convertPoint(root.centerPointsTypes[0])
                ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, startP.x, startP.y);
            }

            ctx.stroke()
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
}
