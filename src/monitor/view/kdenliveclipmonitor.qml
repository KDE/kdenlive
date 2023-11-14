/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
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
    
    FontMetrics {
        id: fontMetrics
        font: fixedFont
    }

    Timer {
        id: thumbTimer
        interval: 3000; running: false;
    }

    signal editCurrentMarker()
    signal endDrag()

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
            labelContainer.opacity = 1
            if (!clipNameLabel.hovered) {
                showAnimate.restart()
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
        }
        DropArea { //Drop area for effects
            id: effectArea
            anchors.fill: parent
            keys: 'kdenlive/effect'
            property string droppedData
            property string droppedDataSource
            onEntered: drag => {
                drag.acceptProposedAction()
                droppedData = drag.getDataAsString('kdenlive/effect')
                droppedDataSource = drag.getDataAsString('kdenlive/effectsource')
            }
            onDropped: {
                controller.addEffect(droppedData, droppedDataSource)
                droppedData = ""
                droppedDataSource = ""
            }
        }
        Item {
            id: monitorOverlay
            anchors.fill: parent

            Item {
                id: audioThumb
                property bool stateVisible: (root.permanentAudiothumb || clipMonitorRuler.containsMouse || thumbMouseArea.containsMouse || dragZone.opacity == 1 || thumbTimer.running || root.showZoomBar)
                property bool isAudioClip: controller.clipType == ProducerType.Audio
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: root.zoomOffset
                }
                height: isAudioClip ? parent.height : parent.height / 6
                //font.pixelSize * 3
                width: parent.width
                visible: (root.permanentAudiothumb || root.showAudiothumb) && (isAudioClip || controller.clipType == ProducerType.AV || controller.clipHasAV)
                Label {
                    id: clipStreamLabel
                    font: fixedFont
                    anchors {
                        bottom: audioThumb.isAudioClip ? parent.bottom : parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                    color: "white"
                    text: controller.clipStream
                    background: Rectangle {
                        color: "#222277"
                    }
                    visible: text != ""
                    padding :4
                }
                onStateVisibleChanged: {
                    // adjust monitor image size
                    if (stateVisible && root.permanentAudiothumb && audioThumb.visible) {
                        controller.rulerHeight = audioThumb.height + root.zoomOffset
                    } else {
                        controller.rulerHeight = root.zoomOffset
                    }
                }

                states: [
                    State { when: audioThumb.stateVisible || audioThumb.isAudioClip;
                        PropertyChanges {   target: audioThumb; opacity: 1.0    } },
                    State { when: !audioThumb.stateVisible && !audioThumb.isAudioClip;
                        PropertyChanges {   target: audioThumb; opacity: 0.0    } }
                ]
                transitions: [ Transition {
                    NumberAnimation { property: "opacity"; duration: audioThumb.isAudioClip ? 0 : 500}
                } ]
                Rectangle {
                    color: "black"
                    opacity: audioThumb.isAudioClip || root.permanentAudiothumb ? 1 : 0.6
                    anchors.fill: parent
                }
                Rectangle {
                    color: "yellow"
                    opacity: 0.3
                    height: parent.height
                    x: controller.zoneIn * timeScale - (audioThumb.width/root.zoomFactor * root.zoomStart)
                    width: (controller.zoneOut - controller.zoneIn) * timeScale
                    visible: controller.zoneIn > 0 || controller.zoneOut < duration - 1
                }
                Repeater {
                    id: streamThumb
                    model: controller.audioStreams.length
                    onCountChanged: {
                        thumbTimer.start()
                    }
                    property double streamHeight: audioThumb.height / streamThumb.count
                    Item {
                        anchors.fill: parent
                        TimelineWaveform {
                            anchors.right: parent.right
                            anchors.left: parent.left
                            height: streamThumb.streamHeight
                            property int aChannels: controller.audioChannels[model.index]
                            y: model.index * height
                            channels: aChannels
                            binId: controller.clipId
                            audioStream: controller.audioStreams[model.index]
                            isFirstChunk: false
                            format: controller.audioThumbFormat
                            normalize: controller.audioThumbNormalize
                            scaleFactor: audioThumb.width / (root.duration - 1) / root.zoomFactor
                            waveInPoint: (root.duration - 1) * root.zoomStart * aChannels
                            waveOutPointWithUpdate: (root.duration - 1) * (root.zoomStart + root.zoomFactor) * aChannels
                            fillColor1: root.thumbColor1
                            fillColor2: root.thumbColor2
                        }
                        Rectangle {
                            width: parent.width
                            y: (model.index + 1) * streamThumb.streamHeight
                            height: 1
                            visible: streamThumb.count > 1 && model.index < streamThumb.count - 1
                            color: 'yellow'
                        }
                    }
                }
                Rectangle {
                    color: "red"
                    width: 1
                    height: parent.height
                    x: controller.position * timeScale - (audioThumb.width/root.zoomFactor * root.zoomStart)
                }
                MouseArea {
                    id: thumbMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onPressed: {
                        if (audioThumb.isAudioClip && mouseY < audioSeekZone.y) {
                            mouse.accepted = false
                            return
                        }
                        var pos = Math.max(mouseX, 0)
                        pos += audioThumb.width/root.zoomFactor * root.zoomStart
                        controller.setPosition(Math.min(pos / root.timeScale, root.duration));
                    }
                    onPositionChanged: {
                        if (!(mouse.modifiers & Qt.ShiftModifier) && audioThumb.isAudioClip && mouseY < audioSeekZone.y) {
                            mouse.accepted = false
                            return
                        }
                        if (mouse.modifiers & Qt.ShiftModifier || pressed) {
                            var pos = Math.max(mouseX, 0)
                            pos += audioThumb.width/root.zoomFactor * root.zoomStart
                            controller.setPosition(Math.min(pos / root.timeScale, root.duration));
                        }
                    }
                    onWheel: wheel => {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            if (wheel.angleDelta.y < 0) {
                                // zoom out
                                clipMonitorRuler.zoomOutRuler(wheel.x)
                            } else {
                                // zoom in
                                clipMonitorRuler.zoomInRuler(wheel.x)
                            }
                        } else {
                            wheel.accepted = false
                        }
                        
                    }
                    Rectangle {
                        id: audioSeekZone
                        width: parent.width
                        height: parent.height / 6
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: audioThumb.isAudioClip ? parent.height * 5 / 12 : 0
                        visible: audioThumb.isAudioClip && thumbMouseArea.containsMouse && thumbMouseArea.mouseY > y
                        color: 'yellow'
                        opacity: 0.5
                        Rectangle {
                            width: parent.width
                            height: 1
                            color: '#000'
                            anchors.top: parent.top
                        }
                        // frame ticks
                        Repeater {
                            id: rulerAudioTicks
                            model: parent.width / root.frameSize + 2
                            Rectangle {
                                x: index * root.frameSize - (clipMonitorRuler.rulerZoomOffset % root.frameSize)
                                anchors.top: audioSeekZone.top
                                height: (index % 5) ? audioSeekZone.height / 6 : audioSeekZone.height / 3
                                width: 1
                                color: '#000'
                                opacity: 0.8
                            }
                        }
                    }
                }
            }
            Menu {
                id: contextMenu
                Instantiator {
                    model: Object.values(controller.lastClips)
                    MenuItem {
                        text: modelData
                        onTriggered: {
                            controller.selectClip(index)
                            showAnimate.restart()
                        }
                    }
                    // Update model when needed
                    onObjectAdded: contextMenu.insertItem(index, object)
                    onObjectRemoved: contextMenu.removeItem(object)
                }
            }
            Rectangle {
                id: labelContainer
                width: childrenRect.width
                height: childrenRect.height
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                }
                color: clipNameLabel.hovered || contextMenu.visible ? "#CC222277" : "#88222277"
                border.color: clipNameLabel.hovered ? "#000000" : "transparent"
                border.width: 1
                radius: 2
                visible: clipName != ""
                ToolButton {
                    id: clipNameLabel
                    hoverEnabled: true
                    icon.name: contextMenu.count > 2 ? "arrow-down" : ""
                    text: clipName
                    enabled: labelContainer.opacity > 0.5
                    onTextChanged: {
                        if (thumbTimer.running) {
                            thumbTimer.stop()
                        }
                        thumbTimer.start()
                    }
                    bottomPadding: 0
                    topPadding: 0
                    topInset: 0
                    bottomInset: 0
                    SequentialAnimation {
                        id: showAnimate
                        running: false
                        NumberAnimation { target: labelContainer; duration: 3000 }
                        NumberAnimation { target: labelContainer; property: "opacity"; to: 0; duration: 1000 }
                    }
                    onClicked: {
                        console.log("CONTEXTX MENU SIZE: ", contextMenu.count)
                        if (contextMenu.count > 2) {
                            if (contextMenu.visible) {
                                contextMenu.close()
                            } else {
                                contextMenu.popup()
                            }
                        }
                    }
                    onHoveredChanged: {
                        if (hovered) {
                            showAnimate.stop()
                            opacity = 1
                        } else {
                            if (!contextMenu.visible) {
                                showAnimate.restart()
                            }
                        }
                    }
                }
            }

            Label {
                id: timecode
                font.family: fontMetrics.font.family
                font.pointSize: 1.5 * fontMetrics.font.pointSize
                objectName: "timecode"
                color: "#ffffff"
                padding: 2
                background: Rectangle {
                    color: "#66000000"
                }
                text: controller.timecode
                visible: root.showTimecode
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    bottomMargin: overlayMargin
                }
            }
            Label {
                id: fpsdropped
                font.family: fontMetrics.font.family
                font.pointSize: 1.5 * fontMetrics.font.pointSize
                objectName: "fpsdropped"
                color: "#ffffff"
                padding: 2
                background: Rectangle {
                    color: root.dropped ? "#99ff0000" : "#66004400"
                }
                text: i18n("%1fps", root.fps)
                visible: root.showFps
                anchors {
                    right: timecode.visible ? timecode.left : parent.right
                    bottom: parent.bottom
                    bottomMargin: overlayMargin
                }
            }
            Label {
                id: labelSpeed
                font: fixedFont
                anchors {
                    left: parent.left
                    top: parent.top
                }
                visible: Math.abs(controller.speed) > 1
                text: "x" + controller.speed
                color: "white"
                background: Rectangle {
                    color: "darkgreen"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: inPoint
                font: fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: overlayMargin
                }
                visible: root.showMarkers && controller.position == controller.zoneIn
                text: controller.zoneIn == controller.zoneOut ? i18n("In/Out Point") : i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                padding:4
                horizontalAlignment: TextInput.AlignHCenter
                MouseArea {
                    id: inPointArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
            Label {
                id: outPoint
                font: fixedFont
                anchors {
                    left: inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                    bottomMargin: overlayMargin
                }
                visible: root.showMarkers && controller.position == controller.zoneOut && controller.zoneOut > controller.zoneIn
                text: i18n("Out Point")
                color: "white"
                background: Rectangle {
                    color: "#770000"
                }
                padding: 4
                horizontalAlignment: TextInput.AlignHCenter
                MouseArea {
                    id: outPointArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
            TextField {
                id: marker
                font: fixedFont
                objectName: "markertext"
                activeFocusOnPress: true
                text: controller.markerComment
                onEditingFinished: {
                    root.markerText = marker.displayText
                    marker.focus = false
                    root.editCurrentMarker()
                }
                anchors {
                    left: outPoint.visible ? outPoint.right : inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                    bottomMargin: overlayMargin
                }
                visible: root.showMarkers && text != ""
                height: inPoint.height
                width: fontMetrics.boundingRect(displayText).width + 10
                horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
                background: Rectangle {
                    color: controller.markerColor
                }
                color: "#000"
                padding: 0
                maximumLength: 20
            }
        }

        Rectangle {
            // Audio or video only drag zone
            id: dragZone
            property string uuid
            x: 2
            y: inPoint.visible || outPoint.visible || marker.visible ? parent.height - inPoint.height - height - 2 - overlayMargin : parent.height - height - 2 - overlayMargin
            width: childrenRect.width
            height: childrenRect.height
            color: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.7)
            radius: 4
            opacity: (dragAudioArea.containsMouse || dragVideoArea.containsMouse  || thumbMouseArea.containsMouse || marker.hovered || inPointArea.containsMouse || outPointArea.containsMouse
                      || (barOverArea.containsMouse && (barOverArea.mouseY >= (parent.height - inPoint.height - height - 2 - (audioThumb.height + root.zoomOffset) - root.baseUnit)))) ? 1 : 0
            visible: controller.clipHasAV
            onOpacityChanged: {
                if (opacity == 1) {
                    videoDragButton.x = 0
                    videoDragButton.y = 0
                    audioDragButton.x = videoDragButton.x + videoDragButton.width
                    audioDragButton.y = 0
                }
            }
            Row {
                id: dragRow
                ToolButton {
                    id: videoDragButton
                    icon.name: "kdenlive-show-video"
                    Drag.active: dragVideoArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "kdenlive/producerslist" : "V" + controller.clipId + "/" + controller.zoneIn + "/" + (controller.zoneOut - 1),
                        "kdenlive/dragid" : dragZone.uuid
                    }
                    MouseArea {
                        id: dragVideoArea
                        hoverEnabled: true
                        anchors.fill: parent
                        propagateComposedEvents: true
                        cursorShape: Qt.PointingHand
                        drag.target: parent
                        property bool dragActive: drag.active
                        onDragActiveChanged: {
                            if (!drag.active) {
                                root.endDrag()
                            }
                        }
                        onPressed: {
                            dragZone.uuid = controller.getUuid()
                        }
                        onExited: {
                            parent.x = 0
                            parent.y = 0
                        }
                    }
                }
                ToolButton {
                    id: audioDragButton
                    icon.name: "audio-volume-medium"
                    Drag.active: dragAudioArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "kdenlive/producerslist" : "A" + controller.clipId + "/" + controller.zoneIn + "/" + (controller.zoneOut - 1),
                        "kdenlive/dragid" : dragZone.uuid
                    }
                    MouseArea {
                        id: dragAudioArea
                        hoverEnabled: true
                        anchors.fill: parent
                        propagateComposedEvents: true
                        cursorShape: Qt.PointingHand
                        drag.target: parent
                        property bool dragActive: drag.active
                        onDragActiveChanged: {
                            if (!drag.active) {
                                root.endDrag()
                            }
                        }
                        onPressed: {
                            dragZone.uuid = controller.getUuid()
                        }
                        onExited: {
                            parent.x = videoDragButton.x + videoDragButton.width
                            parent.y = 0
                        }
                    }
                }
            }
        }
    }
    Item {
        id: clipJobInfo
        x: sceneToolBar.visible && sceneToolBar.rightSide == false ? sceneToolBar.width + 10 : 10
        y: 10
        width: parent.width - 20
        height: childrenRect.height
        visible: root.showClipJobs && controller.clipId > 0
        Column {
            Repeater {
                model: controller.runningJobs
                delegate: Rectangle {
                    id: jobContainer
                    property var uuid: controller.jobsUuids[model.index]
                    width: childrenRect.width + 4
                    height: jobLabel.height + progressBar.height + 4
                    color: "#80333333"
                    radius: 5
                    MouseArea {
                        id: jobsArea
                        hoverEnabled: true
                        anchors.fill: parent
                    }
                    Row {
                        id: labelRow
                        MonitorToolButton {
                            id: iconButton
                            iconName: "window-close"
                            anchors.leftMargin: 4
                            height: jobLabel.height
                            width: height
                            toolTipText: i18n("Terminate Job")
                            onClicked: controller.terminateJob(uuid)
                        }
                        Text {
                            id: jobLabel
                            horizontalAlignment: Text.AlignLeft
                            anchors.leftMargin: 4
                            padding: 2
                            text: modelData
                            font.pointSize: fontMetrics.font.pointSize
                            elide: Text.ElideMiddle
                            color: 'white'
                        }
                    }
                    Rectangle {
                        id: progressBar
                        anchors.top: labelRow.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        height: 6
                        radius: 2
                        color: "#33ffffff"
                        border {
                            color: "#99000000"
                            width: 1
                        }
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            color: 'steelblue'
                            anchors.rightMargin: (parent.width - 2) * (100 - controller.jobsProgress[model.index]) / 100
                        }
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
