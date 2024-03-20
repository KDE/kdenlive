/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

Item {
    id: root
    objectName: "rootcornerscene"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize
    property point profile: controller.profile
    property int overlayType: controller.overlayType
    property color overlayColor: controller.overlayColor
    property point center
    property double scalex
    property double scaley
    property bool captureRightClick: false
    // Zoombar properties
    property double zoomStart: 0
    property double zoomFactor: 1
    property int zoomOffset: 0
    property bool showZoomBar: false
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
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property var centerPoints: []
    onCenterPointsChanged: canvas.requestPaint()
    signal effectPolygonChanged()

    function updateClickCapture() {
        root.captureRightClick = false
    }

    function refreshdar() {
        canvas.darOffset = root.sourcedar < root.profile.x * root.stretch / root.profile.y ? (root.profile.x * root.stretch - root.profile.y * root.sourcedar) / (2 * root.profile.x * root.stretch) :(root.profile.y - root.profile.x * root.stretch / root.sourcedar) / (2 * root.profile.y);
        canvas.requestPaint()
    }

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }

    Canvas {
      id: canvas
      property double handleSize
      property double darOffset : 0
      width: root.width
      height: root.height
      anchors.centerIn: root
      contextType: "2d";
      handleSize: root.baseUnit / 2
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
        Loader {
            anchors.fill: parent
            source: {
                switch(root.overlayType)
                {
                    case 0:
                        return '';
                    case 1:
                        return "OverlayStandard.qml";
                    case 2:
                        return "OverlayMinimal.qml";
                    case 3:
                        return "OverlayCenter.qml";
                    case 4:
                        return "OverlayCenterDiagonal.qml";
                    case 5:
                        return "OverlayThirds.qml";
                }
            }
        }
    }
    MouseArea {
        id: global
        objectName: "global"
        property bool kfrContainsMouse: false
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: kfrContainsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor
        onWheel: wheel => {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onDoubleClicked: {
            controller.addRemoveKeyframe()
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
                        kfrContainsMouse = true;
                        return;
                    }
                    root.requestedKeyFrame = i
                    canvas.requestPaint()
                    kfrContainsMouse = true;
                    return;
                }
              }
              if (root.requestedKeyFrame == -1) {
                  return;
              }
              root.requestedKeyFrame = -1
              kfrContainsMouse = false;
              canvas.requestPaint()
            }
        }
    }
    EffectToolBar {
        id: effectToolBar
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            rightMargin: 4
            leftMargin: 4
        }
    }
    MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        height: controller.rulerHeight
    }
}
