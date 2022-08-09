/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import Kdenlive.Controls 1.0
import QtQuick.Window 2.15
import QtQml.Models 2.15
import QtQml 2.15

import 'Timeline.js' as Logic
import com.enums 1.0

Rectangle {
    id: clipRoot
    property real timeScale: 1
    property string clipName: ''
    property string tagColor: ''
    property string clipResource: ''
    property string mltService: ''
    property string effectNames
    property int modelStart
    property int mixDuration: 0
    property int mixCut: 0
    property real scrollX: 0
    property int inPoint: 0
    property int outPoint: 0
    property int clipDuration: 0
    property int maxDuration: 0
    property bool isAudio: false
    property bool timeremap: false
    property int audioChannels
    property int audioStream: -1
    property bool multiStream: false
    property int aStreamIndex: 0
    property bool showKeyframes: false
    property bool isGrabbed: false
    property bool grouped: false
    property var markers
    property var keyframeModel
    property int clipState: 0
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
    property double xIntegerOffset: 0
    property bool selected: false
    property bool isLocked: parentTrack && parentTrack.isLocked === true
    property bool hasAudio
    property bool canBeAudio
    property bool canBeVideo
    property double speed: 1.0
    property color borderColor: "#000000"
    property string clipThumbId
    property bool forceReloadAudioThumb
    property bool isComposition: false
    property bool hideClipViews: scrollStart > (clipDuration * timeline.scaleFactor) || scrollStart + scrollView.width < 0 || clipRoot.width < root.minClipWidthForViews
    property int slipOffset: boundValue(outPoint - maxDuration + 1, trimmingOffset, inPoint)
    property int scrollStart: scrollView.contentX - (clipRoot.modelStart * timeline.scaleFactor)
    property int mouseXPos: mouseArea.mouseX
    width : Math.round(clipDuration * timeScale)
    opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.8 : 1.0

    signal trimmingIn(var clip, real newDuration, bool shiftTrim, bool controlTrim)
    signal trimmedIn(var clip, bool shiftTrim, bool controlTrim)
    signal initGroupTrim(var clip)
    signal trimmingOut(var clip, real newDuration, bool shiftTrim, bool controlTrim)
    signal trimmedOut(var clip, bool shiftTrim, bool controlTrim)

    onScrollStartChanged: {
        if (!clipRoot.hideClipViews && clipRoot.width > scrollView.width) {
            if (effectRow.item && effectRow.item.kfrCanvas) {
                effectRow.item.kfrCanvas.requestPaint()
            }
        }
    }

    onIsGrabbedChanged: {
        if (clipRoot.isGrabbed) {
            grabItem()
        } else {
            timeline.showToolTip()
            mouseArea.focus = false
        }
    }

    function itemHeight() {
        return clipRoot.height
    }


    function boundValue(min, val, max) {
        return Math.max(min, Math.min(val, max))
    }

    function grabItem() {
        clipRoot.forceActiveFocus()
        mouseArea.focus = true
    }

    function resetSelection() {
        if (effectRow.visible) {
            effectRow.item.resetSelection()
        }
    }

    function clearAndMove(offset) {
        controller.requestClearSelection()
        controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)
        controller.requestAddToSelection(clipRoot.clipId)
    }

    function doesContainMouse(pnt) {
        return pnt.x > 0.5 && pnt.x < clipRoot.width
    }

    onClipResourceChanged: {
        if (itemType === ProducerType.Color) {
            color: Qt.darker(getColor(), 1.5)
        }
    }

    /*onKeyframeModelChanged: {
        if (effectRow.item && effectRow.item.kfrCanvas) {
            effectRow.item.kfrCanvas.requestPaint()
        }
    }*/

    onClipDurationChanged: {
        width = clipDuration * timeScale
        if (parentTrack && parentTrack.isAudio && thumbsLoader.item) {
            // Duration changed, we may need a different number of repeaters
            thumbsLoader.item.reload(1)
        }
    }

    onModelStartChanged: {
        x = modelStart * timeScale
        xIntegerOffset = Math.ceil(x) - x
    }

    onFakePositionChanged: {
        x = fakePosition * timeScale
        xIntegerOffset = Math.ceil(x) - x
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
        } else {
            clipRoot.height = Qt.binding(function () {
                return parentTrack.height
            })
        }
    }

    onForceReloadAudioThumbChanged: {
        // TODO: find a way to force reload of clip thumbs
        if (!isAudio) {
            return;
        }
        if (thumbsLoader.item) {
            thumbsLoader.item.reload(0)
        }
    }

    onTimeScaleChanged: {
        x = modelStart * clipRoot.timeScale;
        xIntegerOffset = Math.ceil(x) - x
        width = clipDuration * clipRoot.timeScale;
        updateLabelOffset()
        if (!clipRoot.hideClipViews) {
            if (effectRow.item && effectRow.item.kfrCanvas) {
                effectRow.item.kfrCanvas.requestPaint()
            }
        }
    }
    onScrollXChanged: {
        updateLabelOffset()
        if (isAudio && thumbsLoader.item) {
            thumbsLoader.item.reload(1)
        }
    }
    
    function updateLabelOffset()
    {
        labelRect.anchors.leftMargin = scrollX > modelStart * clipRoot.timeScale ? scrollX - modelStart * clipRoot.timeScale + (clipRoot.timeremap ? labelRect.height : 0) : clipRoot.timeremap ? labelRect.height : 0
    }

    /*border.color: (clipStatus === ClipStatus.StatusMissing || ClipStatus === ClipStatus.StatusWaiting || clipStatus === ClipStatus.StatusDeleting) ? "#ff0000" : selected ? root.selectionColor : grouped ? root.groupColor : borderColor
    border.width: isGrabbed ? 8 : 2*/

    function updateDrag() {
        var itemPos = mapToItem(tracksContainerArea, 0, 0, clipRoot.width, clipRoot.height)
        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
    }
    
    function showClipInfo() {
        var text = i18n("%1 (%2-%3), Position: %4, Duration: %5".arg(clipRoot.clipName)
                        .arg(timeline.simplifiedTC(clipRoot.inPoint))
                        .arg(timeline.simplifiedTC(clipRoot.outPoint))
                        .arg(timeline.simplifiedTC(clipRoot.modelStart))
                        .arg(timeline.simplifiedTC(clipRoot.clipDuration)))
        timeline.showToolTip(text)
    }

    function getColor() {
        if (clipRoot.clipState === ClipState.Disabled) {
            return '#888'
        }
        if (clipRoot.tagColor) {
            return clipRoot.tagColor
        }
        if (itemType === ProducerType.Text) {
            return titleColor
        }
        if (itemType === ProducerType.Image) {
            return imageColor
        }
        if (itemType === ProducerType.SlideShow) {
            return slideshowColor
        }
        if (itemType === ProducerType.Color) {
            var color = clipResource.substring(clipResource.length - 9)
            if (color[0] === '#') {
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
    property bool noThumbs: (isAudio || itemType === ProducerType.Color || mltService === '')
    property string baseThumbPath: noThumbs ? '' : 'image://thumbnail/' + clipThumbId

    DropArea { //Drop area for clips
        anchors.fill: clipRoot
        keys: 'kdenlive/effect'
        property string dropData
        property string dropSource
        property int dropRow: -1
        onEntered: {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
            updateDrag()
        }
        onDropped: {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                controller.addClipEffect(clipRoot.clipId, dropData)
                if (proxy.seekOnDrop() && (proxy.position < clipRoot.modelStart || proxy.position > clipRoot.modelStart + clipRoot.clipDuration)) {
                    // If timeline cursor is not inside clip, seek to drop position
                    proxy.position = clipRoot.modelStart + drag.x / timeScale
                }
            } else {
                controller.copyClipEffect(clipRoot.clipId, dropSource)
            }
            dropSource = ''
            dropRow = -1
            drag.acceptProposedAction
            root.regainFocus(mapToItem(root, drag.x, drag.y))
            //console.log('KFR VIEW VISIBLE: ', effectRow.visible, ', SOURCE: ', effectRow.source, '\n HIDEVIEW:', clipRoot.hideClipViews<<', UNDEFINED: ', (clipRoot.keyframeModel == undefined))
        }
        onExited: {
            root.endDrag()
        }
    }
    MouseArea {
        id: mouseArea
        enabled: root.activeTool === ProjectTool.SelectTool || root.activeTool === ProjectTool.SlipTool || root.activeTool === ProjectTool.RippleTool
        anchors.fill: clipRoot
        acceptedButtons: Qt.RightButton
        hoverEnabled: root.activeTool === ProjectTool.SelectTool || root.activeTool === ProjectTool.RippleTool
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape
        property bool shiftSlip: false
        property bool controlSlip: false
        onPressed: {
            root.autoScrolling = false
            root.mainItemId = clipRoot.clipId
            if (mouse.button == Qt.RightButton) {
                if (timeline.selection.indexOf(clipRoot.clipId) === -1) {
                    controller.requestAddToSelection(clipRoot.clipId, true)
                }
                root.clickFrame = Math.round(mouse.x / timeline.scaleFactor)
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
            while((clipRoot.modelStart >= offset) && !controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)) {
                offset++;
            }
            timeline.showToolTip(i18n("Position: %1", timeline.simplifiedTC(clipRoot.modelStart)));
        }
        Keys.onRightPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            while(!controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart + offset, true, true, true)) {
                offset++;
            }
            timeline.showToolTip(i18n("Position: %1", timeline.simplifiedTC(clipRoot.modelStart)));
        }
        Keys.onUpPressed: {
            var nextTrack = controller.getNextTrackId(clipRoot.trackId);
            while(!controller.requestClipMove(clipRoot.clipId, nextTrack, clipRoot.modelStart, true, true, true) && nextTrack !== controller.getNextTrackId(nextTrack)) {
                nextTrack = controller.getNextTrackId(nextTrack);
            }
        }
        Keys.onDownPressed: {
            var previousTrack = controller.getPreviousTrackId(clipRoot.trackId);
            while(!controller.requestClipMove(clipRoot.clipId, previousTrack, clipRoot.modelStart, true, true, true) && previousTrack !== controller.getPreviousTrackId(previousTrack)) {
                previousTrack = controller.getPreviousTrackId(previousTrack);
            }
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
            showClipInfo()
        }

        onExited: {
            root.endDrag()
            if (!trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse && !compInArea.containsMouse && !compOutArea.containsMouse) {
                timeline.showToolTip()
            }
        }
        onWheel: zoomByWheel(wheel)

        Loader {
            // Thumbs container
            id: thumbsLoader
            anchors.fill: parent
            anchors.leftMargin: parentTrack.isAudio ? xIntegerOffset : itemBorder.border.width + mixContainer.width
            anchors.rightMargin: parentTrack.isAudio ? clipRoot.width - Math.floor(clipRoot.width) : itemBorder.border.width
            anchors.topMargin: itemBorder.border.width
            anchors.bottomMargin: itemBorder.border.width
            //clip: true
            asynchronous: true
            visible: status == Loader.Ready
            source: (clipRoot.hideClipViews || clipRoot.itemType == 0 || clipRoot.itemType === ProducerType.Color) ? "" : parentTrack.isAudio ? (timeline.showAudioThumbnails ? "ClipAudioThumbs.qml" : "") : timeline.showThumbnails ? "ClipThumbs.qml" : ""
            onStatusChanged: {
                if (!parentTrack.isAudio && thumbsLoader.item) {
                    thumbsLoader.item.initialSpeed = clipRoot.speed
                }
            }
        }

        Rectangle {
            // Border rectangle
            color: 'transparent'
            id: itemBorder
            anchors.fill: parent
            border.color: (clipStatus === ClipStatus.StatusMissing || ClipStatus === ClipStatus.StatusWaiting || clipStatus === ClipStatus.StatusDeleting) ? "#ff0000" : clipRoot.selected ? root.selectionColor : grouped ? root.groupColor : borderColor
            border.width: isGrabbed ? 8 : 2
        }

        Item {
            // Clipping container
            id: container
            anchors.fill: parent
            anchors.margins: itemBorder.border.width
            //clip: true
            property bool showDetails: (!clipRoot.selected || !effectRow.visible) && container.height > 2.2 * labelRect.height
            
            Item {
                // Mix indicator
                id: mixContainer
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: clipRoot.mixDuration * clipRoot.timeScale
                
                Rectangle {
                    id: mixBackground
                    property double mixPos: mixBackground.width - clipRoot.mixCut * clipRoot.timeScale
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    visible: clipRoot.mixDuration > 0
                    color: "mediumpurple"
                    Loader {
                        id: shapeLoader
                        source: clipRoot.mixDuration > 0 ? "MixShape.qml" : ""
                        property bool valid: item !== null
                    }

                    opacity: mixArea.containsMouse || trimInMixArea.pressed || trimInMixArea.containsMouse || root.selectedMix == clipRoot.clipId ? 1 : 0.7
                    border.color: root.selectedMix == clipRoot.clipId ? root.selectionColor : "transparent"
                    border.width: 2
                    MouseArea {
                        // Mix click mouse area
                        id: mixArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPressed: {
                            controller.requestMixSelection(clipRoot.clipId);
                        }
                        onEntered: {
                            var text = i18n("Mix duration: %1, Cut at: %2".arg(timeline.simplifiedTC(clipRoot.mixDuration))
                            .arg(timeline.simplifiedTC(clipRoot.mixDuration - clipRoot.mixCut)))
                            timeline.showToolTip(text)
                        }
                    }
                    Rectangle {
                        id: mixCutPos
                        anchors.right: parent.right
                        anchors.rightMargin: clipRoot.mixCut * clipRoot.timeScale
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: "navy"
                    }
                    MouseArea {
                        // Right mix resize handle
                        id: trimInMixArea
                        anchors.left: parent.left
                        anchors.leftMargin: clipRoot.mixDuration * clipRoot.timeScale
                        height: parent.height
                        width: root.baseUnit / 2
                        visible: root.activeTool === ProjectTool.SelectTool
                        property int previousMix
                        enabled: !isLocked && (pressed || clipRoot.width > 3 * width)
                        hoverEnabled: true
                        drag.target: trimInMixArea
                        drag.axis: Drag.XAxis
                        drag.smoothed: false
                        drag.maximumX: clipRoot.width
                        drag.minimumX: (clipRoot.mixDuration - clipRoot.mixCut) * clipRoot.timeScale
                        property bool sizeChanged: false
                        cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor)
                        onPressed: {
                            root.trimInProgress = true;
                            previousMix = clipRoot.mixDuration
                            root.autoScrolling = false
                            mixOut.color = 'red'
                            anchors.left = undefined
                            parent.anchors.right = undefined
                            mixCutPos.anchors.right = undefined
                        }
                        onReleased: {
                            root.autoScrolling = timeline.autoScroll
                            if (sizeChanged) {
                                controller.resizeStartMix(clipRoot.clipId, Math.round(Math.max(0, x) / clipRoot.timeScale), mouse.modifiers & Qt.ShiftModifier)
                                sizeChanged = false
                            }
                            anchors.left = parent.left
                            parent.anchors.right = mixContainer.right
                            mixBackground.anchors.bottom = mixContainer.bottom
                            mixOut.color = itemBorder.border.color
                            mixCutPos.anchors.right = mixCutPos.parent.right
                            root.trimInProgress = false;
                        }
                        onPositionChanged: {
                            if (mouse.buttons === Qt.LeftButton) {
                                var currentFrame = Math.round(x / clipRoot.timeScale)
                                if (currentFrame != previousMix) {
                                    parent.width = currentFrame * clipRoot.timeScale
                                    sizeChanged = true
                                    if (currentFrame > previousMix) {
                                        timeline.showToolTip(i18n("+%1, Mix duration: %2", timeline.simplifiedTC(currentFrame - previousMix), timeline.simplifiedTC(currentFrame)))
                                    } else {
                                        timeline.showToolTip(i18n("-%1, Mix duration: %2", timeline.simplifiedTC(previousMix - currentFrame), timeline.simplifiedTC(currentFrame)))
                                    }
                                } else {
                                    timeline.showToolTip(i18n("Mix duration: %1", timeline.simplifiedTC(currentFrame)))
                                }
                                if (x < mixCutPos.x) {
                                    // This will delete the mix
                                    mixBackground.anchors.bottom = mixContainer.top
                                } else {
                                    mixBackground.anchors.bottom = mixContainer.bottom
                                }
                            }
                        }
                        onEntered: {
                            if (!pressed) {
                                mixOut.color = 'red'
                                timeline.showToolTip(i18n("Mix duration: %1", timeline.simplifiedTC(clipRoot.mixDuration)))
                            }
                        }
                        onExited: {
                            if (!pressed) {
                                mixOut.color = itemBorder.border.color
                                if (!mouseArea.containsMouse) {
                                    timeline.showToolTip()
                                } else {
                                    clipRoot.showClipInfo()
                                }
                            }
                        }
                        Rectangle {
                            id: mixOut
                            width: itemBorder.border.width
                            height: mixContainer.height
                            color: itemBorder.border.color
                            Drag.active: trimInMixArea.drag.active
                            Drag.proposedAction: Qt.MoveAction
                            visible: trimInMixArea.pressed || (root.activeTool === ProjectTool.SelectTool && !mouseArea.drag.active && parent.enabled)
                        }
                    }
                }
                
            }

            Repeater {
                // Clip markers
                model: markers
                delegate:
                Item {
                    visible: markerBase.x >= 0 && markerBase.x < clipRoot.width
                    Rectangle {
                        id: markerBase
                        width: 1
                        height: container.height
                        x: clipRoot.speed < 0
                           ? (clipRoot.maxDuration - clipRoot.inPoint) * clipRoot.timeScale + (Math.round(model.frame / clipRoot.speed)) * clipRoot.timeScale - itemBorder.border.width
                           : (Math.round(model.frame / clipRoot.speed) - clipRoot.inPoint) * clipRoot.timeScale - itemBorder.border.width;
                        color: model.color
                        ToolTip.visible: markerArea.containsMouse
                        ToolTip.text: textMetrics.text
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                    }
                    Rectangle {
                        visible: mlabel.visible
                        opacity: 0.7
                        x: markerBase.x
                        radius: 2
                        width: mlabel.width + 4
                        height: mlabel.height
                        y: mlabel.y
                        color: model.color
                        MouseArea {
                            z: 10
                            id: markerArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onDoubleClicked: timeline.editMarker(clipRoot.clipId, model.frame)
                            onClicked: proxy.position = clipRoot.modelStart + (clipRoot.speed < 0
                                                                               ? (clipRoot.maxDuration - clipRoot.inPoint) * clipRoot.timeScale + (Math.round(model.frame / clipRoot.speed))
                                                                               : (Math.round(model.frame / clipRoot.speed) - clipRoot.inPoint))
                        }
                    }
                    TextMetrics {
                        id: textMetrics
                        font: miniFont
                        text: model.comment
                        elide: clipRoot.timeScale > 1 ? Text.ElideNone : Text.ElideRight
                        elideWidth: root.maxLabelWidth
                    }
                    Text {
                        id: mlabel
                        visible: timeline.showMarkers && textMetrics.elideWidth > root.baseUnit && height < container.height && (markerBase.x > mlabel.width || container.height > 2 * height)
                        text: textMetrics.elidedText
                        font: miniFont
                        x: markerBase.x + 1
                        y: Math.min(label.height, container.height - height)
                        color: 'white'
                    }
                }
            }

            MouseArea {
                // Left resize handle
                id: trimInMouseArea
                x: -itemBorder.border.width
                height: parent.height
                width: root.baseUnit / 2
                visible: root.activeTool === ProjectTool.SelectTool || (root.activeTool === ProjectTool.RippleTool && clipRoot.mixDuration <= 0 && !controller.hasClipEndMix(clipRoot.clipId))
                enabled: !isLocked && (pressed || clipRoot.width > 3 * width) && clipRoot.clipId == dragProxy.draggedItem
                hoverEnabled: true
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                onPressed: {
                    root.autoScrolling = false
                    root.trimInProgress = true;
                    clipRoot.originalX = clipRoot.x
                    clipRoot.originalDuration = clipDuration
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier
                    if (!shiftTrim && clipRoot.grouped) {
                        clipRoot.initGroupTrim(clipRoot)
                    }
                    if (root.activeTool === ProjectTool.RippleTool) {
                        timeline.requestStartTrimmingMode(clipRoot.clipId, false, false);
                    }
                    trimIn.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    x = -itemBorder.border.width
                    if (sizeChanged) {
                        clipRoot.trimmedIn(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                        if (!controlTrim && root.activeTool !== ProjectTool.RippleTool) {
                            updateDrag()
                        } else {
                            root.endDrag()
                        }
                    } else {
                        if (root.activeTool === ProjectTool.RippleTool) {
                            timeline.requestEndTrimmingMode();
                        }
                        root.groupTrimData = undefined
                    }
                    root.trimInProgress = false;
                }
                onDoubleClicked: {
                    timeline.mixClip(clipRoot.clipId, -1)
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var currentFrame = Math.round((clipRoot.x + (x + itemBorder.border.width)) / clipRoot.timeScale)
                        var currentClipPos = clipRoot.modelStart
                        var delta = currentFrame - currentClipPos
                        if (delta !== 0) {
                            if (delta > 0 && (clipRoot.mixDuration > 0 && clipRoot.mixDuration - clipRoot.mixCut - delta < (clipRoot.mixCut == 0 ? 1 : 0))) {
                                if (clipRoot.mixCut == 0 && clipRoot.mixDuration > 1) {
                                    delta = clipRoot.mixDuration - clipRoot.mixCut - 1
                                } else if (clipRoot.mixCut > 0 && clipRoot.mixDuration > clipRoot.mixCut) {
                                    delta = clipRoot.mixDuration - clipRoot.mixCut
                                } else {
                                    return
                                }
                            }
                            var newDuration = 0;
                            if (root.activeTool === ProjectTool.RippleTool) {
                                newDuration = clipRoot.originalDuration - delta
                            } else {
                                if (maxDuration > 0 && delta < -inPoint && !(mouse.modifiers & Qt.ControlModifier)) {
                                    delta = -inPoint
                                }
                                newDuration = clipDuration - delta
                            }
                            sizeChanged = true
                            clipRoot.trimmingIn(clipRoot, newDuration, shiftTrim, controlTrim)
                        }
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimIn.opacity = 1
                        var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
                        var s = i18n("In:%1, Position:%2", timeline.simplifiedTC(clipRoot.inPoint),timeline.simplifiedTC(clipRoot.modelStart))
                        timeline.showToolTip(s)
                        timeline.showKeyBinding(i18n("<b>Ctrl drag</b> to change speed, <b>Double click</b> to mix with adjacent clip"))
                    }
                }
                onExited: {
                    trimIn.opacity = 0
                    if (!pressed) {
                        if (!mouseArea.containsMouse) {
                            timeline.showToolTip()
                        } else {
                            clipRoot.showClipInfo()
                        }
                        if (!fadeInMouseArea.containsMouse) {
                            timeline.showKeyBinding()
                        }
                    }
                }
                Rectangle {
                    id: trimIn
                    anchors.left: parent.left
                    width: itemBorder.border.width
                    height: parent.height
                    color: 'lawngreen'
                    opacity: 0
                    Drag.active: trimInMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimInMouseArea.pressed || ((root.activeTool === ProjectTool.SelectTool || (root.activeTool === ProjectTool.RippleTool && clipRoot.mixDuration <= 0)) && !mouseArea.drag.active && parent.enabled)

                    /*ToolTip {
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
                    }*/
                }
            }

            MouseArea {
                // Right resize handle
                id: trimOutMouseArea
                anchors.right: parent.right
                anchors.rightMargin: -itemBorder.border.width
                anchors.top: parent.top
                height: parent.height
                width: root.baseUnit / 2
                hoverEnabled: true
                visible: root.activeTool === ProjectTool.SelectTool || (root.activeTool === ProjectTool.RippleTool && clipRoot.mixDuration <= 0)
                enabled: !isLocked && (pressed || clipRoot.width > 3 * width) && clipRoot.clipId == dragProxy.draggedItem
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimOutMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false

                onPressed: {
                    root.autoScrolling = false
                    root.trimInProgress = true;
                    clipRoot.originalDuration = clipDuration
                    anchors.right = undefined
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier
                    if (!shiftTrim && clipRoot.grouped) {
                        clipRoot.initGroupTrim(clipRoot)
                    }
                    if (root.activeTool === ProjectTool.RippleTool) {
                        timeline.requestStartTrimmingMode(clipRoot.clipId, false, true);
                    }

                    trimOut.opacity = 0
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    anchors.right = parent.right
                    if (sizeChanged) {
                        clipRoot.trimmedOut(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                        if (!controlTrim && root.activeTool !== ProjectTool.RippleTool) {
                            updateDrag()
                        } else {
                            root.endDrag()
                        }
                    } else {
                        if (root.activeTool === ProjectTool.RippleTool) {
                            timeline.requestEndTrimmingMode();
                        }
                        root.groupTrimData = undefined
                    }
                    root.trimInProgress = false;
                }
                onDoubleClicked: {
                    timeline.mixClip(clipRoot.clipId, 1)
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newDuration = Math.round((x + width) / clipRoot.timeScale)
                        if (maxDuration > 0 && (newDuration > maxDuration - inPoint) && !(mouse.modifiers & Qt.ControlModifier)) {
                            newDuration = maxDuration - inPoint
                        }
                        if (newDuration != clipDuration) {
                            sizeChanged = true
                            clipRoot.trimmingOut(clipRoot, newDuration, shiftTrim, controlTrim)
                        }
                    }
                }
                onEntered: {
                    if (!pressed) {
                        trimOut.opacity = 1
                        var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
                        var s = i18n("Out:%1, Position:%2", timeline.simplifiedTC(clipRoot.outPoint),timeline.simplifiedTC(clipRoot.modelStart + clipRoot.clipDuration))
                        timeline.showToolTip(s)
                        if (!fadeOutMouseArea.containsMouse) {
                            timeline.showKeyBinding(i18n("<b>Ctrl drag</b> to change speed, <b>Double click</b> to mix with adjacent clip"))
                        }
                    }
                }
                onExited: {
                    trimOut.opacity = 0
                    if (!pressed) {
                         if (!mouseArea.containsMouse) {
                            timeline.showToolTip()
                         } else {
                             var text = i18n("%1 (%2-%3), Position: %4, Duration: %5".arg(clipRoot.clipName)
                        .arg(timeline.simplifiedTC(clipRoot.inPoint))
                        .arg(timeline.simplifiedTC(clipRoot.outPoint))
                        .arg(timeline.simplifiedTC(clipRoot.modelStart))
                        .arg(timeline.simplifiedTC(clipRoot.clipDuration)))
                             timeline.showToolTip(text)
                         }
                         if (!fadeOutMouseArea.containsMouse) {
                             timeline.showKeyBinding()
                         }
                    }
                }
                /*ToolTip {
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
                }*/
                Rectangle {
                    id: trimOut
                    anchors.right: parent.right
                    width: itemBorder.border.width
                    height: parent.height
                    color: 'red'
                    opacity: 0
                    Drag.active: trimOutMouseArea.drag.active
                    Drag.proposedAction: Qt.MoveAction
                    visible: trimOutMouseArea.pressed || ((root.activeTool === ProjectTool.SelectTool || (root.activeTool === ProjectTool.RippleTool && clipRoot.mixDuration <= 0)) && !mouseArea.drag.active && parent.enabled)
                }
            }

            TimelineTriangle {
                // Green fade in triangle
                id: fadeInTriangle
                fillColor: 'green'
                width: Math.min(clipRoot.fadeIn * clipRoot.timeScale, container.width)
                height: parent.height
                anchors.left: parent.left
                anchors.top: parent.top
                opacity: 0.4
            }

            TimelineTriangle {
                // Red fade out triangle
                id: fadeOutCanvas
                fillColor: 'red'
                width: Math.min(clipRoot.fadeOut * clipRoot.timeScale, container.width)
                height: parent.height
                anchors.right: parent.right
                anchors.top: parent.top
                opacity: 0.4
                transform: Scale { xScale: -1; origin.x: fadeOutCanvas.width / 2}
            }

            Item {
                // Clipping container for clip names
                anchors.fill: parent
                anchors.leftMargin: mixContainer.width
                id: nameContainer
                clip: true
                Rectangle {
                    // Debug: Clip Id background
                    id: debugCidRect
                    color: 'magenta'
                    width: debugCid.width + (2 * itemBorder.border.width)
                    height: debugCid.height
                    visible: root.debugmode
                    anchors.left: parent.left
                    anchors.leftMargin: clipRoot.timeremap ? debugCidRect.height : 0
                    Text {
                        // Clip ID text
                        id: debugCid
                        text: clipRoot.clipId
                        font: miniFont
                        anchors {
                            left: debugCidRect.left
                            leftMargin: itemBorder.border.width
                        }
                        color: 'white'
                    }
                }
                Rectangle {
                    // Clip name background
                    id: labelRect
                    color: clipRoot.selected ? 'darkred' : '#66000000'
                    width: label.width + (2 * itemBorder.border.width)
                    height: label.height
                    visible: clipRoot.width > root.baseUnit
                    anchors.left: debugCidRect.visible ? debugCidRect.right : parent.left
                    anchors.leftMargin: clipRoot.timeremap ? labelRect.height : 0
                    Text {
                        // Clip name text
                        id: label
                        property string clipNameString: (clipRoot.isAudio && clipRoot.multiStream) ? ((clipRoot.audioStream > 10000 ? 'Merged' : clipRoot.aStreamIndex) + '|' + clipName ) : clipName
                        text: (clipRoot.speed != 1.0 ? ('[' + Math.round(clipRoot.speed*100) + '%] ') : '') + clipNameString
                        font: miniFont
                        anchors {
                            left: labelRect.left
                            leftMargin: itemBorder.border.width
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
                        onEntered: {
                            var text = positionOffset < 0 ? i18n("Offset: -%1", timeline.simplifiedTC(-positionOffset)) : i18n("Offset: %1", timeline.simplifiedTC(positionOffset))
                            timeline.showToolTip(text)
                        }
                        onExited: {
                            timeline.showToolTip()
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
                    anchors.top: labelRect.top
                    anchors.left: labelRect.right
                    visible: !clipRoot.isAudio && clipRoot.clipStatus === ClipStatus.StatusProxy || clipRoot.clipStatus === ClipStatus.StatusProxyOnly
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
                Rectangle{
                    //remap
                    id:remapRect
                    color: '#cc0033'
                    width: labelRect.height
                    height: labelRect.height
                    anchors.top: labelRect.top
                    anchors.left: nameContainer.left
                    visible: clipRoot.timeremap
                    Text {
                        // Remap R
                        id: remapLabel
                        text: "R"
                        font.pointSize: root.fontUnit +1
                        visible: remapRect.visible
                        anchors {
                            top: remapRect.top
                            left: remapRect.left
                            leftMargin: (labelRect.height-proxyLabel.width)/2
                            topMargin: (labelRect.height-proxyLabel.height)/2
                        }
                        color: 'white'
                        styleColor: 'white'
                    }
                }
            }

            Loader {
                // keyframes container
                id: effectRow
                clip: true
                anchors.fill: parent
                asynchronous: true
                visible: status == Loader.Ready && clipRoot.showKeyframes && clipRoot.keyframeModel && clipRoot.width > 2 * root.baseUnit
                source: clipRoot.hideClipViews || clipRoot.keyframeModel == undefined ? "" : "KeyframeView.qml"
                Binding {
                    target: effectRow.item
                    property: "kfrModel"
                    value: clipRoot.hideClipViews ? undefined : clipRoot.keyframeModel
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "selected"
                    value: clipRoot.selected
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "inPoint"
                    value: clipRoot.inPoint
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "outPoint"
                    value: clipRoot.outPoint
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "modelStart"
                    value: clipRoot.modelStart
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "scrollStart"
                    value: clipRoot.scrollStart
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "scrollStart"
                    value: clipRoot.scrollStart
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
                Binding {
                    target: effectRow.item
                    property: "clipId"
                    value: clipRoot.clipId
                    when: effectRow.status == Loader.Ready && effectRow.item
                    restoreMode: Binding.RestoreBindingOrValue
                }
            }
            Connections {
                target: effectRow.item
                function onSeek(position) { proxy.position = position }
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
                name: 'selectedClip'
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
            onEntered: {
                timeline.showKeyBinding(i18n("<b>Click</b> to add composition"))
            }
            onExited: {
                timeline.showKeyBinding()
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
            onEntered: {
                timeline.showKeyBinding(i18n("<b>Click</b> to add composition"))
            }
            onExited: {
                timeline.showKeyBinding()
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
                //bubbleHelp.hide()
                timeline.showToolTip()
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = clipRoot.clipDuration - Math.floor((x + width / 2 - itemBorder.border.width)/ clipRoot.timeScale)
                    var duration = Math.max(0, delta)
                    duration = Math.min(duration, clipRoot.clipDuration)
                    if (lastDuration != duration) {
                        lastDuration = duration
                        timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        timeline.showToolTip(i18n("Fade out: %1", timeline.simplifiedTC(clipRoot.fadeOut)))
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    if (clipRoot.fadeOut > 0) {
                        timeline.showToolTip(i18n("Fade out: %1", timeline.simplifiedTC(clipRoot.fadeOut)))
                    } else {
                        clipRoot.showClipInfo()
                    }
                    timeline.showKeyBinding(i18n("<b>Drag</b> to adjust fade, <b>Click</b> to add default duration fade"))
                }
            }
            onExited: {
                if (!pressed) {
                    timeline.showKeyBinding()
                    if (mouseArea.containsMouse) {
                        clipRoot.showClipInfo()
                    } else {
                        timeline.showToolTip()
                    }
                }
            }
            Rectangle {
                id: fadeOutControl
                anchors.top: parent.top
                anchors.right: clipRoot.fadeOut > 0 ? undefined : parent.right
                anchors.horizontalCenter: clipRoot.fadeOut > 0 ? parent.horizontalCenter : undefined
                width: fadeOutMouseArea.containsMouse || Drag.active ? parent.width : parent.width / 3
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
            }
        }

        MouseArea {
            // Fade in drag zone
            id: fadeInMouseArea
            anchors.left: container.left
            anchors.leftMargin: clipRoot.fadeIn <= 0 ? 0 : (fadeInTriangle.width - width / 3)
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
                //bubbleHelp.hide()
                timeline.showToolTip()
                anchors.left = container.left
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = Math.round((x + width / 2) / clipRoot.timeScale)
                    var duration = Math.max(0, delta)
                    duration = Math.min(duration, clipRoot.clipDuration - 1)
                    if (duration != clipRoot.fadeIn) {
                        timeline.adjustFade(clipRoot.clipId, 'fadein', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        timeline.showToolTip(i18n("Fade in: %1", timeline.simplifiedTC(clipRoot.fadeIn)))
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    if (clipRoot.fadeIn > 0) {
                        timeline.showToolTip(i18n("Fade in: %1", timeline.simplifiedTC(clipRoot.fadeIn)))
                    } else {
                        clipRoot.showClipInfo()
                    }
                    timeline.showKeyBinding(i18n("<b>Drag</b> to adjust fade, <b>Click</b> to add default duration fade"))
                }
            }
            onExited: {
                if (!pressed) {
                    timeline.showKeyBinding()
                    if (mouseArea.containsMouse) {
                        clipRoot.showClipInfo()
                    } else {
                        timeline.showToolTip()
                    }
                }
            }
            Rectangle {
                id: fadeInControl
                anchors.top: parent.top
                anchors.left: clipRoot.fadeIn > 0 ? undefined : parent.left
                anchors.horizontalCenter: clipRoot.fadeIn > 0 ? parent.horizontalCenter : undefined
                width: fadeInMouseArea.containsMouse || Drag.active ? parent.width : parent.width / 3
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
            }
        }

        Rectangle {
            id: currentRegion
            color: slipControler.color
            anchors {
                right: container.right
                left: container.left
                top: slipControler.top
            }
            height: container.height / 2
            opacity: 0.7
            visible: slipControler.visible
        }
        Item {
            id: slipControler
            property color color: timeline.trimmingMainClip === clipId ? root.selectionColor : activePalette.highlight
            anchors.bottom: container.bottom
            height: container.height
            width: clipRoot.maxDuration * clipRoot.timeScale
            x: - (clipRoot.inPoint - slipOffset) * clipRoot.timeScale
            visible: root.activeTool === ProjectTool.SlipTool && clipRoot.selected && clipRoot.maxDuration > 0 // don't show for endless clips
            property int inPoint: clipRoot.inPoint
            property int outPoint: clipRoot.outPoint
            Rectangle {
                id: slipBackground
                anchors.fill: parent
                color: parent.color
                border.width: 2
                border.color: activePalette.highlightedText
                opacity: 0.3
            }
            Rectangle {
                id: currentRegionMoved
                color: parent.color
                x: slipBackground.x + slipControler.inPoint * clipRoot.timeScale + itemBorder.border.width
                anchors.bottom: parent.bottom
                height: parent.height / 2
                width: container.width
                opacity: 0.7
            }
            Text {
                id: slipLable
                text: i18n("Slip Clip")
                font: miniFont
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                color: activePalette.highlightedText
                opacity: 1
            }
        }
    }
}
