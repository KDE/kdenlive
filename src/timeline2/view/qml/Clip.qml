/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQml.Models 2.15
import QtQml 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

import 'TimelineLogic.js' as Logic

Rectangle {
    id: clipRoot
    SystemPalette { id: activePalette }
    FontMetrics {
        id: fontMetrics
        font: K.UiUtils.fixedFont
    }

    required property K.TimelineController timeline
    required property K.TimelineItemModel controller
    required property bool isPanning

    required property real timeScale
    required property string clipName
    required property string tagColor
    required property string clipResource
    required property string mltService
    required property string effectNames
    required property bool isStackEnabled
    required property int modelStart
    required property int mixDuration
    required property int mixCut
    required property int mixEndDuration
    required property int inPoint
    required property int outPoint
    required property int clipDuration
    required property int maxDuration
    required property bool isAudio
    required property bool timeremap
    required property int audioChannels
    required property int audioStream
    required property bool multiStream
    required property int aStreamIndex
    required property bool showKeyframes
    required property bool isGrabbed
    required property bool grouped
    required property var markers
    required property var keyframeModel
    required property int clipState
    required property int clipStatus
    required property int itemType
    required property int fadeIn
    required property int fadeInMethod
    required property int fadeOut
    required property int fadeOutMethod
    required property int binId
    required property int positionOffset
    required property var parentTrack
    required property int clipId // Id of the clip in the model
    required property int trackId // Id of the parent track in the model
    required property int fakeTid
    required property int fakePosition
    required property bool selected
    required property bool hasAudio
    required property bool canBeAudio
    required property bool canBeVideo
    required property double speed
    required property string clipThumbId
    required property bool forceReloadAudioThumb

    property int originalTrackId: -1
    property int originalX: x
    property int originalDuration: clipDuration
    property int lastValidDuration: clipDuration
    property int draggedX: x
    property double xIntegerOffset: 0
    property bool isLocked: parentTrack && parentTrack.isLocked === true
    property color borderColor: "#000000"
    property bool isComposition: false
    property int slipOffset: boundValue(outPoint - maxDuration + 1, root.trimmingOffset, inPoint)
    visible: fakeTid > -1 || (scrollView.lastVisibleFrame > clipRoot.modelStart && scrollView.firstVisibleFrame <= (clipRoot.modelStart + clipRoot.clipDuration))
    property int scrollStart: visible ? scrollView.contentX - (clipRoot.modelStart * timeScale) : 0
    readonly property bool trimInProgress: trimInMixArea.pressed || trimInMouseArea.pressed || trimOutMouseArea.pressed
    readonly property bool isUserInteracting: mouseArea.pressed || mixArea.pressed || trimInProgress || fadeInMouseArea.pressed || fadeOutMouseArea.pressed
    readonly property bool hideClipViews: !visible || clipRoot.width < root.minClipWidthForViews
    readonly property bool hideDecorations: !K.KdenliveSettings.showClipOverlays || !visible || trimInMouseArea.drag.active || trimOutMouseArea.drag.active || fadeInMouseArea.drag.active || fadeOutMouseArea.drag.active
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
            (thumbsLoader.item as ClipAudioThumbs).reload(1)
        }
        if (!clipRoot.hideClipViews && clipRoot.width > scrollView.width) {
            let kfrView = effectRow.item as KeyframeView
            if (kfrView && kfrView.kfrCanvas) {
                kfrView.kfrCanvas.requestPaint()
            }
        }
    }

    onIsGrabbedChanged: {
        if (clipRoot.isGrabbed) {
            grabItem()
        } else {
            clipRoot.timeline.showToolTip()
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
            (effectRow.item as KeyframeView).resetSelection()
        }
    }

    function clearAndMove(offset) {
        clipRoot.controller.requestClearSelection()
        clipRoot.controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)
        clipRoot.controller.requestAddToSelection(clipRoot.clipId)
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
            (thumbsLoader.item as ClipAudioThumbs).reload(1)
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
            (thumbsLoader.item as ClipAudioThumbs).reload(0)
        }
    }

    onTimeScaleChanged: {
        x = modelStart * clipRoot.timeScale;
        xIntegerOffset = Math.ceil(x) - x
        width = clipDuration * clipRoot.timeScale;
        if (clipRoot.visible) {
            if (!clipRoot.hideClipViews) {
                let kfrView = effectRow.item as KeyframeView
                if (kfrView && kfrView.kfrCanvas) {
                    kfrView.kfrCanvas.requestPaint()
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
        var text = KI18n.i18n("%1 (%2-%3), Position: %4, Duration: %5".arg(clipRoot.clipName)
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.inPoint))
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.outPoint))
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.modelStart))
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.clipDuration)))
        clipRoot.timeline.showToolTip(text)
    }

    function getColor() {
        if (clipRoot.clipState === K.PlaylistState.Disabled) {
            return '#888'
        }
        if (clipRoot.tagColor) {
            return clipRoot.tagColor
        }
        if (itemType === K.ClipType.Text) {
            return clipRoot.timeline.titleColor
        }
        if (itemType === K.ClipType.Image) {
            return clipRoot.timeline.imageColor
        }
        if (itemType === K.ClipType.SlideShow) {
            return clipRoot.timeline.slideshowColor
        }
        if (itemType === K.ClipType.Color) {
            var color = clipResource.substring(clipResource.length - 9)
            if (color[0] === '#') {
                return color
            }
            return '#' + color.substring(color.length - 8, color.length - 2)
        }
        return isAudio? clipRoot.timeline.audioColor : clipRoot.timeline.videoColor
    }

    property bool noThumbs: (isAudio || itemType === K.ClipType.Color || mltService === '')
    property url baseThumbPath: noThumbs ? '' : 'image://thumbnail/' + clipThumbId

    DropArea { //Drop area for clips
        anchors.fill: clipRoot
        keys: ['kdenlive/effect']
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
                clipRoot.controller.addClipEffect(clipRoot.clipId, dropData)
            } else {
                clipRoot.controller.copyClipEffect(clipRoot.clipId, dropSource)
            }
            if (K.KdenliveSettings.seekonaddeffect && (proxy.position < clipRoot.modelStart || proxy.position > clipRoot.modelStart + clipRoot.clipDuration)) {
                // If timeline cursor is not inside clip, seek to drop position
                proxy.position = clipRoot.modelStart + drag.x / clipRoot.timeScale
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
        enabled: !clipRoot.isPanning && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)
        anchors.fill: clipRoot
        acceptedButtons: Qt.RightButton
        hoverEnabled: !clipRoot.isPanning && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape
        onPressed: mouse => {
            root.mainItemId = clipRoot.clipId
            if (mouse.button == Qt.RightButton) {
                if (clipRoot.timeline.selection.indexOf(clipRoot.clipId) === -1) {
                    clipRoot.controller.requestAddToSelection(clipRoot.clipId, true)
                }
                root.clickFrame = Math.round(mouse.x / clipRoot.timeline.scaleFactor)
                root.showClipMenu(clipRoot.clipId)
            }
        }
        Keys.onShortcutOverride: event => {event.accepted = clipRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)}
        Keys.onLeftPressed: event => {
            var offset = event.modifiers === Qt.ShiftModifier ? K.Core.getCurrentFps() : 1
            while((clipRoot.modelStart >= offset) && !clipRoot.controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)) {
                offset = clipRoot.controller.getPreviousBlank( clipRoot.trackId, clipRoot.modelStart - offset)
                if (offset < 0) {
                    break;
                }
                offset -= (clipRoot.clipDuration - 1)
                offset = clipRoot.modelStart - offset
            }
            Logic.scrollToPosIfNeeded(clipRoot.x)
            clipRoot.timeline.showToolTip(KI18n.i18n("Position: %1", clipRoot.timeline.simplifiedTC(clipRoot.modelStart)));
        }
        Keys.onRightPressed: event => {
            var offset = event.modifiers === Qt.ShiftModifier ? K.Core.getCurrentFps() : 1
            while(!clipRoot.controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart + offset, true, true, true)) {
                console.log('insert failed at: ', (clipRoot.modelStart + offset))
                offset = clipRoot.controller.getNextBlank( clipRoot.trackId, clipRoot.modelStart + clipRoot.clipDuration + offset)
                console.log('Next blank is: ', offset)
                if (offset < 0) {
                    break;
                }
                offset -= clipRoot.modelStart
            }
            Logic.scrollToPosIfNeeded(clipRoot.x)
            clipRoot.timeline.showToolTip(KI18n.i18n("Position: %1", clipRoot.timeline.simplifiedTC(clipRoot.modelStart)));
        }
        Keys.onUpPressed: {
            var nextTrack = clipRoot.controller.getNextTrackId(clipRoot.trackId);
            while(!clipRoot.controller.requestClipMove(clipRoot.clipId, nextTrack, clipRoot.modelStart, true, true, true) && nextTrack !== clipRoot.controller.getNextTrackId(nextTrack)) {
                nextTrack = clipRoot.controller.getNextTrackId(nextTrack);
            }
        }
        Keys.onDownPressed: {
            var previousTrack = clipRoot.controller.getPreviousTrackId(clipRoot.trackId);
            while(!clipRoot.controller.requestClipMove(clipRoot.clipId, previousTrack, clipRoot.modelStart, true, true, true) && previousTrack !== clipRoot.controller.getPreviousTrackId(previousTrack)) {
                previousTrack = clipRoot.controller.getPreviousTrackId(previousTrack);
            }
        }
        Keys.onEscapePressed: {
            clipRoot.timeline.grabCurrent()
            //focus = false
        }
        onEntered: {
            if (clipRoot.isPanning) {
                return
            }
            if (clipRoot.clipId > -1) {
                var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
            }
            showClipInfo()
        }

        onExited: {
            if (clipRoot.isPanning) {
                return
            }
            if (!dragProxyArea.pressed) {
                root.endDragIfFocused(clipRoot.clipId)
            }
            if (pressed) {
                if (!trimInMouseArea.containsMouse && !trimOutMouseArea.containsMouse && !compInArea.containsMouse && !compOutArea.containsMouse) {
                    clipRoot.timeline.showToolTip()
                }
            }
            clipRoot.timeline.showToolTip()
        }
        onWheel: wheel => zoomByWheel(wheel)

        Component {
            id: videoThumb
            ClipThumbs {
                parentClip: clipRoot
                initialSpeed: clipRoot.speed
            }
        }

        Component {
            id: audioThumb
            ClipAudioThumbs {
                timeScale: clipRoot.timeScale
                parentClip: clipRoot
                audioColor: clipRoot.timeline.audioColor
            }
        }

        Loader {
            // Thumbs container
            id: thumbsLoader
            anchors.fill: parent
            anchors.leftMargin: clipRoot.parentTrack.isAudio ? clipRoot.xIntegerOffset : itemBorder.border.width + mixContainer.width
            anchors.rightMargin: clipRoot.parentTrack.isAudio ? clipRoot.width - Math.floor(clipRoot.width) : itemBorder.border.width + clipRoot.mixEndDuration * clipRoot.timeScale
            anchors.topMargin: itemBorder.border.width
            anchors.bottomMargin: itemBorder.border.width

            //clip: true
            asynchronous: true
            visible: status == Loader.Ready
            active: clipRoot.visible
                    && !(clipRoot.hideClipViews
                         || clipRoot.itemType == K.ClipType.Unknown
                         || clipRoot.itemType === K.ClipType.Color
                         // not if it is a audio clip, but audio thumbs are disabled
                         || clipRoot.parentTrack.isAudio && !K.KdenliveSettings.audiothumbnails
                         // not if it is a video clip, but video thumbs are disabled
                         || !clipRoot.parentTrack.isAudio && !K.KdenliveSettings.videothumbnails
                         )
            sourceComponent:  clipRoot.parentTrack.isAudio ? audioThumb : videoThumb
        }

        Rectangle {
            // Border rectangle
            color: 'transparent'
            id: itemBorder
            property int handleWidth: Math.max(2, Math.ceil(K.UiUtils.baseSizeMedium / 4))
            anchors.fill: parent
            border.color: {
                let placeholder = (clipRoot.clipStatus === K.FileStatus.StatusMissing
                                   || clipRoot.clipStatus === K.FileStatus.StatusWaiting
                                   || clipRoot.clipStatus === K.FileStatus.StatusDeleting)
                if (placeholder) {
                    return "#ff0000"
                }

                if (clipRoot.selected) {
                    return clipRoot.timeline.selectionColor
                }

                if (clipRoot.grouped) {
                    return clipRoot.timeline.groupColor
                }

                return clipRoot.borderColor
            }
            border.width: clipRoot.isGrabbed ? 8 : 2
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
                    (K.Core.activeTool === K.ToolType.SelectTool
                    || (K.Core.activeTool === K.ToolType.RippleTool && clipRoot.mixDuration <= 0)
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
                    (K.Core.activeTool === K.ToolType.SelectTool
                    || (K.Core.activeTool === K.ToolType.RippleTool && clipRoot.mixDuration <= 0)
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
            property bool handleMini: width < 2 * K.UiUtils.baseSizeMedium
            property bool handleVisible: width > K.UiUtils.baseSizeMedium * 1.2
            
            Item {
                // Mix indicator
                id: mixContainer
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: clipRoot.mixDuration * clipRoot.timeScale
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
                    color: mixSelected ? Qt.rgba(clipRoot.timeline.selectionColor.r, clipRoot.timeline.selectionColor.g, clipRoot.timeline.selectionColor.b, 0.5) : "transparent"
                    Loader {
                        active: mixBackground.visible && container.handleVisible && mixContainer.width > 2 * K.UiUtils.baseSizeMedium
                        asynchronous: true
                        anchors.fill: mixBackground
                        sourceComponent: MixShape {}
                    }

                    opacity: mixArea.containsMouse || trimInMixArea.pressed || trimInMixArea.containsMouse || mixSelected ? 1 : 0.7
                    border.color: mixSelected ? clipRoot.timeline.selectionColor : "white"
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
                        hoverEnabled: !clipRoot.isPanning
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.RightButton | Qt.LeftButton
                        enabled: !clipRoot.isPanning && container.handleVisible && width > K.UiUtils.baseSizeMedium * 0.8
                        onPressed: mouse => {
                            if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                                mouse.accepted = false
                                return
                            }
                            clipRoot.controller.requestMixSelection(clipRoot.clipId);
                            if (mouse.button == Qt.RightButton) {
                                root.clickFrame = Math.round(mouse.x / clipRoot.timeline.scaleFactor)
                                root.showMixMenu(clipRoot.clipId)
                            }
                        }
                        onEntered: {
                            var text = KI18n.i18n("Mix duration: %1, Cut at: %2".arg(clipRoot.timeline.simplifiedTC(clipRoot.mixDuration))
                            .arg(clipRoot.timeline.simplifiedTC(clipRoot.mixDuration - clipRoot.mixCut)))
                            clipRoot.timeline.showToolTip(text)
                        }
                    }
                    MouseArea {
                        // Right mix resize handle
                        id: trimInMixArea
                        anchors.left: parent.left
                        anchors.leftMargin: clipRoot.mixDuration * clipRoot.timeScale
                        height: parent.height
                        width: K.UiUtils.baseSizeMedium / 2
                        visible: K.Core.activeTool === K.ToolType.SelectTool
                        property int previousMix
                        enabled: !clipRoot.isPanning && !clipRoot.isLocked && mixArea.enabled && (pressed || container.handleVisible)
                        hoverEnabled: !clipRoot.isPanning
                        drag.target: trimInMixArea
                        drag.axis: Drag.XAxis
                        drag.smoothed: false
                        drag.maximumX: clipRoot.width
                        drag.minimumX: (clipRoot.mixDuration - clipRoot.mixCut) * clipRoot.timeScale
                        property bool sizeChanged: false
                        cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor)
                        onPressed: (mouse) => {
                            if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                                mouse.accepted = false
                                return
                            }
                            previousMix = clipRoot.mixDuration
                            mixOut.color = 'red'
                            anchors.left = undefined
                            parent.anchors.right = undefined
                            mixCutPos.anchors.right = undefined
                        }
                        onReleased: mouse => {
                            if (sizeChanged) {
                                clipRoot.controller.resizeStartMix(clipRoot.clipId, Math.round(Math.max(0, x) / clipRoot.timeScale), mouse.modifiers & Qt.ShiftModifier)
                                sizeChanged = false
                            }
                            anchors.left = parent.left
                            parent.anchors.right = mixContainer.right
                            mixBackground.anchors.bottom = mixContainer.bottom
                            mixOut.color = itemBorder.border.color
                            mixCutPos.anchors.right = mixCutPos.parent.right
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
                                    clipRoot.timeline.showToolTip(KI18n.i18n("+%1, Mix duration: %2", clipRoot.timeline.simplifiedTC(currentFrame - previousMix), clipRoot.timeline.simplifiedTC(currentFrame)))
                                } else {
                                    clipRoot.timeline.showToolTip(KI18n.i18n("-%1, Mix duration: %2", clipRoot.timeline.simplifiedTC(previousMix - currentFrame), clipRoot.timeline.simplifiedTC(currentFrame)))
                                }
                            } else {
                                clipRoot.timeline.showToolTip(KI18n.i18n("Mix duration: %1", clipRoot.timeline.simplifiedTC(currentFrame)))
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
                            clipRoot.timeline.showToolTip(KI18n.i18n("Mix duration: %1", clipRoot.timeline.simplifiedTC(clipRoot.mixDuration)))
                        }
                        onExited: {
                            if (pressed) {
                                return
                            }
                            mixOut.color = itemBorder.border.color
                            if (!mouseArea.containsMouse) {
                                clipRoot.timeline.showToolTip()
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
                            visible: trimInMixArea.pressed || (K.Core.activeTool === K.ToolType.SelectTool && !mouseArea.drag.active && parent.enabled)
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
                    property int id
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
                        font: K.UiUtils.smallestReadableFont
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
                        visible: K.KdenliveSettings.showmarkers && root.maxLabelWidth > K.UiUtils.baseSizeMedium && height < container.height && (markerBase.x > textMetrics.width || container.height > 2 * height)

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
                            font: K.UiUtils.smallestReadableFont
                            color: '#FFF'
                        }
                        MouseArea {
                            id: markerArea
                            z: 10
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            property bool shiftTrim: false
                            hoverEnabled: !clipRoot.isPanning
                            enabled: !clipRoot.isPanning
                            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: markerBase.markerText
                            ToolTip.delay: 1000
                            ToolTip.timeout: 5000
                            property int startPosition: 0
                            property real startX: 0
                            property int movingMarkerId: -1
                            property int destFrame: 0
                            onDoubleClicked: clipRoot.timeline.editMarker(clipRoot.clipId, markerBase.position)
                            onPressed: (mouse) => {
                                startPosition = markerBase.position
                                var pressedPoint = mapToItem(clipRoot, mouse.x, 0)
                                startX = pressedPoint.x
                                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                                movingMarkerId = markerBase.id
                                destFrame = startPosition
                                console.log('Trying to move marker mid: ', movingMarkerId)
                            }
                            onPositionChanged: (mouse) => {
                                if (pressed && movingMarkerId > -1) {
                                    var currentPoint = mapToItem(clipRoot, mouse.x, 0)
                                    var deltaPx = currentPoint.x - startX
                                    var deltaFrames = Math.round(deltaPx / clipRoot.timeScale * clipRoot.speed)
                                    var newFrame = startPosition + deltaFrames
                                    var timelineFrame = clipRoot.modelStart + (clipRoot.speed < 0
                                        ? (clipRoot.maxDuration - clipRoot.inPoint + Math.round(newFrame / clipRoot.speed))
                                        : (Math.round(newFrame / clipRoot.speed) - clipRoot.inPoint))
                                    var snappedTimelineFrame = clipRoot.timeline.suggestSnapPoint(timelineFrame, mouse.modifiers & Qt.ShiftModifier ? -1 : root.snapping)
                                    if (snappedTimelineFrame === clipRoot.modelStart || snappedTimelineFrame === (clipRoot.modelStart + clipRoot.clipDuration)) {
                                        snappedTimelineFrame = timelineFrame
                                    }
                                    if (clipRoot.speed < 0) {
                                        newFrame = Math.round((snappedTimelineFrame - clipRoot.modelStart) * clipRoot.speed) + clipRoot.maxDuration - clipRoot.inPoint
                                    } else {
                                        newFrame = Math.round((snappedTimelineFrame - clipRoot.modelStart) * clipRoot.speed) + clipRoot.inPoint
                                    }
                                    newFrame = Math.max(clipRoot.inPoint + 1, Math.min(newFrame, clipRoot.outPoint - 1))
                                    if (newFrame !== destFrame) {
                                        var success = clipRoot.timeline.moveClipMarkerWithoutUndo(clipRoot.clipId, movingMarkerId, newFrame)
                                        if (success) {
                                            destFrame = newFrame
                                        }
                                    }
                                }
                            }
                            onReleased: (mouse) => {
                                if (movingMarkerId > -1) {
                                    if (destFrame !== startPosition) {
                                        clipRoot.timeline.moveClipMarkerWithoutUndo(clipRoot.clipId, movingMarkerId, startPosition)
                                        clipRoot.timeline.moveClipMarker(clipRoot.clipId, startPosition, destFrame)
                                    }

                                }
                                movingMarkerId = -1
                            }
                            onClicked: (mouse) => {
                                if (destFrame !== startPosition) return
                                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                                    mouse.accepted = false
                                    return
                                }
                                proxy.position = clipRoot.modelStart + (clipRoot.speed < 0
                                ? clipRoot.maxDuration - clipRoot.inPoint + (Math.round(markerBase.position / clipRoot.speed))
                                : (Math.round(markerBase.position / clipRoot.speed) - clipRoot.inPoint))
                                clipRoot.controller.requestAddToSelection(clipRoot.clipId, shiftTrim ? false : true)
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
                            hoverEnabled: !clipRoot.isPanning
                            enabled: !clipRoot.isPanning
                            cursorShape: Qt.SizeHorCursor
                            acceptedButtons: Qt.LeftButton
                            preventStealing: true
                            
                            property bool isResizing: false
                            property real startX: 0
                            property real globalStartX: 0
                            property real startDuration: 0
                            property real startPosition: 0
                            property real originalEndPosition: 0
                            
                            onPressed: (mouse) => {
                                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
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
                                    clipRoot.timeline.resizeMarker(clipRoot.clipId, startPosition, markerBase.duration, true, markerBase.position)
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
                            hoverEnabled: !clipRoot.isPanning
                            enabled: !clipRoot.isPanning
                            cursorShape: Qt.SizeHorCursor
                            acceptedButtons: Qt.LeftButton
                            preventStealing: true
                            
                            property bool isResizing: false
                            property real startX: 0
                            property real globalStartX: 0
                            property real startDuration: 0
                            property real startPosition: 0
                            
                            onPressed: (mouse) => {
                                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
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
                                    clipRoot.timeline.resizeMarker(clipRoot.clipId, startPosition, markerBase.duration, false)
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
                    model: container.width > 3 * K.UiUtils.baseSizeMedium ? clipRoot.markers : 0
                    anchors.fill: parent
                    delegate: Loader {
                        id: loader
                        required property var modelData
                        property bool isInside: modelData.frame > clipRoot.inPoint && modelData.frame < clipRoot.outPoint
                        asynchronous: true
                        active: clipRoot.visible && isInside
                        Binding {
                            target: loader.item
                            property: "position"
                            value: loader.modelData.frame
                            when: loader.isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "markerText"
                            value: loader.modelData.comment
                            when: loader.isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "markerColor"
                            value: loader.modelData.color
                            when: loader.isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "hasRange"
                            value: loader.modelData.hasRange || false
                            when: loader.isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "duration"
                            value: loader.modelData.duration || 0
                            when: loader.isInside && loader.status == Loader.Ready
                        }
                        Binding {
                            target: loader.item
                            property: "id"
                            value: loader.modelData.id
                            when: loader.isInside && loader.status == Loader.Ready
                        }
                        sourceComponent: markerComponent
                    }
                }
            }

            MouseArea {
                // Left resize handle
                id: trimInMouseArea
                x: -itemBorder.border.width
                anchors.top: container.top
                height: container.height
                width: container.handleMini ? K.UiUtils.baseSizeMedium / 2 : K.UiUtils.baseSizeMedium
                visible: {
                    if (!enabled) {
                        return false
                    }

                    if (K.Core.activeTool === K.ToolType.SelectTool) {
                        return true
                    }

                    let hasMix = clipRoot.mixDuration > 0 || clipRoot.controller.hasClipEndMix(clipRoot.clipId)
                    if (K.Core.activeTool === K.ToolType.RippleTool && !hasMix) {
                        return true
                    }

                    return false
                }
                enabled: !clipRoot.isLocked && (pressed || (container.handleVisible && (mixArea.enabled || clipRoot.mixDuration == 0))) && clipRoot.clipId == dragProxy.draggedItem
                hoverEnabled: true
                drag.target: trimInMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                property int lastDuration
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                onPressed: mouse => {
                    clipRoot.originalX = clipRoot.x
                    clipRoot.originalDuration = clipRoot.clipDuration
                    lastDuration = clipRoot.clipDuration
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier
                                    && clipRoot.itemType != K.ClipType.Color && clipRoot.itemType != K.ClipType.Timeline
                                    && clipRoot.itemType != K.ClipType.Playlist && clipRoot.itemType != K.ClipType.Image
                    if (!shiftTrim && (clipRoot.grouped || clipRoot.controller.hasMultipleSelection())) {
                        clipRoot.initGroupTrim(clipRoot.clipId)
                    }
                    if (K.Core.activeTool === K.ToolType.RippleTool) { //TODO
                        clipRoot.timeline.requestStartTrimmingMode(clipRoot.clipId, false, false);
                    }
                }
                onReleased: {
                    trimIn.opacity = 0
                    x = -itemBorder.border.width
                    if (sizeChanged) {
                        clipRoot.trimmedIn(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                        if (!controlTrim && K.Core.activeTool !== K.ToolType.RippleTool) {
                            updateDrag()
                        } else {
                            root.endDrag()
                        }
                    } else {
                        if (K.Core.activeTool === K.ToolType.RippleTool) { //TODO
                            clipRoot.timeline.requestEndTrimmingMode();
                        } else if (clipRoot.timeline.selection.indexOf(clipRoot.clipId) === -1) {
                            clipRoot.controller.requestAddToSelection(clipRoot.clipId, shiftTrim ? false : true)
                        } else if (shiftTrim) {
                            clipRoot.controller.requestRemoveFromSelection(clipRoot.clipId)
                        }

                        root.groupTrimData = undefined
                    }
                }
                onDoubleClicked: {
                    if (clipRoot.mixDuration == 0) {
                        clipRoot.timeline.mixClip(clipRoot.clipId, -1)
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
                            if (K.Core.activeTool === K.ToolType.RippleTool) {
                                newDuration = clipRoot.originalDuration - delta
                            } else {
                                if (clipRoot.maxDuration > 0 && delta < -clipRoot.inPoint && !(mouse.modifiers & Qt.ControlModifier)) {
                                    delta = -clipRoot.inPoint
                                }
                                newDuration = clipRoot.clipDuration - delta
                            }
                            if (newDuration != lastDuration) {
                                sizeChanged = true
                                clipRoot.trimmingIn(clipRoot, newDuration, shiftTrim, controlTrim)
                                lastDuration = newDuration
                            }
                        }
                    }
                }
                onEntered: {
                    if (!pressed && !root.isDragging()) {
                        trimIn.opacity = 1
                        var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
                        var s = KI18n.i18n("In:%1, Position:%2", clipRoot.timeline.simplifiedTC(clipRoot.inPoint),clipRoot.timeline.simplifiedTC(clipRoot.modelStart))
                        clipRoot.timeline.showToolTip(s)
                        if (clipRoot.mixDuration == 0) {
                            clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Ctrl drag</b> to change speed, <b>Double click</b> to mix with adjacent clip"))
                        } else {
                            clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to change mix duration"))
                        }
                    }
                }
                onExited: {
                    if (!pressed) {
                        trimIn.opacity = 0
                        if (!mouseArea.containsMouse) {
                            clipRoot.timeline.showToolTip()
                        } else {
                            clipRoot.showClipInfo()
                        }
                        if (!fadeInMouseArea.containsMouse) {
                            clipRoot.timeline.showKeyBinding()
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
                width: container.handleMini ? K.UiUtils.baseSizeMedium / 2 : K.UiUtils.baseSizeMedium
                hoverEnabled: true
                visible: enabled && (K.Core.activeTool === K.ToolType.SelectTool
                                     || (K.Core.activeTool === K.ToolType.RippleTool && clipRoot.mixDuration <= 0))
                enabled: !clipRoot.isLocked && (pressed || container.handleVisible) && clipRoot.clipId == dragProxy.draggedItem
                property bool shiftTrim: false
                property bool controlTrim: false
                property bool sizeChanged: false
                property int lastDuration
                cursorShape: (enabled && (containsMouse || pressed) ? Qt.SizeHorCursor : Qt.OpenHandCursor)
                drag.target: trimOutMouseArea
                drag.axis: Drag.XAxis
                drag.smoothed: false

                onPressed: mouse => {
                    clipRoot.originalDuration = clipRoot.clipDuration
                    lastDuration = clipRoot.clipDuration
                    anchors.right = undefined
                    shiftTrim = mouse.modifiers & Qt.ShiftModifier
                    controlTrim = mouse.modifiers & Qt.ControlModifier
                               && clipRoot.itemType != K.ClipType.Color
                               && clipRoot.itemType != K.ClipType.Timeline
                               && clipRoot.itemType != K.ClipType.Playlist
                               && clipRoot.itemType != K.ClipType.Image
                    if (!shiftTrim && (clipRoot.grouped || clipRoot.controller.hasMultipleSelection())) {
                        clipRoot.initGroupTrim(clipRoot.clipId)
                    }
                    if (K.Core.activeTool === K.ToolType.RippleTool) {
                        clipRoot.timeline.requestStartTrimmingMode(clipRoot.clipId, false, true);
                    }
                }
                onReleased: {
                    trimOut.opacity = 0
                    anchors.right = parent.right
                    if (sizeChanged) {
                        clipRoot.trimmedOut(clipRoot, shiftTrim, controlTrim)
                        sizeChanged = false
                        if (!controlTrim && K.Core.activeTool !== K.ToolType.RippleTool) {
                            updateDrag()
                        } else {
                            root.endDrag()
                        }
                    } else {
                        if (K.Core.activeTool === K.ToolType.RippleTool) {
                            clipRoot.timeline.requestEndTrimmingMode();
                        } else if (clipRoot.timeline.selection.indexOf(clipRoot.clipId) === -1) {
                            clipRoot.controller.requestAddToSelection(clipRoot.clipId, shiftTrim ? false : true)
                        } else if (shiftTrim) {
                            clipRoot.controller.requestRemoveFromSelection(clipRoot.clipId)
                        }
                        root.groupTrimData = undefined
                    }
                }
                onDoubleClicked: {
                    clipRoot.timeline.mixClip(clipRoot.clipId, 1)
                }
                onPositionChanged: mouse => {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newDuration = Math.round((x + width + itemBorder.border.width) / clipRoot.timeScale)
                        if (clipRoot.maxDuration > 0 && (newDuration > clipRoot.maxDuration - clipRoot.inPoint) && !(mouse.modifiers & Qt.ControlModifier)) {
                            newDuration = clipRoot.maxDuration - clipRoot.inPoint
                        }
                        if (newDuration != lastDuration) {
                            sizeChanged = true
                            clipRoot.trimmingOut(clipRoot, newDuration, shiftTrim, controlTrim)
                            lastDuration = newDuration
                        }
                    }
                }
                onEntered: {
                    if (!pressed && !root.isDragging()) {
                        trimOut.opacity = 1
                        var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
                        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
                        var s = KI18n.i18n("Out:%1, Position:%2", clipRoot.timeline.simplifiedTC(clipRoot.outPoint),clipRoot.timeline.simplifiedTC(clipRoot.modelStart + clipRoot.clipDuration))
                        clipRoot.timeline.showToolTip(s)
                        if (!fadeOutMouseArea.containsMouse) {
                            clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Ctrl drag</b> to change speed, <b>Double click</b> to mix with adjacent clip"))
                        }
                    }
                }
                onExited: {
                    if (!pressed) {
                        trimOut.opacity = 0
                         if (!mouseArea.containsMouse) {
                            clipRoot.timeline.showToolTip()
                         } else {
                             var text = KI18n.i18n("%1 (%2-%3), Position: %4, Duration: %5".arg(clipRoot.clipName)
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.inPoint))
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.outPoint))
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.modelStart))
                        .arg(clipRoot.timeline.simplifiedTC(clipRoot.clipDuration)))
                             clipRoot.timeline.showToolTip(text)
                         }
                         if (!fadeOutMouseArea.containsMouse) {
                             clipRoot.timeline.showKeyBinding()
                         }
                    }
                }
            }

            K.TimelineTriangle {
                // Green fade in triangle
                id: fadeInTriangle
                color: 'green'
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
                color: 'red'
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
                        PropertyChanges { nameContainer.opacity: 1.0 }
                    },
                    State { when: clipRoot.hideDecorations
                        PropertyChanges { nameContainer.opacity: 0.0 }
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
                    visible: K.KdenliveSettings.uiDebugMode
                    anchors.left: parent.left
                    anchors.leftMargin: clipRoot.timeremap ? debugCidRect.height : 0
                    Text {
                        // Clip ID text
                        id: debugCid
                        text: clipRoot.clipId
                        font: K.UiUtils.smallestReadableFont
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
                    visible: clipRoot.width > K.UiUtils.baseSizeMedium
                    anchors.left: debugCidRect.visible ? debugCidRect.right : parent.left
                    anchors.leftMargin: clipRoot.timeremap ? labelRect.height : 0
                    Text {
                        // Clip name text
                        id: label
                        property string clipNameString: (clipRoot.isAudio && clipRoot.multiStream) ? ((clipRoot.audioStream > 10000 ? 'Merged' : clipRoot.aStreamIndex) + '|' + clipRoot.clipName ) : clipRoot.clipName
                        text: (clipRoot.speed != 1.0 ? ('[' + Math.round(clipRoot.speed*100) + '%] ') : '') + clipNameString
                        font: K.UiUtils.smallestReadableFont
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
                    visible: labelRect.visible && clipRoot.positionOffset != 0
                    MouseArea {
                        id: offsetArea
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        anchors.fill: parent
                        onClicked: {
                            clearAndMove(clipRoot.positionOffset)
                        }
                        onEntered: {
                            var text = clipRoot.positionOffset < 0 ? KI18n.i18n("Offset: -%1", clipRoot.timeline.simplifiedTC(-clipRoot.positionOffset))
                                                          : KI18n.i18n("Offset: %1", clipRoot.timeline.simplifiedTC(clipRoot.positionOffset))
                            text += KI18n.i18n(" <b>Click</b> to align clips")
                            clipRoot.timeline.showToolTip(text)
                        }
                        onExited: {
                            clipRoot.timeline.showToolTip()
                        }
                        Text {
                            id: offsetLabel
                            text: clipRoot.positionOffset
                            font: K.UiUtils.smallestReadableFont
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
                                clipRoot.timeline.setEffectsEnabled(clipRoot.clipId, !clipRoot.isStackEnabled)
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
                            family: K.UiUtils.smallestReadableFont.family
                            pointSize: K.UiUtils.smallestReadableFont.pointSize
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
                        text: KI18n.i18nc("@label The first letter of Proxy, used as abbreviation", "P")
                        font.pointSize: fontMetrics.font.pointSize +1
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
                        font.pointSize: fontMetrics.font.pointSize +1
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
                visible: status == Loader.Ready && clipRoot.showKeyframes && clipRoot.keyframeModel && hasKeyframes && clipRoot.width > 2 * K.UiUtils.baseSizeMedium
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
                    clipRoot.color: clipRoot.timeline.lockedColor
                    clipRoot.opacity: 0.8
                    clipRoot.z: 0
                }
            },
            State {
                name: 'normal'
                when: clipRoot.selected === false
                PropertyChanges {
                    clipRoot.color: Qt.darker(getColor(), 1.5)
                    clipRoot.z: 0
                }
            },
            State {
                name: 'selectedClip'
                when: clipRoot.selected === true
                PropertyChanges {
                    clipRoot.color: getColor()
                    clipRoot.z: 3
                }
            }
        ]

        MouseArea {
            // Add start composition area
            id: compInArea
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: Math.min(K.UiUtils.baseSizeMedium, container.height / 3)
            height: width
            hoverEnabled: !clipRoot.isPanning
            cursorShape: Qt.PointingHandCursor
            visible: !clipRoot.isAudio
            enabled: !clipRoot.isPanning && !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId && compositionIn.visible
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                root.mainItemId = -1
                clipRoot.timeline.addCompositionToClip('', clipRoot.clipId, 0)
            }
            onEntered: {
                clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Click</b> to add composition"))
            }
            onExited: {
                clipRoot.timeline.showKeyBinding()
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
            width: Math.min(K.UiUtils.baseSizeMedium, container.height / 3)
            height: width
            hoverEnabled: !clipRoot.isPanning
            cursorShape: Qt.PointingHandCursor
            enabled: !clipRoot.isPanning && !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId && compositionOut.visible
            visible: !clipRoot.isAudio
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                root.mainItemId = -1
                clipRoot.timeline.addCompositionToClip('', clipRoot.clipId, clipRoot.clipDuration - 1)
            }
            onEntered: {
                clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Click</b> to add composition"))
            }
            onExited: {
                clipRoot.timeline.showKeyBinding()
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
            width: Math.min(K.UiUtils.baseSizeMedium, container.height / 3)
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
            property string fadeString: clipRoot.timeline.simplifiedTC(clipRoot.fadeOut)
            drag.smoothed: false
            onClicked: {
                if (clipRoot.fadeOut == 0) {
                    clipRoot.timeline.adjustFade(clipRoot.clipId, 'fadeout', 0, -2)
                }
            }
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                startFadeOut = clipRoot.fadeOut
                dragStarted = startFadeOut > 0
                startMousePos = mouse.x
                anchors.right = undefined
                fadeOutCanvas.opacity = 0.6
            }
            onReleased: {
                fadeOutCanvas.opacity = 0.4
                anchors.right = parent.right
                var duration = clipRoot.fadeOut
                clipRoot.timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, startFadeOut)
                //bubbleHelp.hide()
                clipRoot.timeline.showToolTip()
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
                        clipRoot.timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        clipRoot.timeline.showToolTip(KI18n.i18n("Fade out: %1", fadeString))
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    if (clipRoot.fadeOut > 0) {
                        clipRoot.timeline.showToolTip(KI18n.i18n("Fade out: %1", fadeString))
                    } else {
                        clipRoot.showClipInfo()
                    }
                    clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to adjust fade, <b>Click</b> to add default duration fade"))
                }
            }
            onExited: {
                if (!pressed) {
                    clipRoot.timeline.showKeyBinding()
                    if (mouseArea.containsMouse) {
                        clipRoot.showClipInfo()
                    } else {
                        clipRoot.timeline.showToolTip()
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
                enabled: !clipRoot.isLocked && !dragProxy.isComposition
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
            width: Math.min(K.UiUtils.baseSizeMedium, container.height / 3)
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
            property string fadeString: clipRoot.timeline.simplifiedTC(clipRoot.fadeIn)
            visible: container.handleVisible && mouseArea.containsMouse && !dragProxyArea.pressed
            onClicked: {
                if (clipRoot.fadeIn == 0) {
                    clipRoot.timeline.adjustFade(clipRoot.clipId, 'fadein', 0, -2)
                }
            }
            onPressed: mouse => {
                if (mouse.modifiers & Qt.ControlModifier && (K.Core.activeTool === K.ToolType.SelectTool || K.Core.activeTool === K.ToolType.RippleTool)) {
                    mouse.accepted = false
                    return
                }
                startFadeIn = clipRoot.fadeIn
                dragStarted = startFadeIn > 0
                startMousePos = mouse.x
                anchors.left = undefined
                fadeInTriangle.opacity = 0.6
                // parentTrack.clipSelected(clipRoot, parentTrack) TODO
            }
            onReleased: {
                fadeInTriangle.opacity = 0.4
                clipRoot.timeline.adjustFade(clipRoot.clipId, 'fadein', clipRoot.fadeIn, startFadeIn)
                //bubbleHelp.hide()
                clipRoot.timeline.showToolTip()
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
                        clipRoot.timeline.adjustFade(clipRoot.clipId, 'fadein', duration, -1)
                        // Show fade duration as time in a "bubble" help.
                        clipRoot.timeline.showToolTip(KI18n.i18n("Fade in: %1", fadeString))
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    if (clipRoot.fadeIn > 0) {
                        clipRoot.timeline.showToolTip(KI18n.i18n("Fade in: %1", fadeString))
                    } else {
                        clipRoot.showClipInfo()
                    }
                    clipRoot.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to adjust fade, <b>Click</b> to add default duration fade"))
                }
            }
            onExited: {
                if (!pressed) {
                    clipRoot.timeline.showKeyBinding()
                    if (mouseArea.containsMouse) {
                        clipRoot.showClipInfo()
                    } else {
                        clipRoot.timeline.showToolTip()
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
                enabled: !clipRoot.isLocked && !dragProxy.isComposition
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
            property color color: clipRoot.timeline.trimmingMainClip === clipRoot.clipId ? clipRoot.timeline.selectionColor : activePalette.highlight
            anchors.bottom: container.bottom
            height: container.height
            width: clipRoot.maxDuration * clipRoot.timeScale
            x: - (clipRoot.inPoint - clipRoot.slipOffset) * clipRoot.timeScale
            visible: K.Core.activeTool === K.ToolType.SlipTool && clipRoot.selected && clipRoot.maxDuration > 0 // don't show for endless clips
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
            text: KI18n.i18n("Slip Clip")
            font: K.UiUtils.smallestReadableFont
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            color: activePalette.highlightedText
            visible: slipControler.visible
            opacity: 1
        }
    }
}
