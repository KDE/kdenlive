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
    property int displayFrame: controller.position
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
    property var boxCoords: [0, 0, 0, 0]
    // The frame positions that have points defined
    property var keyframes: []
    property int maskStart: -1
    property int maskEnd: -1
    property int overlayType: controller.overlayType
    property color thumbColor1: controller.thumbColor1
    property color thumbColor2: controller.thumbColor2
    property color overlayColor: controller.overlayColor
    property bool isClipMonitor: true
    property int dragType: 0
    property string baseThumbPath
    property int overlayMargin: 0
    property int maskMode: controller.maskMode
    Component.onCompleted: {
        controller.rulerHeight = root.zoomOffset
    }

    onDisplayFrameChanged: {
        if (root.maskStart > -1 && (root.displayFrame < root.maskStart || root.displayFrame > root.maskEnd)) {
            outsideLabel.visible = true
        } else if (outsideLabel.visible) {
            outsideLabel.visible = false
        }
    }

    onMaskModeChanged: {
        if (maskMode == MaskModeType.MaskPreview) {
            generateLabel.visible = false
        }
    }

    function updatePoints(keyframes, types, points) {
        root.keyframes = keyframes
        root.centerPointsTypes = types
        root.centerPoints = points
    }

    function updateRect(keyframes, box) {
        root.keyframes = keyframes
        if (box.length == 4 && box[2] > 0) {
            root.boxCoords = box
        } else {
            root.boxCoords = [0, 0, 0, 0]
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

    signal moveControlPoint(int index, real x, real y)
    signal addControlPoint(real x, real y, bool extend, bool exclude)
    signal addControlRect(real x, real y, real width, real height, bool extend)
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
        id: monitorframe
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
                // Pan is when user want to move the view
                property bool isPanEvent: false
                // Rect is when drawing a selection rect
                property bool isRectEvent: false
                property real clickPointX: 0
                property real clickPointY: 0
                property real xPos: 0
                property real yPos: 0
                onPressed: mouse => {
                    console.log('GOT FRAME HEIGHT: ', frame.height)
                    if (maskMode != MaskModeType.MaskPreview) {
                        shiftClick = mouse.modifiers & Qt.ShiftModifier
                        ctrlClick = mouse.modifiers & Qt.ControlModifier
                        root.captureRightClick
                        clickPointX = mouseX
                        clickPointY = mouseY
                        selectionRect.x = mouseX
                        selectionRect.y = mouseY
                        isRectEvent = false
                        isPanEvent = false
                    } else {
                        mouse.accepted = false;
                    }
                    handleEvent = mouse.button == Qt.LeftButton
                }
                onPositionChanged: mouse => {
                    if (pressed && !isPanEvent && maskMode < 2 && ctrlClick && (Math.abs(mouseX - selectionRect.x) + Math.abs(mouseY - selectionRect.y) > Qt.styleHints.startDragDistance)) {
                        isPanEvent = true
                        mouse.accepted = true;
                    } else if (!isPanEvent) {
                        if (isRectEvent) {
                            selectionRect.width = Math.abs(mouseX - clickPointX)
                            if (mouseX < clickPointX) {
                                selectionRect.x = mouseX
                            }
                            selectionRect.height = Math.abs(mouseY - clickPointY)
                            if (mouseY < clickPointY) {
                                selectionRect.y = mouseY
                            }
                        } else if (pressed && (Math.abs(mouseX - selectionRect.x) + Math.abs(mouseY - selectionRect.y) > Qt.styleHints.startDragDistance)) {
                            isRectEvent = true
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
                onReleased: mouse => {
                    console.log("Monitor SCENE RELEASED...")
                    if (maskMode == 2) {
                        mouse.accepted = false
                        handleEvent = false
                        return;
                    }
                    root.captureRightClick = false
                    selectionRect.visible = false
                    if (handleEvent) {
                        if (isRectEvent) {
                            // Rect selection
                            xPos = selectionRect.x / frame.width
                            yPos = selectionRect.y / frame.height
                            addControlRect(xPos, yPos, selectionRect.width / frame.width, selectionRect.height / frame.height, shiftClick)
                            generateLabel.visible = true
                        } else if (!isPanEvent) {
                            // Single point selection
                            xPos = mouse.x / frame.width
                            yPos = mouse.y / frame.height
                            addControlPoint(xPos, yPos, shiftClick, ctrlClick)
                            generateLabel.visible = true
                        }
                    }
                    handleEvent = false
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
                source: maskMode != MaskModeType.MaskPreview ? controller.previewOverlay : ''
                asynchronous: true
                opacity: controller.maskOpacity / 100
                visible: maskMode != MaskModeType.MaskPreview
                onSourceChanged: {
                    generateLabel.visible = false
                    if (opacity == 0 && source != '') {
                        // Update opacity to ensure we see something
                        controller.maskOpacity = 50
                    }
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
                        property bool isNegative: root.centerPointsTypes[index] == 0
                        x: root.centerPoints[index].x * frame.width - width / 2
                        y: root.centerPoints[index].y * frame.height - height / 2
                        color: isNegative ? "#FF990000" : "#FF006600"
                        height: baseUnit * 1.5
                        width: height
                        radius: 180
                        border.width: 2
                        border.color: "white"
                        Rectangle {
                            anchors.fill: kfrPoint
                            anchors.leftMargin: kfrPoint.width / 4
                            anchors.rightMargin: kfrPoint.width / 4
                            anchors.topMargin: kfrPoint.height / 2 - 1
                            anchors.bottomMargin: kfrPoint.height / 2 - 1
                            color: "#FFFFFF"
                        }
                        Rectangle {
                            visible: !kfrPoint.isNegative
                            anchors.fill: kfrPoint
                            anchors.leftMargin: kfrPoint.width / 2 - 1
                            anchors.rightMargin: kfrPoint.width / 2 - 1
                            anchors.topMargin: kfrPoint.height / 4
                            anchors.bottomMargin: kfrPoint.height / 4
                            color: "#FFFFFF"
                        }
                        MouseArea {
                            anchors.fill: kfrPoint
                            cursorShape: Qt.PointingHand
                            drag.target: kfrPoint
                            drag.smoothed: false
                            onPressed: mouse => {
                                root.captureRightClick = true
                                mouse.accepted = true
                            }
                            onReleased: mouse => {
                                mouse.accepted = true
                                root.captureRightClick = false
                                var positionInFrame = mapToItem(frame, mouse.x, mouse.y)
                                moveControlPoint(index, positionInFrame.x / frame.width, positionInFrame.y / frame.height)
                                generateLabel.visible = true
                            }

                        }
                    }
                }
                Rectangle {
                    id: frameBox
                    color: '#33ffffff'
                    border.color: '#ff0000'
                    border.width: 1
                    x: root.boxCoords[0] * frame.width
                    y: root.boxCoords[1] * frame.height
                    width: root.boxCoords[2] * frame.width
                    height: root.boxCoords[3] * frame.height
                    visible: root.boxCoords[2] > 0
                }
            }
        }
    }
    Label {
        id: generateLabel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.topMargin: 10
        padding: 5
        text: keyframes.length == 0 ? i18n("Select an object in the image first") : maskMode != MaskModeType.MaskPreview ? i18n("Generating image mask") : i18n("Generating video mask")
        visible: false
        background: Rectangle {
            color: keyframes.length == 0 ? "darkred" : Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.8)
            radius: 5
        }
    }
    Label {
        id: infoLabel
        anchors.centerIn: parent
        padding: 5
        text: maskMode != MaskModeType.MaskPreview ? i18n("Click on an object or draw a box to start a mask.\nShift+click to include another zone.\nCtrl+click to exclude a zone.") : i18n("Previewing video mask")
        visible: root.centerPoints.length == 0 && !frameBox.visible && !frameArea.containsMouse && !generateLabel.visible && !outsideLabel.visible && keyframes.length == 0
        background: Rectangle {
            color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.8)
            radius: 5
        }
    }
    Label {
        id: outsideLabel
        anchors.centerIn: parent
        padding: 5
        text: i18n("You are outside of the time zone defined\nfor the mask and cannot add keyframes.\n\n\n")
        visible: false
        color: 'white'
        background: Rectangle {
            color: 'darkred'
            radius: 5
        }
        ToolButton {
            anchors.bottom: outsideLabel.bottom
            anchors.left: outsideLabel.left
            text: i18n("Go to mask start")
            onPressed: mouse =>{
                root.captureRightClick = true
                mouse.accepted = true
            }
            onReleased: mouse => {
                root.updateClickCapture()
            }
            onClicked: controller.position = root.maskStart
        }
        ToolButton {
            anchors.bottom: outsideLabel.bottom
            anchors.right: outsideLabel.right
            text: i18n("Go to mask end")
            onPressed: mouse =>{
                root.captureRightClick = true
                mouse.accepted = true
            }
            onReleased: mouse => {
                root.updateClickCapture()
            }
            onClicked: controller.position = root.maskEnd
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
    Timer {
        id: firstTimer
        interval: 3000; running: true; repeat: false
    }
    Rectangle {
        id: monitoredge
        anchors.fill: monitorframe
        color: 'transparent'
        border.width: firstTimer.running ? 4 : 1
        border.color: 'darkred'
        Label {
            anchors.horizontalCenter: monitoredge.horizontalCenter
            text: i18n('Mask Mode')
            padding: 5
            background: Rectangle {
                color: 'darkred'
            }
            visible: firstTimer.running
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
            model:root.keyframes
            anchors.fill: parent
            Rectangle {
                id: marker
                property int kf: modelData + root.maskStart
                anchors.bottom: clipMonitorRuler.bottom
                color: 'red'
                width: clipMonitorRuler.height / 2
                height: width
                radius: width
                x: kf * root.timeScale - (frame.width/root.zoomFactor * root.zoomStart) - width / 2
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onPressed: mouse =>{
                        root.captureRightClick = true
                        mouse.accepted = true
                    }
                    onClicked: {
                        controller.position = marker.kf
                    }
                    onReleased: mouse => {
                        root.updateClickCapture()
                    }
                }
            }
        }
    }
}
