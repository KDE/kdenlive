/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Shapes 1.15

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "rooteffectscene"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize
    property rect adjustedFrame
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
    property double offsetx : 0
    property double offsety : 0
    property double lockratio : -1
    property double timeScale: 1
    property double frameSize: 10
    property int duration: 300
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int mouseRulerPos: 0
    property bool rotatable: false
    property double rotation: 0
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    onRotationChanged: canvas.requestPaint()
    property bool iskeyframe
    property bool cursorOutsideEffect: true
    property bool disableHandles: root.cursorOutsideEffect && root.centerPoints.length > 1
    property bool showHandles: (root.iskeyframe || K.KdenliveSettings.autoKeyframe) && !disableHandles
    property int requestedKeyFrame: 0
    property var centerPoints: []
    property var centerPointsTypes: []
    signal effectChanged(rect frame)
    signal centersChanged()
    signal effectRotationChanged(double rotation)

    Component.onCompleted: {
      // adjust monitor image size if audio thumb is displayed
        controller.rulerHeight = root.zoomOffset
    }

    function updateEffectRect(rect) {
      if (moveArea.pressed) {
          // We are moving a rect, don't update
          return
      }
      root.framesize = rect
    }

    function updatePoints(types, points) {
      if (global.pressed) {
        return
      }
      root.centerPointsTypes = types
      root.centerPoints = points
      canvas.requestPaint()
    }

    function getSnappedPos(position) {
      if (!K.KdenliveSettings.showMonitorGrid) {
        return position
      }
      var deltax = Math.round(position.x / root.scalex)
      var deltay = Math.round(position.y / root.scaley)
      deltax = Math.round(deltax / K.KdenliveSettings.monitorGridH) * K.KdenliveSettings.monitorGridH
      deltay = Math.round(deltay / K.KdenliveSettings.monitorGridV) * K.KdenliveSettings.monitorGridV
      return Qt.point(deltax * root.scalex, deltay * root.scaley)
    }

    function updateClickCapture() {
        root.captureRightClick = false
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
      width: root.width
      height: root.height
      anchors.centerIn: root
      contextType: "2d";
      handleSize: root.baseUnit / 2
      renderStrategy: Canvas.Threaded;
      onPaint:
      {
        if (context && root.centerPoints.length > 0) {
            context.beginPath()
            context.strokeStyle = Qt.rgba(1, 0, 0, 0.5)
            context.fillStyle = Qt.rgba(1, 0, 0, 1)
            context.lineWidth = 1
            context.setLineDash([3,3])
            var p1 = convertPoint(root.centerPoints[0])
            context.moveTo(p1.x, p1.y)
            context.clearRect(0,0, width, height);
            if (!root.disableHandles) {
              for(var i = 0; i < root.centerPoints.length; i++)
              {
                context.translate(p1.x, p1.y)
                context.rotate(Math.PI/4);
                context.fillRect(- handleSize, 0, 2 * handleSize, 1);
                context.fillRect(0, - handleSize, 1, 2 * handleSize);
                context.rotate(-Math.PI/4);
                context.translate(-p1.x, -p1.y)
                if (i + 1 < root.centerPoints.length)
                {
                    var end = convertPoint(root.centerPoints[i + 1])

                    if (root.centerPointsTypes.length != root.centerPoints.length || root.centerPointsTypes[i] == 0) {
                        context.lineTo(end.x, end.y)
                        p1 = end
                        continue
                    }

                    var j = i - 1
                    if (j < 0) {
                        j = 0
                    }
                    var pre = convertPoint(root.centerPoints[j])
                    j = i + 2
                    if (j >= root.centerPoints.length) {
                        j = root.centerPoints.length - 1
                    }
                    var post = convertPoint(root.centerPoints[j])
                    var c1 = substractPoints(end, pre, 6.0)
                    var c2 = substractPoints(p1, post, 6.0)
                    c1 = addPoints(c1, p1)
                    c2 = addPoints(c2, end)
                    context.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, end.x, end.y);
                } else {
                    context.lineTo(p1.x, p1.y)
                }
                p1 = end
            }
            context.stroke()
            context.restore()
          }
        }
    }

    function convertPoint(p)
    {
        var x = frame.x + p.x * root.scalex
        var y = frame.y + p.y * root.scaley
        return Qt.point(x,y);
    }
    function addPoints(p1, p2)
    {
        var x = p1.x + p2.x
        var y = p1.y + p2.y
        return Qt.point(x,y);
    }
    function distance(p1, p2)
    {
        var x = p1.x + p2.x
        var y = p1.y + p2.y
        return Math.sqrt(Math.pow(p2.x - p1.x, 2) + Math.pow(p2.y - p1.y, 2));
    }
    function substractPoints(p1, p2, f)
    {
        var x = (p1.x - p2.x) / f
        var y = (p1.y - p2.y) / f
        return Qt.point(x,y);
    }
  }
    Rectangle {
        id: frame
        objectName: "referenceframe"
        width: root.profile.x * root.scalex
        height: root.profile.y * root.scaley
        x: root.center.x - width / 2 - root.offsetx;
        y: root.center.y - height / 2 - root.offsety;
        color: "transparent"
        border.color: "#ffffff00"

        K.MonitorOverlay {
            anchors.fill: frame
            color: K.KdenliveSettings.overlayColor
            overlayType: root.overlayType
        }

        Repeater {
          model: K.KdenliveSettings.showMonitorGrid ? Math.floor(root.profile.x / K.KdenliveSettings.monitorGridH) : 0
          Rectangle {
            required property int index
            opacity: 0.3
            color: root.overlayColor
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
            color: root.overlayColor
            height: 1
            width: frame.width - 1
            y: ((index + 1) * K.KdenliveSettings.monitorGridV * root.scaley)
          }
        }
    }
    MouseArea {
        id: global
        property bool isMoving : false
        anchors.fill: parent
        anchors.bottomMargin: clipMonitorRuler.height
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: handleContainsMouse ? Qt.PointingHandCursor : (moveArea.containsMouse && !root.cursorOutsideEffect) ? Qt.SizeAllCursor : Qt.ArrowCursor
        readonly property bool handleContainsMouse: {
              if (isMoving) {
                  return true;
              }
              if (root.disableHandles) {
                root.requestedKeyFrame = -1
                return false
              }
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

        onPositionChanged: mouse => {
            if (!pressed) {
                mouse.accepted = false
                return
            }
            if (root.requestedKeyFrame != -1) {
                  isMoving = true
                  if (!K.KdenliveSettings.showMonitorGrid) {
                    root.centerPoints[root.requestedKeyFrame].x = (mouseX - frame.x) / root.scalex;
                    root.centerPoints[root.requestedKeyFrame].y = (mouseY - frame.y) / root.scaley;
                  } else {
                    var positionInFrame = mapToItem(frame, mouse.x, mouse.y)
                    var adjustedMouse = getSnappedPos(positionInFrame)
                    root.centerPoints[root.requestedKeyFrame].x = adjustedMouse.x / root.scalex;
                    root.centerPoints[root.requestedKeyFrame].y = adjustedMouse.y / root.scaley;
                  }
                  canvas.requestPaint()
                  root.centersChanged()
            }
        }

        onPressed: mouse => {
            root.captureRightClick = true
            if (mouse.button & Qt.LeftButton) {
                if (mouse.modifiers & Qt.AltModifier) {
                    controller.switchFocusClip()
                } else if (root.requestedKeyFrame >= 0 && !isMoving) {
                    controller.seekToKeyframe(root.requestedKeyFrame, 0);
                }
            }
            isMoving = false
        }
        onDoubleClicked: {
            controller.addRemoveKeyframe()
        }
        onReleased: {
            root.captureRightClick = false
            root.requestedKeyFrame = -1
            isMoving = false;
        }
        onEntered: {
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis","<shortcut>Double click</shortcut> to add a keyframe, <shortcut>Shift drag</shortcut> for proportional rescale, <shortcut>Ctrl drag</shortcut> for center-based rescale, <shortcut>Hover right</shortcut> for toolbar, <shortcut>Click</shortcut> on a center to seek to its keyframe"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
        }
        onWheel: wheel => {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
    }

    Rectangle {
        id: framerect
        property bool dragging: false
        property bool isRotating: false
        property double handlesBottomMargin: root.height - clipMonitorRuler.height - framerect.y - framerect.height < root.baseUnit/2 ? 0 : -root.baseUnit/2
        property double handlesRightMargin: root.width - framerect.x - framerect.width < root.baseUnit/2 ? 0 : -root.baseUnit/2
        property double handlesTopMargin: framerect.y < root.baseUnit/2 ? 0 : -root.baseUnit/2
        property double handlesLeftMargin: framerect.x < root.baseUnit/2 ? 0 : -root.baseUnit/2

        function transformPoint(x, y, source) {
            // Convert to framerect's coordinate system
            var point = source.mapToItem(framerect, x, y)
            // Apply inverse rotation
            var angle = -root.rotation * Math.PI / 180
            var cos = Math.cos(angle)
            var sin = Math.sin(angle)
            var centerX = framerect.width / 2
            var centerY = framerect.height / 2
            var dx = point.x - centerX
            var dy = point.y - centerY
            var rotatedX = centerX + dx * cos - dy * sin
            var rotatedY = centerY + dx * sin + dy * cos
            // Map to frame coordinates
            return framerect.mapToItem(frame, rotatedX, rotatedY)
        }

        x: frame.x + root.framesize.x * root.scalex
        y: frame.y + root.framesize.y * root.scaley
        width: root.framesize.width * root.scalex
        height: root.framesize.height * root.scaley
        enabled: root.iskeyframe || K.KdenliveSettings.autoKeyframe
        color: "transparent"
        border.color: root.disableHandles ? 'transparent' : "#ff0000"
        transform: Rotation {
            origin.x: framerect.width/2
            origin.y: framerect.height/2
            angle: root.rotation
        }
        Shape {
            id: shape
            anchors.fill: parent
            visible: root.disableHandles
            ShapePath {
                strokeColor: 'red'
                strokeWidth: 1
                fillColor: 'transparent'
                strokeStyle: ShapePath.DashLine
                dashPattern: [4, 4]
                PathLine {
                    x: 0
                    y: 0
                }
                PathLine {
                    x: shape.width
                    y: 0
                }
                PathLine {
                    x: shape.width
                    y: shape.height
                }
                PathLine {
                    x: 0
                    y: shape.height
                }
                PathLine {
                    x: 0
                    y: 0
                }
            }
        }
        MouseArea {
          id: moveArea
          anchors.fill: parent
          cursorShape: handleContainsMouse ? Qt.PointingHandCursor : enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
          propagateComposedEvents: true
          property var mouseClickPos
          property var frameClicksize: Qt.point(0, 0)
          enabled: root.showHandles
          hoverEnabled: true
          readonly property bool handleContainsMouse: {
              if (pressed) {
                  return false;
              }
              if (root.centerPoints.length <= 1) {
                root.requestedKeyFrame = -1
                return false
              }
              for(var i = 0; i < root.centerPoints.length; i++)
              {
                var p1 = canvas.convertPoint(root.centerPoints[i])
                if (Math.abs(p1.x - (mouseX + framerect.x)) <= canvas.handleSize && Math.abs(p1.y - (mouseY + framerect.y)) <= canvas.handleSize) {
                    root.requestedKeyFrame = i
                    return true
                }
              }
              root.requestedKeyFrame = -1
              return false
          }
          onPressed: mouse => {
            if (mouse.button & Qt.LeftButton) {
                if (mouse.modifiers & Qt.AltModifier) {
                    mouse.accepted = true
                    root.captureRightClick = true
                    controller.switchFocusClip()
                    return;
                } else if (handleContainsMouse) {
                    // Moving to another keyframe
                    mouse.accepted = false
                    return
                } else {
                    // Ok, get ready for the move
                    mouseClickPos = mapToItem(frame, mouse.x, mouse.y)
                    frameClicksize.x = framesize.x * root.scalex
                    frameClicksize.y = framesize.y * root.scaley
                }
                mouse.accepted = true
                root.captureRightClick = true
            }
          }
          onReleased: mouse => {
            root.captureRightClick = false
            root.requestedKeyFrame = -1
            mouse.accepted = true
          }
          onPositionChanged: mouse => {
            if (pressed) {
                var delta = mapToItem(frame, mouse.x, mouse.y)
                delta.x += - mouseClickPos.x + frameClicksize.x
                delta.y += - mouseClickPos.y + frameClicksize.y
                var adjustedMouse = getSnappedPos(delta)
                framesize.x = adjustedMouse.x / root.scalex;
                framesize.y = adjustedMouse.y / root.scaley;
                if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                  controller.addRemoveKeyframe();
                }
                root.effectChanged(root.framesize)
                mouse.accepted = true
            }
          }
        }
        Rectangle {
            id: rotationHandle
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                topMargin: parent.height / 4
            }
            width: root.baseUnit
            height: width
            radius: width / 2
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.rotatable && root.showHandles
            opacity: framerect.dragging ? 0 : root.iskeyframe ? 1 : 0.4
            
            K.OverlayLabel {
                id: rotationLabel
                visible: framerect.isRotating
                anchors {
                    bottom: rotationHandle.top
                    horizontalCenter: rotationHandle.horizontalCenter
                    bottomMargin: rotationHandle.height + 10
                }
                text: Math.round(root.rotation) + "°"
            }
            
            MouseArea {
                property point center
                property int oldMouseX
                property int oldMouseY
                id: rotateMouseArea
                acceptedButtons: Qt.LeftButton
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                
                onPressed: mouse => {
                    root.captureRightClick = true
                    framerect.dragging = false
                    framerect.isRotating = false
                    oldMouseX = mouseX
                    oldMouseY = mouseY
                    center = Qt.point(framerect.x + framerect.width/2, framerect.y + framerect.height/2)
                }
                
                onPositionChanged: mouse => {
                    if (pressed) {
                        if (!framerect.isRotating && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                            return
                        }
                        framerect.isRotating = true

                        var mousePos = mapToItem(null, mouse.x, mouse.y)
                        var angle = Math.atan2(mousePos.y - center.y, mousePos.x - center.x) * (180/Math.PI)
                        angle = (angle + 90) % 360

                        root.rotation = angle
                        
                        if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                            controller.addRemoveKeyframe()
                        }
                        
                        root.effectRotationChanged(root.rotation)
                    }
                }
                
                onReleased: {
                    root.captureRightClick = false
                    framerect.dragging = false
                    framerect.isRotating = false
                }
                
                onDoubleClicked: {
                    // Reset rotation to 0 on double-click
                    root.rotation = 0
                    root.effectRotationChanged(0)
                    if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                        controller.addRemoveKeyframe()
                    }
                }
            }
        }
        Rectangle {
            id: tlhandle
            anchors {
              top: parent.top
              left: parent.left
              topMargin: framerect.handlesTopMargin
              leftMargin: framerect.handlesLeftMargin
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              acceptedButtons: Qt.LeftButton
              anchors.fill: parent
              cursorShape: Qt.SizeFDiagCursor

              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.x = Math.min(adjustedMouse.x, (framesize.x + framesize.width) * root.scalex - 1)
                  adjustedMouse.y = Math.min(adjustedMouse.y, (framesize.y + framesize.height) * root.scaley - 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = Math.max(adjustedMouse.x - framesize.x * root.scalex, adjustedMouse.y - framesize.y * root.scaley)
                      if (delta == 0) {
                        return
                      }
                      var newwidth = framerect.width - delta
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ? root.lockratio : handleRatio))
                      adjustedFrame.y = (framerect.y - frame.y) / root.scaley + framesize.height - adjustedFrame.height;
                      adjustedFrame.x = (framerect.x - frame.x) / root.scalex + framesize.width - adjustedFrame.width
                  } else {
                    adjustedFrame.x = adjustedMouse.x / root.scalex
                    adjustedFrame.y = adjustedMouse.y / root.scaley;
                    if (adjustedFrame.x != framesize.x) {
                      adjustedFrame.width = framesize.x + framesize.width - (adjustedMouse.x / root.scalex);
                    }
                    if (adjustedFrame.y != framesize.y) {
                      adjustedFrame.height = framesize.y + framesize.height - (adjustedMouse.y / root.scaley);
                    }
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      adjustedFrame.width -= (framesize.width - adjustedFrame.width)
                      adjustedFrame.height -= (framesize.height - adjustedFrame.height)
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
            K.OverlayLabel {
                id: effectposLabel
                visible: moveArea.pressed
                anchors {
                    top: parent.bottom
                    left: parent.right
                }
                text: framesize.x.toFixed(0) + "x" + framesize.y.toFixed(0)
            }
        }
        Rectangle {
            id: tophandle
            anchors {
              top: parent.top
              topMargin: framerect.handlesTopMargin
              horizontalCenter: parent.horizontalCenter
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              acceptedButtons: Qt.LeftButton
              anchors.fill: parent
              cursorShape: Qt.SizeVerCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.y = Math.min(adjustedMouse.y, (framesize.y + framesize.height) * root.scaley - 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = adjustedMouse.y - framesize.y * root.scaley
                      if (delta == 0) {
                        return
                      }
                      var newheight = framerect.height - delta
                      adjustedFrame.height = Math.round(newheight / root.scalex);
                      adjustedFrame.width = Math.round(adjustedFrame.height * (root.lockratio > 0 ? root.lockratio : handleRatio))
                      adjustedFrame.y = (framerect.y - frame.y) / root.scaley + framesize.height - adjustedFrame.height;
                      adjustedFrame.x = (framerect.x - frame.x) / root.scalex + framesize.width - adjustedFrame.width
                  } else {
                    adjustedFrame.y = adjustedMouse.y / root.scaley;
                    if (adjustedFrame.y != framesize.y) {
                      adjustedFrame.height = framesize.y + framesize.height - (adjustedMouse.y / root.scaley);
                    }
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      adjustedFrame.width -= (framesize.width - adjustedFrame.width)
                      adjustedFrame.height -= (framesize.height - adjustedFrame.height)
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: bottomhandle
            anchors {
              bottom: parent.bottom
              bottomMargin: framerect.handlesBottomMargin
              horizontalCenter: parent.horizontalCenter
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              acceptedButtons: Qt.LeftButton
              anchors.fill: parent
              cursorShape: Qt.SizeVerCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.y = Math.max(adjustedMouse.y, framesize.y * root.scaley + 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = adjustedMouse.y - (framesize.y + framesize.height) * root.scaley
                      if (delta == 0) {
                        return
                      }
                      var newheight = framerect.height + delta
                      adjustedFrame.height = Math.round(newheight / root.scalex);
                      adjustedFrame.width = Math.round(adjustedFrame.height * (root.lockratio > 0 ? root.lockratio : handleRatio))
                  } else {
                      adjustedFrame.height = adjustedMouse.y / root.scaley - adjustedFrame.y;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      var xOffset = framesize.width - adjustedFrame.width
                      adjustedFrame.x += xOffset
                      adjustedFrame.width -= xOffset
                      var yOffset = framesize.height - adjustedFrame.height
                      adjustedFrame.y += yOffset
                      adjustedFrame.height -= yOffset
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: lefthandle
            anchors {
              verticalCenter: parent.verticalCenter
              left: parent.left
              leftMargin: framerect.handlesLeftMargin
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              acceptedButtons: Qt.LeftButton
              anchors.fill: parent
              cursorShape: Qt.SizeHorCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.x = Math.min(adjustedMouse.x, (framesize.x + framesize.width) * root.scalex - 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = adjustedMouse.x - framesize.x * root.scaley
                      if (delta == 0) {
                        return
                      }
                      var newwidth = framerect.width - delta
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ? root.lockratio : handleRatio))
                      adjustedFrame.x = (framerect.x - frame.x) / root.scalex + framesize.width - adjustedFrame.width
                  } else {
                      adjustedFrame.x = adjustedMouse.x / root.scalex
                      if (adjustedFrame.x != framesize.x) {
                        adjustedFrame.width = framesize.x + framesize.width - (adjustedMouse.x / root.scalex);
                      }
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      adjustedFrame.width -= (framesize.width - adjustedFrame.width)
                      var yOffset = framesize.height - adjustedFrame.height
                      adjustedFrame.y += yOffset
                      adjustedFrame.height -= yOffset
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: righthandle
            anchors {
              verticalCenter: parent.verticalCenter
              right: parent.right
              rightMargin: framerect.handlesRightMargin

            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              acceptedButtons: Qt.LeftButton
              anchors.fill: parent
              cursorShape: Qt.SizeHorCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.x = Math.max(adjustedMouse.x, framesize.x * root.scalex + 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = adjustedMouse.x - (framesize.x + framesize.width) * root.scaley
                      if (delta == 0) {
                        return
                      }

                      var newwidth = framerect.width + delta
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ? root.lockratio : handleRatio))
                  } else {
                      adjustedFrame.width = (adjustedMouse.x / root.scalex) - framesize.x;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      var xOffset = framesize.width - adjustedFrame.width
                      adjustedFrame.x += xOffset
                      adjustedFrame.width -= xOffset
                      var yOffset = framesize.height - adjustedFrame.height
                      adjustedFrame.y += yOffset
                      adjustedFrame.height -= yOffset

                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: trhandle
            anchors {
              top: parent.top
              right: parent.right
              topMargin: framerect.handlesTopMargin
              rightMargin: framerect.handlesRightMargin
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              anchors.fill: parent
              cursorShape: Qt.SizeBDiagCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.x = Math.max(adjustedMouse.x, framesize.x * root.scalex + 1)
                  adjustedMouse.y = Math.min(adjustedMouse.y, (framesize.y + framesize.height) * root.scaley - 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var newwidth = adjustedMouse.x - framesize.x * root.scalex
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                      adjustedFrame.y = (framerect.y - frame.y) / root.scaley + framesize.height - adjustedFrame.height
                  } else {
                      adjustedFrame.y = adjustedMouse.y / root.scaley;
                      adjustedFrame.width = (adjustedMouse.x / root.scalex) - framesize.x;
                      if (adjustedFrame.y != framesize.y) {
                        adjustedFrame.height = framesize.y + framesize.height - (adjustedMouse.y / root.scaley);
                      }
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      var xOffset = framesize.width - adjustedFrame.width
                      adjustedFrame.x += xOffset
                      adjustedFrame.width -= xOffset
                      adjustedFrame.height -= (framesize.height - adjustedFrame.height)
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: blhandle
            anchors {
              bottom: parent.bottom
              left: parent.left
              bottomMargin: framerect.handlesBottomMargin
              leftMargin: framerect.handlesLeftMargin
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              anchors.fill: parent
              cursorShape: Qt.SizeBDiagCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                  adjustedFrame = framesize
                  var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                  var adjustedMouse = getSnappedPos(positionInFrame)
                  adjustedMouse.x = Math.min(adjustedMouse.x, (framesize.x + framesize.width) * root.scalex - 1)
                  adjustedMouse.y = Math.max(adjustedMouse.y, framesize.y * root.scaley + 1)
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var newwidth = (framesize.x + framesize.width) * root.scalex - adjustedMouse.x
                      adjustedFrame.x = (framerect.x + (framerect.width - newwidth) - frame.x) / root.scalex;
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                  } else {
                    adjustedFrame.x = adjustedMouse.x / root.scalex
                    if (adjustedFrame.x != framesize.x) {
                      adjustedFrame.width = framesize.x + framesize.width - (adjustedMouse.x / root.scalex);
                    }
                    adjustedFrame.height = (adjustedMouse.y / root.scaley) - framesize.y;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      adjustedFrame.width -= (framesize.width - adjustedFrame.width)
                      var yOffset = framesize.height - adjustedFrame.height
                      adjustedFrame.y += yOffset
                      adjustedFrame.height -= yOffset
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
            }
        }
        K.OverlayLabel {
            id: effectsizeLabel
            visible: framerect.dragging
            anchors {
                bottom: framerect.bottom
                right: framerect.right
                bottomMargin: 2 * root.baseUnit * framesize.height / framesize.width
                rightMargin: 2 * root.baseUnit
            }
            text: framesize.width.toFixed(0) + "x" + framesize.height.toFixed(0)
        }
        Rectangle {
            id: brhandle
            anchors {
              bottom: parent.bottom
              right: parent.right
              bottomMargin: framerect.handlesBottomMargin
              rightMargin: framerect.handlesRightMargin
            }
            width: root.baseUnit
            height: width
            color: "#99ffffff"
            border.color: "#ff0000"
            visible: root.showHandles
            opacity: framerect.dragging || framerect.isRotating ? 0 : root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              anchors.fill: parent
              cursorShape: Qt.SizeFDiagCursor
              onPressed: mouse => {
                  root.captureRightClick = true
                  framerect.dragging = false
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  if (!framerect.dragging && Math.abs(mouseX - oldMouseX) + Math.abs(mouseY - oldMouseY) < Qt.styleHints.startDragDistance) {
                    return
                  }
                  framerect.dragging = true
                   adjustedFrame = framesize
                   var positionInFrame = framerect.transformPoint(mouse.x, mouse.y, this)
                   var adjustedMouse = getSnappedPos(positionInFrame)
                   adjustedMouse.x = Math.max(adjustedMouse.x, framesize.x * root.scalex + 1)
                  adjustedMouse.y = Math.max(adjustedMouse.y, framesize.y * root.scaley + 1)
                   if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var newwidth = adjustedMouse.x - framesize.x * root.scalex
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                  } else {
                    adjustedFrame.width = (adjustedMouse.x / root.scalex) - framesize.x;
                    adjustedFrame.height = (adjustedMouse.y / root.scaley) - framesize.y;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      var xOffset = framesize.width - adjustedFrame.width
                      adjustedFrame.x += xOffset
                      adjustedFrame.width -= xOffset
                      var yOffset = framesize.height - adjustedFrame.height
                      adjustedFrame.y += yOffset
                      adjustedFrame.height -= yOffset
                  }
                  framesize = adjustedFrame
                  if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    controller.addRemoveKeyframe();
                  }
                  root.effectChanged(root.framesize)
                }
              }
              onReleased: {
                  root.captureRightClick = false
                  framerect.dragging = false
                  handleRatio = 1
              }
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
