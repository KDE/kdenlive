/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQml.Models 2.15
import QtQml 2.15

import org.kde.kdenlive as K

import 'TimelineLogic.js' as Logic

Rectangle {
    id: clipRoot
    property real timeScale: 1
    property string clipName: ''
    property string tagColor: ''
    property string clipResource: ''
    property string mltService: ''
    property string effectNames
    property bool isStackEnabled
    property int modelStart
    property int mixDuration: 0
    property int mixCut: 0
    property int mixEndDuration: 0
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
    property int fadeInMethod: 0
    property int fadeOut: 0
    property int fadeOutMethod: 0
    property int binId: 0
    property int positionOffset: 0
    property var parentTrack
    property int trackIndex //Index in track repeater
    property int clipId: -1     //Id of the clip in the model
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
    property int slipOffset: boundValue(outPoint - maxDuration + 1, root.trimmingOffset, inPoint)
    visible: scrollView.lastVisibleFrame > clipRoot.modelStart && scrollView.firstVisibleFrame <= (clipRoot.modelStart + clipRoot.clipDuration)
    property int scrollStart: visible ? scrollView.contentX - (clipRoot.modelStart * root.timeScale) : 0
    property bool hideClipViews: !visible || clipRoot.width < root.minClipWidthForViews
    property bool hideDecorations: !root.showClipOverlays || !visible || trimInMouseArea.drag.active || trimOutMouseArea.drag.active || fadeInMouseArea.drag.active || fadeOutMouseArea.drag.active
    property int mouseXPos: mouseArea.mouseX
    width : Math.round(clipDuration * timeScale)
    opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.8 : 1.0
    enabled: !clipDropArea.containsDrag && !compoArea.containsDrag

    signal trimmingIn(var clip, real newDuration, bool shiftTrim, bool controlTrim)
    signal trimmedIn(var clip, bool shiftTrim, bool controlTrim)
    signal initGroupTrim(int clipId)
    signal trimmingOut(var clip, real newDuration, bool shiftTrim, bool controlTrim)
    signal trimmedOut(var clip, bool shiftTrim, bool controlTrim)

    onVisibleChanged: {
        if (clipRoot.visible) {
            updateLabelOffset()
        }
    }

    onScrollStartChanged: {
        if (!clipRoot.visible) {
            return
        }
        //console.log('SCROLL START: ', clipRoot.scrollStart, '; VISIBLE: ', clipRoot.visible)
        updateLabelOffset()
        if (isAudio && thumbsLoader.item) {
            thumbsLoader.item.reload(1)
        }
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
        if (itemType === K.ClipType.Color) {
            color: Qt.darker(getColor(), 1.5)
        }
    }

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
        if (fakePosition > -1) {
            x = fakePosition * timeScale
            xIntegerOffset = Math.ceil(x) - x
        }
    }
    onFakeTidChanged: {
        if (clipRoot.fakeTid > -1 && parentTrack) {
            if (clipRoot.parent != dragContainer) {
                // Clip is parented to a track, reparent to allow moving outside of the track
                clipRoot.parent = dragContainer
            }
            clipRoot.y = Logic.getTrackById(clipRoot.fakeTid).y
            clipRoot.height = Logic.getTrackById(clipRoot.fakeTid).height
        } else if (parentTrack) {
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
        if (clipRoot.visible) {
            if (!clipRoot.hideClipViews) {
                if (effectRow.item && effectRow.item.kfrCanvas) {
                    effectRow.item.kfrCanvas.requestPaint()
                }
            }
        }
    }
    function updateLabelOffset()
    {
        nameContainer.anchors.leftMargin = clipRoot.scrollStart > 0 ? (mixContainer.width + labelRect.width > clipRoot.width ? mixContainer.width : Math.max(clipRoot.scrollStart, mixContainer.width + mixBackground.border.width)) : mixContainer.width + mixBackground.border.width
    }

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
        if (clipRoot.clipState === K.PlaylistState.Disabled) {
            return '#888'
        }
        if (clipRoot.tagColor) {
            return clipRoot.tagColor
        }
        if (itemType === K.ClipType.Text) {
            return titleColor
        }
        if (itemType === K.ClipType.Image) {
            return imageColor
        }
        if (itemType === K.ClipType.SlideShow) {
            return slideshowColor
        }
        if (itemType === K.ClipType.Color) {
            var color = clipResource.substring(clipResource.length - 9)
            if (color[0] === '#') {
                return color
            }
            return '#' + color.substring(color.length - 8, color.length - 2)
        }
        return isAudio? root.audioColor : root.videoColor
    }

    property bool noThumbs: (isAudio || itemType === K.ClipType.Color || mltService === '')
    property string baseThumbPath: noThumbs ? '' : 'image://thumbnail/' + clipThumbId

    DropArea { //Drop area for clips
        anchors.fill: clipRoot
        keys: 'kdenlive/effect'
        property string dropData
        property string dropSource
        onEntered: drag => {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
            updateDrag()
        }
        onDropped: drag => {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                controller.addClipEffect(clipRoot.clipId, dropData)
            } else {
                controller.copyClipEffect(clipRoot.clipId, dropSource)
            }
            if (K.KdenliveSettings.seekonaddeffect && (proxy.position < clipRoot.modelStart || proxy.position > clipRoot.modelStart + clipRoot.clipDuration)) {
                // If timeline cursor is not inside clip, seek to drop position
                proxy.position = clipRoot.modelStart + drag.x / timeScale
            }
            dropSource = ''
            drag.acceptProposedAction()
            root.regainFocus(mapToItem(root, drag.x, drag.y))
            //console.log('KFR VIEW VISIBLE: ', effectRow.visible, ', SOURCE: ', effectRow.source, '\n HIDEVIEW:', clipRoot.hideClipViews<<', UNDEFINED: ', (clipRoot.keyframeModel == undefined))
        }
        onExited: {
            root.endDrag()
        }
    }
    MouseArea {
        id: mouseArea
        enabled: !root.isPanning && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)
        anchors.fill: clipRoot
        acceptedButtons: Qt.RightButton
        hoverEnabled: !root.isPanning && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape
        onPressed: mouse => {
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
        Keys.onShortcutOverride: event => {event.accepted = clipRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)}
        Keys.onLeftPressed: event => {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            while((clipRoot.modelStart >= offset) && !controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)) {
                offset = controller.getPreviousBlank( clipRoot.trackId, clipRoot.modelStart - offset)
                if (offset < 0) {
                    break;
                }
                offset -= (clipRoot.clipDuration - 1)
                offset = clipRoot.modelStart - offset
            }
            Logic.scrollToPosIfNeeded(clipRoot.x)
            timeline.showToolTip(i18n("Position: %1", timeline.simplifiedTC(clipRoot.modelStart)));
        }
        Keys.onRightPressed: event => {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            while(!controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart + offset, true, true, true)) {
                console.log('insert failed at: ', (clipRoot.modelStart + offset))
                offset = controller.getNextBlank( clipRoot.trackId, clipRoot.modelStart + clipRoot.clipDuration + offset)
                console.log('Next blank is: ', offset)
                if (offset < 0) {
                    break;
                }
                offset -= clipRoot.modelStart
            }
            Logic.scrollToPosIfNeeded(clipRoot.x)
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
        onEntered: {
            if (root.isPanning) {
                return
            }
            if (clipRoot.clipId > -1) {
                var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
            }
            showClipInfo()
        }

        onExited: {
            if (root.isPanning) {
                return
            }
            if (!dragProxyArea.pressed) {
                root.endDragIfFocused(clipRoot.clipId)
            }
            if (pressed) {
                if (!trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse && !compInArea.containsMouse && !compOutArea.containsMouse) {
                    timeline.showToolTip()
                }
            }
            timeline.showToolTip()
        }
        onWheel: wheel => zoomByWheel(wheel)

        Loader {
            // Thumbs container
            id: thumbsLoader
            anchors.fill: parent
            anchors.leftMargin: parentTrack.isAudio ? xIntegerOffset : itemBorder.border.width + mixContainer.width
            anchors.rightMargin: parentTrack.isAudio ? clipRoot.width - Math.floor(clipRoot.width) : itemBorder.border.width + clipRoot.mixEndDuration * clipRoot.timeScale
            anchors.topMargin: itemBorder.border.width
            anchors.bottomMargin: itemBorder.border.width

            //clip: true
            asynchronous: true
            visible: status == Loader.Ready
            active: clipRoot.visible
            source: {
                if (clipRoot.hideClipViews || clipRoot.itemType == 0 || clipRoot.itemType === K.ClipType.Color) {
                    return ""
                }
                if (parentTrack.isAudio) {
                    return K.KdenliveSettings.audiothumbnails ? "ClipAudioThumbs.qml" : ""
                }
                if (K.KdenliveSettings.videothumbnails) {
                    return "ClipThumbs.qml"
                }
                return ""
            }
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
            property int handleWidth: Math.max(2, Math.ceil(root.baseUnit / 4))
            anchors.fill: parent
            border.color: {
                let placeholder = (clipStatus === K.FileStatus.StatusMissing || clipStatus === K.FileStatus.StatusWaiting || clipStatus === K.FileStatus.StatusDeleting)
                if (placeholder) {
                    return "#ff0000"
                }

                if (clipRoot.selected) {
                    return root.selectionColor
                }

                if (grouped) {
                    return root.groupColor
                }

                return borderColor
            }
            border.width: isGrabbed ? 8 : 2
            Rectangle {
                id: trimOut
                anchors.right: itemBorder.right
                width: itemBorder.handleWidth
                height: itemBorder.height
                color: 'red'
                opacity: 0
                Drag.active: trimOutMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction
                visible: trimOutMouseArea.pressed || (
                    (root.activeTool === K.ToolType.SelectTool
                    || (root.activeTool === K.ToolType.RippleTool && clipRoot.mixDuration <= 0)
                    ) && !mouseArea.drag.active && parent.enabled)
            }
            Rectangle {
                id: trimIn
                anchors.left: itemBorder.left
                width: itemBorder.handleWidth
                height: parent.height
                color: 'lawngreen'
                opacity: 0
                Drag.active: trimInMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction
                visible: trimInMouseArea.pressed || (
                    (root.activeTool === K.ToolType.SelectTool
                    || (root.activeTool === K.ToolType.RippleTool && clipRoot.mixDuration <= 0)
                    ) && !mouseArea.drag.active && parent.enabled)
            }
        }

        Item {
            // Clipping container
            id: container
            anchors.fill: parent
            anchors.margins: itemBorder.border.width
            //clip: true
            property bool showDetails: (!clipRoot.selected || !effectRow.visible) && container.height > 2.2 * labelRect.height
            property bool handleMini: width < 2 * root.baseUnit
            property bool handleVisible: width > root.baseUnit * 1.2
            
            Item {
                // Mix indicator
                id: mixContainer
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: clipRoot.mixDuration * root.timeScale
                onWidthChanged: {
                    if (clipRoot.visible) {
                        updateLabelOffset()
                    }
                }
                
                Rectangle {
                    id: mixBackground
                    property double mixPos: mixBackground.width - clipRoot.mixCut * clipRoot.timeScale
                    property bool mixSelected: root.selectedMix == clipRoot.clipId
                    anchors.fill: parent
                    visible: clipRoot.mixDuration > 0
                    color: mixSelected ? root.selectionColor : "transparent"
                    Loader {
                        active: mixBackground.visible
                        asynchronous: true
                        source: container.handleVisible && mixContainer.width > 2 * root.baseUnit ? "MixShape.qml" : ""
                    }

                    opacity: mixArea.containsMouse || trimInMixArea.pressed || trimInMixArea.containsMouse || mixSelected ? 1 : 0.7
                    border.color: mixSelected ? root.selectionColor : "white"
                    border.width: clipRoot.mixDuration > 0 ? 2 : 0
                    radius: 3
                    Rectangle {
                        id: mixCutPos
                        anchors.right: parent.right
                        anchors.rightMargin: clipRoot.mixCut * clipRoot.timeScale
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: itemBorder.border.color
                    }
                    MouseArea {
                        // Mix click mouse area
                        id: mixArea
                        anchors.fill: parent
                        hoverEnabled: !root.isPanning
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.RightButton | Qt.LeftButton
                        enabled: !root.isPanning && container.handleVisible && width > root.baseUnit * 0.8
                        onPressed: mouse => {
                            if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                                mouse.accepted = false
                                return
                            }
                            controller.requestMixSelection(clipRoot.clipId);
                            root.autoScrolling = false
                            if (mouse.button == Qt.RightButton) {
                                root.clickFrame = Math.round(mouse.x / timeline.scaleFactor)
                                root.showMixMenu(clipRoot.clipId)
                                root.autoScrolling = timeline.autoScroll
                            }
                        }
                        onEntered: {
                            var text = i18n("Mix duration: %1, Cut at: %2".arg(timeline.simplifiedTC(clipRoot.mixDuration))
                            .arg(timeline.simplifiedTC(clipRoot.mixDuration - clipRoot.mixCut)))
                            timeline.showToolTip(text)
                        }
                    }
                    MouseArea {
                        // Right mix resize handle
                        id: trimInMixArea
                        anchors.left: parent.left
                        anchors.leftMargin: clipRoot.mixDuration * clipRoot.timeScale
                        height: parent.height
                        width: root.baseUnit / 2
                        visible: root.activeTool === K.ToolType.SelectTool
                        property int previousMix
                        enabled: !root.isPanning && !isLocked && mixArea.enabled && (pressed || container.handleVisible)
                        hoverEnabled: !root.isPanning
                        drag.target: trimInMixArea
                        drag.axis: Drag.XAxis
                        drag.smoothed: false
                        drag.maximumX: clipRoot.width
                        drag.minimumX: (clipRoot.mixDuration - clipRoot.mixCut) * clipRoot.timeScale
                        property bool sizeChanged: false
                        cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor)
                        onPressed: {
                            if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                                mouse.accepted = false
                                return
                            }
                            root.trimInProgress = true;
                            previousMix = clipRoot.mixDuration
                            root.autoScrolling = false
                            mixOut.color = 'red'
                            anchors.left = undefined
                            parent.anchors.right = undefined
                            mixCutPos.anchors.right = undefined
                        }
                        onReleased: mouse => {
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
                        onPositionChanged: mouse => {
                            if (mouse.buttons !== Qt.LeftButton) {
                                return
                            }
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
                        onEntered: {
                            if (pressed) {
                                return
                            }
                            mixOut.color = 'red'
                            timeline.showToolTip(i18n("Mix duration: %1", timeline.simplifiedTC(clipRoot.mixDuration)))
                        }
                        onExited: {
                            if (pressed) {
                                return
                            }
                            mixOut.color = itemBorder.border.color
                            if (!mouseArea.containsMouse) {
                                timeline.showToolTip()
                            } else {
                                clipRoot.showClipInfo()
                            }
                        }
                        Rectangle {
                            id: mixOut
                            width: itemBorder.border.width
                            height: mixContainer.height
                            color: itemBorder.border.color
                            Drag.active: trimInMixArea.drag.active
                            Drag.proposedAction: Qt.MoveAction
                            visible: trimInMixArea.pressed || (root.activeTool === K.ToolType.SelectTool && !mouseArea.drag.active && parent.enabled)
                        }
                    }
                }
                
            }
            Component {
                id: markerComponent
                Rectangle {
                    id: markerBase
                    property string markerText
                    property color markerColor
                    property int position
                    property bool hasRange: false
                    property real duration: 0
                    width: hasRange ? Math.max(1, Math.round(duration / clipRoot.speed * clipRoot.timeScale)) : 1
                    height: hasRange ? textMetrics.height + 2 : container.height
                    x: clipRoot.speed < 0
                    ? (clipRoot.maxDuration - clipRoot.inPoint) * clipRoot.timeScale + (Math.round(position / clipRoot.speed)) * clipRoot.timeScale - itemBorder.border.width
                    : (Math.round(position / clipRoot.speed) - clipRoot.inPoint) * clipRoot.timeScale - itemBorder.border.width;
                    y: hasRange ? Math.min(label.height, container.height - textMetrics.height) : 0
                    color: hasRange ? Qt.rgba(markerColor.r, markerColor.g, markerColor.b, 0.7) : markerColor
                    border.color: hasRange ? markerColor : "transparent"
                    border.width: hasRange ? 1 : 0
                    radius: hasRange ? 2 : 0

                    Rectangle {
                        visible: markerBase.hasRange
                        x: 0
                        y: -markerBase.y
                        width: 1
                        height: container.height
                        color: markerBase.markerColor
                    }
                    
                    // Tapered end effect for range markers
                    Rectangle {
                        id: clipRangeEndTaper
                        visible: markerBase.hasRange
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.min(parent.width / 8, 10)
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Qt.rgba(markerBase.markerColor.r, markerBase.markerColor.g, markerBase.markerColor.b, 0.3) }
                            GradientStop { position: 1.0; color: Qt.rgba(markerBase.markerColor.r, markerBase.markerColor.g, markerBase.markerColor.b, 0.1) }
                        }
                    }
                    
                    TextMetrics {
                        id: textMetrics
                        font: miniFont
                        text: markerBase.markerText
                        elide: clipRoot.timeScale > 1 ? Text.ElideNone : Text.ElideRight
                        elideWidth: root.maxLabelWidth
                    }
                    Rectangle {
                        id: labelRectangle
                        width: textMetrics.width + 4
                        height: textMetrics.height
                        color: markerBase.hasRange ? "transparent" : markerBase.markerColor
                        radius: 2
                        opacity: 0.7
                        visible: K.KdenliveSettings.showmarkers && root.maxLabelWidth > root.baseUnit && height < container.height && (markerBase.x > textMetrics.width || container.height > 2 * height)

                        anchors {
                            top: parent.top
                            left: parent.left
                            topMargin: markerBase.hasRange ? (parent.height - height) / 2 : Math.min(label.height, container.height - height)
                        }
                        Text {
                            id: mlabel
                            text: textMetrics.elidedText
                            topPadding: -1
                            leftPadding: 2
                            rightPadding: 2
                            font: miniFont
                            color: '#FFF'
                        }
                        MouseArea {
                            z: 10
                            id: markerArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            property bool shiftTrim: false
                            hoverEnabled: !root.isPanning
                            enabled: !root.isPanning
                            ToolTip.visible: containsMouse
                            ToolTip.text: markerBase.markerText
                            ToolTip.delay: 1000
                            ToolTip.timeout: 5000
                            onDoubleClicked: timeline.editMarker(clipRoot.clipId, markerBase.position)
                            onPressed: mouse => {
                                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                            }

                            onClicked: {
                                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                                    mouse.accepted = false
                                    return
                                }
                                proxy.position = clipRoot.modelStart + (clipRoot.speed < 0
                                ? clipRoot.maxDuration - clipRoot.inPoint + (Math.round(markerBase.position / clipRoot.speed))
                                : (Math.round(markerBase.position / clipRoot.speed) - clipRoot.inPoint))
                                controller.requestAddToSelection(clipRoot.clipId, shiftTrim ? false : true)
                            }
                        }
                    }
                    
                    // Left resize handle for range markers
                    Rectangle {
                        id: leftResizeHandle
                        visible: markerBase.hasRange && markerBase.width > 10
                        width: 4
                        height: markerBase.height
                        x: 0
                        y: 0
                        color: Qt.darker(markerBase.markerColor, 1.3)
                        opacity: leftResizeArea.containsMouse || leftResizeArea.isResizing ? 0.8 : 0.5
                        
                        MouseArea {
                            id: leftResizeArea
                            anchors.fill: parent
                            anchors.margins: -2
                            z: 15
                            hoverEnabled: !root.isPanning
                            enabled: !root.isPanning
                            cursorShape: Qt.SizeHorCursor
                            acceptedButtons: Qt.LeftButton
                            preventStealing: true
                            
                            property bool isResizing: false
                            property real startX: 0
                            property real globalStartX: 0
                            property real startDuration: 0
                            property real startPosition: 0
                            property real originalEndPosition: 0
                            
                            onPressed: {
                                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                                    mouse.accepted = false
                                    return
                                }
                                isResizing = true
                                startX = mouseX
                                globalStartX = mapToGlobal(Qt.point(mouseX, 0)).x
                                startDuration = markerBase.duration
                                startPosition = markerBase.position
                                originalEndPosition = markerBase.position + markerBase.duration
                                cursorShape = Qt.SizeHorCursor
                            }
                            
                            onPositionChanged: {
                                if (isResizing) {
                                    var globalCurrentX = mapToGlobal(Qt.point(mouseX, 0)).x
                                    var realDeltaX = globalCurrentX - globalStartX
                                    
                                    var deltaFrames = Math.round(realDeltaX / clipRoot.timeScale * clipRoot.speed)
                                    var newStartPosition = Math.max(clipRoot.inPoint * clipRoot.speed, startPosition + deltaFrames)
                                    var newDuration = Math.max(1, originalEndPosition - newStartPosition)
                                    
                                    markerBase.position = newStartPosition
                                    markerBase.duration = newDuration
                                    
                                    cursorShape = Qt.SizeHorCursor
                                }
                            }
                            
                            onReleased: {
                                if (isResizing) {
                                    timeline.resizeMarker(clipRoot.clipId, startPosition, markerBase.duration, true, markerBase.position)
                                    isResizing = false
                                    markerBase.position = Qt.binding(function() { return loader.modelData.frame })
                                    markerBase.duration = Qt.binding(function() { return loader.modelData.duration || 0 })
                                    
                                    cursorShape = Qt.SizeHorCursor
                                }
                            }
                            
                            onCanceled: {
                                if (isResizing) {
                                    isResizing = false
                                    markerBase.position = Qt.binding(function() { return loader.modelData.frame })
                                    markerBase.duration = Qt.binding(function() { return loader.modelData.duration || 0 })
                                }
                            }
                        }
                    }
                    
                    // Right resize handle for range markers
                    Rectangle {
                        id: rightResizeHandle
                        visible: markerBase.hasRange && markerBase.width > 10
                        width: 4
                        height: markerBase.height
                        anchors.right: parent.right
                        y: 0
                        color: Qt.darker(markerBase.markerColor, 1.3)
                        opacity: rightResizeArea.containsMouse || rightResizeArea.isResizing ? 0.8 : 0.5
                        
                        MouseArea {
                            id: rightResizeArea
                            anchors.fill: parent
                            anchors.margins: -2
                            z: 15
                            hoverEnabled: !root.isPanning
                            enabled: !root.isPanning
                            cursorShape: Qt.SizeHorCursor
                            acceptedButtons: Qt.LeftButton
                            preventStealing: true
                            
                            property bool isResizing: false
                            property real startX: 0
                            property real globalStartX: 0
                            property real startDuration: 0
                            property real startPosition: 0
                            
                            onPressed: {
                                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                                    mouse.accepted = false
                                    return
                                }
                                isResizing = true
                                startX = mouseX
                                globalStartX = mapToGlobal(Qt.point(mouseX, 0)).x
                                startDuration = markerBase.duration
                                startPosition = markerBase.position
                                cursorShape = Qt.SizeHorCursor
                            }
                            
                            onPositionChanged: {
                                if (isResizing) {
                                    var globalCurrentX = mapToGlobal(Qt.point(mouseX, 0)).x
                                    var realDeltaX = globalCurrentX - globalStartX
                                    
                                    var deltaFrames = Math.round(realDeltaX / clipRoot.timeScale * clipRoot.speed)
                                    var maxEnd = (clipRoot.inPoint + clipRoot.outPoint) * clipRoot.speed
                                    var newDuration = Math.max(1, Math.min(startDuration + deltaFrames, maxEnd - startPosition))
                                    
                                    markerBase.duration = newDuration
                                    
                                    cursorShape = Qt.SizeHorCursor
                                }
                            }
                            
                            onReleased: {
                                if (isResizing) {
                                    timeline.resizeMarker(clipRoot.clipId, startPosition, markerBase.duration, false)
                                    isResizing = false
                                    markerBase.position = Qt.binding(function() { return loader.modelData.frame })
                                    markerBase.duration = Qt.binding(function() { return loader.modelData.duration || 0 })
                                    
                                    cursorShape = Qt.SizeHorCursor
                                }
                            }
                            
                            onCanceled: {
                                if (isResizing) {
                                    isResizing = false
                                    markerBase.position = Qt.binding(function() { return loader.modelData.frame })
                                    markerBase.duration = Qt.binding(function() { return loader.modelData.duration || 0 })
                                }
                            }
                        }
                    }
                }
            }

            Item {
                // Clipping container
                anchors.fill: container
                clip: true

                Repeater {
                    // Clip markers
                    id: markersContainer
                    model: container.width > 3 * root.baseUnit ? markers : 0
                    anchors.fill: parent
                    delegate: Loader {
                        id: loader
                        required property var modelData
                        property bool isInside: modelData.frame > clipRoot.inPoint && modelData.frame < clipRoot.outPoint
                        asynchronous: true
                        active: clipRoot.visible
                        Binding {
                            target: loader.item
                            property: "position"
                            value: modelData.frame
                            when: isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "markerText"
                            value: modelData.comment
                            when: isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "markerColor"
                            value: modelData.color
                            when: isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "hasRange"
                            value: modelData.hasRange || false
                            when: isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "duration"
                            value: modelData.duration || 0
                            when: isInside && loader.status == Loader.Ready
                        }
                        sourceComponent: {
                            if (isInside) {
                                return markerComponent;
                            } else {
                                return null;
                            }
                        }
                    }
                }
            }

            MouseArea {
                // Left resize handle
                id: trimInMouseArea
                x: -itemBorder.border.width
                anchors.top: container.top
                height: container.height
                width: container.handleMini ? root.baseUnit / 2 : root.baseUnit
                visible: {
                    if (!enabled) {
                        return false
                    }

                    if (root.activeTool === K.ToolType.SelectTool) {
                        return true
                    }

                    let hasMix = clipRoot.mixDuration > 0 || controller.hasClipEndMix(clipRoot.clipId)
                    if (root.activeTool === K.ToolType.RippleTool && !hasMix) {
                        return true
                    }

                    return false
                }
                enabled: !isLocked && (pressed || (container.handleVisible && (mixArea.enabled || clipRoot.mixDuration == 0))) && clipRoot.clipId == dragProxy.draggedItem
                hoverEnabled: true
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                onPressed: mouse => {
                    root.autoScrolling = false
                    root.trimInProgress = true;
                    clipRoot.originalX = clipRoot.x
                    clipRoot.originalDuration = clipDuration
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier && itemType != K.ClipType.Color && itemType != K.ClipType.Timeline && itemType != K.ClipType.Playlist && itemType != K.ClipType.Image
                    if (!shiftTrim && (clipRoot.grouped || controller.hasMultipleSelection())) {
                        clipRoot.initGroupTrim(clipRoot.clipId)
                    }
                    if (root.activeTool === K.ToolType.RippleTool) { //TODO
                        timeline.requestStartTrimmingMode(clipRoot.clipId, false, false);
                    }
                }
                onReleased: {
                    trimIn.opacity = 0
                    root.autoScrolling = timeline.autoScroll
                    x = -itemBorder.border.width
                    if (sizeChanged) {
                        clipRoot.trimmedIn(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                        if (!controlTrim && root.activeTool !== K.ToolType.RippleTool) {
                            updateDrag()
                        } else {
                            root.endDrag()
                        }
                    } else {
                        if (root.activeTool === K.ToolType.RippleTool) { //TODO
                            timeline.requestEndTrimmingMode();
                        } else if (timeline.selection.indexOf(clipRoot.clipId) === -1) {
                            controller.requestAddToSelection(clipRoot.clipId, shiftTrim ? false : true)
                        } else if (shiftTrim) {
                            controller.requestRemoveFromSelection(clipRoot.clipId)
                        }

                        root.groupTrimData = undefined
                    }
                    root.trimInProgress = false;
                }
                onDoubleClicked: {
                    if (clipRoot.mixDuration == 0) {
                        timeline.mixClip(clipRoot.clipId, -1)
                    }
                }
                onPositionChanged: mouse => {
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
                            if (root.activeTool === K.ToolType.RippleTool) {
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
                    if (!pressed && !root.isDragging()) {
                        trimIn.opacity = 1
                        var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
                        var s = i18n("In:%1, Position:%2", timeline.simplifiedTC(clipRoot.inPoint),timeline.simplifiedTC(clipRoot.modelStart))
                        timeline.showToolTip(s)
                        if (clipRoot.mixDuration == 0) {
                            timeline.showKeyBinding(i18n("<b>Ctrl drag</b> to change speed, <b>Double click</b> to mix with adjacent clip"))
                        } else {
                            timeline.showKeyBinding(i18n("<b>Drag</b> to change mix duration"))
                        }
                    }
                }
                onExited: {
                    if (!pressed) {
                        trimIn.opacity = 0
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
            }

            MouseArea {
                // Right resize handle
                id: trimOutMouseArea
                anchors.right: container.right
                anchors.rightMargin: -itemBorder.border.width
                anchors.top: container.top
                height: container.height
                width: container.handleMini ? root.baseUnit / 2 : root.baseUnit
                hoverEnabled: true
                visible: enabled && (root.activeTool === K.ToolType.SelectTool
                                     || (root.activeTool === K.ToolType.RippleTool && clipRoot.mixDuration <= 0))
                enabled: !isLocked && (pressed || container.handleVisible) && clipRoot.clipId == dragProxy.draggedItem
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimOutMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false

                onPressed: mouse => {
                    root.autoScrolling = false
                    root.trimInProgress = true;
                    clipRoot.originalDuration = clipDuration
                    anchors.right = undefined
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier && itemType != K.ClipType.Color && itemType != K.ClipType.Timeline && itemType != K.ClipType.Playlist && itemType != K.ClipType.Image
                    if (!shiftTrim && (clipRoot.grouped || controller.hasMultipleSelection())) {
                        clipRoot.initGroupTrim(clipRoot.clipId)
                    }
                    if (root.activeTool === K.ToolType.RippleTool) {
                        timeline.requestStartTrimmingMode(clipRoot.clipId, false, true);
                    }
                }
                onReleased: {
                    trimOut.opacity = 0
                    root.autoScrolling = timeline.autoScroll
                    anchors.right = parent.right
                    if (sizeChanged) {
                        clipRoot.trimmedOut(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                        if (!controlTrim && root.activeTool !== K.ToolType.RippleTool) {
                            updateDrag()
                        } else {
                            root.endDrag()
                        }
                    } else {
                        if (root.activeTool === K.ToolType.RippleTool) {
                            timeline.requestEndTrimmingMode();
                        } else if (timeline.selection.indexOf(clipRoot.clipId) === -1) {
                            controller.requestAddToSelection(clipRoot.clipId, shiftTrim ? false : true)
                        } else if (shiftTrim) {
                            controller.requestRemoveFromSelection(clipRoot.clipId)
                        }
                        root.groupTrimData = undefined
                    }
                    root.trimInProgress = false;
                }
                onDoubleClicked: {
                    timeline.mixClip(clipRoot.clipId, 1)
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newDuration = Math.round((x + width + itemBorder.border.width) / clipRoot.timeScale)
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
                    if (!pressed && !root.isDragging()) {
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
                    if (!pressed) {
                        trimOut.opacity = 0
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
            }

            K.TimelineTriangle {
                // Green fade in triangle
                id: fadeInTriangle
                fillColor: 'green'
                curveType: clipRoot.fadeInMethod
                width: Math.min(clipRoot.fadeIn * clipRoot.timeScale, container.width)
                height: parent.height
                visible: width > 2
                anchors.left: parent.left
                anchors.top: parent.top
                opacity: 0.4
            }

            K.TimelineTriangle {
                // Red fade out triangle
                id: fadeOutCanvas
                fillColor: 'red'
                curveType: clipRoot.fadeOutMethod
                width: Math.min(clipRoot.fadeOut * clipRoot.timeScale, container.width)
                height: parent.height
                anchors.right: parent.right
                anchors.top: parent.top
                visible: width > 2
                endFade: true
                opacity: 0.4
                transform: Scale { xScale: -1; origin.x: fadeOutCanvas.width / 2 }
            }

            Item {
                // Clipping container for clip names
                id: nameContainer
                anchors.fill: parent
                anchors.leftMargin: clipRoot.scrollStart > 0 ? (mixContainer.width + labelRect.width > clipRoot.width ? mixContainer.width : Math.max(clipRoot.scrollStart, mixContainer.width + mixBackground.border.width)) : mixContainer.width + mixBackground.border.width
                clip: true
                states: [
                    State { when: !clipRoot.hideDecorations
                        PropertyChanges { target: nameContainer; opacity: 1.0 }
                    },
                    State { when: clipRoot.hideDecorations
                        PropertyChanges { target: nameContainer; opacity: 0.0 }
                    }
                ]
                transitions: Transition {
                    NumberAnimation { property: "opacity"; duration: 250}
                }

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
                    color: clipRoot.selected ? (root.mainItemId == clipRoot.clipId ? '#FFCC0000' : '#FF800000') : '#66000000'
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
                        topPadding: -2
                        bottomPadding: -1
                        anchors {
                            left: labelRect.left
                            leftMargin: itemBorder.border.width
                        }
                        color: "#FFFFFF"
                        //style: Text.Outline
                        //styleColor: 'black'
                    }
                }

                Rectangle {
                    // Offset info
                    id: offsetRect
                    color: 'darkgreen'
                    width: offsetLabel.width + radius
                    height: labelRect.height
                    radius: height/3
                    anchors.left: proxyRect.visible ? proxyRect.right : labelRect.right
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
                            text += i18n(" <b>Click</b> to align clips")
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
                    width: effectLabel.width + effectsToggle.width + 4
                    height: effectLabel.height
                    anchors.top: labelRect.bottom
                    anchors.left: labelRect.left
                    visible: labelRect.visible && clipRoot.effectNames != '' && container.showDetails
                    Rectangle {
                        // effects toggle button background
                        id: effectsToggle
                        color: clipRoot.isStackEnabled ? '#fdbc4b' : 'black'
                        visible: clipRoot.width > 2.5 * effectLabel.height
                        width: visible ? effectsRect.height : 0
                        height: effectsRect.height
                        ToolButton {
                            id: effectButton
                            height: effectsRect.height
                            width: effectsRect.height
                            onClicked: {
                                timeline.setEffectsEnabled(clipRoot.clipId, !clipRoot.isStackEnabled)
                            }

                            icon {
                                name: 'tools-wizard'
                                color: clipRoot.isStackEnabled ? 'black' : 'white'
                                height: effectLabel.height
                                width: effectLabel.height
                            }
                            anchors {
                                top: parent.top
                                left: parent.left
                                leftMargin: 1
                            }
                        }
                    }
                    Text {
                        // Effect names text
                        id: effectLabel
                        text: clipRoot.effectNames
                        font {
                            family: miniFont.family
                            pointSize: miniFont.pointSize
                            strikeout: !clipRoot.isStackEnabled
                        }
                        visible: effectsRect.visible
                        anchors {
                            top: effectsToggle.top
                            left: effectsToggle.right
                            leftMargin: 2
                            rightMargin: 2
                        }
                        color: 'white'
                        styleColor: 'black'
                    }
               }
               Rectangle{
                    //proxy 
                    id: proxyRect
                    color: '#fdbc4b'
                    width: labelRect.height
                    height: labelRect.height
                    anchors.top: labelRect.top
                    anchors.left: labelRect.right
                    visible: !clipRoot.isAudio && clipRoot.clipStatus === K.FileStatus.StatusProxy || clipRoot.clipStatus === K.FileStatus.StatusProxyOnly
                    Text {
                        // Proxy P
                        id: proxyLabel
                        text: i18nc("@label The first letter of Proxy, used as abbreviation", "P")
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
                property bool hasKeyframes: false
                active: clipRoot.visible
                visible: status == Loader.Ready && clipRoot.showKeyframes && clipRoot.keyframeModel && hasKeyframes && clipRoot.width > 2 * root.baseUnit
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
                when: clipRoot.isLocked === true
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
            hoverEnabled: !root.isPanning
            cursorShape: Qt.PointingHandCursor
            visible: !clipRoot.isAudio
            enabled: !root.isPanning && !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId && compositionIn.visible
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                root.mainItemId = -1
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
            hoverEnabled: !root.isPanning
            cursorShape: Qt.PointingHandCursor
            enabled: !root.isPanning && !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId && compositionOut.visible
            visible: !clipRoot.isAudio
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                root.mainItemId = -1
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
            visible: container.handleVisible && mouseArea.containsMouse && !dragProxyArea.pressed
            property int startFadeOut
            property int lastDuration: -1
            property int startMousePos
            property bool dragStarted: false
            property string fadeString: timeline.simplifiedTC(clipRoot.fadeOut)
            drag.smoothed: false
            onClicked: {
                if (clipRoot.fadeOut == 0) {
                    timeline.adjustFade(clipRoot.clipId, 'fadeout', 0, -2)
                }
            }
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                root.autoScrolling = false
                startFadeOut = clipRoot.fadeOut
                dragStarted = startFadeOut > 0
                startMousePos = mouse.x
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
            onPositionChanged: mouse => {
                if (mouse.buttons === Qt.LeftButton) {
                    if (!dragStarted && startMousePos - mouse.x < 3) {
                        return
                    }
                    dragStarted = true
                    var delta = clipRoot.clipDuration - Math.floor((x + width / 2 - itemBorder.border.width)/ clipRoot.timeScale)
                    var duration = Math.max(0, delta)
                    duration = Math.min(duration, clipRoot.clipDuration)
                    if (lastDuration != duration) {
                        lastDuration = duration
                        timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        timeline.showToolTip(i18n("Fade out: %1", fadeString))
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    if (clipRoot.fadeOut > 0) {
                        timeline.showToolTip(i18n("Fade out: %1", fadeString))
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
            ToolTip.visible: (containsMouse || pressed || drag.active)
            ToolTip.delay: (pressed || drag.active) ? 0 : 1000
            ToolTip.text: fadeString
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
            property int startMousePos
            property bool dragStarted: false
            property string fadeString: timeline.simplifiedTC(clipRoot.fadeIn)
            visible: container.handleVisible && mouseArea.containsMouse && !dragProxyArea.pressed
            onClicked: {
                if (clipRoot.fadeIn == 0) {
                    timeline.adjustFade(clipRoot.clipId, 'fadein', 0, -2)
                }
            }
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                root.autoScrolling = false
                startFadeIn = clipRoot.fadeIn
                dragStarted = startFadeIn > 0
                startMousePos = mouse.x
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
            onPositionChanged: mouse => {
                if (mouse.buttons === Qt.LeftButton) {
                    if (!dragStarted && mouse.x - startMousePos < 3) {
                        return
                    }
                    dragStarted = true
                    var delta = Math.round((x + width / 2) / clipRoot.timeScale)
                    var duration = Math.max(0, delta)
                    duration = Math.min(duration, clipRoot.clipDuration - 1)
                    if (duration != clipRoot.fadeIn) {
                        timeline.adjustFade(clipRoot.clipId, 'fadein', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        timeline.showToolTip(i18n("Fade in: %1", fadeString))
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    if (clipRoot.fadeIn > 0) {
                        timeline.showToolTip(i18n("Fade in: %1", fadeString))
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
            ToolTip.visible: (containsMouse || pressed || drag.active)
            ToolTip.delay: (pressed || drag.active) ? 0 : 1000
            ToolTip.text: fadeString
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
            visible: root.activeTool === K.ToolType.SlipTool && clipRoot.selected && clipRoot.maxDuration > 0 // don't show for endless clips
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
        }
        Text {
            id: slipLabel
            text: i18n("Slip Clip")
            font: miniFont
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            color: activePalette.highlightedText
            visible: slipControler.visible
            opacity: 1
        }
    }
}
