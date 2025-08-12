/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick 2.15

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "root"

    SystemPalette {
        id: activePalette
    }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property int itemType: 0
    property point profile: controller.profile
    property double zoom
    property point center
    property double scalex: 1.
    property double scaley: 1.
    property bool captureRightClick: false
    // Zoombar properties
    // The start position of the zoomed area, between 0 and 1
    property double zoomStart: 0
    // The zoom factor (between 0 and 1). 1 means no zoom, 0.5 means 2x zoom
    property double zoomFactor: 1
    // The pixel height of zoom bar, used to offset markers info
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double offsetx: 0
    property double offsety: 0
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
    property bool showToolbar: false
    property string clipName: controller.clipName
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300 // last selectable frame of the timecode display
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property bool isClipMonitor: true
    property int dragType: 0
    property string baseThumbPath
    property int overlayMargin: (audioThumb.stateVisible && !audioThumb.isAudioClip && audioThumb.visible) ? (audioThumb.height + root.zoomOffset) : root.zoomOffset + (audioThumb.isAudioClip && audioSeekZone.visible) ? audioSeekZone.height : 0
    Component.onCompleted: {
        // adjust monitor image size if audio thumb is displayed
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioThumb.visible) {
            controller.rulerHeight = audioThumb.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }

    function updateClickCapture() {
        root.captureRightClick = false
    }

    FontMetrics {
        id: fontMetrics
        font.family: fixedFont
    }

    Timer {
        id: thumbTimer
        interval: 3000; running: false;
    }

    signal editCurrentMarker()


    function updateScrolling() {
        if (thumbMouseArea.pressed) {
            var pos = Math.max(thumbMouseArea.mouseX, 0)
            pos += audioThumb.width / root.zoomFactor * root.zoomStart
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
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioThumb.visible) {
            controller.rulerHeight = audioThumb.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }

    onZoomOffsetChanged: {
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioThumb.visible) {
            controller.rulerHeight = audioThumb.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }

    onHeightChanged: {
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioThumb.visible) {
            controller.rulerHeight = (audioThumb.isAudioClip ? (root.height - controller.rulerHeight) : (root.height - controller.rulerHeight) / 6) + root.zoomOffset
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
                pos += width / root.zoomFactor * root.zoomStart
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

    K.SceneToolBar {
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

            K.MonitorOverlay {
                anchors.fill: frame
                color: K.KdenliveSettings.overlayColor
                overlayType: root.overlayType
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
                property bool stateVisible: (K.KdenliveSettings.alwaysShowMonitorAudio || clipMonitorRuler.containsMouse || thumbMouseArea.containsMouse || dragZone.opacity == 1 || thumbTimer.running || root.showZoomBar)
                property bool isAudioClip: controller.clipType == K.ClipType.Audio
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: root.zoomOffset
                }
                height: isAudioClip ? parent.height : parent.height / 6
                //font.pixelSize * 3
                width: parent.width
                visible: (K.KdenliveSettings.alwaysShowMonitorAudio || root.showAudiothumb) && (isAudioClip || controller.clipType == K.ClipType.AV || controller.clipHasAV)

                Label {
                    id: clipStreamLabel
                    font.family: fixedFont
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
                    padding: 4
                }
                onStateVisibleChanged: {
                    // adjust monitor image size
                    if (K.KdenliveSettings.alwaysShowMonitorAudio && audioThumb.visible) {
                        controller.rulerHeight = audioThumb.height + root.zoomOffset
                    } else {
                        controller.rulerHeight = root.zoomOffset
                    }
                }

                states: [
                    State {
                        when: audioThumb.stateVisible || audioThumb.isAudioClip;
                        PropertyChanges {
                            audioThumb.opacity: 1.0
                        }
                    },
                    State {
                        when: !audioThumb.stateVisible && !audioThumb.isAudioClip;
                        PropertyChanges {
                            audioThumb.opacity: 0.0
                        }
                    }
                ]
                transitions: [Transition {
                    NumberAnimation {
                        property: "opacity";
                        duration: audioThumb.isAudioClip ? 0 : 500
                    }
                }]
                Rectangle {
                    color: "black"
                    opacity: audioThumb.isAudioClip || K.KdenliveSettings.alwaysShowMonitorAudio ? 1 : 0.6
                    anchors.fill: parent
                }
                Rectangle {
                    color: "yellow"
                    opacity: 0.3
                    height: parent.height
                    x: controller.zoneIn * timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
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
                        K.TimelineWaveform {
                            id: waveform
                            anchors.right: parent.right
                            anchors.left: parent.left
                            height: streamThumb.streamHeight
                            property int aChannels: controller.audioChannels[model.index]
                            y: model.index * waveform.height
                            channels: aChannels
                            binId: controller.clipId
                            audioStream: controller.audioStreams[model.index]
                            format: K.KdenliveSettings.displayallchannels
                            normalize: K.KdenliveSettings.normalizechannels
                            property int aClipDuration: root.duration + 1
                            scaleFactor: audioThumb.width / aClipDuration / root.zoomFactor
                            waveInPoint: waveform.aClipDuration * root.zoomStart
                            waveOutPoint: waveform.aClipDuration * (root.zoomStart + root.zoomFactor)
                            fgColorEven: K.KdenliveSettings.thumbColor1
                            fgColorOdd: K.KdenliveSettings.thumbColor2
                            bgColorEven: "#00000000"
                            bgColorOdd: "#00000000"
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
                    x: controller.position * timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
                }
                MouseArea {
                    id: thumbMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onEntered: {
                        // Show clip name
                        if (labelContainer.opacity == 0) {
                            labelContainer.opacity = 1
                            contextMenu.opacity = 1
                            if (!clipNameLabel.hovered) {
                                showAnimate.restart()
                            }
                        }
                    }
                    onPressed: {
                        if (audioThumb.isAudioClip && mouseY < audioSeekZone.y) {
                            mouse.accepted = false
                            return
                        }
                        var pos = Math.max(mouseX, 0)
                        pos += audioThumb.width / root.zoomFactor * root.zoomStart
                        controller.setPosition(Math.min(pos / root.timeScale, root.duration));
                    }
                    onPositionChanged: mouse => {
                        if (!(mouse.modifiers & Qt.ShiftModifier) && audioThumb.isAudioClip && mouseY < audioSeekZone.y) {
                            mouse.accepted = false
                            return
                        }
                        if (mouse.modifiers & Qt.ShiftModifier || pressed) {
                            var pos = Math.max(mouseX, 0)
                            pos += audioThumb.width / root.zoomFactor * root.zoomStart
                            controller.setPosition(Math.min(pos / root.timeScale, root.duration));
                        }
                    }
                    onWheel: wheel => {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            if (wheel.angleDelta.y == 0) {
                                // Don't trigger zoom if delta is null
                                return
                            }
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
                    model: controller.lastClips
                    MenuItem {
                        text: modelData
                        font.family: fixedFont
                        onTriggered: {
                            controller.selectClip(index)
                            //showAnimate.restart()
                        }
                    }
                    // Update model when needed
                    onObjectAdded: (index, object) => contextMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => contextMenu.removeItem(object)
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
                    icon.name: controller.lastClips.length > 1 ? "arrow-down" : ""
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
                        NumberAnimation {
                            target: labelContainer; duration: 3000
                        }
                        onStarted: {
                            contextMenu.opacity = 1
                        }
                        onFinished: {
                            if (contextMenu.visible) {
                                contextMenu.close()
                            }
                            fadeAnimate.start()
                        }
                    }
                    ParallelAnimation {
                        id: fadeAnimate
                        running: false
                        NumberAnimation {
                            target: labelContainer; property: "opacity";
                            to: 0; duration: 1000
                        }
                    }
                    onClicked: {
                        if (controller.lastClips.length > 1) {
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
                    color: controller.monitorIsActive ? "#DD006600": "#66000000"
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
                font.family: fixedFont
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
                font.family: fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: overlayMargin
                }
                visible: root.showMarkers && controller.position == controller.zoneIn && root.duration > 0
                text: controller.zoneIn == controller.zoneOut ? i18n("In/Out Point") : i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                padding: 4
                horizontalAlignment: TextInput.AlignHCenter
                MouseArea {
                    id: inPointArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
            Label {
                id: outPoint
                font.family: fixedFont
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
                font.family: fixedFont
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
            width: videoDragButton.width * 2
            height: videoDragButton.height
            color: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.5)
            radius: 4
            opacity: (audioDragButton.hovered || videoDragButton.hovered || thumbMouseArea.containsMouse || marker.hovered || inPointArea.containsMouse || outPointArea.containsMouse || dragAudioArea.active || dragVideoArea.active
                || (barOverArea.containsMouse && (barOverArea.mouseY >= (parent.height - inPoint.height - height - 2 - (audioThumb.height + root.zoomOffset) - root.baseUnit)))) ? 1 : 0
            visible: controller.clipHasAV
            MouseArea {
                id: buttonArea
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                hoverEnabled: true
            }
            Rectangle {
                width: dragZone.width / 2
                height: dragZone.height
                radius: 4
                color: activePalette.highlight
                visible: videoDragButton.hovered || videoDragButton.isDragging
            }
            Rectangle {
                anchors.right: dragZone.right
                width: dragZone.width / 2
                height: dragZone.height
                radius: 4
                color: activePalette.highlight
                visible: audioDragButton.hovered || audioDragButton.isDragging
            }
            Row {
                id: dragRow
                ToolButton {
                    id: videoDragButton
                    property bool isDragging
                    hoverEnabled: true
                    icon.name: "kdenlive-show-video"
                    focusPolicy: Qt.NoFocus
                    Drag.active: dragVideoArea.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "text/producerslist": "V" + controller.clipId + "/" + controller.zoneIn + "/" + (controller.zoneOut - 1),
                        "text/dragid": dragZone.uuid
                    }
                    Drag.onDragStarted: {
                        videoDragButton.isDragging = true
                        dragZone.uuid = controller.getUuid()
                    }
                    Drag.onDragFinished: dropAction => {
                        videoDragButton.isDragging = false
                        dragVideoArea.enabled = false
                        root.captureRightClick = false
                    }
                    onPressed: {
                        dragVideoArea.enabled = true
                        videoDragButton.grabToImage(function(result) {
                            videoDragButton.Drag.imageSource = result.url
                        })
                    }
                    DragHandler {
                        id: dragVideoArea
                        acceptedButtons: Qt.LeftButton
                        target: null
                        enabled: false
                    }
                }
                ToolButton {
                    id: audioDragButton
                    property bool isDragging
                    hoverEnabled: true
                    icon.name: "audio-volume-medium"
                    focusPolicy: Qt.NoFocus
                    Drag.active: dragAudioArea.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "text/producerslist": "A" + controller.clipId + "/" + controller.zoneIn + "/" + (controller.zoneOut - 1),
                        "text/dragid": dragZone.uuid
                    }
                    Drag.onDragStarted: {
                        audioDragButton.isDragging = true
                        dragZone.uuid = controller.getUuid()
                    }
                    Drag.onDragFinished: {
                        audioDragButton.isDragging = false
                        root.captureRightClick = false
                        dragAudioArea.enabled = false
                    }
                    onPressed: {
                        dragAudioArea.enabled = true
                        audioDragButton.grabToImage(function(result) {
                            audioDragButton.Drag.imageSource = result.url
                        })
                    }
                    DragHandler {
                        id: dragAudioArea
                        acceptedButtons: Qt.LeftButton
                        target: null
                        enabled: false
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
        ColumnLayout {
            Repeater {
                model: controller.runningJobs
                delegate: Rectangle {
                    id: jobContainer
                    property var uuid: controller.jobsUuids[model.index]
                    width: childrenRect.width + 4
                    Layout.fillWidth: true
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
                        K.MonitorToolButton {
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
    K.MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        visible: root.duration > 0
        height: controller.rulerHeight
        Repeater {
            model: controller.clipBounds
            anchors.fill: parent
            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 1
                property point bd: controller.clipBoundary(model.index)
                x: bd.x * root.timeScale - (audioThumb.width / root.zoomFactor * root.zoomStart)
                width: bd.y * root.timeScale
                height: 2
                color: 'goldenrod'
            }
        }
    }
}
