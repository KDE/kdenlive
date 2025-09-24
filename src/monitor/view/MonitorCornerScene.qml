/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

import org.kde.kdenlive as K
import "SnappingLogic.js" as SnappingLogic

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
    property bool cursorOutsideEffect: controller.cursorOutsideEffect
    property int requestedKeyFrame
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property var centerPoints: []
    signal effectPolygonChanged()
    Component.onCompleted: {
        // adjust monitor image size if audio thumb is displayed
        controller.rulerHeight = root.zoomOffset
    }

    function updatePoints(types, points) {
        if (global.pressed) {
            return
        }
        root.centerPoints = points
        canvas.requestPaint()
    }

    function getSnappedPos(position) {
        if (!K.KdenliveSettings.showMonitorGrid) {
            return position
        }
        return SnappingLogic.getSnappedPoint(position, K.KdenliveSettings.monitorGridH, K.KdenliveSettings.monitorGridV)
    }

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
    onCursorOutsideEffectChanged: {
        canvas.requestPaint()
    }

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }

    Canvas {
      id: canvas
      property double handleSize: root.baseUnit * 0.5
      property double darOffset : 0
      property color fillColor: Qt.rgba(1, 1, 1, 0.5)
      property color selectedColor: activePalette.highlight
      width: root.width
      height: root.height
      anchors.centerIn: root
      contextType: "2d";
      renderTarget: Canvas.FramebufferObject
      renderStrategy: Canvas.Cooperative
      onPaint:
      {
        var ctx = getContext('2d')
        //if (context) {
            ctx.clearRect(0,0, width, height);
            ctx.beginPath()
            ctx.strokeStyle = Qt.rgba(1, 0, 0, 0.5)
            ctx.fillStyle = canvas.fillColor
            ctx.lineWidth = 2
            var p1 = convertPoint(root.centerPoints[0])
            var p2 = convertPoint(root.centerPoints[1])
            var p3 = convertPoint(root.centerPoints[2])
            var p4 = convertPoint(root.centerPoints[3])
            //console.log('paint' + p1);

          // Handles
          if (controller.isKeyframe && !root.cursorOutsideEffect) {
            if (root.requestedKeyFrame == 0) {
                ctx.fillStyle = canvas.selectedColor
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = canvas.fillColor
            } else {
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            }
            ctx.strokeRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
            if (root.requestedKeyFrame == 1) {
                ctx.fillStyle = canvas.selectedColor
                ctx.fillRect(p2.x - handleSize, p2.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = canvas.fillColor
            } else {
                ctx.fillRect(p2.x - handleSize, p2.y - handleSize, 2 * handleSize, 2 * handleSize);
            }
            ctx.strokeRect(p2.x - handleSize, p2.y - handleSize, 2 * handleSize, 2 * handleSize);
            if (root.requestedKeyFrame == 2) {
                ctx.fillStyle = canvas.selectedColor
                ctx.fillRect(p3.x - handleSize, p3.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = canvas.fillColor
            } else {
                ctx.fillRect(p3.x - handleSize, p3.y - handleSize, 2 * handleSize, 2 * handleSize);
            }
            ctx.strokeRect(p3.x - handleSize, p3.y - handleSize, 2 * handleSize, 2 * handleSize);
            if (root.requestedKeyFrame == 3) {
                ctx.fillStyle = canvas.selectedColor
                ctx.fillRect(p4.x - handleSize, p4.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = canvas.fillColor
            } else {
                ctx.fillRect(p4.x - handleSize, p4.y - handleSize, 2 * handleSize, 2 * handleSize);
            }
            ctx.strokeRect(p4.x - handleSize, p4.y - handleSize, 2 * handleSize, 2 * handleSize);
          }
          // Rect
          if (root.cursorOutsideEffect) {
            ctx.setLineDash([4]);
          } else {
            ctx.setLineDash([]);
          }
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

        Repeater {
            model: K.KdenliveSettings.showMonitorGrid ? Math.floor(root.profile.x / K.KdenliveSettings.monitorGridH) : 0
            Rectangle {
                required property int index
                opacity: 0.3
                color: K.KdenliveSettings.overlayColor
                height: frame.height - 1
                width: 1
                x: ((index + 1) * K.KdenliveSettings.monitorGridH * root.scalex)
            }
        }
        Repeater {
            model: K.KdenliveSettings.showMonitorGrid ? Math.floor(root.profile.y / K.KdenliveSettings.monitorGridV) : 0
            Rectangle {
                required property int index
                opacity: 0.3
                color: K.KdenliveSettings.overlayColor
                height: 1
                width: frame.width - 1
                y: ((index + 1) * K.KdenliveSettings.monitorGridV * root.scaley)
            }
        }

        K.MonitorOverlay {
            anchors.fill: frame
            color: K.KdenliveSettings.overlayColor
            overlayType: root.overlayType
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
            if (controller.isKeyframe == false) return;
            if (pressed && root.requestedKeyFrame >= 0) {
                var mousePos = Qt.point(mouseX - frame.x, mouseY - frame.y)
                var logicalMousePos = Qt.point(mousePos.x / root.scalex, mousePos.y / root.scaley)
                var adjustedMouse = getSnappedPos(logicalMousePos)
                root.centerPoints[root.requestedKeyFrame].x = adjustedMouse.x;
                root.centerPoints[root.requestedKeyFrame].y = adjustedMouse.y;
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
