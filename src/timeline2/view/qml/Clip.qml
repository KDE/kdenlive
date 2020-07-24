/*
 * Copyright (c) 2013-2016 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.11
import QtQuick.Controls 2.4
import Kdenlive.Controls 1.0
import QtQml.Models 2.11
import QtQuick.Window 2.2
import 'Timeline.js' as Logic
import com.enums 1.0

Rectangle {
    id: clipRoot
    property real timeScale: 1
    property string clipName: ''
    property string clipResource: ''
    property string mltService: ''
    property string effectNames
    property bool isProxy: false
    property int modelStart
    property real scrollX: 0
    property int inPoint: 0
    property int outPoint: 0
    property int clipDuration: 0
    property int maxDuration: 0
    property bool isAudio: false
    property int audioChannels
    property int audioStream: -1
    property bool multiStream: false
    property int aStreamIndex: 0
    property bool showKeyframes: false
    property bool isGrabbed: false
    property bool grouped: false
    property var markers
    property var keyframeModel
    property int clipStatus: 0
    property int itemType: 0
    property int fadeIn: 0
    property int fadeOut: 0
    property int binId: 0
    property int positionOffset: 0
    property var parentTrack
    property int trackIndex //Index in track repeater
    property int clipId     //Id of the clip in the model
    property int trackId: -1 // Id of the parent track in the model
    property int fakeTid: -1
    property int fakePosition: 0
    property int originalTrackId: -1
    property int originalX: x
    property int originalDuration: clipDuration
    property int lastValidDuration: clipDuration
    property int draggedX: x
    property bool selected: false
    property bool isLocked: parentTrack && parentTrack.isLocked == true
    property bool hasAudio
    property bool canBeAudio
    property bool canBeVideo
    property double speed: 1.0
    property color borderColor: 'black'
    property bool forceReloadThumb
    property bool isComposition: false
    property bool hideClipViews: false
    property var groupTrimData
    property int scrollStart: scrollView.contentX - (clipRoot.modelStart * timeline.scaleFactor)
    property int mouseXPos: mouseArea.mouseX
    width : clipDuration * timeScale
    opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.8 : 1.0

    signal trimmingIn(var clip, real newDuration, var mouse, bool shiftTrim, bool controlTrim)
    signal trimmedIn(var clip, bool shiftTrim, bool controlTrim)
    signal initGroupTrim(var clip)
    signal trimmingOut(var clip, real newDuration, var mouse, bool shiftTrim, bool controlTrim)
    signal trimmedOut(var clip, bool shiftTrim, bool controlTrim)

    onScrollStartChanged: {
        clipRoot.hideClipViews = scrollStart > (clipDuration * timeline.scaleFactor) || scrollStart + scrollView.contentItem.width < 0
    }

    onIsGrabbedChanged: {
        if (clipRoot.isGrabbed) {
            grabItem()
        } else {
            mouseArea.focus = false
        }
    }

    function grabItem() {
        clipRoot.forceActiveFocus()
        mouseArea.focus = true
    }

    function clearAndMove(offset) {
        controller.requestClearSelection()
        controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)
        controller.requestAddToSelection(clipRoot.clipId)
    }

    onClipResourceChanged: {
        if (itemType == ProducerType.Color) {
            color: Qt.darker(getColor(), 1.5)
        }
    }
    ToolTip {
        visible: mouseArea.containsMouse && !dragProxyArea.pressed && !trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse && !fadeInMouseArea.containsMouse && !fadeOutMouseArea.containsMouse && !fadeInMouseArea.drag.active && !fadeOutMouseArea.drag.active && !trimInMouseArea.drag.active && !trimOutMouseArea.drag.active
        delay: 1000
        timeout: 5000
        background: Rectangle {
            color: activePalette.alternateBase
            border.color: activePalette.light
        }
        contentItem: Label {
            color: activePalette.text
            font: miniFont
            text: '%1 (%2-%3)\n%4: %5'.arg(label.text)
                        .arg(timeline.simplifiedTC(clipRoot.inPoint))
                        .arg(timeline.simplifiedTC(clipRoot.outPoint))
                        .arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(clipRoot.clipDuration))
        }
    }

    onKeyframeModelChanged: {
        if (effectRow.keyframecanvas) {
            console.log('keyframe model changed............')
            effectRow.keyframecanvas.requestPaint()
        }
    }

    onClipDurationChanged: {
        width = clipDuration * timeScale;
        if (parentTrack && parentTrack.isAudio && thumbsLoader.item) {
            // Duration changed, we may need a different number of repeaters
            thumbsLoader.item.reload(1)
        }
    }

    onModelStartChanged: {
        x = modelStart * timeScale;
    }

    onFakePositionChanged: {
        x = fakePosition * timeScale;
    }
    onFakeTidChanged: {
        if (clipRoot.fakeTid > -1 && parentTrack) {
            if (clipRoot.parent != dragContainer) {
                var pos = clipRoot.mapToGlobal(clipRoot.x, clipRoot.y);
                clipRoot.parent = dragContainer
                pos = clipRoot.mapFromGlobal(pos.x, pos.y)
                clipRoot.x = pos.x
                clipRoot.y = pos.y
            }
            clipRoot.y = Logic.getTrackById(clipRoot.fakeTid).y
            clipRoot.height = Logic.getTrackById(clipRoot.fakeTid).height
        }
    }

    onForceReloadThumbChanged: {
        // TODO: find a way to force reload of clip thumbs
        if (thumbsLoader.item) {
            thumbsLoader.item.reload(0)
        }
    }

    onTimeScaleChanged: {
        x = modelStart * timeScale;
        width = clipDuration * timeScale;
        labelRect.x = scrollX > modelStart * timeScale ? scrollX - modelStart * timeScale : clipRoot.border.width
    }
    onScrollXChanged: {
        labelRect.x = scrollX > modelStart * timeScale ? scrollX - modelStart * timeScale : clipRoot.border.width
    }

    border.color: selected ? root.selectionColor : grouped ? root.groupColor : borderColor
    border.width: isGrabbed ? 8 : 2

    function updateDrag() {
        var itemPos = mapToItem(tracksContainerArea, 0, 0, clipRoot.width, clipRoot.height)
        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
    }

    function getColor() {
        if (clipStatus == ClipState.Disabled) {
            return 'grey'
        }
        if (itemType == ProducerType.Text) {
            return titleColor
        }
        if (itemType == ProducerType.Image) {
            return imageColor
        }
        if (itemType == ProducerType.SlideShow) {
            return slideshowColor
        }
        if (itemType == ProducerType.Color) {
            var color = clipResource.substring(clipResource.length - 9)
            if (color[0] == '#') {
                return color
            }
            return '#' + color.substring(color.length - 8, color.length - 2)
        }
        return isAudio? root.audioColor : root.videoColor
    }

/*    function reparent(track) {
        console.log('TrackId: ',trackId)
        parent = track
        height = track.height
        parentTrack = track
        trackId = parentTrack.trackId
        console.log('Reparenting clip to Track: ', trackId)
        //generateWaveform()
    }
*/
    property bool noThumbs: (isAudio || itemType == ProducerType.Color || mltService === '')
    property string baseThumbPath: noThumbs ? '' : 'image://thumbnail/' + binId + '/' + documentId + '/#'

    DropArea { //Drop area for clips
        anchors.fill: clipRoot
        keys: 'kdenlive/effect'
        property string dropData
        property string dropSource
        property int dropRow: -1
        onEntered: {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
        }
        onDropped: {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                controller.addClipEffect(clipRoot.clipId, dropData);
            } else {
                controller.copyClipEffect(clipRoot.clipId, dropSource);
            }
            dropSource = ''
            dropRow = -1
            drag.acceptProposedAction
        }
    }
    MouseArea {
        id: mouseArea
        enabled: root.activeTool === 0
        anchors.fill: clipRoot
        acceptedButtons: Qt.RightButton
        hoverEnabled: root.activeTool === 0
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape
        onPressed: {
            root.autoScrolling = false
            root.mainItemId = clipRoot.clipId
            if (mouse.button == Qt.RightButton) {
                if (timeline.selection.indexOf(clipRoot.clipId) == -1) {
                    controller.requestAddToSelection(clipRoot.clipId, true)
                }
                root.mainFrame = Math.round(mouse.x / timeline.scaleFactor)
                root.showClipMenu(clipRoot.clipId)
                root.autoScrolling = timeline.autoScroll
            }
        }
        onReleased: {
            root.autoScrolling = timeline.autoScroll
        }
        Keys.onShortcutOverride: event.accepted = clipRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)
        Keys.onLeftPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true);
        }
        Keys.onRightPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart + offset, true, true, true);
        }
        Keys.onUpPressed: {
            controller.requestClipMove(clipRoot.clipId, controller.getNextTrackId(clipRoot.trackId), clipRoot.modelStart, true, true, true);
        }
        Keys.onDownPressed: {
            controller.requestClipMove(clipRoot.clipId, controller.getPreviousTrackId(clipRoot.trackId), clipRoot.modelStart, true, true, true);
        }
        Keys.onEscapePressed: {
            timeline.grabCurrent()
            //focus = false
        }
        onPositionChanged: {
            var mapped = parentTrack.mapFromItem(clipRoot, mouse.x, mouse.y).x
            root.mousePosChanged(Math.round(mapped / timeline.scaleFactor))
        }
        onEntered: {
            var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
            initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
        }

        onExited: {
            endDrag()
        }
        onWheel: zoomByWheel(wheel)

        Loader {
            // Thumbs container
            id: thumbsLoader
            anchors.fill: parent
            anchors.leftMargin: parentTrack.isAudio ? 0 : clipRoot.border.width
            anchors.rightMargin: parentTrack.isAudio ? 0 : clipRoot.border.width
            anchors.topMargin: clipRoot.border.width
            anchors.bottomMargin: clipRoot.border.width
            clip: true
            asynchronous: true
            visible: status == Loader.Ready
            source: clipRoot.hideClipViews || clipRoot.itemType == 0 || clipRoot.itemType === ProducerType.Color ? "" : parentTrack.isAudio ? (timeline.showAudioThumbnails ? "ClipAudioThumbs.qml" : "") : timeline.showThumbnails ? "ClipThumbs.qml" : ""
        }

        Item {
            // Clipping container
            id: container
            anchors.fill: parent
            anchors.margins: clipRoot.border.width
            //clip: true
            property bool showDetails: (!clipRoot.selected || !effectRow.visible) && container.height > 2.2 * labelRect.height

            Repeater {
                // Clip markers
                model: markers
                delegate:
                Item {
                    anchors.fill: parent
                    visible: markerBase.x >= 0 && markerBase.x < clipRoot.width
                    Rectangle {
                        id: markerBase
                        width: 1
                        height: parent.height
                        x: clipRoot.speed < 0 ? (clipRoot.maxDuration - clipRoot.inPoint) * timeScale + (Math.round(model.frame / clipRoot.speed)) * timeScale - clipRoot.border.width : (Math.round(model.frame / clipRoot.speed) - clipRoot.inPoint) * timeScale - clipRoot.border.width;
                        color: model.color
                    }
                    Rectangle {
                        visible: mlabel.visible
                        opacity: 0.7
                        x: markerBase.x
                        radius: 2
                        width: mlabel.width + 4
                        height: mlabel.height
                        anchors {
                            bottom: parent.verticalCenter
                        }
                        color: model.color
                        MouseArea {
                            z: 10
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onDoubleClicked: timeline.editMarker(clipRoot.clipId, model.frame)
                            onClicked: proxy.position = (clipRoot.x + markerBase.x) / timeline.scaleFactor
                        }
                    }
                    TextMetrics {
                        id: textMetrics
                        font: miniFont
                        text: model.comment
                        elide: root.timeScale > 1 ? Text.ElideNone : Text.ElideRight
                        elideWidth: root.maxLabelWidth
                    }
                    Text {
                        id: mlabel
                        visible: timeline.showMarkers && textMetrics.elideWidth > root.baseUnit
                        text: textMetrics.elidedText
                        font: miniFont
                        x: markerBase.x
                        anchors {
                            bottom: parent.verticalCenter
                            topMargin: 2
                            leftMargin: 2
                        }
                        color: 'white'
                    }
                }
            }

            MouseArea {
                // Left resize handle
                id: trimInMouseArea
                x: -clipRoot.border.width
                height: parent.height
                width: root.baseUnit / 2
                visible: root.activeTool === 0
                enabled: !isLocked && (pressed || clipRoot.width > 3 * width)
                hoverEnabled: true
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor);
                onPressed: {
                    root.autoScrolling = false
                    clipRoot.originalX = clipRoot.x
                    clipRoot.originalDuration = clipDuration
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier
                    if (!shiftTrim && clipRoot.grouped) {
                        clipRoot.initGroupTrim(clipRoot)
                    }
                    trimIn.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    x = -clipRoot.border.width
                    if (sizeChanged) {
                        clipRoot.trimmedIn(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                    }
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var currentFrame = Math.round((clipRoot.x + (x + clipRoot.border.width)) / timeScale)
                        var currentClipPos = clipRoot.modelStart
                        var delta = currentFrame - currentClipPos
                        if (delta !== 0) {
                            if (maxDuration > 0 && delta < -inPoint && !(mouse.modifiers & Qt.ControlModifier)) {
                                delta = -inPoint
                            }
                            var newDuration =  clipDuration - delta
                            sizeChanged = true
                            clipRoot.trimmingIn(clipRoot, newDuration, mouse, shiftTrim, controlTrim)
                        }
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimIn.opacity = 1
                    }
                }
                onExited: {
                    trimIn.opacity = 0
                }
                Rectangle {
                    id: trimIn
                    anchors.left: parent.left
                    width: clipRoot.border.width
                    height: parent.height
                    color: 'lawngreen'
                    opacity: 0
                    Drag.active: trimInMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimInMouseArea.pressed || (root.activeTool === 0 && !mouseArea.drag.active && parent.enabled)

                    ToolTip {
                        visible: trimInMouseArea.containsMouse && !trimInMouseArea.pressed
                        delay: 1000
                        timeout: 5000
                        background: Rectangle {
                            color: activePalette.alternateBase
                            border.color: activePalette.light
                        }
                        contentItem: Label {
                            color: activePalette.text
                            font: miniFont
                            text: i18n("In:%1\nPosition:%2", timeline.simplifiedTC(clipRoot.inPoint),timeline.simplifiedTC(clipRoot.modelStart))
                        }
                    }
                }
            }

            MouseArea {
                // Right resize handle
                id: trimOutMouseArea
                anchors.right: parent.right
                anchors.rightMargin: -clipRoot.border.width
                anchors.top: parent.top
                height: parent.height
                width: root.baseUnit / 2
                hoverEnabled: true
                visible: root.activeTool === 0
                enabled: !isLocked && (pressed || clipRoot.width > 3 * width)
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor);
                drag.target: trimOutMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false

                onPressed: {
                    root.autoScrolling = false
                    clipRoot.originalDuration = clipDuration
                    anchors.right = undefined
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier
                    if (!shiftTrim && clipRoot.grouped) {
                        clipRoot.initGroupTrim(clipRoot)
                    }
                    trimOut.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    anchors.right = parent.right
                    if (sizeChanged) {
                        clipRoot.trimmedOut(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                    }
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newDuration = Math.round((x + width) / timeScale)
                        if (maxDuration > 0 && (newDuration > maxDuration - inPoint) && !(mouse.modifiers & Qt.ControlModifier)) {
                            newDuration = maxDuration - inPoint
                        }
                        if (newDuration != clipDuration) {
                            sizeChanged = true
                            clipRoot.trimmingOut(clipRoot, newDuration, mouse, shiftTrim, controlTrim)
                        }
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimOut.opacity = 1
                    }
                }
                onExited: trimOut.opacity = 0
                ToolTip {
                    visible: trimOutMouseArea.containsMouse && !trimOutMouseArea.pressed
                    delay: 1000
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        font: miniFont
                        text: i18n("Out: ") + timeline.simplifiedTC(clipRoot.outPoint)
                    }
                }
                Rectangle {
                    id: trimOut
                    anchors.right: parent.right
                    width: clipRoot.border.width
                    height: parent.height
                    color: 'red'
                    opacity: 0
                    Drag.active: trimOutMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimOutMouseArea.pressed || (root.activeTool === 0 && !mouseArea.drag.active && parent.enabled)
                }
            }

            TimelineTriangle {
                // Green fade in triangle
                id: fadeInTriangle
                fillColor: 'green'
                width: Math.min(clipRoot.fadeIn * timeScale, container.width)
                height: parent.height
                anchors.left: parent.left
                anchors.top: parent.top
                opacity: 0.4
            }

            TimelineTriangle {
                // Red fade out triangle
                id: fadeOutCanvas
                fillColor: 'red'
                width: Math.min(clipRoot.fadeOut * timeScale, container.width)
                height: parent.height
                anchors.right: parent.right
                anchors.top: parent.top
                opacity: 0.4
                transform: Scale { xScale: -1; origin.x: fadeOutCanvas.width / 2}
            }

            Item {
                // Clipping container for clip names
                anchors.fill: parent
                clip: true
                Rectangle {
                    // Clip name background
                    id: labelRect
                    color: clipRoot.selected ? 'darkred' : '#66000000'
                    y: 0
                    width: label.width + 2 * clipRoot.border.width
                    height: label.height
                    visible: clipRoot.width > width / 2
                    Text {
                        // Clip name text
                        id: label
                        property string clipNameString: (clipRoot.isAudio && clipRoot.multiStream) ? ((clipRoot.audioStream > 10000 ? 'Merged' : clipRoot.aStreamIndex) + '|' + clipName ) : clipName
                        text: (clipRoot.speed != 1.0 ? ('[' + Math.round(clipRoot.speed*100) + '%] ') : '') + clipNameString
                        font: miniFont
                        anchors {
                            top: labelRect.top
                            left: labelRect.left
                            leftMargin: clipRoot.border.width
                        }
                        color: 'white'
                        //style: Text.Outline
                        //styleColor: 'black'
                    }
                }

                Rectangle {
                    // Offset info
                    id: offsetRect
                    color: 'darkgreen'
                    width: offsetLabel.width + radius
                    height: offsetLabel.height
                    radius: height/3
                    x: labelRect.width + 4
                    y: 2
                    visible: labelRect.visible && positionOffset != 0
                    MouseArea {
                        id: offsetArea
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        anchors.fill: parent
                        onClicked: {
                            clearAndMove(positionOffset)
                        }
                        ToolTip {
                            visible: offsetArea.containsMouse
                            delay: 1000
                            timeout: 5000
                            background: Rectangle {
                                color: activePalette.alternateBase
                                border.color: activePalette.light
                            }
                            contentItem: Label {
                                color: activePalette.text
                                font: miniFont
                                text: positionOffset < 0 ? i18n("Offset: -%1", timeline.simplifiedTC(-positionOffset)) : i18n("Offset: %1", timeline.simplifiedTC(positionOffset))
                            }
                        }
                        Text {
                            id: offsetLabel
                            text: positionOffset
                            font: miniFont
                            anchors {
                                horizontalCenter: parent.horizontalCenter
                                topMargin: 1
                                leftMargin: 1
                            }
                            color: 'white'
                            style: Text.Outline
                            styleColor: 'black'
                        }
                    }
                }

                Rectangle {
                    // effect names background
                    id: effectsRect
                    color: '#555555'
                    width: effectLabel.width + 2
                    height: effectLabel.height
                    x: labelRect.x
                    anchors.top: labelRect.bottom
                    visible: labelRect.visible && clipRoot.effectNames != '' && container.showDetails
                    Text {
                        // Effect names text
                        id: effectLabel
                        text: clipRoot.effectNames
                        font: miniFont
                        visible: effectsRect.visible
                        anchors {
                            top: effectsRect.top
                            left: effectsRect.left
                            leftMargin: 1
                            // + ((isAudio || !settings.timelineShowThumbnails) ? 0 : inThumbnail.width) + 1
                        }
                        color: 'white'
                        //style: Text.Outline
                        styleColor: 'black'
                    }
               }
               Rectangle{
                    //proxy 
                    id:proxyRect
                    color: '#fdbc4b'
                    width: labelRect.height
                    height: labelRect.height
                    x: labelRect.x
                    anchors.top: labelRect.top
                    anchors.left: labelRect.right
                    visible: clipRoot.isProxy && !clipRoot.isAudio
                    Text {
                        // Proxy P
                        id: proxyLabel
                        text: "P"
                        font.pointSize: root.fontUnit +1
                        visible: proxyRect.visible
                        anchors {
                            top: proxyRect.top
                            left: proxyRect.left
                            leftMargin: (labelRect.height-proxyLabel.width)/2
                            topMargin: (labelRect.height-proxyLabel.height)/2
                        }
                        color: 'black'
                        styleColor: 'black'
                    }
                }
            }

            KeyframeView {
                id: effectRow
                clip: true
                anchors.fill: parent
                visible: clipRoot.showKeyframes && clipRoot.keyframeModel
                selected: clipRoot.selected
                inPoint: clipRoot.inPoint
                outPoint: clipRoot.outPoint
                masterObject: clipRoot
                kfrModel: clipRoot.hideClipViews ? 0 : clipRoot.keyframeModel
            }
        }

        states: [
            State {
                name: 'locked'
                when: isLocked
                PropertyChanges {
                    target: clipRoot
                    color: root.lockedColor
                    opacity: 0.8
                    z: 0
                }
            },
            State {
                name: 'normal'
                when: clipRoot.selected === false
                PropertyChanges {
                    target: clipRoot
                    color: Qt.darker(getColor(), 1.5)
                    z: 0
                }
            },
            State {
                name: 'selected'
                when: clipRoot.selected === true
                PropertyChanges {
                    target: clipRoot
                    color: getColor()
                    z: 3
                }
            }
        ]

        MouseArea {
            // Add start composition area
            id: compInArea
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: Math.min(root.baseUnit, container.height / 3)
            height: width
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            visible: !clipRoot.isAudio
            enabled: !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId && compositionIn.visible
            onPressed: {
                timeline.addCompositionToClip('', clipRoot.clipId, 0)
            }
            ToolTip {
                visible: compInArea.containsMouse && !dragProxyArea.pressed
                delay: 1000
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    font: miniFont
                    text: i18n("Click to add composition")
                }
            }
            Rectangle {
                // Start composition box
                id: compositionIn
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: compInArea.containsMouse ? parent.width : 5
                height: width
                radius: width / 2
                visible: clipRoot.width > 4 * parent.width && mouseArea.containsMouse && !dragProxyArea.pressed
                color: Qt.darker('mediumpurple')
                border.width: 3
                border.color: 'mediumpurple'
                Behavior on width { NumberAnimation { duration: 100 } }
            }
        }

        MouseArea {
            // Add end composition area
            id: compOutArea
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Math.min(root.baseUnit, container.height / 3)
            height: width
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            enabled: !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId && compositionOut.visible
            visible: !clipRoot.isAudio
            onPressed: {
                timeline.addCompositionToClip('', clipRoot.clipId, clipRoot.clipDuration - 1)
            }
            ToolTip {
                visible: compOutArea.containsMouse && !dragProxyArea.pressed
                delay: 1000
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    font: miniFont
                    text: i18n("Click to add composition")
                }
            }
            Rectangle {
                // End composition box
                id: compositionOut
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: compOutArea.containsMouse ? parent.height : 5
                height: width
                radius: width / 2
                visible: clipRoot.width > 4 * parent.width && mouseArea.containsMouse && !dragProxyArea.pressed
                color: Qt.darker('mediumpurple')
                border.width: 3
                border.color: 'mediumpurple'
                Behavior on width { NumberAnimation { duration: 100 } }
            }
        }

        MouseArea {
            // Fade out drag zone
            id: fadeOutMouseArea
            anchors.right: parent.right
            anchors.rightMargin: clipRoot.fadeOut <= 0 ? 0 : fadeOutCanvas.width - width / 2
            anchors.top: parent.top
            width: Math.min(root.baseUnit, container.height / 3)
            height: width
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            drag.target: fadeOutMouseArea
            drag.axis: Drag.XAxis
            drag.minimumX: - Math.ceil(width / 2)
            drag.maximumX: container.width + Math.ceil(width / 4)
            visible: clipRoot.width > 3 * width && mouseArea.containsMouse && !dragProxyArea.pressed
            property int startFadeOut
            property int lastDuration: -1
            drag.smoothed: false
            onClicked: {
                if (clipRoot.fadeOut == 0) {
                    timeline.adjustFade(clipRoot.clipId, 'fadeout', 0, -2)
                }
            }
            onPressed: {
                root.autoScrolling = false
                startFadeOut = clipRoot.fadeOut
                anchors.right = undefined
                fadeOutCanvas.opacity = 0.6
            }
            onReleased: {
                fadeOutCanvas.opacity = 0.4
                root.autoScrolling = timeline.autoScroll
                anchors.right = parent.right
                var duration = clipRoot.fadeOut
                timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, startFadeOut)
                bubbleHelp.hide()
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = clipRoot.clipDuration - Math.floor((x + width / 2 - clipRoot.border.width)/ timeScale)
                    var duration = Math.max(0, delta)
                    duration = Math.min(duration, clipRoot.clipDuration)
                    if (lastDuration != duration) {
                        lastDuration = duration
                        timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        var s = timeline.simplifiedTC(clipRoot.fadeOut)
                        bubbleHelp.show(clipRoot.x + x, parentTrack.y + parentTrack.height, s)
                    }
                }
            }
            Rectangle {
                id: fadeOutControl
                anchors.top: parent.top
                anchors.right: clipRoot.fadeOut > 0 ? undefined : parent.right
                anchors.horizontalCenter: clipRoot.fadeOut > 0 ? parent.horizontalCenter : undefined
                width: fadeOutMouseArea.containsMouse || Drag.active ? parent.width : 5
                height: width
                radius: width / 2
                color: 'darkred'
                border.width: 3
                border.color: 'red'
                enabled: !isLocked && !dragProxy.isComposition
                Drag.active: fadeOutMouseArea.drag.active
                Behavior on width { NumberAnimation { duration: 100 } }
                Rectangle {
                    id: fadeOutMarker
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: 'red'
                    height: container.height
                    width: 1
                    visible : clipRoot.fadeOut > 0 && (fadeOutMouseArea.containsMouse || fadeOutMouseArea.drag.active)
                }
                ToolTip {
                    visible: clipRoot.fadeOut > 0 && fadeOutMouseArea.containsMouse && !fadeOutMouseArea.drag.active
                    delay: 1000
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        font: miniFont
                        text: '%1: %2'.arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(clipRoot.fadeOut))
                    }
                }
            }
        }

        MouseArea {
            // Fade in drag zone
            id: fadeInMouseArea
            anchors.left: container.left
            anchors.leftMargin: clipRoot.fadeIn <= 0 ? 0 : (fadeInTriangle.width - width / 2)
            anchors.top: parent.top
            width: Math.min(root.baseUnit, container.height / 3)
            height: width
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            drag.target: fadeInMouseArea
            drag.minimumX: - Math.ceil(width / 2)
            drag.maximumX: container.width - width / 2
            drag.axis: Drag.XAxis
            drag.smoothed: false
            property int startFadeIn
            visible: clipRoot.width > 3 * width && mouseArea.containsMouse && !dragProxyArea.pressed
            onClicked: {
                if (clipRoot.fadeIn == 0) {
                    timeline.adjustFade(clipRoot.clipId, 'fadein', 0, -2)
                }
            }
            onPressed: {
                root.autoScrolling = false
                startFadeIn = clipRoot.fadeIn
                anchors.left = undefined
                fadeInTriangle.opacity = 0.6
                // parentTrack.clipSelected(clipRoot, parentTrack) TODO
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
                fadeInTriangle.opacity = 0.4
                timeline.adjustFade(clipRoot.clipId, 'fadein', clipRoot.fadeIn, startFadeIn)
                bubbleHelp.hide()
                anchors.left = container.left
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = Math.round((x + width / 2) / timeScale)
                    var duration = Math.max(0, delta)
                    duration = Math.min(duration, clipRoot.clipDuration - 1)
                    if (duration != clipRoot.fadeIn) {
                        timeline.adjustFade(clipRoot.clipId, 'fadein', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        var s = timeline.simplifiedTC(clipRoot.fadeIn)
                        bubbleHelp.show(clipRoot.x + x, parentTrack.y + parentTrack.height, s)
                    }
                }
            }
            Rectangle {
                id: fadeInControl
                anchors.top: parent.top
                anchors.left: clipRoot.fadeIn > 0 ? undefined : parent.left
                anchors.horizontalCenter: clipRoot.fadeIn > 0 ? parent.horizontalCenter : undefined
                width: fadeInMouseArea.containsMouse || Drag.active ? parent.width : 5
                height: width
                radius: width / 2
                color: 'green'
                border.width: 3
                border.color: '#FF66FFFF'
                enabled: !isLocked && !dragProxy.isComposition
                Drag.active: fadeInMouseArea.drag.active
                Behavior on width { NumberAnimation { duration: 100 } }
                Rectangle {
                    id: fadeInMarker
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: '#FF66FFFF'
                    height: container.height
                    width: 1
                    visible : clipRoot.fadeIn > 0 && (fadeInMouseArea.containsMouse || fadeInMouseArea.drag.active)
                }
                ToolTip {
                    visible: clipRoot.fadeIn > 0 && fadeInMouseArea.containsMouse && !fadeInMouseArea.drag.active
                    delay: 1000
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        font: miniFont
                        text: '%1: %2'.arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(clipRoot.fadeIn))
                    }
                }
            }
        }
    }
}
