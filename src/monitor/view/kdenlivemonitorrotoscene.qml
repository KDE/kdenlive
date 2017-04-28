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
    property double zoom
    property double scalex : 1
    property double scaley : 1
    property double stretch : 1
    property double sourcedar : 1
    property double offsetx : 0
    property double offsety : 0
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onSourcedarChanged: refreshdar()
    property bool iskeyframe : true
    property bool isDefined: false
    property int requestedKeyFrame : -1
    property int requestedSubKeyFrame : -1
    // The coordinate points where the bezier curve passes
    property var centerPoints : []
    // The control points for the bezier curve points (2 controls points for each coordinate)
    property var centerPointsTypes : []
    property bool showToolbar: false
    onCenterPointsTypesChanged: checkDefined()
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

    function checkDefined() {
        root.isDefined = root.centerPointsTypes.length > 0
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
            if (root.centerPoints.length == 0) {
                // no points defined yet
                return
            }
            var p1 = convertPoint(root.centerPoints[0])
            var startP = p1;
            ctx.moveTo(p1.x, p1.y)
            if (!isDefined) {
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                for (var i = 1; i < root.centerPoints.length; i++) {
                    p1 = convertPoint(root.centerPoints[i])
                    ctx.lineTo(p1.x, p1.y);
                    ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
            } else {
                var c1; var c2
                for (var i = 0; i < root.centerPoints.length; i++) {
                    p1 = convertPoint(root.centerPoints[i])
                    // Control points
                    var subkf = false
                    if (i == 0) {
                        c1 = convertPoint(root.centerPointsTypes[root.centerPointsTypes.length - 1])
                        if (root.requestedSubKeyFrame == root.centerPointsTypes.length - 1) {
                            subkf = true
                        }
                    } else {
                        c1 = convertPoint(root.centerPointsTypes[2*i - 1])
                        if (root.requestedSubKeyFrame == 2*i - 1) {
                            subkf = true
                        }
                    }
                    c2 = convertPoint(root.centerPointsTypes[2*i])
                    ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, p1.x, p1.y);
                    if (iskeyframe) {
                        if (subkf) {
                            ctx.fillStyle = Qt.rgba(1, 1, 0, 0.8)
                            ctx.fillRect(c1.x - handleSize/2, c1.y - handleSize/2, handleSize, handleSize);
                            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                        } else {
                            ctx.fillRect(c1.x - handleSize/2, c1.y - handleSize/2, handleSize, handleSize);
                        }
                        if (root.requestedSubKeyFrame == 2 * i) {
                            ctx.fillStyle = Qt.rgba(1, 1, 0, 0.8)
                            ctx.fillRect(c2.x - handleSize/2, c2.y - handleSize/2, handleSize, handleSize);
                            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                        } else {
                            ctx.fillRect(c2.x - handleSize/2, c2.y - handleSize/2, handleSize, handleSize);
                        }
                        c1 = convertPoint(root.centerPointsTypes[2*i + 1])
                        ctx.lineTo(c1.x, c1.y);
                        ctx.moveTo(p1.x, p1.y)
                        ctx.lineTo(c2.x, c2.y);
                        ctx.moveTo(p1.x, p1.y)
                        if (i == root.requestedKeyFrame) {
                            ctx.fillStyle = Qt.rgba(1, 1, 0, 0.8)
                            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                        } else {
                            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                        }
                    }
                }
                if (root.centerPoints.length > 2) {
                    var c1 = convertPoint(root.centerPointsTypes[root.centerPointsTypes.length - 1])
                    var c2 = convertPoint(root.centerPointsTypes[0])
                    ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, startP.x, startP.y);
                }
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
        x: root.center.x - width / 2 - root.offsetx
        y: root.center.y - height / 2 - root.offsety
        color: "transparent"
        border.color: "#ffffff00"
    }

    MouseArea {
        id: global
        objectName: "global"
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        width: root.width; height: root.height
        property bool containsMouse
        anchors.centerIn: root
        hoverEnabled: true
        cursorShape: containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: {
            if (!root.isDefined) {
                if (mouse.button == Qt.RightButton) {
                    // close shape, define control points
                    var p0; var p1; var p2
                    for (var i = 0; i < root.centerPoints.length; i++) {
                        p1 = root.centerPoints[i]
                        if (i == 0) {
                            p0 = root.centerPoints[root.centerPoints.length - 1]
                        } else {
                            p0 = root.centerPoints[i - 1]
                        }
                        if (i == root.centerPoints.length - 1) {
                            p2 = root.centerPoints[0]
                        } else {
                            p2 = root.centerPoints[i + 1]
                        }
                        var ctrl1 = Qt.point((p0.x - p1.x) / 5, (p0.y - p1.y) / 5);
                        var ctrl2 = Qt.point((p2.x - p1.x) / 5, (p2.y - p1.y) / 5);
                        root.centerPointsTypes.push(Qt.point(p1.x + ctrl1.x, p1.y + ctrl1.y))
                        root.centerPointsTypes.push(Qt.point(p1.x + ctrl2.x, p1.y + ctrl2.y))
                    }
                    root.isDefined = true;
                    root.effectPolygonChanged()
                    canvas.requestPaint()
                } else {
                    var newPoint = Qt.point((mouseX - frame.x) / root.scalex, (mouseY - frame.y) / root.scaley);
                    root.centerPoints.push(newPoint)
                    canvas.requestPaint()
                }
            }
        }

        onDoubleClicked: {
            root.addKeyframe()
        }

        onPositionChanged: {
            if (root.iskeyframe == false) return;
            if (isDefined && pressed) {
                if (root.requestedKeyFrame >= 0) {
                    var xDiff = (mouseX - frame.x) / root.scalex - root.centerPoints[root.requestedKeyFrame].x
                    var yDiff = (mouseY - frame.y) / root.scaley - root.centerPoints[root.requestedKeyFrame].y
                    root.centerPoints[root.requestedKeyFrame].x += xDiff
                    root.centerPoints[root.requestedKeyFrame].y += yDiff
                    root.centerPointsTypes[root.requestedKeyFrame * 2].x += xDiff
                    root.centerPointsTypes[root.requestedKeyFrame * 2].y += yDiff
                    root.centerPointsTypes[root.requestedKeyFrame * 2 + 1].x += xDiff
                    root.centerPointsTypes[root.requestedKeyFrame * 2 + 1].y += yDiff
                    canvas.requestPaint()
                    root.effectPolygonChanged()
                } else if (root.requestedSubKeyFrame >= 0) {
                    root.centerPointsTypes[root.requestedSubKeyFrame].x = (mouseX - frame.x) / root.scalex
                    root.centerPointsTypes[root.requestedSubKeyFrame].y = (mouseY - frame.y) / root.scaley
                    canvas.requestPaint()
                    root.effectPolygonChanged()
                }
            } else if (root.centerPoints.length > 0) {
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
              for(var i = 0; i < root.centerPointsTypes.length; i++)
              {
                var p1 = canvas.convertPoint(root.centerPointsTypes[i])
                if (Math.abs(p1.x - mouseX) <= canvas.handleSize/2 && Math.abs(p1.y - mouseY) <= canvas.handleSize/2) {
                    if (i == root.requestedSubKeyFrame) {
                        containsMouse = true;
                        return;
                    }
                    root.requestedSubKeyFrame = i
                    canvas.requestPaint()
                    containsMouse = true;
                    return;
                } 
              }
              if (root.requestedKeyFrame == -1 && root.requestedSubKeyFrame == -1) {
                  return;
              }
              root.requestedKeyFrame = -1
              root.requestedSubKeyFrame = -1
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
