/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

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
    property double offsetx : 0
    property double offsety : 0
    property double lockratio : -1
    property double timeScale: 1
    property double frameSize: 10
    property int duration: 300
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int mouseRulerPos: 0
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    property bool iskeyframe
    property int requestedKeyFrame: 0
    property var centerPoints: []
    property var centerPointsTypes: []
    onCenterPointsChanged: canvas.requestPaint()
    signal effectChanged()
    signal centersChanged()

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
        property bool isMoving : false
        anchors.fill: parent
        anchors.bottomMargin: clipMonitorRuler.height
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: handleContainsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor
        readonly property bool handleContainsMouse: {
              if (isMoving) {
                  return true;
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
                  root.centerPoints[root.requestedKeyFrame].x = (mouseX - frame.x) / root.scalex;
                  root.centerPoints[root.requestedKeyFrame].y = (mouseY - frame.y) / root.scaley;
                  canvas.requestPaint()
                  root.centersChanged()
            }
        }

        onPressed: mouse => {
            if (mouse.button & Qt.LeftButton) {
                if (root.requestedKeyFrame >= 0 && !isMoving) {
                    controller.seekToKeyframe();
                }
            }
            isMoving = false

        }
        onDoubleClicked: {
            controller.addRemoveKeyframe()
        }
        onReleased: {
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
        property color hoverColor: activePalette.highlight //"#ffff00"
        x: frame.x + root.framesize.x * root.scalex
        y: frame.y + root.framesize.y * root.scaley
        width: root.framesize.width * root.scalex
        height: root.framesize.height * root.scaley
        color: "transparent"
        border.color: "#ffff0000"
        Rectangle {
            id: tlhandle
            anchors {
            top: parent.top
            left: parent.left
            }
            width: root.baseUnit
            height: width
            color: "red"
            visible: root.iskeyframe || controller.autoKeyframe
            opacity: root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              acceptedButtons: Qt.LeftButton
              width: parent.width; height: parent.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeFDiagCursor
              onEntered: { 
                if (!pressed) {
                  tlhandle.color = framerect.hoverColor
                }
              }
              onExited: {
                if (!pressed) {
                  tlhandle.color = '#ff0000'
                }
              }
              onPressed: mouse => {
                  if (root.iskeyframe == false && controller.autoKeyframe) {
                    console.log('ADDREMOVE THAND PRESSED')
                    controller.addRemoveKeyframe();
                  }
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
                  tlhandle.color = framerect.hoverColor
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  adjustedFrame = framesize
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = Math.max(mouseX - oldMouseX, mouseY - oldMouseY)
                      var newwidth = framerect.width - delta
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                      adjustedFrame.y = (framerect.y - frame.y) / root.scaley + framesize.height - adjustedFrame.height;
                      adjustedFrame.x = (framerect.x - frame.x) / root.scalex + framesize.width - adjustedFrame.width;
                  } else {
                    adjustedFrame.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scalex;
                    adjustedFrame.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scaley;
                    adjustedFrame.width = (framerect.width - (mouseX - oldMouseX)) / root.scalex;
                    adjustedFrame.height = (framerect.height - (mouseY - oldMouseY)) / root.scaley;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      adjustedFrame.width -= (framesize.width - adjustedFrame.width)
                      adjustedFrame.height -= (framesize.height - adjustedFrame.height)
                  }
                  framesize = adjustedFrame
                  root.effectChanged()
                }
              }
              onReleased: mouse => {
                  effectsize.visible = false
                  tlhandle.color = '#ff0000'
                  handleRatio = 1
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
            id: trhandle
            anchors {
            top: parent.top
            right: parent.right
            }
            width: root.baseUnit
            height: width
            color: "red"
            visible: root.iskeyframe || controller.autoKeyframe
            opacity: root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              width: parent.width; height: parent.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeBDiagCursor
              onEntered: {
                if (!pressed) {
                  trhandle.color = framerect.hoverColor
                }
              }
              onExited: {
                if (!pressed) {
                  trhandle.color = '#ff0000'
                }
              }
              onPressed: mouse => {
                  if (root.iskeyframe == false && controller.autoKeyframe) {
                      console.log('ADDREMOVE TRAND PRESSED')
                    controller.addRemoveKeyframe();
                  }
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
                  trhandle.color = framerect.hoverColor
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  adjustedFrame = framesize
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = Math.max(oldMouseX - mouseX, mouseY - oldMouseY)
                      var newwidth = framerect.width - delta
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                      adjustedFrame.y = (framerect.y - frame.y) / root.scaley + framesize.height - adjustedFrame.height;
                  } else {
                      adjustedFrame.width = (framerect.width + (mouseX - oldMouseX)) / root.scalex;
                      adjustedFrame.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scaley;
                      adjustedFrame.height = (framerect.height - (mouseY - oldMouseY)) / root.scaley;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      var xOffset = framesize.width - adjustedFrame.width
                      adjustedFrame.x += xOffset
                      adjustedFrame.width -= xOffset
                      adjustedFrame.height -= (framesize.height - adjustedFrame.height)
                  }
                  framesize = adjustedFrame
                  root.effectChanged()
                }
              }
              onReleased: {
                  effectsize.visible = false
                  trhandle.color = '#ff0000'
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: blhandle
            anchors {
            bottom: parent.bottom
            left: parent.left
            }
            width: root.baseUnit
            height: width
            color: "red"
            visible: root.iskeyframe || controller.autoKeyframe
            opacity: root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              width: parent.width; height: parent.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeBDiagCursor
              onEntered: {
                if (!pressed) {
                  blhandle.color = framerect.hoverColor
                }
              }
              onExited: {
                if (!pressed) {
                  blhandle.color = '#ff0000'
                }
              }
              onPressed: mouse => {
                  if (root.iskeyframe == false && controller.autoKeyframe) {
                      console.log('ADDREMOVE BLAND PRESSED')
                    controller.addRemoveKeyframe();
                  }
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
                  blhandle.color = framerect.hoverColor
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                  adjustedFrame = framesize
                  if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = Math.max(mouseX - oldMouseX, oldMouseY - mouseY)
                      var newwidth = framerect.width - delta
                      adjustedFrame.x = (framerect.x + (framerect.width - newwidth) - frame.x) / root.scalex;
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                  } else {
                    adjustedFrame.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scalex;
                    adjustedFrame.width = (framerect.width - (mouseX - oldMouseX)) / root.scalex;
                    adjustedFrame.height = (framerect.height + (mouseY - oldMouseY)) / root.scaley;
                  }
                  if (mouse.modifiers & Qt.ControlModifier) {
                      adjustedFrame.width -= (framesize.width - adjustedFrame.width)
                      var yOffset = framesize.height - adjustedFrame.height
                      adjustedFrame.y += yOffset
                      adjustedFrame.height -= yOffset
                  }
                  framesize = adjustedFrame
                  root.effectChanged()
                }
              }
              onReleased: {
                  effectsize.visible = false
                  blhandle.color = '#ff0000'
                  handleRatio = 1
              }
            }
        }
        Rectangle {
            id: brhandle
            anchors {
            bottom: parent.bottom
            right: parent.right
            }
            width: root.baseUnit
            height: width
            color: "red"
            visible: root.iskeyframe || controller.autoKeyframe
            opacity: root.iskeyframe ? 1 : 0.4
            MouseArea {
              property int oldMouseX
              property int oldMouseY
              property double handleRatio: 1
              width: parent.width; height: parent.height
              anchors.centerIn: parent
              hoverEnabled: true
              cursorShape: Qt.SizeFDiagCursor
              onEntered: {
                if (!pressed) {
                  brhandle.color = framerect.hoverColor
                }
              }
              onExited: {
                if (!pressed) {
                  brhandle.color = '#ff0000'
                }
              }
              onPressed: mouse => {
                  if (root.iskeyframe == false && controller.autoKeyframe) {
                      console.log('ADDREMOVE BRHAND PRESSED')
                    controller.addRemoveKeyframe();
                  }
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectsize.visible = true
                  brhandle.color = framerect.hoverColor
                  handleRatio = framesize.width / framesize.height
              }
              onPositionChanged: mouse => {
                if (pressed) {
                   adjustedFrame = framesize
                   if (root.lockratio > 0 || mouse.modifiers & Qt.ShiftModifier) {
                      var delta = Math.max(oldMouseX - mouseX, oldMouseY - mouseY)
                      var newwidth = framerect.width - delta
                      adjustedFrame.width = Math.round(newwidth / root.scalex);
                      adjustedFrame.height = Math.round(adjustedFrame.width / (root.lockratio > 0 ?root.lockratio : handleRatio))
                  } else {
                    adjustedFrame.width = (framerect.width + (mouseX - oldMouseX)) / root.scalex;
                    adjustedFrame.height = (framerect.height + (mouseY - oldMouseY)) / root.scaley;
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
                  root.effectChanged()
                }
              }
              onReleased: {
                  effectsize.visible = false
                  brhandle.color = '#ff0000'
                  handleRatio = 1
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
            width: root.iskeyframe ? effectsize.height : effectsize.height / 2
            height: width
            radius: width / 2
            border.color: centerArea.containsMouse || centerArea.pressed ? framerect.hoverColor : "#ff0000"
            border.width: 1
            color: centerArea.containsMouse || centerArea.pressed ? framerect.hoverColor : "transparent"
            opacity: centerArea.containsMouse || centerArea.pressed ? 0.5 : 1
            MouseArea {
                id: centerArea
              width: effectsize.height * 1.5; height: effectsize.height * 1.5
              anchors.centerIn: parent
              property int oldMouseX
              property int oldMouseY
              hoverEnabled: true
              enabled: root.iskeyframe || controller.autoKeyframe
              cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
              onPressed: {
                  if (root.iskeyframe == false && controller.autoKeyframe) {
                      console.log('ADDREMOVE CENTER PRESSED')
                    controller.addRemoveKeyframe();
                  }
                  oldMouseX = mouseX
                  oldMouseY = mouseY
                  effectpos.visible = true
              }
              onPositionChanged: {
                  if (pressed) {
                      framesize.x = (framerect.x + (mouseX - oldMouseX) - frame.x) / root.scalex;
                      framesize.y = (framerect.y + (mouseY - oldMouseY) - frame.y) / root.scaley;
                      root.effectChanged()
                  }
              }
              onReleased: {
                  effectpos.visible = false
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
