/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Effects
import Kdenlive.Controls 1.0
import QtQuick 2.15
import com.enums 1.0

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property int itemType: 0
    property point profile: controller.profile
    property double zoom
    property point center
    property double scalex
    property double scaley
    property bool captureRightClick: false
    // Zoombar properties
    // The start position of the zoomed area, between 0 and 1
    property double zoomStart: 0
    // The zoom factor (between 0 and 1). 0.5 means 2x zoom
    property double zoomFactor: 1
    // The pixel height of zoom bar, used to offset markers info
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double offsetx : 0
    property double offsety : 0
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showSafezone: false
    // Display hover audio thumbnails overlay
    property bool showAudiothumb: false
    property bool showClipJobs: false
    // Always display audio thumbs under video
    property bool permanentAudiothumb: false
    property bool showToolbar: false
    property string clipName: controller.clipName
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property var centerPoints: []
    property var centerPointsTypes: []
    property var maskColors: ["#ffffff", "#ff0000", "#ffff00", "#0000ff", "#000000"]
    property int overlayType: controller.overlayType
    property color thumbColor1: controller.thumbColor1
    property color thumbColor2: controller.thumbColor2
    property color overlayColor: controller.overlayColor
    property bool isClipMonitor: true
    property int dragType: 0
    property string baseThumbPath
    property int overlayMargin: 0
    property int maskMode: controller.maskMode

    onMaskModeChanged: {
        if (maskMode == 1) {
            generateLabel.visible = false
        }
    }

    function updatePoints(types, points) {
        root.centerPointsTypes = types
        root.centerPoints = points
    }

    function updateRect(box) {
        if (box.length == 4 && box[2] > 0) {
            frameBox.x = box[0] * frame.width
            frameBox.y = box[1] * frame.height
            frameBox.width = box[2] * frame.width
            frameBox.height = box[3] * frame.height
            frameBox.visible = true
        } else {
            frameBox.visible = false
        }
    }

    function updateClickCapture() {
        root.captureRightClick = false
    }
    
    FontMetrics {
        id: fontMetrics
        font: fixedFont
    }

    Timer {
        id: thumbTimer
        interval: 3000; running: false;
    }

    signal addControlPoint(real x, real y, bool extend, bool exclude)
    signal addControlRect(real x, real y, real width, real height, bool extend, bool exclude)
    signal generateMask()
    signal exitMaskPreview()

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }
    
    onZoomOffsetChanged: {
        controller.rulerHeight = root.zoomOffset
    }
    
    onHeightChanged: {
        controller.rulerHeight = root.zoomOffset
    }

    function updatePalette() {
        clipMonitorRuler.forceRepaint()
    }

    function switchOverlay() {
        if (controller.overlayType >= 5) {
            controller.overlayType = 0
        } else {
            controller.overlayType = controller.overlayType + 1;
        }
        root.overlayType = controller.overlayType
    }

    Item {
        height: root.height - controller.rulerHeight
        width: root.width
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            x: root.center.x - width / 2 - root.offsetx;
            y: root.center.y - height / 2 - root.offsety;

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
            MouseArea {
                id: frameArea
                hoverEnabled: true
                anchors.fill: frame
                property bool shiftClick: false
                property bool ctrlClick: false
                property bool handleEvent: false
                property bool isDragEvent: false
                property real clickPointX: 0
                property real clickPointY: 0
                property real xPos: 0
                property real yPos: 0
                onPressed: mouse => {
                    if (maskMode == 0) {
                        shiftClick = mouse.modifiers & Qt.ShiftModifier
                        ctrlClick = mouse.modifiers & Qt.ControlModifier
                        root.captureRightClick
                        clickPointX = mouseX
                        clickPointY = mouseY
                        selectionRect.x = mouseX
                        selectionRect.y = mouseY
                        isDragEvent = false
                    } else {
                        mouse.accepted = false;
                    }
                    handleEvent = mouse.button == Qt.LeftButton
                }
                onPositionChanged: {
                    if (maskMode == 0 && ctrlClick) {
                        isDragEvent = true
                    } else {
                        if (isDragEvent) {
                            selectionRect.width = Math.abs(mouseX - clickPointX)
                            if (mouseX < clickPointX) {
                                selectionRect.x = mouseX
                            }
                            selectionRect.height = Math.abs(mouseY - clickPointY)
                            if (mouseY < clickPointY) {
                                selectionRect.y = mouseY
                            }
                        } else if (pressed && (Math.abs(mouseX - selectionRect.x) + Math.abs(mouseY - selectionRect.y) > Qt.styleHints.startDragDistance)) {
                            isDragEvent = true
                            selectionRect.visible = true
                            if (mouseX < selectionRect.x) {
                                selectionRect.width = selectionRect.x + selectionRect.width - mouseX
                                selectionRect.x = mouseX
                            } else {
                                selectionRect.width = mouseX - selectionRect.x
                            }
                            if (mouseY < selectionRect.y) {
                                selectionRect.height = selectionRect.y + selectionRect.height - mouseY
                                selectionRect.y = mouseY
                            } else {
                                selectionRect.height = mouseY - selectionRect.y
                            }
                        }
                    }
                }
                onReleased: {
                    if (maskMode == 1) {
                        mouse.accepted = false;
                        return;
                    }
                    if (maskMode == 0) {
                        root.captureRightClick = false
                    }
                    selectionRect.visible = false
                    if (!ctrlClick && handleEvent) {
                        if (isDragEvent) {
                            // Rect selection
                            xPos = selectionRect.x / frame.width
                            yPos = selectionRect.y / frame.height
                            addControlRect(xPos, yPos, selectionRect.width / frame.width, selectionRect.height / frame.height, shiftClick, ctrlClick)
                            generateLabel.visible = true
                        } else {
                            // Single point selection
                            xPos = mouse.x / frame.width
                            yPos = mouse.y / frame.height
                            addControlPoint(xPos, yPos, shiftClick, ctrlClick)
                            generateLabel.visible = true
                        }
                    }
                }
                /*onClicked: mouse => {
                    if (maskMode == 1) {
                        mouse.accepted = false;
                        return;
                    }
                    if (mouse.button == Qt.LeftButton && !isDragEvent) {
                        xPos = mouse.x / frame.width
                        yPos = mouse.y / frame.height
                        addControlPoint(xPos, yPos, shiftClick, ctrlClick)
                        generateLabel.visible = true
                    }
                }*/
                Rectangle {
                    id: selectionRect
                    color: '#66ffffff'
                    border.color: 'red'
                    border.width: 1
                }
            }
            Image {
                id: maskPreview
                anchors.fill: frame
                source: maskMode == 0 ? controller.previewOverlay : ''
                asynchronous: true
                opacity: controller.maskOpacity / 100
                visible: maskMode == 0
                onSourceChanged: {
                    generateLabel.visible = false
                }
                MultiEffect {
                    source: maskPreview
                    anchors.fill: parent
                    colorization: 1
                    enabled: controller.maskColor > 0
                    colorizationColor: maskColors[controller.maskColor]
                }
            }
            Item {
                anchors.fill: frame
                Repeater {
                    model: root.centerPoints.length
                    delegate:
                    Rectangle {
                        id: kfrPoint
                        required property int index
                        x: root.centerPoints[index].x * frame.width
                        y: root.centerPoints[index].y * frame.height
                        color: root.centerPointsTypes[index] == 1 ? "#FF00FF00" : "#FFFF0000"
                        height: baseUnit / 2
                        width: height
                        radius: 180
                        border.width: 1
                        border.color: "red"
                        MouseArea {
                            anchors.fill: kfrPoint
                            cursorShape: Qt.PointingHand
                            drag.target: kfrPoint
                            onPressed: mouse => {
                                root.captureRightClick = true
                                mouse.accepted = true
                            }
                            onReleased: mouse => {
                                mouse.accepted = true
                                root.captureRightClick = false
                            }

                        }
                    }
                }
                Rectangle {
                    id: frameBox
                    visible: false
                    color: 'transparent'
                    border.color: '#ff0000'
                    border.width: 1
                }
            }
            Label {
                id: generateLabel
                anchors.top: frame.top
                anchors.left: frame.left
                anchors.leftMargin: 10
                anchors.topMargin: 10
                padding: 5
                text: maskMode == 0 ? i18n("Generating image mask") : i18n("Generating video mask")
                visible: false
                background: Rectangle {
                    color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.8)
                    radius: 5
                }
            }
            Label {
                id: infoLabel
                anchors.centerIn: frame
                padding: 5
                text: maskMode == 0 ? i18n("Click on an object to start a mask.\nShift+click to include another zone.\nCtrl+click to exclude a zone.") : i18n("Previewing video mask")
                visible: root.centerPoints.length == 0 && !frameArea.containsMouse && !generateLabel.visible
                background: Rectangle {
                    color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.8)
                    radius: 5
                }
            }
        }
    }
    MaskToolBar {
        id: sceneToolBar
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
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
        visible: root.duration > 0
        height: controller.rulerHeight
        Repeater {
            model:controller.clipBounds
            anchors.fill: parent
            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 1
                property point bd: controller.clipBoundary(model.index)
                x: bd.x * root.timeScale - (frame.width/root.zoomFactor * root.zoomStart)
                width: bd.y * root.timeScale
                height: 2
                color: 'goldenrod'
            }
        }
    }
}
