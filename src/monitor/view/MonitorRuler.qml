/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick 2.15
import org.kde.kdenlive as Kdenlive

// Monitor ruler
Rectangle {
    id: ruler
    color: activePalette.base
    property bool containsMouse: rulerMouseArea.containsMouse
    property bool seekingFinished : controller.seekFinished
    // The width of the visible part
    property double rulerZoomWidth: root.zoomFactor * width
    // The pixel offset
    property double rulerZoomOffset: root.zoomStart * width / root.zoomFactor
    // The scroll factor on seek
    property int seekOffset: root.baseUnit * 0.7
    
    property int playheadPosition: controller.position
    Rectangle {
        color: activePalette.light
        width: parent.width
        height: 1
    }
    
    Timer {
        id: scrollTimer
        interval: 200; running: false;
        onTriggered: {
            if (root.seeking) {
                // Check if seeking ruler
                controller.position = Math.max(0, Math.min((root.mouseRulerPos + ruler.rulerZoomOffset) / root.timeScale, root.duration))
            }
        }
    }
    
    onPlayheadPositionChanged: {
        if (root.zoomFactor == 1) {
            return
        }
        var scaledPosition = ruler.playheadPosition * root.timeScale - ruler.rulerZoomOffset
        if (scaledPosition < root.baseUnit) {
            if (scaledPosition < 0) {
                root.zoomStart = Math.max(0, (rulerZoomOffset - seekOffset + scaledPosition) * root.zoomFactor) / ruler.width
            } else {
                root.zoomStart = Math.max(0, (rulerZoomOffset - seekOffset) * root.zoomFactor) / ruler.width
            }
            if (root.zoomStart > 0) {
                scrollTimer.start()
            }
        } else if (scaledPosition > ruler.width - root.baseUnit) {
            if (scaledPosition > ruler.width) {
                root.zoomStart = Math.min(ruler.width - rulerZoomWidth, (rulerZoomOffset + seekOffset + scaledPosition - ruler.width) * root.zoomFactor) / ruler.width
            } else {
                root.zoomStart = Math.min(ruler.width - rulerZoomWidth, (rulerZoomOffset + seekOffset) * root.zoomFactor) / ruler.width
            }
            if (root.zoomStart < ruler.width - rulerZoomWidth) {
                scrollTimer.start()
            }
        }
    }

    function zoomInRuler(xPos)
    {
        root.showZoomBar = true
        var currentX = playhead.x
        var currentCursor = playhead.x + playhead.width / 2 + ruler.rulerZoomOffset
        
        // Adjust zoom factor
        root.zoomFactor = Math.min(1, root.zoomFactor / 1.2)
        if (root.zoomFactor * ruler.width < root.baseUnit / 2) {
            // Don't allow too large zoom
            root.zoomFactor = root.baseUnit / 2 / ruler.width
        }
        // Always try to have cursor pos centered in zoom
        var cursorPos = Math.max(0, controller.position / root.duration - root.zoomFactor / 2)
        if (cursorPos + root.zoomFactor > 1) {
            cursorPos = 1 - root.zoomFactor
        }
        root.zoomStart = cursorPos
    }
    
    function zoomOutRuler(xPos)
    {
        root.zoomFactor = Math.min(1, root.zoomFactor * 1.2)
        if (root.zoomFactor == 1) {
            root.zoomStart = 0
            root.showZoomBar = false
        } else {
            // Always try to have cursor pos centered in zoom
            var cursorPos = Math.max(0, controller.position / root.duration - root.zoomFactor / 2)
            if (cursorPos + root.zoomFactor > 1) {
                cursorPos = 1 - root.zoomFactor
            }
            root.zoomStart = cursorPos
        }
    }

    onSeekingFinishedChanged : {
        playhead.opacity = seekingFinished ? 1 : 0.5
    }

    onRulerZoomWidthChanged: {
        updateRuler()
    }

    Timer {
        id: zoneToolTipTimer
        interval: 3000; running: false;
    }
    function forceRepaint()
    {
        ruler.color = activePalette.base
        // Enforce repaint
        rulerTicks.model = 0
        rulerTicks.model = ruler.rulerZoomWidth / root.frameSize + 2
        playhead.fillColor = activePalette.windowText
    }

    function updateRuler()
    {
        var projectFps = controller.fps()
        root.timeScale = ruler.width / (root.duration + 1) / root.zoomFactor
        var displayedLength = root.duration * root.zoomFactor / projectFps;
        if (displayedLength < 3 ) {
            // 1 frame tick
            root.frameSize = root.timeScale
        } else if (displayedLength < 30) {
            // 1 second tick
            root.frameSize = projectFps * root.timeScale
        } else if (displayedLength < 150) {
            // 5 second tick
            root.frameSize = 5 * projectFps * root.timeScale
        } else if (displayedLength < 300) {
            // 10 second tick
            root.frameSize = 10 * projectFps * root.timeScale
        } else if (displayedLength < 900) {
            // 30 second tick
            root.frameSize = 30 * projectFps * root.timeScale
        } else if (displayedLength < 1800) {
            // 1 min. tick
            root.frameSize = 60 * projectFps * root.timeScale
        } else if (displayedLength < 9000) {
            // 5 min tick
            root.frameSize = 300 * projectFps * root.timeScale
        } else if (displayedLength < 18000) {
            // 10 min tick
            root.frameSize = 600 * projectFps * root.timeScale
        } else {
            // 30 min tick
            root.frameSize = 18000 * projectFps * root.timeScale
        }
    }

    // Ruler zone
    Rectangle {
        id: zone
        visible: controller.zoneOut >= controller.zoneIn
        color: activePalette.highlight
        x: controller.zoneIn * root.timeScale - ruler.rulerZoomOffset
        width: (controller.zoneOut - controller.zoneIn) * root.timeScale
        property bool zoneHovered: rulerMouseArea.pressed == false && controller.zoneOut >= controller.zoneIn && ((rulerMouseArea.containsMouse && rulerMouseArea.mouseX >= zone.x && rulerMouseArea.mouseX < zone.x + zone.width) || trimOutMouseArea.containsMouse || trimOutMouseArea.pressed || trimInMouseArea.containsMouse)
        anchors.bottom: parent.bottom
        height: ruler.height / 2
        opacity: 0.8
        onXChanged: {
            if (zone.visible) {
                zoneToolTipTimer.start()
            }
        }
        onWidthChanged: {
            if (zone.visible) {
                zoneToolTipTimer.start()
            }
        }
    }

    // frame ticks
    Repeater {
        id: rulerTicks
        model: ruler.width / root.frameSize + 2
        Rectangle {
            x: index * root.frameSize - (ruler.rulerZoomOffset % root.frameSize)
            anchors.bottom: ruler.bottom
            height: (index % 5) ? ruler.height / 4 : ruler.height / 2
            width: 1
            color: activePalette.windowText
            opacity: 0.5
        }
    }
    MouseArea {
        id: rulerMouseArea
        anchors.fill: parent
        //propagateComposedEvents: true
        hoverEnabled: true
        onPressed: mouse => {
            root.captureRightClick = true
            if (mouse.buttons === Qt.LeftButton) {
                var pos = Math.max(mouseX, 0)
                root.seeking = true
                root.mouseRulerPos = mouseX
                controller.position = Math.min((pos + ruler.rulerZoomOffset) / root.timeScale, root.duration);
                mouse.accepted = true
            }
        }
        onReleased: mouse => {
            root.updateClickCapture()
            root.seeking = false
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                var pos = Math.max(mouseX, 0)
                root.mouseRulerPos = pos
                if (pressed) {
                    controller.position = Math.min((pos + ruler.rulerZoomOffset) / root.timeScale, root.duration);
                }
            }
        }
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                if (wheel.angleDelta.y < 0) {
                    // zoom out
                    zoomOutRuler(wheel.x)
                } else {
                    // zoom in
                    zoomInRuler(wheel.x)
                }
            } else {
                wheel.accepted = false
            }
        }
        onEntered: {
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis", "<shortcut>Wheel</shortcut> or <shortcut>arrows</shortcut> to seek 1 frame, <shortcut>Shift</shortcut> to seek 1 second, <shortcut>Alt</shortcut> to seek to marker, <shortcut>Home</shortcut> / <shortcut>End</shortcut> to go to first / last frame"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
        }
    }
    // Zone duration indicator
    Rectangle {
        visible: zone.zoneHovered || zoneToolTipTimer.running
        width: inLabel.contentWidth + 4
        height: inLabel.contentHeight + 2
        property int centerPos: zone.x + zone.width / 2 - inLabel.contentWidth / 2
        x: centerPos < 0 ? 0 : centerPos > ruler.width - inLabel.contentWidth ? ruler.width - inLabel.contentWidth - 2 : centerPos
        color: activePalette.alternateBase
        anchors.bottom: ruler.top
        Label {
            id: inLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignBottom
            text: trimInMouseArea.containsMouse || trimInMouseArea.pressed ? controller.toTimecode(controller.zoneIn) + '>' + controller.toTimecode(controller.zoneOut - controller.zoneIn) : trimOutMouseArea.containsMouse || trimOutMouseArea.pressed ? controller.toTimecode(controller.zoneOut - controller.zoneIn) + '<' + controller.toTimecode(controller.zoneOut - 1) : controller.toTimecode(controller.zoneOut - controller.zoneIn)
            font: fixedFont
            color: activePalette.text
        }
    }
    Kdenlive.TimelinePlayhead {
        id: playhead
        visible: controller.position > -1
        height: ruler.height * 0.5
        width: ruler.height * 1
        opacity: 1
        anchors.top: ruler.top
        z: 2
        fillColor: activePalette.windowText
        x: controller.position * root.timeScale - ruler.rulerZoomOffset - (width / 2)
    }
    MouseArea {
        id: trimInMouseArea
        x: zone.x - root.baseUnit * 0.4
        y: zone.y
        height: zone.height
        width: root.baseUnit * .8
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        drag {
            target: trimInMouseArea
            axis: Drag.XAxis
            smoothed: false
            minimumX: 0
            maximumX: ruler.width
            threshold: 1
        }
        onPressed: {
            // break binding
            root.captureRightClick = true
            x = x
            controller.startZoneMove()
        }
        onReleased: {
            root.updateClickCapture()
            x = Qt.binding(function() { return zone.x - root.baseUnit * .4 })
            controller.endZoneMove()
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                controller.zoneIn = Math.max(0, Math.round((x + (root.baseUnit * .4) + ruler.rulerZoomOffset) / root.timeScale))
                if (mouse.modifiers & Qt.ShiftModifier) {
                    controller.position = controller.zoneIn
                }
            }
        }
        onEntered: {
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis", "<shortcut>Drag</shortcut> to set zone in point, <shortcut>Shift+Drag</shortcut> to seek while adjusting zone in"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
        }
        Rectangle {
            id: trimIn
            anchors.fill: parent
            anchors.leftMargin: root.baseUnit * .4
            color: 'white'
            opacity: zone.zoneHovered || trimInMouseArea.containsMouse || trimInMouseArea.drag.active ? 0.6 : 0
        }
    }
    MouseArea {
        id: trimOutMouseArea
        x: zone.x + zone.width - (root.baseUnit * .4)
        y: zone.y
        width: root.baseUnit * .8
        height: zone.height
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        drag {
            target: trimOutMouseArea
            axis: Drag.XAxis
            smoothed: false
            minimumX: 0
            maximumX: ruler.width - trimOut.width
            threshold: 1
        }
        onPressed: {
            // Break binding
            root.captureRightClick = true
            x = x
            controller.startZoneMove()
        }
        onReleased: {
            root.updateClickCapture()
            x = Qt.binding(function() { return zone.x + zone.width - (root.baseUnit * .4) })
            controller.endZoneMove()
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                controller.zoneOut = Math.round((x + (root.baseUnit * .4) + ruler.rulerZoomOffset) / root.timeScale)
                if (mouse.modifiers & Qt.ShiftModifier) {
                    controller.position = controller.zoneOut
                }
            }
        }
        onEntered: {
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis", "<shortcut>Drag</shortcut> to set zone out point, <shortcut>Shift+Drag</shortcut> to seek while adjusting zone out"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
        }
        Rectangle {
            id: trimOut
            anchors.fill: parent
            anchors.rightMargin: root.baseUnit * .4
            color: 'white'
            opacity: zone.zoneHovered || trimOutMouseArea.containsMouse || trimOutMouseArea.drag.active ? 0.6 : 0
        }
    }

    // markers
    Repeater {
        model: markersModel
        delegate:
        Item {
            id: guideRoot
            anchors.fill: parent
            property bool isRangeMarker: model.hasRange
            property real markerDuration: model.duration

            Rectangle {
                id: rangeSpan
                visible: guideRoot.isRangeMarker
                x: (model.frame * root.timeScale) - ruler.rulerZoomOffset
                width: Math.max(1, guideRoot.markerDuration * root.timeScale)
                height: parent.height
                color: Qt.rgba(model.color.r, model.color.g, model.color.b, 0.5)
            }

            Rectangle {
                id: markerBase
                width: 1
                height: parent.height
                x: (model.frame) * root.timeScale - ruler.rulerZoomOffset;
                color: model.color
            }

            Rectangle {
                id: markerEnd
                visible: guideRoot.isRangeMarker
                width: 1
                height: parent.height
                x: (model.frame + guideRoot.markerDuration) * root.timeScale - ruler.rulerZoomOffset;
                color: model.color
            }

            Rectangle {
                id: markerTooltip
                visible: !rulerMouseArea.pressed && (guideArea.containsMouse || (rulerMouseArea.containsMouse && Math.abs(rulerMouseArea.mouseX - markerBase.x) < 4))
                property int guidePos: markerBase.x - mlabel.contentWidth / 2
                x: guidePos < 0 ? 0 : (guidePos > (parent.width - mlabel.contentWidth) ? parent.width - mlabel.contentWidth : guidePos)
                radius: 2
                width: Math.max(mlabel.contentWidth, imageTooltip.width + 2)
                height: mlabel.contentHeight + imageTooltip.height
                anchors {
                    bottom: parent.top
                }
                color: model.color
                Image {
                    id: imageTooltip
                    visible: markerTooltip.visible && root.baseThumbPath != undefined
                    source: visible ? root.baseThumbPath + model.frame : ''
                    asynchronous: true
                    height: visible ? 4 * mlabel.height : 0
                    fillMode: Image.PreserveAspectFit
                    anchors {
                        horizontalCenter: markerTooltip.horizontalCenter
                        top: parent.top
                        topMargin: 1
                    }
                }
                Text {
                    id: mlabel
                    text: model.comment + (guideRoot.isRangeMarker ? " (" + controller.toTimecode(guideRoot.markerDuration) + ")" : "")
                    font: fixedFont
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    anchors {
                        bottom: parent.bottom
                        left: parent.left
                        right: parent.right
                    }
                    color: '#000'
                }
                MouseArea {
                    z: 10
                    id: guideArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    //onDoubleClicked: timeline.editMarker(clipRoot.binId, model.frame)
                    onClicked: {
                        controller.position = model.frame
                    }
                }
            }
            
            Rectangle {
                id: leftResizeHandle
                visible: guideRoot.isRangeMarker && rangeSpan.width > 10
                width: 4
                height: parent.height
                x: rangeSpan.x
                y: 0
                color: Qt.darker(model.color, 1.3)
                                    opacity: leftResizeArea.containsMouse || leftResizeArea.isResizing ? 0.8 : 0.5
                
                MouseArea {
                    id: leftResizeArea
                    anchors.fill: parent
                    anchors.margins: -2
                    z: 15
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    
                    property bool isResizing: false
                    property real startX: 0
                    property real globalStartX: 0
                    property real startDuration: 0
                    property real startPosition: 0
                    property real originalEndPosition: 0
                    property real currentNewStartPosition: 0
                    property real currentNewDuration: 0
                    
                    onPressed: {
                        root.captureRightClick = true
                        isResizing = true
                        startX = mouseX
                        globalStartX = mapToGlobal(Qt.point(mouseX, 0)).x
                        startDuration = guideRoot.markerDuration
                        startPosition = model.frame
                        originalEndPosition = model.frame + guideRoot.markerDuration
                        lastUpdateTime = new Date().getTime()
                        cursorShape = Qt.SizeHorCursor
                    }
                    
                    onPositionChanged: {
                        if (isResizing) {
                            var globalCurrentX = mapToGlobal(Qt.point(mouseX, 0)).x
                            var realDeltaX = globalCurrentX - globalStartX

                            var deltaFrames = Math.round(realDeltaX / root.timeScale)
                            var newStartPosition = Math.max(0, startPosition + deltaFrames)
                            var newDuration = Math.max(1, originalEndPosition - newStartPosition)

                            currentNewStartPosition = newStartPosition
                            currentNewDuration = newDuration

                            rangeSpan.x = (newStartPosition * root.timeScale) - ruler.rulerZoomOffset
                            rangeSpan.width = Math.max(1, newDuration * root.timeScale)
                            markerBase.x = (newStartPosition) * root.timeScale - ruler.rulerZoomOffset

                            cursorShape = Qt.SizeHorCursor
                        }
                    }
                    
                    onReleased: {
                        if (isResizing) {
                            controller.resizeMarker(startPosition, currentNewDuration, true, currentNewStartPosition)
                            isResizing = false
                            rangeSpan.x = Qt.binding(function() { return (model.frame * root.timeScale) - ruler.rulerZoomOffset })
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * root.timeScale) })
                            markerBase.x = Qt.binding(function() { return (model.frame) * root.timeScale - ruler.rulerZoomOffset; })
                        }
                        root.updateClickCapture()
                    }
                    
                    onCanceled: {
                        if (isResizing) {
                            isResizing = false
                            rangeSpan.x = Qt.binding(function() { return (model.frame * root.timeScale) - ruler.rulerZoomOffset })
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * root.timeScale) })
                            markerBase.x = Qt.binding(function() { return (model.frame) * root.timeScale - ruler.rulerZoomOffset; })
                        }
                    }
                }
            }
            
            // Right resize handle for range markers
            Rectangle {
                id: rightResizeHandle
                visible: guideRoot.isRangeMarker && rangeSpan.width > 10
                width: 4
                height: parent.height
                x: rangeSpan.x + rangeSpan.width - width
                y: 0
                color: Qt.darker(model.color, 1.3)
                                    opacity: rightResizeArea.containsMouse || rightResizeArea.isResizing ? 0.8 : 0.5
                
                MouseArea {
                    id: rightResizeArea
                    anchors.fill: parent
                    anchors.margins: -2
                    z: 15
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    
                    property bool isResizing: false
                    property real startX: 0
                    property real globalStartX: 0
                    property real startDuration: 0
                    property real startPosition: 0
                    property real currentNewDuration: 0
                    property real lastUpdateTime: 0
                    
                    onPressed: {
                        root.captureRightClick = true
                        isResizing = true
                        startX = mouseX
                        globalStartX = mapToGlobal(Qt.point(mouseX, 0)).x
                        startDuration = guideRoot.markerDuration
                        startPosition = model.frame
                        lastUpdateTime = new Date().getTime()
                        cursorShape = Qt.SizeHorCursor
                    }
                    
                    onPositionChanged: {
                        if (isResizing) {
                            var globalCurrentX = mapToGlobal(Qt.point(mouseX, 0)).x
                            var realDeltaX = globalCurrentX - globalStartX
                            
                            var deltaFrames = Math.round(realDeltaX / root.timeScale)
                            var newDuration = Math.max(1, startDuration + deltaFrames)
                            
                            currentNewDuration = newDuration
                            
                            rangeSpan.width = Math.max(1, newDuration * root.timeScale)
                            markerEnd.x = (startPosition + newDuration) * root.timeScale - ruler.rulerZoomOffset
                            
                            cursorShape = Qt.SizeHorCursor
                            
                        }
                    }
                    
                    onReleased: {
                        if (isResizing) {
                            controller.resizeMarker(startPosition, currentNewDuration, false)
                            isResizing = false
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * root.timeScale) })
                            markerEnd.x = Qt.binding(function() { return (model.frame + guideRoot.markerDuration) * root.timeScale - ruler.rulerZoomOffset; })
                        }
                        root.updateClickCapture()
                    }
                    
                    onCanceled: {
                        if (isResizing) {
                            isResizing = false
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * root.timeScale) })
                            markerEnd.x = Qt.binding(function() { return (model.frame + guideRoot.markerDuration) * root.timeScale - ruler.rulerZoomOffset; })
                        }
                    }
                }
            }
        }
    }
}

