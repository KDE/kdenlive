/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
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
    property int overlayType: controller.overlayType
    property color thumbColor1: controller.thumbColor1
    property color thumbColor2: controller.thumbColor2
    property color overlayColor: controller.overlayColor
    property bool isClipMonitor: true
    property int dragType: 0
    property string baseThumbPath
    property int overlayMargin: (audioThumb.stateVisible && !audioThumb.isAudioClip && audioThumb.visible) ? (audioThumb.height + root.zoomOffset) : root.zoomOffset + (audioThumb.isAudioClip && audioSeekZone.visible) ? audioSeekZone.height : 0

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

    signal addControlPoint(real x, real y, bool exclude)

    function updateScrolling()
    {
        if (thumbMouseArea.pressed) {
            var pos = Math.max(thumbMouseArea.mouseX, 0)
            pos += audioThumb.width/root.zoomFactor * root.zoomStart
            controller.setPosition(Math.min(pos / root.timeScale, root.duration));
            
        }
    }

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }
    onClipNameChanged: {
        // Animate clip name
        labelContainer.opacity = 1
        contextMenu.opacity = 1
        if (!clipNameLabel.hovered) {
            showAnimate.restart()
        }

        // adjust monitor image size if audio thumb is displayed
        if (audioThumb.stateVisible && root.permanentAudiothumb && audioThumb.visible) {
            controller.rulerHeight = audioThumb.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }
    
    onZoomOffsetChanged: {
        if (audioThumb.stateVisible && root.permanentAudiothumb && audioThumb.visible) {
            controller.rulerHeight = audioThumb.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }
    
    onHeightChanged: {
        if (audioThumb.stateVisible && root.permanentAudiothumb && audioThumb.visible) {
            controller.rulerHeight = (audioThumb.isAudioClip ? (root.height - controller.rulerHeight) : (root.height - controller.rulerHeight)/ 6) + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
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

    MouseArea {
        id: barOverArea
        hoverEnabled: true
        // Enable to block hide menu event
        acceptedButtons: contextMenu.visible ? Qt.LeftButton : Qt.NoButton
        anchors.fill: parent
        onPositionChanged: mouse => {
            if (mouse.modifiers & Qt.ShiftModifier) {
                var pos = Math.max(mouseX, 0)
                pos += width/root.zoomFactor * root.zoomStart
                controller.setPosition(Math.min(pos / root.timeScale, root.duration));
            }
        }
        onWheel: wheel => {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onEntered: {
            // Show clip name
            if (labelContainer.opacity == 0) {
                labelContainer.opacity = 1
                contextMenu.opacity = 1
                if (!clipNameLabel.hovered) {
                    showAnimate.restart()
                }
            }
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis", "<shortcut>Click</shortcut> to play, <shortcut>Double click</shortcut> for fullscreen, <shortcut>Hover right</shortcut> for toolbar, <shortcut>Wheel</shortcut> or <shortcut>arrows</shortcut> to seek, <shortcut>Ctrl wheel</shortcut> to zoom"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
        }
    }

    SceneToolBar {
        id: sceneToolBar
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
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
            MouseArea { //Drop area for effects
                id: effectArea
                anchors.fill: frame
                property real xPos: 0
                property real yPos: 0
                onClicked: mouse => {
                    if (mouse.button == Qt.LeftButton) {
                        xPos = mouse.x / frame.width
                        yPos = mouse.y / frame.width
                        addControlPoint(xPos, yPos, mouse.modifiers & Qt.ShiftModifier == false)
                    }
                }
            }
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
                x: bd.x * root.timeScale - (audioThumb.width/root.zoomFactor * root.zoomStart)
                width: bd.y * root.timeScale
                height: 2
                color: 'goldenrod'
            }
        }
    }
}
