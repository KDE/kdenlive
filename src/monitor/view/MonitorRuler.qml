/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick.Controls 2.15
import QtQuick 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

// Monitor ruler
Rectangle {
    id: ruler
    color: activePalette.base
    required property K.MonitorProxy monitorController
    required property int duration

    property bool containsMouse: rulerMouseArea.containsMouse
    property int mouseRulerPos: 0
    property bool seeking: false
    property bool seekingFinished : monitorController.seekFinished
    // The width of the visible part
    property double rulerZoomWidth: monitorController.timeZoomFactor * width
    // The pixel offset
    property double rulerZoomOffset: monitorController.timeZoomOffset * width / monitorController.timeZoomFactor
    // The scroll factor on seek
    property int seekOffset: K.UiUtils.baseSizeMedium * 0.7
    property double tickDistance: {
        let projectFps = K.Core.getCurrentFps();
        let displayedLength = ruler.duration * monitorController.timeZoomFactor / projectFps;
        if (displayedLength < 3 ) {
            // 1 frame tick
            return ruler.timeScale
        } else if (displayedLength < 30) {
            // 1 second tick
            return projectFps * ruler.timeScale
        } else if (displayedLength < 150) {
            // 5 second tick
            return 5 * projectFps * ruler.timeScale
        } else if (displayedLength < 300) {
            // 10 second tick
            return 10 * projectFps * ruler.timeScale
        } else if (displayedLength < 900) {
            // 30 second tick
            return 30 * projectFps * ruler.timeScale
        } else if (displayedLength < 1800) {
            // 1 min. tick
            return 60 * projectFps * ruler.timeScale
        } else if (displayedLength < 9000) {
            // 5 min tick
            return 300 * projectFps * ruler.timeScale
        } else if (displayedLength < 18000) {
            // 10 min tick
            return 600 * projectFps * ruler.timeScale
        } else {
            // 30 min tick
            return 18000 * projectFps * ruler.timeScale
        }
    }

    property double timeScale: ruler.width / (ruler.duration + 1) / monitorController.timeZoomFactor
    
    property int playheadPosition: monitorController.position
    SystemPalette { id: activePalette }

    Rectangle {
        color: activePalette.light
        width: parent.width
        height: 1
    }
    
    Timer {
        id: scrollTimer
        interval: 200; running: false;
        onTriggered: {
            if (ruler.seeking) {
                // Check if seeking ruler
                ruler.monitorController.position = Math.max(0, Math.min((ruler.mouseRulerPos + ruler.rulerZoomOffset) / ruler.timeScale, ruler.duration))
            }
        }
    }
    
    onPlayheadPositionChanged: {
        if (monitorController.timeZoomFactor == 1) {
            return
        }
        var scaledPosition = ruler.playheadPosition * ruler.timeScale - ruler.rulerZoomOffset
        if (scaledPosition < K.UiUtils.baseSizeMedium) {
            if (scaledPosition < 0) {
                monitorController.timeZoomOffset = Math.max(0, (rulerZoomOffset - ruler.seekOffset + scaledPosition) * monitorController.timeZoomFactor) / ruler.width
            } else {
                monitorController.timeZoomOffset = Math.max(0, (rulerZoomOffset - ruler.seekOffset) * monitorController.timeZoomFactor) / ruler.width
            }
            if (monitorController.timeZoomOffset > 0) {
                scrollTimer.start()
            }
        } else if (scaledPosition > ruler.width - K.UiUtils.baseSizeMedium) {
            if (scaledPosition > ruler.width) {
                monitorController.timeZoomOffset = Math.min(ruler.width - rulerZoomWidth, (rulerZoomOffset + seekOffset + scaledPosition - ruler.width) * monitorController.timeZoomFactor) / ruler.width
            } else {
                monitorController.timeZoomOffset = Math.min(ruler.width - rulerZoomWidth, (rulerZoomOffset + seekOffset) * monitorController.timeZoomFactor) / ruler.width
            }
            if (monitorController.timeZoomOffset < ruler.width - rulerZoomWidth) {
                scrollTimer.start()
            }
        }
    }

    function zoomInRuler(xPos)
    {
        var currentX = playhead.x
        var currentCursor = playhead.x + playhead.width / 2 + ruler.rulerZoomOffset
        
        // Adjust zoom factor
        monitorController.timeZoomFactor = Math.min(1, monitorController.timeZoomFactor / 1.2)
        if (monitorController.timeZoomFactor * ruler.width < K.UiUtils.baseSizeMedium / 2) {
            // Don't allow too large zoom
            monitorController.timeZoomFactor = K.UiUtils.baseSizeMedium / 2 / ruler.width
        }
        // Always try to have cursor pos centered in zoom
        var cursorPos = Math.max(0, ruler.monitorController.position / ruler.duration - monitorController.timeZoomFactor / 2)
        if (cursorPos + monitorController.timeZoomFactor > 1) {
            cursorPos = 1 - monitorController.timeZoomFactor
        }
        monitorController.timeZoomOffset = cursorPos
    }
    
    function zoomOutRuler(xPos)
    {
        monitorController.timeZoomFactor = Math.min(1, monitorController.timeZoomFactor * 1.2)
        if (monitorController.timeZoomFactor == 1) {
            monitorController.timeZoomOffset = 0
        } else {
            // Always try to have cursor pos centered in zoom
            var cursorPos = Math.max(0, ruler.monitorController.position / ruler.duration - monitorController.timeZoomFactor / 2)
            if (cursorPos + monitorController.timeZoomFactor > 1) {
                cursorPos = 1 - monitorController.timeZoomFactor
            }
            monitorController.timeZoomOffset = cursorPos
        }
    }

    onSeekingFinishedChanged : {
        playhead.opacity = seekingFinished ? 1 : 0.5
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
        rulerTicks.model = ruler.rulerZoomWidth / ruler.tickDistance + 2
        playhead.color = activePalette.windowText
    }

    // Ruler zone
    Rectangle {
        id: zone
        visible: ruler.monitorController.zoneOut >= ruler.monitorController.zoneIn
        color: activePalette.highlight
        x: ruler.monitorController.zoneIn * ruler.timeScale - ruler.rulerZoomOffset
        width: (ruler.monitorController.zoneOut - ruler.monitorController.zoneIn) * ruler.timeScale
        property bool zoneHovered: rulerMouseArea.pressed == false && ruler.monitorController.zoneOut >= ruler.monitorController.zoneIn && ((rulerMouseArea.containsMouse && rulerMouseArea.mouseX >= zone.x && rulerMouseArea.mouseX < zone.x + zone.width) || trimOutMouseArea.containsMouse || trimOutMouseArea.pressed || trimInMouseArea.containsMouse)
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
        model: ruler.width / ruler.tickDistance + 2
        Rectangle {
            required property int index
            x: index * ruler.tickDistance - (ruler.rulerZoomOffset % ruler.tickDistance)
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
            if (mouse.buttons === Qt.LeftButton) {
                var pos = Math.max(mouseX, 0)
                ruler.seeking = true
                ruler.mouseRulerPos = mouseX
                ruler.monitorController.position = Math.min((pos + ruler.rulerZoomOffset) / ruler.timeScale, ruler.duration);
                mouse.accepted = true
            }
        }
        onReleased: mouse => {
            ruler.seeking = false
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                var pos = Math.max(mouseX, 0)
                ruler.mouseRulerPos = pos
                if (pressed) {
                    ruler.monitorController.position = Math.min((pos + ruler.rulerZoomOffset) / ruler.timeScale, ruler.duration);
                }
            }
        }
        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                if (wheel.angleDelta.y < 0) {
                    // zoom out
                    ruler.zoomOutRuler(wheel.x)
                } else {
                    // zoom in
                    ruler.zoomInRuler(wheel.x)
                }
            } else {
                wheel.accepted = false
            }
        }
        onEntered: {
            ruler.monitorController.setWidgetKeyBinding(KI18n.xi18nc("@info:whatsthis", "<shortcut>Wheel</shortcut> or <shortcut>arrows</shortcut> to seek 1 frame, <shortcut>Shift</shortcut> to seek 1 second, <shortcut>Alt</shortcut> to seek to marker, <shortcut>Home</shortcut> / <shortcut>End</shortcut> to go to first / last frame"));
        }
        onExited: {
            ruler.monitorController.setWidgetKeyBinding();
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
            text: trimInMouseArea.containsMouse || trimInMouseArea.pressed ? ruler.monitorController.toTimecode(ruler.monitorController.zoneIn) + '>' + ruler.monitorController.toTimecode(ruler.monitorController.zoneOut - ruler.monitorController.zoneIn) : trimOutMouseArea.containsMouse || trimOutMouseArea.pressed ? ruler.monitorController.toTimecode(ruler.monitorController.zoneOut - ruler.monitorController.zoneIn) + '<' + ruler.monitorController.toTimecode(ruler.monitorController.zoneOut - 1) : ruler.monitorController.toTimecode(ruler.monitorController.zoneOut - ruler.monitorController.zoneIn)
            font: K.UiUtils.fixedFont
            color: activePalette.text
        }
    }
    K.TimelinePlayhead {
        id: playhead
        visible: ruler.monitorController.position > -1
        height: ruler.height * 0.5
        width: ruler.height * 1
        opacity: 1
        anchors.top: ruler.top
        z: 2
        color: activePalette.windowText
        x: ruler.monitorController.position * ruler.timeScale - ruler.rulerZoomOffset - (playhead.width / 2)
    }
    MouseArea {
        id: trimInMouseArea
        x: zone.x - K.UiUtils.baseSizeMedium * 0.4
        y: zone.y
        height: zone.height
        width: K.UiUtils.baseSizeMedium * .8
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
            x = x
            ruler.monitorController.startZoneMove()
        }
        onReleased: {
            x = Qt.binding(function() { return zone.x - K.UiUtils.baseSizeMedium * .4 })
            ruler.monitorController.endZoneMove()
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                ruler.monitorController.zoneIn = Math.max(0, Math.round((x + (K.UiUtils.baseSizeMedium * .4) + ruler.rulerZoomOffset) / ruler.timeScale))
                if (mouse.modifiers & Qt.ShiftModifier) {
                    ruler.monitorController.position = ruler.monitorController.zoneIn
                }
            }
        }
        onEntered: {
            ruler.monitorController.setWidgetKeyBinding(KI18n.xi18nc("@info:whatsthis", "<shortcut>Drag</shortcut> to set zone in point, <shortcut>Shift+Drag</shortcut> to seek while adjusting zone in"));
        }
        onExited: {
            ruler.monitorController.setWidgetKeyBinding();
        }
        Rectangle {
            id: trimIn
            anchors.fill: parent
            anchors.leftMargin: K.UiUtils.baseSizeMedium * .4
            color: 'white'
            opacity: zone.zoneHovered || trimInMouseArea.containsMouse || trimInMouseArea.drag.active ? 0.6 : 0
        }
    }
    MouseArea {
        id: trimOutMouseArea
        x: zone.x + zone.width - (K.UiUtils.baseSizeMedium * .4)
        y: zone.y
        width: K.UiUtils.baseSizeMedium * .8
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
            x = x
            ruler.monitorController.startZoneMove()
        }
        onReleased: {
            x = Qt.binding(function() { return zone.x + zone.width - (K.UiUtils.baseSizeMedium * .4) })
            ruler.monitorController.endZoneMove()
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                ruler.monitorController.zoneOut = Math.round((x + (K.UiUtils.baseSizeMedium * .4) + ruler.rulerZoomOffset) / ruler.timeScale)
                if (mouse.modifiers & Qt.ShiftModifier) {
                    ruler.monitorController.position = ruler.monitorController.zoneOut
                }
            }
        }
        onEntered: {
            ruler.monitorController.setWidgetKeyBinding(KI18n.xi18nc("@info:whatsthis", "<shortcut>Drag</shortcut> to set zone out point, <shortcut>Shift+Drag</shortcut> to seek while adjusting zone out"));
        }
        onExited: {
            ruler.monitorController.setWidgetKeyBinding();
        }
        Rectangle {
            id: trimOut
            anchors.fill: parent
            anchors.rightMargin: K.UiUtils.baseSizeMedium * .4
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
            required property var model
            property bool isRangeMarker: model.hasRange
            property real markerDuration: model.duration

            Rectangle {
                id: rangeSpan
                visible: guideRoot.isRangeMarker
                x: (guideRoot.model.frame * ruler.timeScale) - ruler.rulerZoomOffset
                width: Math.max(1, guideRoot.markerDuration * ruler.timeScale)
                height: parent.height
                color: Qt.rgba(guideRoot.model.color.r, guideRoot.model.color.g, guideRoot.model.color.b, 0.5)
            }

            Rectangle {
                id: markerBase
                width: 1
                height: parent.height
                x: (guideRoot.model.frame) * ruler.timeScale - ruler.rulerZoomOffset;
                color: guideRoot.model.color
            }

            Rectangle {
                id: markerEnd
                visible: guideRoot.isRangeMarker
                width: 1
                height: parent.height
                x: (guideRoot.model.frame + guideRoot.markerDuration) * ruler.timeScale - ruler.rulerZoomOffset;
                color: guideRoot.model.color
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
                color: guideRoot.model.color
                Image {
                    id: imageTooltip
                    visible: markerTooltip.visible && root.baseThumbPath != undefined
                    source: imageTooltip.visible ? root.baseThumbPath + guideRoot.model.frame : ''
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
                    text: guideRoot.model.comment + (guideRoot.isRangeMarker ? " (" + ruler.monitorController.toTimecode(guideRoot.markerDuration) + ")" : "")
                    font: K.UiUtils.fixedFont
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    anchors {
                        bottom: parent.bottom
                        left: parent.left
                        right: parent.right
                    }
                    color: '#000000'
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
                        ruler.monitorController.position = guideRoot.model.frame
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
                color: Qt.darker(guideRoot.model.color, 1.3)
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
                        isResizing = true
                        startX = mouseX
                        globalStartX = mapToGlobal(Qt.point(mouseX, 0)).x
                        startDuration = guideRoot.markerDuration
                        startPosition = guideRoot.model.frame
                        originalEndPosition = guideRoot.model.frame + guideRoot.markerDuration
                        cursorShape = Qt.SizeHorCursor
                    }
                    
                    onPositionChanged: {
                        if (isResizing) {
                            var globalCurrentX = mapToGlobal(Qt.point(mouseX, 0)).x
                            var realDeltaX = globalCurrentX - globalStartX

                            var deltaFrames = Math.round(realDeltaX / ruler.timeScale)
                            var newStartPosition = Math.max(0, startPosition + deltaFrames)
                            var newDuration = Math.max(1, originalEndPosition - newStartPosition)

                            currentNewStartPosition = newStartPosition
                            currentNewDuration = newDuration

                            rangeSpan.x = (newStartPosition * ruler.timeScale) - ruler.rulerZoomOffset
                            rangeSpan.width = Math.max(1, newDuration * ruler.timeScale)
                            markerBase.x = (newStartPosition) * ruler.timeScale - ruler.rulerZoomOffset

                            cursorShape = Qt.SizeHorCursor
                        }
                    }
                    
                    onReleased: {
                        if (isResizing) {
                            ruler.monitorController.resizeMarker(startPosition, currentNewDuration, true, currentNewStartPosition)
                            isResizing = false
                            rangeSpan.x = Qt.binding(function() { return (guideRoot.model.frame * ruler.timeScale) - ruler.rulerZoomOffset })
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * ruler.timeScale) })
                            markerBase.x = Qt.binding(function() { return (guideRoot.model.frame) * ruler.timeScale - ruler.rulerZoomOffset; })
                        }
                    }
                    
                    onCanceled: {
                        if (isResizing) {
                            isResizing = false
                            rangeSpan.x = Qt.binding(function() { return (guideRoot.model.frame * ruler.timeScale) - ruler.rulerZoomOffset })
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * ruler.timeScale) })
                            markerBase.x = Qt.binding(function() { return (guideRoot.model.frame) * ruler.timeScale - ruler.rulerZoomOffset; })
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
                color: Qt.darker(guideRoot.model.color, 1.3)
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

                    onPressed: {
                        isResizing = true
                        startX = mouseX
                        globalStartX = mapToGlobal(Qt.point(mouseX, 0)).x
                        startDuration = guideRoot.markerDuration
                        startPosition = guideRoot.model.frame
                        cursorShape = Qt.SizeHorCursor
                    }
                    
                    onPositionChanged: {
                        if (isResizing) {
                            var globalCurrentX = mapToGlobal(Qt.point(mouseX, 0)).x
                            var realDeltaX = globalCurrentX - globalStartX
                            
                            var deltaFrames = Math.round(realDeltaX / ruler.timeScale)
                            var newDuration = Math.max(1, startDuration + deltaFrames)
                            
                            currentNewDuration = newDuration
                            
                            rangeSpan.width = Math.max(1, newDuration * ruler.timeScale)
                            markerEnd.x = (startPosition + newDuration) * ruler.timeScale - ruler.rulerZoomOffset
                            
                            cursorShape = Qt.SizeHorCursor
                            
                        }
                    }
                    
                    onReleased: {
                        if (isResizing) {
                            ruler.monitorController.resizeMarker(startPosition, currentNewDuration, false)
                            isResizing = false
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * ruler.timeScale) })
                            markerEnd.x = Qt.binding(function() { return (guideRoot.model.frame + guideRoot.markerDuration) * ruler.timeScale - ruler.rulerZoomOffset; })
                        }
                    }
                    
                    onCanceled: {
                        if (isResizing) {
                            isResizing = false
                            rangeSpan.width = Qt.binding(function() { return Math.max(1, guideRoot.markerDuration * ruler.timeScale) })
                            markerEnd.x = Qt.binding(function() { return (guideRoot.model.frame + guideRoot.markerDuration) * ruler.timeScale - ruler.rulerZoomOffset; })
                        }
                    }
                }
            }
        }
    }
}

