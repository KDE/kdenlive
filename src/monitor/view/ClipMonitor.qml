/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick

import org.kde.ki18n

import org.kde.kdenlive as K
import 'Utils.js' as Utils

Item {
    id: root
    objectName: "root"

    SystemPalette {
        id: activePalette
    }

    // default size, but scalable by user
    height: 300; width: 400
    required property K.MonitorProxy controller
    property string markerText
    property int itemType: 0
    property point profile: controller.profile
    property double zoom
    property point center
    property double scalex: 1.
    property double scaley: 1.
    property bool seeking: false
    // Zoombar properties
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
    // Display hover audio thumbnails overlay
    property bool showAudiothumb: false
    property bool showClipJobs: false
    // Always display audio thumbs under video
    property bool showToolbar: false
    property string clipName: controller.clipName
    property int duration: 300 // last selectable frame of the timecode display
    property int mouseRulerPos: 0
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property bool isClipMonitor: true
    property int dragType: 0
    property string baseThumbPath
    property bool inLowerThird: (audioView.containsMyMouse || marker.hovered || inPointArea.containsMouse || cursorArea.containsMouse || overlayFPS.containsMouse || overlayTC.containsMouse || outPointArea.containsMouse || (barOverArea.containsMouse && (barOverArea.mouseY >= barOverArea.height / 2)))
    property int overlayMargin: (audioView.state === 'showAudio' && !audioView.isAudioClip && audioView.visible) ? (audioView.height + root.zoomOffset) : root.zoomOffset
    Component.onCompleted: {
        // adjust monitor image size if audio thumb is displayed
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioView.visible) {
            controller.rulerHeight = audioView.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }

    FontMetrics {
        id: fontMetrics
        font: K.UiUtils.fixedFont
    }

    Timer {
        id: thumbTimer
        interval: 3000; running: false;
    }

    signal editCurrentMarker()

    onClipNameChanged: {
        // Animate clip name
        labelContainer.opacity = 1
        contextMenu.opacity = 1
        if (!clipNameLabel.hovered) {
            showAnimate.restart()
        }

        // adjust monitor image size if audio thumb is displayed
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioView.visible) {
            controller.rulerHeight = audioView.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }

    onZoomOffsetChanged: {
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioView.visible) {
            controller.rulerHeight = audioView.height + root.zoomOffset
        } else {
            controller.rulerHeight = root.zoomOffset
        }
    }

    onHeightChanged: {
        if (K.KdenliveSettings.alwaysShowMonitorAudio && audioView.visible) {
            controller.rulerHeight = (audioView.isAudioClip ? (root.height - controller.rulerHeight) : (root.height - controller.rulerHeight) / 6) + root.zoomOffset
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
        onContainsMouseChanged: {
            if (containsMouse) {
                if (!cursorArea.pressed) {
                    root.controller.dragType = ''
                }
            } else {
                root.controller.dragType = '-'
            }
        }
        onPositionChanged: mouse => {
            if (mouse.modifiers & Qt.ShiftModifier) {
                var pos = Math.max(mouseX, 0)
                pos += width / root.controller.timeZoomFactor * root.controller.timeZoomOffset
                root.controller.setPosition(Math.min(pos / root.timeScale, root.duration));
            }
        }
        onWheel: wheel => {
            root.controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
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
        monitorController: root.controller
    }

    Item {
        height: root.height - root.controller.rulerHeight
        width: root.width
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            x: root.center.x - width / 2 - root.offsetx
            y: root.center.y - height / 2 - root.offsety

            K.MonitorOverlay {
                anchors.fill: frame
                color: K.KdenliveSettings.overlayColor
                overlayType: root.overlayType
            }
            K.MonitorSafeZone {
                id: safeZone
                anchors.fill: frame
                showSafeZone: root.controller.showSafezone
                profile: root.controller.profile
            }
        }
        DropArea { //Drop area for effects
            id: effectArea
            anchors.fill: parent
            keys: ['kdenlive/effect']
            property string droppedData
            property string droppedDataSource
            onEntered: drag => {
                drag.acceptProposedAction()
                droppedData = drag.getDataAsString('kdenlive/effect')
                droppedDataSource = drag.getDataAsString('kdenlive/effectsource')
            }
            onDropped: {
                root.controller.addEffect(droppedData, droppedDataSource)
                droppedData = ""
                droppedDataSource = ""
            }
        }
        Item {
            id: monitorOverlay
            anchors.fill: parent
            AudioView {
                id: audioView
                monitorController: root.controller
                timeScale: clipMonitorRuler.timeScale
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: root.zoomOffset
                }
                height: isAudioClip ? parent.height : parent.height / 5
                width: parent.width
                enabled: !cursorArea.containsMouse
                dragButtonsVisible: root.inLowerThird
                dirty: !root.controller.audioSynced
                visible: isAudioClip || ((K.KdenliveSettings.alwaysShowMonitorAudio || root.showAudiothumb) && (root.controller.clipType === K.ClipType.AV || root.controller.clipHasAV))
            }
            Menu {
                id: contextMenu
                Instantiator {
                    model: root.controller.lastClips
                    MenuItem {
                        required property int index
                        required property var modelData
                        text: modelData
                        font: K.UiUtils.fixedFont
                        onTriggered: {
                            root.controller.selectClip(index)
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
                visible: root.controller.clipName != ""
                ToolButton {
                    id: clipNameLabel
                    hoverEnabled: true
                    icon.name: root.controller.lastClips.length > 1 ? "arrow-down" : ""
                    text: root.controller.clipName
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
                        if (root.controller.lastClips.length > 1) {
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
                    color: root.controller.monitorIsActive ? "#DD006600": "#66000000"
                }
                text: root.controller.timecode
                visible: root.showTimecode
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    bottomMargin: root.overlayMargin
                }
                MouseArea {
                    id: overlayTC
                    anchors.fill: parent
                    //acceptedButtons: Qt.NoButton
                    hoverEnabled: true
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
                text: KI18n.i18n("%1fps", root.fps)
                visible: root.showFps
                anchors {
                    right: timecode.visible ? timecode.left : parent.right
                    bottom: parent.bottom
                    bottomMargin: root.overlayMargin
                }
                MouseArea {
                    id: overlayFPS
                    anchors.fill: parent
                    //acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                }
            }
            Label {
                id: labelSpeed
                font: K.UiUtils.fixedFont
                anchors {
                    left: parent.left
                    top: parent.top
                }
                visible: Math.abs(root.controller.speed) > 1
                text: "x" + root.controller.speed
                color: "white"
                background: Rectangle {
                    color: "darkgreen"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: inPoint
                font: K.UiUtils.fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: root.overlayMargin
                }
                visible: root.showMarkers && root.controller.position == root.controller.zoneIn && root.duration > 0
                text: root.controller.zoneIn == root.controller.zoneOut ? KI18n.i18n("In/Out Point") : KI18n.i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                padding: 4
                horizontalAlignment: TextInput.AlignHCenter
                MouseArea {
                    id: inPointArea
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                }
            }
            Label {
                id: outPoint
                font: K.UiUtils.fixedFont
                anchors {
                    left: inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                    bottomMargin: root.overlayMargin
                }
                visible: root.showMarkers && root.controller.position == root.controller.zoneOut && root.controller.zoneOut > root.controller.zoneIn
                text: KI18n.i18n("Out Point")
                color: "white"
                background: Rectangle {
                    color: "#770000"
                }
                padding: 4
                horizontalAlignment: TextInput.AlignHCenter
                MouseArea {
                    id: outPointArea
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                }
            }
            TextField {
                id: marker
                font: K.UiUtils.fixedFont
                objectName: "markertext"
                activeFocusOnPress: true
                text: root.controller.markerComment
                onEditingFinished: {
                    root.markerText = marker.displayText
                    marker.focus = false
                    root.editCurrentMarker()
                }
                anchors {
                    left: outPoint.visible ? outPoint.right : inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                    bottomMargin: root.overlayMargin
                }
                visible: root.showMarkers && text != ""
                height: inPoint.height
                width: fontMetrics.boundingRect(displayText).width + 10
                horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
                background: Rectangle {
                    color: root.controller.markerColor
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
            y: inPoint.visible || outPoint.visible || marker.visible ? parent.height - inPoint.height - height - 2 - root.overlayMargin : parent.height - height - 2 - root.overlayMargin
            property bool showVideoDrag: root.controller.clipHasAV || !audioView.isAudioClip
            height: K.UiUtils.baseSizeMedium * 3
            width: showVideoDrag ? height * 2 : height
            color: Qt.rgba(activePalette.base.r, activePalette.base.g, activePalette.base.b, 0.5)
            radius: 4
            opacity: root.inLowerThird ? 1 : 0
            visible: root.controller.clipHasAV || audioView.isAudioClip
            Rectangle {
                id: videoDragArea
                height: dragZone.height
                width: height
                radius: 4
                color: activePalette.highlight
                border.width: 1
                border.color: activePalette.base
                visible: dragZone.showVideoDrag && (cursorArea.containsMouse && cursorArea.leftSide)
            }
            Rectangle {
                id: audioDragArea
                anchors.right: dragZone.right
                height: dragZone.height
                width: height
                radius: 4
                color: activePalette.highlight
                border.width: 1
                border.color: activePalette.base
                visible: (cursorArea.containsMouse && !cursorArea.leftSide)
            }
            Row {
                Item {
                    width: dragZone.height
                    height: width
                    visible: dragZone.showVideoDrag
                    Image {
                        anchors.fill: parent
                        anchors.margins: height / 6
                        source: "image://icon/kdenlive-show-video"
                    }
                }
                Item {
                    width: dragZone.height
                    height: width
                    Image {
                        anchors.fill: parent
                        anchors.margins: height / 6
                        source: "image://icon/audio-volume-medium"
                    }
                }
            }

            MouseArea {
                id: cursorArea
                // ToolButtons don't allow setting a cursor shape, so workaround
                anchors.fill: dragZone
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true
                property bool leftSide: dragZone.showVideoDrag ? mouseX < width / 2 : false
                cursorShape: Qt.OpenHandCursor
                propagateComposedEvents: true
                onPressed: mouse => {
                    root.controller.dragType = leftSide ? 'V' : 'A'
                    mouse.accepted = false
                }
                onReleased: mouse => {
                    root.controller.dragType = ''
                    mouse.accepted = false
                }
            }
            ToolTip {
                visible: cursorArea.containsMouse && !cursorArea.drag.active
                delay: 1000
                text: cursorArea.leftSide ? KI18n.i18n("Drag to add only video to timeline") : KI18n.i18n("Drag to add only audio to timeline")
            }
        }
    }
    /*MouseArea {
        id: barPosArea
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
        hoverEnabled: true
        property bool inLowerThird: (containsMouse && mouseY > height / 2) || audioView.containsMouse
        onInLowerThirdChanged: {
            console.log('LOWER AREA CONTAINS MOUSE: ', inLowerThird)
        }
        //cursorShape: cursorArea.containsMouse ? Qt.OpenHandCursor : Qt.ArrowCursor
        onEntered: {
            // Show clip name
            if (labelContainer.opacity == 0) {
                labelContainer.opacity = 1
                contextMenu.opacity = 1
                if (!clipNameLabel.hovered) {
                    showAnimate.restart()
                }
            }
            controller.setWidgetKeyBinding(KI18n.xi18nc("@info:whatsthis", "<shortcut>Click</shortcut> to play, <shortcut>Double click</shortcut> for fullscreen, <shortcut>Hover right</shortcut> for toolbar, <shortcut>Wheel</shortcut> or <shortcut>arrows</shortcut> to seek, <shortcut>Ctrl wheel</shortcut> to zoom"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
        }
    }*/
    Item {
        id: clipJobInfo
        x: sceneToolBar.visible && sceneToolBar.rightSide == false ? sceneToolBar.width + 10 : 10
        y: 10
        width: parent.width - 20
        height: childrenRect.height
        visible: root.showClipJobs && root.controller.clipId > 0
        ColumnLayout {
            Repeater {
                model: root.controller.runningJobs
                delegate: Rectangle {
                    id: jobContainer
                    required property int index
                    required property var modelData
                    property var uuid: root.controller.jobsUuids[index]
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
                            toolTipText: KI18n.i18n("Terminate Job")
                            onClicked: root.controller.terminateJob(jobContainer.uuid)
                        }
                        Text {
                            id: jobLabel
                            horizontalAlignment: Text.AlignLeft
                            anchors.leftMargin: 4
                            padding: 2
                            text: jobContainer.modelData
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
                            anchors.rightMargin: (parent.width - 2) * (100 - root.controller.jobsProgress[jobContainer.index]) / 100
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
        height: root.controller.rulerHeight
        monitorController: root.controller
        duration: root.duration
        Repeater {
            model: root.controller.clipBounds
            anchors.fill: parent
            // Usage bar
            Rectangle {
                required property int index
                anchors.top: parent.top
                anchors.topMargin: 1
                property point bd: root.controller.clipBoundary(index)
                x: bd.x * clipMonitorRuler.timeScale - (clipMonitorRuler.width / root.controller.timeZoomFactor * root.controller.timeZoomOffset)
                width: bd.y * clipMonitorRuler.timeScale
                height: 2
                color: 'goldenrod'
            }
        }
    }
}
