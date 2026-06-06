/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "rootrotoscene"

    SystemPalette { id: activePalette }
    // default size, but scalable by user
    height: 300; width: 400
    required property K.MonitorProxy controller
    property string comment
    property string framenum
    property point profile: controller.profile
    property point center
    property int overlayType: controller.overlayType
    property double scalex : 1
    property double scaley : 1
    property bool seeking: false
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double stretch : 1
    property double sourcedar : 1
    property double offsetx : 0
    property double offsety : 0
    property int duration: 300
    property int mouseRulerPos: 0
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onSourcedarChanged: refreshdar()
    property bool isKeyframe : controller.isKeyframe
    property bool autoKeyframe: K.KdenliveSettings.autoKeyframe
    property bool isDefined: false
    property int requestedKeyFrame : -1
    property int requestedSubKeyFrame : -1
    // Index in points array where we want to insert a new point
    property int addedPointIndex : -1
    // Position of the added point
    property var addPointPossible : Qt.point(0, 0)
    // Index of the hovered resize handle
    property int resizeContainsMouse: 0
    property bool requestedCenter : false
    // Display resize handles and rect if true
    property bool displayResize : false
    // The coordinate points where the bezier curve passes
    property var centerPoints : []
    property var centerCross : []
    // The control points for the bezier curve points (2 controls points for each coordinate)
    property var centerPointsTypes : []
    // The coordinate points for bounding box
    property var topRight: []
    property var bottomLeft: []
    property bool showToolbar: false
    property string emptyCanvasKeyBindInfo: KI18n.xi18nc("@info:whatsthis", "<shortcut>Click</shortcut> to add points, <shortcut>Right click</shortcut> to close shape.")
    property string defaultKeyBindInfo: KI18n.xi18nc("@info:whatsthis", "<shortcut>Double click</shortcut> on center to resize, <shortcut>Double click</shortcut> on line segment to add new point, <shortcut>Double click</shortcut> point to delete it, <shortcut>Double click</shortcut> background to create new keyframe, <shortcut>Hover right</shortcut> for toolbar");
    property string resizeKeyBindInfo: KI18n.xi18nc("@info:whatsthis", "<shortcut>Shift drag handle</shortcut> for center-based resize")
    onCenterPointsTypesChanged: checkDefined()
    signal effectPolygonChanged(var points, var centers)
    focus: true
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Return) {
            if (!root.isDefined && root.centerPoints.length > 2) {
                root.closeShape()
                event.accepted = true;
            }
        } else {
            event.accepted = false;
        }
    }
    Component.onCompleted: {
        // adjust monitor image size if audio thumb is displayed
        controller.rulerHeight = root.zoomOffset
    }

    onIsKeyframeChanged: {
        if (root.displayResize && !K.KdenliveSettings.autoKeyframe) {
            root.displayResize = false
        }
        if (!global.pressed) {
            canvas.requestPaint()
        }
    }
    onDisplayResizeChanged: {
        controller.setWidgetKeyBinding(root.displayResize ? resizeKeyBindInfo : defaultKeyBindInfo);
    }

    onIsDefinedChanged: {
        controller.setWidgetKeyBinding(root.isDefined ? defaultKeyBindInfo : emptyCanvasKeyBindInfo);
    }

    onAutoKeyframeChanged: {
        canvas.requestPaint()
    }

    function closeShape() {
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
        root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
        canvas.requestPaint()
    }

    function refreshdar() {
        canvas.darOffset = root.sourcedar < root.profile.x * root.stretch / root.profile.y ? (root.profile.x * root.stretch - root.profile.y * root.sourcedar) / (2 * root.profile.x * root.stretch) :(root.profile.y - root.profile.x * root.stretch / root.sourcedar) / (2 * root.profile.y);
        canvas.requestPaint()
    }

    function checkDefined() {
        if (root.isDefined != root.centerPointsTypes.length > 0) {
            root.isDefined = root.centerPointsTypes.length > 0
            if (!root.isDefined) {
                root.requestedKeyFrame = -1
            }
            canvas.requestPaint()
        }
    }

    function updatePoints(types, points) {
        if (global.pressed) {
            return
        }
        root.centerPointsTypes = types
        root.centerPoints = points
        canvas.requestPaint()
    }

    Item {
        id: monitorOverlay
        height: root.height - root.controller.rulerHeight
        width: root.width

    Canvas {
      id: canvas
      property double handleSize: K.UiUtils.baseSizeMedium * 0.5
      property double darOffset : 0
      property color fillColor: Qt.rgba(1, 1, 1, 0.5)
      anchors.fill: parent
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
            if (root.centerPoints.length == 0) {
                // no points defined yet
                return
            }
            var p1 = convertPoint(root.centerPoints[0])
            var startP = p1;
            ctx.moveTo(p1.x, p1.y)
            if (!root.isDefined) {
                // We are still building the shape, only draw points connected with lines
                if (root.requestedKeyFrame == 0) {
                    ctx.fillStyle = activePalette.highlight
                }
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = canvas.fillColor
                for (var i = 1; i < root.centerPoints.length; i++) {
                    p1 = convertPoint(root.centerPoints[i])
                    ctx.lineTo(p1.x, p1.y);
                    if (i == root.requestedKeyFrame) {
                        ctx.fillStyle = activePalette.highlight
                        ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                        ctx.fillStyle = canvas.fillColor
                    } else {
                        ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                    }
                    ctx.strokeRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
            } else {
                var c1; var c2
                var alphaColor = Qt.hsla(activePalette.highlight.hslHue, activePalette.highlight.hslSaturation, activePalette.highlight.hslLightness, 0.5)
                if (root.controller.cursorOutsideEffect) {
                    ctx.setLineDash([4]);
                } else {
                    ctx.setLineDash([]);
                }
                for (i = 0; i < root.centerPoints.length; i++) {
                    p1 = convertPoint(root.centerPoints[i])
                    // Control points
                    var subkf = false
                    if (i == 0) {
                        c1 = convertPoint(root.centerPointsTypes[root.centerPointsTypes.length - 1])
                        if (root.requestedSubKeyFrame == root.centerPointsTypes.length - 1) {
                            subkf = true
                        }
                        root.topRight.x = p1.x
                        root.topRight.y = p1.y
                        root.bottomLeft.x = p1.x
                        root.bottomLeft.y = p1.y
                    } else {
                        c1 = convertPoint(root.centerPointsTypes[2*i - 1])
                        if (root.requestedSubKeyFrame == 2*i - 1) {
                            subkf = true
                        }
                        // Find bounding box
                        root.topRight.x = Math.max(p1.x, root.topRight.x)
                        root.topRight.y = Math.min(p1.y, root.topRight.y)
                        root.bottomLeft.x = Math.min(p1.x, root.bottomLeft.x)
                        root.bottomLeft.y = Math.max(p1.y, root.bottomLeft.y)
                    }
                    c2 = convertPoint(root.centerPointsTypes[2*i])
                    ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, p1.x, p1.y);
                    if ((root.controller.isKeyframe || root.autoKeyframe) && !root.displayResize && !root.controller.cursorOutsideEffect) {
                        // Draw control points and segments
                        if (subkf) {
                            ctx.fillStyle = activePalette.highlight
                            ctx.fillRect(c1.x - handleSize/2, c1.y - handleSize/2, handleSize, handleSize);
                            ctx.fillStyle = canvas.fillColor
                        } else {
                            ctx.fillRect(c1.x - handleSize/2, c1.y - handleSize/2, handleSize, handleSize);
                        }
                        if (root.requestedSubKeyFrame == 2 * i) {
                            ctx.fillStyle = activePalette.highlight
                            ctx.fillRect(c2.x - handleSize/2, c2.y - handleSize/2, handleSize, handleSize);
                            ctx.fillStyle = canvas.fillColor
                        } else {
                            ctx.fillRect(c2.x - handleSize/2, c2.y - handleSize/2, handleSize, handleSize);
                        }
                        if (i == root.requestedKeyFrame) {
                            ctx.fillStyle = activePalette.highlight
                            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                            ctx.fillStyle = canvas.fillColor
                        } else {
                            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                        }
                        ctx.strokeRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                        c1 = convertPoint(root.centerPointsTypes[2*i + 1])
                        ctx.lineTo(c1.x, c1.y)
                        ctx.moveTo(p1.x, p1.y)
                        ctx.lineTo(c2.x, c2.y)
                        ctx.moveTo(p1.x, p1.y)
                    }
                }
                if (root.centerPoints.length > 2) {
                    // Close shape
                    c1 = convertPoint(root.centerPointsTypes[root.centerPointsTypes.length - 1])
                    c2 = convertPoint(root.centerPointsTypes[0])
                    ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, startP.x, startP.y);
                }
                // Calculate and draw center
                root.centerCross.x = root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x)/2
                root.centerCross.y = root.topRight.y + (root.bottomLeft.y - root.topRight.y)/2
                ctx.moveTo(root.centerCross.x - K.UiUtils.baseSizeMedium/2, root.centerCross.y - K.UiUtils.baseSizeMedium/2)
                ctx.lineTo(root.centerCross.x + K.UiUtils.baseSizeMedium/2, root.centerCross.y + K.UiUtils.baseSizeMedium/2)
                ctx.moveTo(root.centerCross.x + K.UiUtils.baseSizeMedium/2, root.centerCross.y - K.UiUtils.baseSizeMedium/2)
                ctx.lineTo(root.centerCross.x - K.UiUtils.baseSizeMedium/2, root.centerCross.y + K.UiUtils.baseSizeMedium/2)
            }
            ctx.stroke()
            if (root.addedPointIndex > -1 && !root.displayResize) {
                // Ghost point where a new one could be inserted
                ctx.beginPath()
                ctx.fillStyle = activePalette.highlight
                ctx.strokeStyle = activePalette.highlight
                ctx.lineWidth = 1
                ctx.roundedRect(root.addPointPossible.x - canvas.handleSize, root.addPointPossible.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize, canvas.handleSize, canvas.handleSize);
                if (root.addedPointIndex === 0) {
                    p1 = canvas.convertPoint(root.centerPoints[root.centerPoints.length - 1])
                } else {
                    p1 = canvas.convertPoint(root.centerPoints[root.addedPointIndex - 1])
                }
                // Segment where the point would be added
                ctx.moveTo(p1.x, p1.y)
                var p2 = convertPoint(root.centerPoints[root.addedPointIndex])
                ctx.lineTo(p2.x, p2.y)
                ctx.stroke()
            }
            if (root.displayResize) {
                // Draw resize rectangle / handles
                ctx.beginPath()
                ctx.fillStyle = alphaColor
                ctx.strokeStyle = activePalette.highlight
                ctx.lineWidth = 1
                ctx.rect(root.bottomLeft.x, root.topRight.y, root.topRight.x - root.bottomLeft.x, root.bottomLeft.y - root.topRight.y)
                if (root.resizeContainsMouse == 4) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.bottomLeft.x - canvas.handleSize, root.bottomLeft.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.bottomLeft.x - canvas.handleSize, root.bottomLeft.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 3) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.topRight.x - canvas.handleSize, root.bottomLeft.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.topRight.x - canvas.handleSize, root.bottomLeft.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 2) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.topRight.x - canvas.handleSize, root.topRight.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.topRight.x - canvas.handleSize, root.topRight.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 1) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.bottomLeft.x - canvas.handleSize, root.topRight.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.bottomLeft.x - canvas.handleSize, root.topRight.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 5) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2 - handleSize, root.topRight.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2 - handleSize, root.topRight.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 7) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2 - canvas.handleSize, root.bottomLeft.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2 - canvas.handleSize, root.bottomLeft.y - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 6) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.topRight.x - canvas.handleSize, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2 - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.topRight.x - canvas.handleSize, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2 - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                if (root.resizeContainsMouse == 8) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(root.bottomLeft.x - canvas.handleSize, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2 - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(root.bottomLeft.x - canvas.handleSize, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2 - canvas.handleSize, 2 * canvas.handleSize, 2 * canvas.handleSize);
                }
                ctx.stroke()
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
        K.MonitorSafeZone {
            id: safeZone
            anchors.fill: frame
            color: K.KdenliveSettings.safeColor
            showSafeZone: root.controller.showSafezone
            profile: root.controller.profile
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: label.contentWidth + 6
        height: label.contentHeight + 6
        visible: !root.isDefined && !global.containsMouse
        opacity: 0.8
        Text {
            id: label
            text: KI18n.i18n("Click to add points,\nright click or press enter to close shape.")
            font: K.UiUtils.fixedFont
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            anchors {
                fill: parent
            }
            color: 'black'
         }
        color: "yellow"
    }

    MouseArea {
        id: global
        objectName: "global"
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        anchors.fill: parent
        property bool pointContainsMouse
        property bool centerContainsMouse
        property double lastMouseX
        property double lastMouseY
        property int lastMousePos
        hoverEnabled: true
        cursorShape: (!root.isDefined || pointContainsMouse || centerContainsMouse || root.addedPointIndex >= 0 || root.resizeContainsMouse > 0 ) ? Qt.PointingHandCursor : Qt.ArrowCursor
        onEntered: {
            if(!root.isDefined) {
                root.controller.setWidgetKeyBinding(root.emptyCanvasKeyBindInfo);
            } else if(root.displayResize){
                root.controller.setWidgetKeyBinding(root.resizeKeyBindInfo);
            } else {
                root.controller.setWidgetKeyBinding(root.defaultKeyBindInfo);
            }
        }
        onPressed: mouse=> {
            lastMouseX = mouse.x
            lastMouseY = mouse.y
        }
        onExited: {
            root.controller.setWidgetKeyBinding()
        }
        onWheel: wheel => {
            root.controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onDoubleClicked: {
            if (root.isDefined) {
                if (root.controller.isKeyframe == false && K.KdenliveSettings.autoKeyframe) {
                    root.controller.addRemoveKeyframe();
                }
                if (root.displayResize) {
                    // Disable resize mode
                    root.displayResize = false
                    canvas.requestPaint()
                    return
                } else if (centerContainsMouse) {
                    // Enable resize mode
                    root.displayResize = true
                    canvas.requestPaint()
                    return
                }
                if (root.requestedKeyFrame > -1) {
                    // Remove existing keyframe
                    if (root.centerPoints.length > 3) {
                        root.centerPoints.splice(root.requestedKeyFrame, 1)
                        root.centerPointsTypes.splice(2 * root.requestedKeyFrame, 2)
                        root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
                        root.requestedKeyFrame = -1
                        canvas.requestPaint()
                    }
                    return
                }
                // Add new keyframe
                if (root.addPointPossible.x > 0 && root.addedPointIndex >= 0) {
                    var p0; var p1; var p2;
                    var newPoint = Qt.point((root.addPointPossible.x - frame.x) / root.scalex, (root.addPointPossible.y - frame.y) / root.scaley);
                    p1 = root.centerPoints[root.addedPointIndex]
                    if (root.addedPointIndex == 0) {
                        p0 = root.centerPoints[root.centerPoints.length - 1]
                    } else {
                        p0 = root.centerPoints[root.addedPointIndex - 1]
                    }
                    var ctrl1 = Qt.point((p0.x - newPoint.x) / 5, (p0.y - newPoint.y) / 5);
                    var ctrl2 = Qt.point((p1.x - newPoint.x) / 5, (p1.y - newPoint.y) / 5);
                    if (root.addedPointIndex == 0) {
                        root.centerPoints.push(newPoint)
                        root.centerPointsTypes.push(Qt.point(newPoint.x + ctrl1.x, newPoint.y + ctrl1.y))
                        root.centerPointsTypes.push(Qt.point(newPoint.x + ctrl2.x, newPoint.y + ctrl2.y))
                    } else {
                        root.centerPoints.splice(root.addedPointIndex, 0, newPoint)
                        root.centerPointsTypes.splice(2 * root.addedPointIndex, 0, Qt.point(newPoint.x + ctrl2.x, newPoint.y + ctrl2.y))
                        root.centerPointsTypes.splice(2 * root.addedPointIndex, 0, Qt.point(newPoint.x + ctrl1.x, newPoint.y + ctrl1.y))
                    }
                    root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
                    canvas.requestPaint()
                }
            }
        }

        onClicked: mouse => {
            if (!root.isDefined) {
                if (mouse.button === Qt.RightButton && root.centerPoints.length > 2) {
                    root.closeShape()
                } else if (root.requestedKeyFrame < 0) {
                    var newPoint = Qt.point((mouseX - frame.x) / root.scalex, (mouseY - frame.y) / root.scaley);
                    root.centerPoints.push(newPoint)
                    canvas.requestPaint()
                }
            }
        }

        onPositionChanged: mouse => {
            if (pressed) {
                if (root.resizeContainsMouse > 0) {
                    // resizing shape
                    var movingCorner = []
                    var referenceCorner = []
                    if (root.resizeContainsMouse == 1) {
                        // Top left resize
                        movingCorner = Qt.point(root.bottomLeft.x, root.topRight.y)
                        referenceCorner = Qt.point(root.topRight.x, root.bottomLeft.y)
                    } else if (root.resizeContainsMouse == 2) {
                        // Top right resize
                        movingCorner = Qt.point(root.topRight.x, root.topRight.y)
                        referenceCorner = Qt.point(root.bottomLeft.x, root.bottomLeft.y)
                    } else if (root.resizeContainsMouse == 3) {
                        // Bottom right resize
                        movingCorner = Qt.point(root.topRight.x, root.bottomLeft.y)
                        referenceCorner = Qt.point(root.bottomLeft.x, root.topRight.y)
                    } else if (root.resizeContainsMouse == 4) {
                        // Bottom left resize
                        movingCorner = Qt.point(root.bottomLeft.x, root.bottomLeft.y)
                        referenceCorner = Qt.point(root.topRight.x, root.topRight.y)
                    } else if (root.resizeContainsMouse == 5) {
                        // top resize
                        movingCorner = Qt.point(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2, root.topRight.y)
                        referenceCorner = Qt.point(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2, root.bottomLeft.y)
                    } else if (root.resizeContainsMouse == 7) {
                        // bottom resize
                        movingCorner = Qt.point(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2, root.bottomLeft.y)
                        referenceCorner = Qt.point(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2, root.topRight.y)
                    } else if (root.resizeContainsMouse == 6) {
                        // right resize
                        movingCorner = Qt.point(root.topRight.x, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2)
                        referenceCorner = Qt.point(root.bottomLeft.x, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2)
                    } else if (root.resizeContainsMouse == 8) {
                        // left resize
                        movingCorner = Qt.point(root.bottomLeft.x, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2)
                        referenceCorner = Qt.point(root.topRight.x, root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2)
                    }
                    if(mouse.modifiers && mouse.modifiers === Qt.ShiftModifier) {
                        referenceCorner = Qt.point(root.centerCross.x, root.centerCross.y)
                    }
                    var originalDist = Math.sqrt( Math.pow(movingCorner.x - referenceCorner.x, 2) + Math.pow(movingCorner.y - referenceCorner.y, 2) );
                    var mouseDist = Math.sqrt( Math.pow(mouseX - referenceCorner.x, 2) + Math.pow(mouseY - referenceCorner.y, 2) );
                    var factor = Math.max(0.1, mouseDist / originalDist)
                    for (var j = 0; j < root.centerPoints.length; j++) {
                        if (root.resizeContainsMouse != 5 && root.resizeContainsMouse!= 7) {
                            root.centerPoints[j].x = (referenceCorner.x - frame.x + (root.centerPoints[j].x * root.scalex - (referenceCorner.x - frame.x)) * factor) / root.scalex
                            root.centerPointsTypes[j * 2].x = (referenceCorner.x - frame.x + (root.centerPointsTypes[j * 2].x * root.scalex - (referenceCorner.x - frame.x)) * factor) / root.scalex
                            root.centerPointsTypes[j * 2 + 1].x = (referenceCorner.x - frame.x + (root.centerPointsTypes[j * 2 + 1].x * root.scalex - (referenceCorner.x - frame.x)) * factor) / root.scalex
                        }
                        if (root.resizeContainsMouse != 6 && root.resizeContainsMouse!= 8) {
                            root.centerPoints[j].y = (referenceCorner.y - frame.y + (root.centerPoints[j].y * root.scaley - (referenceCorner.y - frame.y)) * factor) / root.scaley
                            root.centerPointsTypes[j * 2].y = (referenceCorner.y - frame.y + (root.centerPointsTypes[j * 2].y * root.scaley - (referenceCorner.y - frame.y)) * factor) / root.scaley
                            root.centerPointsTypes[j * 2 + 1].y = (referenceCorner.y - frame.y + (root.centerPointsTypes[j * 2 + 1].y * root.scaley - (referenceCorner.y - frame.y)) * factor) / root.scaley
                        }
                    }
                    canvas.requestPaint()
                    root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
                    return
                }
                if (centerContainsMouse) {
                    // moving shape
                    if (root.controller.position == lastMousePos && K.KdenliveSettings.autoKeyframe && root.controller.speed > 0) {
                        // Don't try to update existing keyframe when playing
                        return
                    }
                    var xDiff = (mouse.x - lastMouseX) / root.scalex
                    var yDiff = (mouse.y - lastMouseY) / root.scaley
                    lastMouseX = mouse.x
                    lastMouseY = mouse.y
                    lastMousePos = root.controller.position
                    for (j = 0; j < root.centerPoints.length; j++) {
                        root.centerPoints[j].x += xDiff
                        root.centerPoints[j].y += yDiff
                        root.centerPointsTypes[j * 2].x += xDiff
                        root.centerPointsTypes[j * 2].y += yDiff
                        root.centerPointsTypes[j * 2 + 1].x += xDiff
                        root.centerPointsTypes[j * 2 + 1].y += yDiff
                    }
                    canvas.requestPaint()
                    root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
                    return
                }
                if (root.requestedKeyFrame >= 0) {
                    var xDiff = (mouseX - frame.x) / root.scalex - root.centerPoints[root.requestedKeyFrame].x
                    var yDiff = (mouseY - frame.y) / root.scaley - root.centerPoints[root.requestedKeyFrame].y
                    root.centerPoints[root.requestedKeyFrame].x += xDiff
                    root.centerPoints[root.requestedKeyFrame].y += yDiff
                    if (root.centerPointsTypes.length > root.requestedKeyFrame * 2 + 1) {
                        root.centerPointsTypes[root.requestedKeyFrame * 2].x += xDiff
                        root.centerPointsTypes[root.requestedKeyFrame * 2].y += yDiff
                        root.centerPointsTypes[root.requestedKeyFrame * 2 + 1].x += xDiff
                        root.centerPointsTypes[root.requestedKeyFrame * 2 + 1].y += yDiff
                    }
                    canvas.requestPaint()
                    if (root.isDefined) {
                        root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
                    }
                } else if (root.requestedSubKeyFrame >= 0) {
                    root.centerPointsTypes[root.requestedSubKeyFrame].x = (mouseX - frame.x) / root.scalex
                    root.centerPointsTypes[root.requestedSubKeyFrame].y = (mouseY - frame.y) / root.scaley
                    canvas.requestPaint()
                    root.effectPolygonChanged(root.centerPoints, root.centerPointsTypes)
                }
            } else if ((root.controller.isKeyframe || K.KdenliveSettings.autoKeyframe) && root.centerPoints.length > 0) {
              // Check if we are over a keyframe
              if (!root.displayResize) {
                  root.addPointPossible = Qt.point(0, 0)
                  for(var i = 0; i < root.centerPoints.length; i++)
                  {
                      var p1 = canvas.convertPoint(root.centerPoints[i])
                      if (Math.abs(p1.x - mouseX) <= canvas.handleSize && Math.abs(p1.y - mouseY) <= canvas.handleSize) {
                          if (i == root.requestedKeyFrame) {
                              centerContainsMouse = false
                              pointContainsMouse = true;
                              root.addedPointIndex = -1
                              return;
                          }
                          root.requestedKeyFrame = i
                          centerContainsMouse = false
                          pointContainsMouse = true;
                          root.addedPointIndex = -1
                          canvas.requestPaint()
                          return;
                      }
                  }
                  // Check if we are on a control point
                  for(var i = 0; i < root.centerPointsTypes.length; i++)
                  {
                      var p1 = canvas.convertPoint(root.centerPointsTypes[i])
                      if (Math.abs(p1.x - mouseX) <= canvas.handleSize/2 && Math.abs(p1.y - mouseY) <= canvas.handleSize/2) {
                          if (i == root.requestedSubKeyFrame) {
                              centerContainsMouse = false
                              pointContainsMouse = true;
                              root.addedPointIndex = -1
                              return;
                          }
                          root.requestedSubKeyFrame = i
                          root.addedPointIndex = -1
                          centerContainsMouse = false
                          pointContainsMouse = true;
                          canvas.requestPaint()
                          return;
                      } 
                  }

                  // Check if we are on a line segment
                  if (root.isDefined) {
                      var p0; var p1; var p2; var dab; var dap; var dbp;
                      var newPoint = Qt.point((mouseX - frame.x) / root.scalex, (mouseY - frame.y) / root.scaley);
                      for (var i = 0; i < root.centerPoints.length; i++) {
                          p1 = root.centerPoints[i]
                          if (i == 0) {
                              p0 = root.centerPoints[root.centerPoints.length - 1]
                          } else {
                              p0 = root.centerPoints[i - 1]
                          }
                          dab = Math.sqrt(Math.pow(p1.x - p0.x, 2) + Math.pow(p1.y - p0.y, 2))
                          dap = Math.sqrt(Math.pow(newPoint.x - p0.x, 2) + Math.pow(newPoint.y - p0.y, 2))
                          dbp = Math.sqrt(Math.pow(p1.x - newPoint.x, 2) + Math.pow(p1.y - newPoint.y, 2))
                          if (Math.abs(dab - dap - dbp) * root.scalex < 4 && dap > dab / 4 && dbp > dab / 4) {
                              root.addPointPossible = Qt.point(mouseX, mouseY)
                              root.addedPointIndex = i
                              root.requestedKeyFrame = -1
                              root.requestedSubKeyFrame = -1
                              centerContainsMouse = false
                              pointContainsMouse = false
                              canvas.requestPaint()
                              return
                          }
                      }
                  }
                  root.addedPointIndex = -1
              }
              // Check if we are on center point
              if (Math.abs(root.centerCross.x - mouseX) <= canvas.handleSize && Math.abs(root.centerCross.y - mouseY) <= canvas.handleSize) {
                    centerContainsMouse = true;
                    pointContainsMouse = false;
                    canvas.requestPaint()
                    return;
              }
              if (root.displayResize) {
                  var currentResize = root.resizeContainsMouse
                  root.resizeContainsMouse = 0
                  // Check if we are on a resize handle
                  if (Math.abs(root.bottomLeft.x - mouseX) <= canvas.handleSize) {
                      // close to left side
                      if (Math.abs(root.bottomLeft.y - mouseY) <= canvas.handleSize) {
                          // on the bottom left handle
                          root.resizeContainsMouse = 4
                      } else if (Math.abs(root.topRight.y - mouseY) <= canvas.handleSize) {
                          // on the top left handle
                          root.resizeContainsMouse = 1
                      } else if (Math.abs(root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2 - mouseY) <= canvas.handleSize) {
                          // Middle left handle
                          root.resizeContainsMouse = 8
                      }
                  } else if (Math.abs(root.topRight.x - mouseX) <= canvas.handleSize) {
                      // close to right side
                      if (Math.abs(root.bottomLeft.y - mouseY) <= canvas.handleSize) {
                          // on the bottom right handle
                          root.resizeContainsMouse = 3
                      } else if (Math.abs(root.topRight.y - mouseY) <= canvas.handleSize) {
                          // on the top right handle
                          root.resizeContainsMouse = 2
                      } else if (Math.abs(root.topRight.y + (root.bottomLeft.y - root.topRight.y) / 2 - mouseY) <= canvas.handleSize) {
                          // Middle left handle
                          root.resizeContainsMouse = 6
                      }
                  } else if (Math.abs(root.bottomLeft.x + (root.topRight.x - root.bottomLeft.x) / 2 - mouseX) <= canvas.handleSize) {
                      // horizontal center
                      if (Math.abs(root.bottomLeft.y - mouseY) <= canvas.handleSize) {
                          // on the bottom center handle
                          root.resizeContainsMouse = 7
                      } else if (Math.abs(root.topRight.y - mouseY) <= canvas.handleSize) {
                          // on the top center handle
                          root.resizeContainsMouse = 5
                      }
                  }
                  if (currentResize != root.resizeContainsMouse) {
                      canvas.requestPaint()
                      return;
                  }
              }
              root.requestedKeyFrame = -1
              root.requestedSubKeyFrame = -1
              pointContainsMouse = false;
              centerContainsMouse = false
              canvas.requestPaint()
            }
        }
    }
}
    EffectToolBar {
        id: effectToolBar
        monitorController: root.controller
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
        height: root.controller.rulerHeight
        monitorController: root.controller
        duration: root.duration
    }

}
