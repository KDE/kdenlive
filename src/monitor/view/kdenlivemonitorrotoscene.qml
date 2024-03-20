/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    objectName: "rootrotoscene"

    SystemPalette { id: activePalette }
    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property point profile: controller.profile
    property point center
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int overlayType: controller.overlayType
    property color overlayColor: controller.overlayColor
    property double scalex : 1
    property double scaley : 1
    property bool captureRightClick: true
    // Zoombar properties
    property double zoomStart: 0
    property double zoomFactor: 1
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double stretch : 1
    property double sourcedar : 1
    property double offsetx : 0
    property double offsety : 0
    property double frameSize: 10
    property int duration: 300
    property double timeScale: 1
    property int mouseRulerPos: 0
    onOffsetxChanged: canvas.requestPaint()
    onOffsetyChanged: canvas.requestPaint()
    onScalexChanged: canvas.requestPaint()
    onScaleyChanged: canvas.requestPaint()
    onSourcedarChanged: refreshdar()
    property bool iskeyframe : true
    property bool autoKeyframe: controller.autoKeyframe
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
    property string emptyCanvasKeyBindInfo: xi18nc("@info:whatsthis", "<shortcut>Click</shortcut> to add points, <shortcut>Right click</shortcut> to close shape.")
    property string defaultKeyBindInfo: xi18nc("@info:whatsthis", "<shortcut>Double click</shortcut> on center to resize, <shortcut>Double click</shortcut> on line segment to add new point, <shortcut>Double click</shortcut> point to delete it, <shortcut>Double click</shortcut> background to create new keyframe, <shortcut>Hover right</shortcut> for toolbar");
    property string resizeKeyBindInfo: xi18nc("@info:whatsthis", "<shortcut>Shift drag handle</shortcut> for center-based resize")
    onCenterPointsTypesChanged: checkDefined()
    signal effectPolygonChanged(var points, var centers)
    signal seekToKeyframe()
    focus: true
    Keys.onPressed: (event)=> {
        if (event.key == Qt.Key_Return) {
            if (!root.isDefined && root.centerPoints.length > 2) {
                root.closeShape()
                event.accepted = true;
            }
        }
    }

    function updateClickCapture() {
        if (root.isDefined) {
            root.captureRightClick = false
        } else {
            root.captureRightClick = true
        }
    }

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }

    onIskeyframeChanged: {
        console.log('KEYFRAME CHANGED: ', iskeyframe)
        if (root.displayResize && !controller.autoKeyframe) {
            root.displayResize = false
        }
        canvas.requestPaint()
    }
    onDisplayResizeChanged: {
        controller.setWidgetKeyBinding(root.displayResize ? resizeKeyBindInfo : defaultKeyBindInfo);
    }

    onIsDefinedChanged: {
        controller.setWidgetKeyBinding(root.isDefined ? defaultKeyBindInfo : emptyCanvasKeyBindInfo);
        root.captureRightClick = false
    }

    onAutoKeyframeChanged: {
        canvas.requestPaint()
    }

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
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
        root.isDefined = root.centerPointsTypes.length > 0
        canvas.requestPaint()
    }

    Item {
        id: monitorOverlay
        height: root.height - controller.rulerHeight
        width: root.width

    Canvas {
      id: canvas
      property double handleSize
      property double darOffset : 0
      anchors.fill: parent
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
            if (root.centerPoints.length == 0) {
                // no points defined yet
                return
            }
            var p1 = convertPoint(root.centerPoints[0])
            var startP = p1;
            ctx.moveTo(p1.x, p1.y)
            if (!isDefined) {
                // We are still building the shape, only draw points connected with lines
                if (root.requestedKeyFrame == 0) {
                    ctx.fillStyle = activePalette.highlight
                }
                ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                for (var i = 1; i < root.centerPoints.length; i++) {
                    p1 = convertPoint(root.centerPoints[i])
                    ctx.lineTo(p1.x, p1.y);
                    if (i == root.requestedKeyFrame) {
                        ctx.fillStyle = activePalette.highlight
                        ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                        ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                    } else {
                        ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                    }
                }
            } else {
                var c1; var c2
                var alphaColor = Qt.hsla(activePalette.highlight.hslHue, activePalette.highlight.hslSaturation, activePalette.highlight.hslLightness, 0.5)
                for (var i = 0; i < root.centerPoints.length; i++) {
                    p1 = convertPoint(root.centerPoints[i])
                    // Control points
                    var subkf = false
                    if (i == 0) {
                        c1 = convertPoint(root.centerPointsTypes[root.centerPointsTypes.length - 1])
                        if (root.requestedSubKeyFrame == root.centerPointsTypes.length - 1) {
                            subkf = true
                        }
                        topRight.x = p1.x
                        topRight.y = p1.y
                        bottomLeft.x = p1.x
                        bottomLeft.y = p1.y
                    } else {
                        c1 = convertPoint(root.centerPointsTypes[2*i - 1])
                        if (root.requestedSubKeyFrame == 2*i - 1) {
                            subkf = true
                        }
                        // Find bounding box
                        topRight.x = Math.max(p1.x, topRight.x)
                        topRight.y = Math.min(p1.y, topRight.y)
                        bottomLeft.x = Math.min(p1.x, bottomLeft.x)
                        bottomLeft.y = Math.max(p1.y, bottomLeft.y)
                    }
                    c2 = convertPoint(root.centerPointsTypes[2*i])
                    ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, p1.x, p1.y);
                    if ((iskeyframe || autoKeyframe) && !root.displayResize) {
                        // Draw control points and segments
                        if (subkf) {
                            ctx.fillStyle = activePalette.highlight
                            ctx.fillRect(c1.x - handleSize/2, c1.y - handleSize/2, handleSize, handleSize);
                            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                        } else {
                            ctx.fillRect(c1.x - handleSize/2, c1.y - handleSize/2, handleSize, handleSize);
                        }
                        if (root.requestedSubKeyFrame == 2 * i) {
                            ctx.fillStyle = activePalette.highlight
                            ctx.fillRect(c2.x - handleSize/2, c2.y - handleSize/2, handleSize, handleSize);
                            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                        } else {
                            ctx.fillRect(c2.x - handleSize/2, c2.y - handleSize/2, handleSize, handleSize);
                        }
                        if (i == root.requestedKeyFrame) {
                            ctx.fillStyle = activePalette.highlight
                            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                            ctx.fillStyle = Qt.rgba(1, 0, 0, 0.5)
                        } else {
                            ctx.fillRect(p1.x - handleSize, p1.y - handleSize, 2 * handleSize, 2 * handleSize);
                        }
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
                centerCross.x = bottomLeft.x + (topRight.x - bottomLeft.x)/2
                centerCross.y = topRight.y + (bottomLeft.y - topRight.y)/2
                ctx.moveTo(centerCross.x - root.baseUnit/2, centerCross.y - root.baseUnit/2)
                ctx.lineTo(centerCross.x + root.baseUnit/2, centerCross.y + root.baseUnit/2)
                ctx.moveTo(centerCross.x + root.baseUnit/2, centerCross.y - root.baseUnit/2)
                ctx.lineTo(centerCross.x - root.baseUnit/2, centerCross.y + root.baseUnit/2)
            }
            ctx.stroke()
            if (root.addedPointIndex > -1 && !root.displayResize) {
                // Ghost point where a new one could be inserted
                ctx.beginPath()
                ctx.fillStyle = activePalette.highlight
                ctx.strokeStyle = activePalette.highlight
                ctx.lineWidth = 1
                ctx.roundedRect(addPointPossible.x - handleSize, addPointPossible.y - handleSize, 2 * handleSize, 2 * handleSize, handleSize, handleSize);
                if (root.addedPointIndex === 0) {
                    p1 = convertPoint(root.centerPoints[root.centerPoints.length - 1])
                } else {
                    p1 = convertPoint(root.centerPoints[root.addedPointIndex - 1])
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
                ctx.rect(bottomLeft.x, topRight.y, topRight.x - bottomLeft.x, bottomLeft.y - topRight.y)
                if (root.resizeContainsMouse == 4) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(bottomLeft.x - handleSize, bottomLeft.y - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(bottomLeft.x - handleSize, bottomLeft.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 3) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(topRight.x - handleSize, bottomLeft.y - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(topRight.x - handleSize, bottomLeft.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 2) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(topRight.x - handleSize, topRight.y - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(topRight.x - handleSize, topRight.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 1) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(bottomLeft.x - handleSize, topRight.y - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(bottomLeft.x - handleSize, topRight.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 5) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(bottomLeft.x + (topRight.x - bottomLeft.x) / 2 - handleSize, topRight.y - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(bottomLeft.x + (topRight.x - bottomLeft.x) / 2 - handleSize, topRight.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 7) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(bottomLeft.x + (topRight.x - bottomLeft.x) / 2 - handleSize, bottomLeft.y - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(bottomLeft.x + (topRight.x - bottomLeft.x) / 2 - handleSize, bottomLeft.y - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 6) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(topRight.x - handleSize, topRight.y + (bottomLeft.y - topRight.y) / 2 - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(topRight.x - handleSize, topRight.y + (bottomLeft.y - topRight.y) / 2 - handleSize, 2 * handleSize, 2 * handleSize);
                }
                if (root.resizeContainsMouse == 8) {
                    ctx.fillStyle = activePalette.highlight
                    ctx.fillRect(bottomLeft.x - handleSize, topRight.y + (bottomLeft.y - topRight.y) / 2 - handleSize, 2 * handleSize, 2 * handleSize);
                    ctx.fillStyle = alphaColor
                } else {
                    ctx.fillRect(bottomLeft.x - handleSize, topRight.y + (bottomLeft.y - topRight.y) / 2 - handleSize, 2 * handleSize, 2 * handleSize);
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

    Rectangle {
        anchors.centerIn: parent
        width: label.contentWidth + 6
        height: label.contentHeight + 6
        visible: !root.isDefined && !global.containsMouse
        opacity: 0.8
        Text {
            id: label
            text: i18n("Click to add points,\nright click or press enter to close shape.")
            font: fixedFont
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
        hoverEnabled: true
        cursorShape: (!root.isDefined || pointContainsMouse || centerContainsMouse || addedPointIndex >= 0 || resizeContainsMouse > 0 ) ? Qt.PointingHandCursor : Qt.ArrowCursor
        onEntered: {
            if(!root.isDefined) {
                controller.setWidgetKeyBinding(emptyCanvasKeyBindInfo);
            } else if(root.displayResize){
                controller.setWidgetKeyBinding(resizeKeyBindInfo);
            } else {
                controller.setWidgetKeyBinding(defaultKeyBindInfo);
            }
        }
        onExited: {
            controller.setWidgetKeyBinding()
        }
        onWheel: wheel => {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onDoubleClicked: {
            if (root.isDefined) {
                if (root.iskeyframe == false && controller.autoKeyframe) {
                    controller.addRemoveKeyframe();
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
                if (addPointPossible.x > 0 && root.addedPointIndex >= 0) {
                    var p0; var p1; var p2;
                    var newPoint = Qt.point((addPointPossible.x - frame.x) / root.scalex, (addPointPossible.y - frame.y) / root.scaley);
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
                if (mouse.button == Qt.RightButton && root.centerPoints.length > 2) {
                    root.closeShape()
                } else if (root.requestedKeyFrame < 0) {
                    var newPoint = Qt.point((mouseX - frame.x) / root.scalex, (mouseY - frame.y) / root.scaley);
                    root.centerPoints.push(newPoint)
                    canvas.requestPaint()
                }
            }
        }

        onPositionChanged: mouse => {
            if (pressed && root.iskeyframe == false) {
                if (controller.autoKeyframe) {
                    controller.addRemoveKeyframe();
                } else {
                    return;
                }
            }
            if (pressed) {
                if (root.resizeContainsMouse > 0) {
                    // resizing shape
                    var movingCorner = []
                    var referenceCorner = []
                    if (root.resizeContainsMouse == 1) {
                        // Top left resize
                        movingCorner = Qt.point(bottomLeft.x, topRight.y)
                        referenceCorner = Qt.point(topRight.x, bottomLeft.y)
                    } else if (root.resizeContainsMouse == 2) {
                        // Top right resize
                        movingCorner = Qt.point(topRight.x, topRight.y)
                        referenceCorner = Qt.point(bottomLeft.x, bottomLeft.y)
                    } else if (root.resizeContainsMouse == 3) {
                        // Bottom right resize
                        movingCorner = Qt.point(topRight.x, bottomLeft.y)
                        referenceCorner = Qt.point(bottomLeft.x, topRight.y)
                    } else if (root.resizeContainsMouse == 4) {
                        // Bottom left resize
                        movingCorner = Qt.point(bottomLeft.x, bottomLeft.y)
                        referenceCorner = Qt.point(topRight.x, topRight.y)
                    } else if (root.resizeContainsMouse == 5) {
                        // top resize
                        movingCorner = Qt.point(bottomLeft.x + (topRight.x - bottomLeft.x) / 2, topRight.y)
                        referenceCorner = Qt.point(bottomLeft.x + (topRight.x - bottomLeft.x) / 2, bottomLeft.y)
                    } else if (root.resizeContainsMouse == 7) {
                        // bottom resize
                        movingCorner = Qt.point(bottomLeft.x + (topRight.x - bottomLeft.x) / 2, bottomLeft.y)
                        referenceCorner = Qt.point(bottomLeft.x + (topRight.x - bottomLeft.x) / 2, topRight.y)
                    } else if (root.resizeContainsMouse == 6) {
                        // right resize
                        movingCorner = Qt.point(topRight.x, topRight.y + (bottomLeft.y - topRight.y) / 2)
                        referenceCorner = Qt.point(bottomLeft.x, topRight.y + (bottomLeft.y - topRight.y) / 2)
                    } else if (root.resizeContainsMouse == 8) {
                        // left resize
                        movingCorner = Qt.point(bottomLeft.x, topRight.y + (bottomLeft.y - topRight.y) / 2)
                        referenceCorner = Qt.point(topRight.x, topRight.y + (bottomLeft.y - topRight.y) / 2)
                    }
                    if(mouse.modifiers && mouse.modifiers === Qt.ShiftModifier) {
                        referenceCorner = Qt.point(centerCross.x, centerCross.y)
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
                    var xDiff = (mouseX - centerCross.x) / root.scalex
                    var yDiff = (mouseY - centerCross.y) / root.scaley
                    for (var j = 0; j < root.centerPoints.length; j++) {
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
            } else if ((root.iskeyframe || controller.autoKeyframe) && root.centerPoints.length > 0) {
              // Check if we are over a keyframe
              if (!root.displayResize) {
                  addPointPossible = Qt.point(0, 0)
                  for(var i = 0; i < root.centerPoints.length; i++)
                  {
                      var p1 = canvas.convertPoint(root.centerPoints[i])
                      if (Math.abs(p1.x - mouseX) <= canvas.handleSize && Math.abs(p1.y - mouseY) <= canvas.handleSize) {
                          if (i == root.requestedKeyFrame) {
                              centerContainsMouse = false
                              pointContainsMouse = true;
                              addedPointIndex = -1
                              return;
                          }
                          root.requestedKeyFrame = i
                          centerContainsMouse = false
                          pointContainsMouse = true;
                          addedPointIndex = -1
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
                              addedPointIndex = -1
                              return;
                          }
                          root.requestedSubKeyFrame = i
                          addedPointIndex = -1
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
                              addPointPossible = Qt.point(mouseX, mouseY)
                              addedPointIndex = i
                              root.requestedKeyFrame = -1
                              root.requestedSubKeyFrame = -1
                              centerContainsMouse = false
                              pointContainsMouse = false
                              canvas.requestPaint()
                              return
                          }
                      }
                  }
                  addedPointIndex = -1
              }
              // Check if we are on center point
              if (Math.abs(centerCross.x - mouseX) <= canvas.handleSize && Math.abs(centerCross.y - mouseY) <= canvas.handleSize) {
                    centerContainsMouse = true;
                    pointContainsMouse = false;
                    canvas.requestPaint()
                    return;
              }
              if (root.displayResize) {
                  var currentResize = root.resizeContainsMouse
                  root.resizeContainsMouse = 0
                  // Check if we are on a resize handle
                  if (Math.abs(bottomLeft.x - mouseX) <= canvas.handleSize) {
                      // close to left side
                      if (Math.abs(bottomLeft.y - mouseY) <= canvas.handleSize) {
                          // on the bottom left handle
                          root.resizeContainsMouse = 4
                      } else if (Math.abs(topRight.y - mouseY) <= canvas.handleSize) {
                          // on the top left handle
                          root.resizeContainsMouse = 1
                      } else if (Math.abs(topRight.y + (bottomLeft.y - topRight.y) / 2 - mouseY) <= canvas.handleSize) {
                          // Middle left handle
                          root.resizeContainsMouse = 8
                      }
                  } else if (Math.abs(topRight.x - mouseX) <= canvas.handleSize) {
                      // close to right side
                      if (Math.abs(bottomLeft.y - mouseY) <= canvas.handleSize) {
                          // on the bottom right handle
                          root.resizeContainsMouse = 3
                      } else if (Math.abs(topRight.y - mouseY) <= canvas.handleSize) {
                          // on the top right handle
                          root.resizeContainsMouse = 2
                      } else if (Math.abs(topRight.y + (bottomLeft.y - topRight.y) / 2 - mouseY) <= canvas.handleSize) {
                          // Middle left handle
                          root.resizeContainsMouse = 6
                      }
                  } else if (Math.abs(bottomLeft.x + (topRight.x - bottomLeft.x) / 2 - mouseX) <= canvas.handleSize) {
                      // horizontal center
                      if (Math.abs(bottomLeft.y - mouseY) <= canvas.handleSize) {
                          // on the bottom center handle
                          root.resizeContainsMouse = 7
                      } else if (Math.abs(topRight.y - mouseY) <= canvas.handleSize) {
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
