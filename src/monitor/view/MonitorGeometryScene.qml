/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Shapes 1.15

import org.kde.kdenlive as K
import "ResizeLogic.js" as ResizeLogic

Item {
    id: root
    objectName: "rooteffectscene"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize: _framesize
    // private property used to prevent circular updates when frame is transformed via Monitor, the change is signaled to cpp, 
    // it updates its model and calls us back to update the Monitor
    property rect _framesize
    property rect pendingFramesize // : Qt.rect(0, 0, 0, 0)
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
    property double rotation: _rotation
    // private property used to prevent circular updates when frame is transformed via Monitor, the change is signaled to cpp, 
    // it updates its model and calls us back to update the Monitor
    property double _rotation: 0
    property double pendingRotation: 0
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    onRotationChanged: {
        if (!transformedFrame.isRotating) {
            _rotation = rotation
            canvas.requestPaint()
        }
    }
    onFramesizeChanged: {
        if (!(transformedFrame.isResizing || transformedFrame.isMoving)) _framesize = framesize
    }
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
    
    function shouldFlipText(rotationAngle) {
        // Flip text when rotation is between 90-270 degrees to prevent upside-down text
        // Direction/sign of the rotation does not matter for this purpose
        var normalizedRotation = Math.abs(rotationAngle)
        return normalizedRotation >= 90 && normalizedRotation <= 270
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

    // Red Rectangle shown while a resizing, moving or rotation operation is in progress to show the current position while transformedFrame is hidden.
    // The Resize/Move/Rotate handles (and their MouseAreas) are bound to the original transformedFrame so we can savefly update this pending Rectangle instead
    // while the user is dragging it to the desired position.
    Rectangle {
        id: pendingFrame
        visible: transformedFrame.isResizing || transformedFrame.isRotating || transformedFrame.isMoving
        width: root.pendingFramesize.width * root.scalex
        height: root.pendingFramesize.height * root.scaley
        color: "transparent"
        border.color: root.disableHandles ? 'transparent' : "#ff0000"
        x: frame.x + root.pendingFramesize.x * root.scalex
        y: frame.y + root.pendingFramesize.y * root.scaley
        transform: Rotation {
            origin.x: pendingFrame.width/2
            origin.y: pendingFrame.height/2
            angle: root.pendingRotation
        }
        antialiasing: true

        // Rotation label
        K.OverlayLabel {
            id: rotationLabel
            visible: transformedFrame.isRotating
            anchors {
                bottom: pendingFrame.bottom
                bottomMargin: 3./4. * pendingFrame.height + 10.
                horizontalCenter: pendingFrame.horizontalCenter
            }
            text: Math.round(root.pendingRotation) + "°"
            flipText: shouldFlipText(root.pendingRotation)
        }
        
        // Position label
        K.OverlayLabel {
            id: effectposLabel
            visible: transformedFrame.isMoving
            anchors {
                top: pendingFrame.top
                left: pendingFrame.left
                topMargin: 2 * Math.min(1.5 * root.baseUnit, root.baseUnit * pendingFramesize.height / pendingFramesize.width)
                leftMargin: 2 * root.baseUnit
            }
            text: pendingFramesize.x.toFixed(0) + " x " + pendingFramesize.y.toFixed(0)
            flipText: shouldFlipText(root.pendingRotation)
        }

        // Size label
        K.OverlayLabel {
            id: effectsizeLabel
            visible: transformedFrame.isResizing
            anchors {
                bottom: pendingFrame.bottom
                right: pendingFrame.right
                bottomMargin: 2 * Math.min(1.5 * root.baseUnit, root.baseUnit * pendingFramesize.height / pendingFramesize.width)
                rightMargin: 2 * root.baseUnit
            }
            text: pendingFramesize.width.toFixed(0) + " x " + pendingFramesize.height.toFixed(0)
            flipText: shouldFlipText(root.pendingRotation)
        }
    }

    // Red Rectangle representing the transformed frame after applying rotation, moving and resizing parameters.
    Rectangle {
        id: transformedFrame
        property bool isMoving: false
        property bool isResizing: false
        property bool isRotating: false
        property int smallRectMargin: transformedFrame.width < 2 * root.baseUnit || transformedFrame.height < 2 * root.baseUnit ? root.baseUnit : 0
        property double handlesBottomMargin: root.height - clipMonitorRuler.height - transformedFrame.y - transformedFrame.height < root.baseUnit/2 ? 0 : -root.baseUnit/2 - smallRectMargin
        property double handlesRightMargin: root.width - transformedFrame.x - transformedFrame.width < root.baseUnit/2 ? 0 : -root.baseUnit/2 - smallRectMargin
        property double handlesTopMargin: transformedFrame.y < root.baseUnit/2 ? 0 : -root.baseUnit/2 - smallRectMargin
        property double handlesLeftMargin: transformedFrame.x < root.baseUnit/2 ? 0 : -root.baseUnit/2 - smallRectMargin

        x: frame.x + root._framesize.x * root.scalex
        y: frame.y + root._framesize.y * root.scaley
        width: root._framesize.width * root.scalex
        height: root._framesize.height * root.scaley
        enabled: root.iskeyframe || K.KdenliveSettings.autoKeyframe
        color: "transparent"
        border.color: root.disableHandles ? 'transparent' : "#ff0000"
        opacity: (isMoving || isResizing || isRotating) ? 0 : 1
        transform: Rotation {
            origin.x: transformedFrame.width/2
            origin.y: transformedFrame.height/2
            angle: root._rotation
        }
        antialiasing: true
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
                if (Math.abs(p1.x - (mouseX + transformedFrame.x)) <= canvas.handleSize && Math.abs(p1.y - (mouseY + transformedFrame.y)) <= canvas.handleSize) {
                    root.requestedKeyFrame = i
                    return true
                }
              }
              root.requestedKeyFrame = -1
              return false
          }
          onPressed: mouse => {
            if (mouse.button & Qt.LeftButton) {
                transformedFrame.isMoving = true
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
                    root.pendingFramesize = _framesize
                    root.pendingRotation = root._rotation
                    mouseClickPos = mapToItem(frame, mouse.x, mouse.y)
                    frameClicksize.x = _framesize.x * root.scalex
                    frameClicksize.y = _framesize.y * root.scaley
                }
                mouse.accepted = true
                root.captureRightClick = true
            }
          }
          onReleased: mouse => {
            root.captureRightClick = false
            root.requestedKeyFrame = -1
            mouse.accepted = true

            root._framesize = root.pendingFramesize
            root.framesize = root._framesize
            transformedFrame.isMoving = false
          }
          onPositionChanged: mouse => {
            if (pressed) {
                var delta = mapToItem(frame, mouse.x, mouse.y)
                delta.x += - mouseClickPos.x + frameClicksize.x
                delta.y += - mouseClickPos.y + frameClicksize.y
                var adjustedMouse = getSnappedPos(delta)
                root.pendingFramesize.x = adjustedMouse.x / root.scalex;
                root.pendingFramesize.y = adjustedMouse.y / root.scaley;
                if (root.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                  controller.addRemoveKeyframe();
                }
                root.effectChanged(root.pendingFramesize)
                mouse.accepted = true
            }
          }
        }

        RotationHandle {
            id: rotationHandle
            baseUnit: root.baseUnit
            rotatable: root.rotatable
            showHandle: root.showHandles
            smallRectMargin: transformedFrame.smallRectMargin
            iskeyframe: root.iskeyframe
            rotationAngle: root._rotation
            
            onRotationStart: {
                root.pendingFramesize = root._framesize
                root.pendingRotation = root._rotation
                transformedFrame.isRotating = true
            }
            
            onRotationEnd: {
                root._rotation = root.pendingRotation
                root.rotation = root._rotation
                transformedFrame.isRotating = false
            }
            
            onRotationChanged: (angle) => {
                root.pendingRotation = angle
                root.effectRotationChanged(angle)
            }
            
            onCaptureRightClick: (capture) => {
                root.captureRightClick = capture
            }
            
            onAddRemoveKeyframe: {
                controller.addRemoveKeyframe()
            }
        }
        
        // Generate all 8 resize handles (4 corners, 4 edges)
        Repeater {
            model: [
                ResizeLogic.HandleTypes.TOP_LEFT,
                ResizeLogic.HandleTypes.TOP,
                ResizeLogic.HandleTypes.TOP_RIGHT,
                ResizeLogic.HandleTypes.LEFT,
                ResizeLogic.HandleTypes.RIGHT,
                ResizeLogic.HandleTypes.BOTTOM_LEFT,
                ResizeLogic.HandleTypes.BOTTOM,
                ResizeLogic.HandleTypes.BOTTOM_RIGHT
            ]
            
            ResizeHandle {
                // Configuration
                handleType: modelData
                baseUnit: root.baseUnit
                showHandle: root.showHandles
                otherResizeHandleInUse: transformedFrame.isResizing
                
                // Frame state
                parentFramesize: root._framesize
                scalex: root.scalex  
                scaley: root.scaley
                lockRatio: root.lockratio
                isKeyframe: root.iskeyframe
                
                // Margin properties
                handlesTopMargin: transformedFrame.handlesTopMargin
                handlesBottomMargin: transformedFrame.handlesBottomMargin
                handlesLeftMargin: transformedFrame.handlesLeftMargin
                handlesRightMargin: transformedFrame.handlesRightMargin
                
                // Rotation angle for handle mapping
                rotationAngle: root._rotation
                
                onResize: (adjustedFrame) => {
                    root.pendingFramesize = adjustedFrame
                    root.effectChanged(root.pendingFramesize)
                }
                
                onResizeStart: () => {
                    transformedFrame.isResizing = true
                    root.pendingFramesize = root._framesize
                    root.pendingRotation = root._rotation
                }
                
                onResizeEnd: () => {
                    root._framesize = root.pendingFramesize
                    root.framesize = root._framesize
                    transformedFrame.isResizing = false
                }
                
                onCaptureRightClick: (capture) => {
                    root.captureRightClick = capture
                }
                
                onAddRemoveKeyframe: () => {
                    controller.addRemoveKeyframe()
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
