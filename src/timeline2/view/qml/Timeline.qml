/*
    SPDX-FileCopyrightText: 2017-2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2020 Sashmita Raghav
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-FileCopyrightText: 2025 Abdias J

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQml.Models 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import org.kde.kdenlive as K
import 'TimelineLogic.js' as Logic
import 'Utils.js' as Utils

Rectangle {
    id: root
    objectName: "timelineview"
    SystemPalette { id: activePalette }
    color: activePalette.window
    property bool debugmode: K.KdenliveSettings.uiDebugMode
    property bool showClipOverlays: K.KdenliveSettings.showClipOverlays
    property bool validMenu: false
    property bool subtitleMoving: false
    property var subtitleItem
    property color textColor: activePalette.text
    property var groupTrimData
    property bool trimInProgress: false
    property bool isPanning: tracksArea.isCursorHidden
    property bool captureRightClick: false
    property bool dragInProgress: dragProxyArea.pressed || dragProxyArea.drag.active || groupTrimData !== undefined || spacerGroup > -1 || trimInProgress || clipDropArea.containsDrag || compoArea.containsDrag
    property int trimmingOffset: 0
    property int trimmingClickFrame: -1

    function screenForGlobalPos(globalPos) {
        const screens = Qt.application.screens
        if (!screens || screens.length === 0) {
            return null
        }
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i]
            const left = s.virtualX
            const right = s.virtualX + s.width
            const top = s.virtualY
            const bottom = s.virtualY + s.height
            if (globalPos.x >= left && globalPos.x < right && globalPos.y >= top && globalPos.y < bottom) {
                return s
            }
        }
        // Fallback to nearest screen center if the point is slightly outside
        let best = screens[0]
        let bestDist = Number.MAX_VALUE
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i]
            const cx = s.virtualX + s.width / 2
            const cy = s.virtualY + s.height / 2
            const dist = Math.pow(globalPos.x - cx, 2) + Math.pow(globalPos.y - cy, 2)
            if (dist < bestDist) {
                bestDist = dist
                best = s
            }
        }
        return best
    }

    function screenEdges(globalPos) {
        const s = screenForGlobalPos(globalPos)
        if (!s) {
            return null
        }
        return {
            left: s.virtualX,
            right: s.virtualX + s.width,
            top: s.virtualY,
            bottom: s.virtualY + s.height
        }
    }

    Timer {
        id: doubleClickTimer
        interval: root.doubleClickInterval
        repeat: false
    }

    signal clipClicked()
    signal showClipMenu(int cid)
    signal showMixMenu(int cid)
    signal showCompositionMenu()
    signal showTimelineMenu()
    signal showRulerMenu()
    signal showHeaderMenu()
    signal showTargetMenu(int ix)
    signal markerActivated(int frame)
    signal zoomIn(bool onMouse)
    signal zoomOut(bool onMouse)
    signal processingDrag(bool dragging)
    signal showSubtitleClipMenu()
    signal updateTimelineMousePos(int frame, int duration)

    readonly property font miniFont: ({
        pixelSize: miniFontSize
    })

    FontMetrics {
        id: fontMetrics
        font: miniFont
    }

    onDragInProgressChanged: {
        processingDrag(!root.dragInProgress)
    }

    function endBinDrag() {
        clipDropArea.processDrop()
        timeline.keepAudioTargets(false)
    }

    function startAudioRecord(tid) {
        var tk = Logic.getTrackById(tid)
        recordPlaceHolder.y = Qt.binding(function() { return tk.y + subtitleTrack.height })
        recordPlaceHolder.height = Qt.binding(function() { return tk.height })
        var startFrame = root.consumerPosition
        recordStartPlaceHolder.x = Qt.binding(function() { return startFrame * root.timeScale })
        recordPlaceHolder.visible = true
        recordPlaceHolder.width = Qt.binding(function() { return audiorec.recDuration * root.timeScale })
    }

    function stopAudioRecord() {
        recordPlaceHolder.visible = false
        recordStartPlaceHolder.x = 0
        recordStartPlaceHolder.y = 0
        recordPlaceHolder.width = 0
        recordPlaceHolder.height = 0
    }

    function fitZoom() {
        return scrollView.width / (timeline.duration * 1.1)
    }

    function scrollPos() {
        return scrollView.contentX
    }

    function goToStart(pos) {
        scrollView.contentX = pos
    }

    function switchSubtitleTrack() {
        if (subtitleTrack.height > root.collapsedHeight) {
            subtitleTrack.height = root.collapsedHeight
        } else {
            subtitleTrack.height = root.baseUnit * 2.5 * ((maxSubLayer == 0)? 2: (maxSubLayer + 1))
        }
    }

    function highlightSub(ix) {
        var currentSub = subtitlesRepeater.itemAt(ix)
        currentSub.editText()
    }

    function checkDeletion(itemId) {
        if (dragProxy.draggedItem === itemId) {
            endDrag()
        }
        if (itemId === mainItemId) {
            mainItemId = -1
        }
    }

    function getActiveTrackStreamPos() {
        // Return the relative y click position, to display the context menu
        return Logic.getTrackYFromId(timeline.activeTrack) + rulercontainer.height - scrollView.contentY
    }

    function updatePalette() {
        root.color = activePalette.window
        root.textColor = activePalette.text
        playhead.fillColor = activePalette.windowText
        ruler.dimmedColor = (activePalette.text.r + activePalette.text.g + activePalette.text.b > 1.5) ? Qt.darker(activePalette.text, 1.3) : Qt.lighter(activePalette.text, 1.3)
        ruler.dimmedColor2 = (activePalette.text.r + activePalette.text.g + activePalette.text.b > 1.5) ? Qt.darker(activePalette.text, 2.2) : Qt.lighter(activePalette.text, 2.2)
        ruler.repaintRuler()
        // Disable caching for track header icons
        root.paletteUnchanged = false
    }

    function moveSelectedTrack(offset) {
        var newTrack
        var max = tracksRepeater.count;
        if (timeline.activeTrack < 0 ) {
            if (offset <0) {
                newTrack = -2
            } else {
                if (showSubtitles) {
                    newTrack = 0
                } else {
                    newTrack = max
                }
            }
        } else {
            var cTrack = Logic.getTrackIndexFromId(timeline.activeTrack)
            newTrack = cTrack + offset
        }
        if (newTrack < 0) {
            if (showSubtitles && newTrack === -1) {
                timeline.activeTrack = -2
                return
            }
            newTrack = max - 1;
        } else if (newTrack >= max) {
            if (showSubtitles) {
                timeline.activeTrack = -2
                return
            }
            newTrack = 0;
        }
        timeline.activeTrack = tracksRepeater.itemAt(newTrack).trackInternalId
    }

    function zoomByWheel(wheel) {
        if (wheel.modifiers & Qt.AltModifier) {
            // Seek to next snap
            if (wheel.angleDelta.x > 0) {
                timeline.triggerAction('monitor_seek_snap_backward')
            } else {
                timeline.triggerAction('monitor_seek_snap_forward')
            }
        } else if (wheel.modifiers & Qt.ControlModifier) {
            root.wheelAccumulatedDelta += wheel.angleDelta.y;
            // Zoom
            if (root.wheelAccumulatedDelta >= defaultDeltasPerStep) {
                root.zoomIn(true);
                root.wheelAccumulatedDelta = 0;
            } else if (root.wheelAccumulatedDelta <= -defaultDeltasPerStep) {
                root.zoomOut(true);
                root.wheelAccumulatedDelta = 0;
            }
        } else if (wheel.modifiers & Qt.ShiftModifier) {
            if (K.KdenliveSettings.scrollvertically || rubberSelect.visible) {
                horizontalScroll(wheel)
            } else {
                verticalScroll(wheel)
            }
        } else {
            if (K.KdenliveSettings.scrollvertically) {
                verticalScroll(wheel)
            } else {
                horizontalScroll(wheel)
            }
        }
        wheel.accepted = true
    }

    function horizontalScroll(wheel) {
        var initialX = scrollView.contentX
        if (wheel.angleDelta.y < 0) {
            scrollView.contentX = Math.max(0, Math.min(scrollView.contentX - wheel.angleDelta.y, timeline.fullDuration * root.timeScale - scrollView.width))
        } else {
            scrollView.contentX = Math.max(scrollView.contentX - wheel.angleDelta.y, 0)
        }
        if (dragProxyArea.pressed && dragProxy.draggedItem > -1) {
            dragProxy.x += scrollView.contentX - initialX
            dragProxyArea.moveItem()
        } else if (rubberSelect.visible) {
            var newX = tracksArea.mouseX + scrollView.contentX
            if (newX < rubberSelect.originX) {
                rubberSelect.x = newX
                rubberSelect.width = rubberSelect.originX - newX
            } else {
                rubberSelect.x = rubberSelect.originX
                rubberSelect.width = newX - rubberSelect.originX
            }
        }
    }

    function verticalScroll(wheel) {
        var initialY = scrollView.contentY
        if (wheel.angleDelta.y < 0) {
            scrollView.contentY = Math.max(0, Math.min(scrollView.contentY - wheel.angleDelta.y, trackHeaders.height + subtitleTrackHeader.height - tracksArea.height + horZoomBar.height + ruler.height))
        } else {
            scrollView.contentY = Math.max(scrollView.contentY - wheel.angleDelta.y, 0)
        }
        if (dragProxyArea.pressed && dragProxy.draggedItem > -1) {
            dragProxyArea.moveItem()
        }
    }

    function continuousScrolling(x, y, upMove = 10) {
        // This provides continuous scrolling at the left/right edges.
        var maxScroll = trackHeaders.height + subtitleTrack.height
        y = Math.min(y, maxScroll)
        y += ruler.height
        if (x > scrollView.contentX + scrollView.width - root.baseUnit * 3) {
            scrollTimer.horizontal = root.baseUnit
            scrollTimer.interval = 80
            scrollTimer.start()
        } else if (x < 50) {
            scrollView.contentX = 0;
            scrollTimer.horizontal = 0
            scrollTimer.stop()
        } else if (x < scrollView.contentX + root.baseUnit * 3) {
            scrollTimer.horizontal = -root.baseUnit
            scrollTimer.interval = 80
            scrollTimer.start()
        } else {
            if (y > scrollView.contentY + scrollView.height + ruler.height - root.baseUnit) {
                scrollTimer.vertical = root.baseUnit
                scrollTimer.horizontal = 0
                scrollTimer.interval = 200
                scrollTimer.start()
            } else if (upMove > 6 && scrollView.contentY > 0 && (y - (scrollView.contentY + ruler.height ) < root.baseUnit)) {
                scrollTimer.vertical = -root.baseUnit
                scrollTimer.horizontal = 0
                scrollTimer.interval = 200
                scrollTimer.start()
            } else {
                scrollTimer.vertical = 0
                scrollTimer.horizontal = 0
                scrollTimer.stop()
            }
        }
    }

    function getMouseFrame() {
        return getMousePos() / root.timeScale
    }

    function getMousePos() {
        var posInWidget = timeline.getMousePosInTimeline()
        return Math.max(0, scrollView.contentX + posInWidget.x - trackHeaders.width)
    }

    function getMouseX() {
        var posInWidget = timeline.getMousePosInTimeline()
        return Math.max(0, posInWidget.x - trackHeaders.width)
    }

    function getScrollPos() {
        return scrollView.contentX
    }

    function setScrollPos(pos) {
        return scrollView.contentX = pos
    }

    function getCopiedItemId() {
        return copiedClip
    }

    function getMouseTrackFromPos(offset) {
        return Logic.getTrackIdFromPos(offset - ruler.height + scrollView.contentY - subtitleTrack.height)
    }

    function getMouseTrack() {
        var posInWidget = timeline.getMousePosInTimeline()
        return Logic.getTrackIdFromPos(posInWidget.y - ruler.height + scrollView.contentY - subtitleTrack.height)
    }

function getTrackColor(audio, header) {
    var isDarkTheme = activePalette.window.hslLightness < activePalette.windowText.hslLightness
    var lighterColor = activePalette.base.hslLightness > activePalette.alternateBase.hslLightness ? activePalette.base : activePalette.alternateBase;
    var darkerColor = activePalette.base.hslLightness > activePalette.alternateBase.hslLightness ? activePalette.alternateBase : activePalette.base;

    if (isDarkTheme) {
        // For dark themes, choose the lighter color
        return header ? Qt.darker(lighterColor, 1.05) : lighterColor;
    } else {
        // For light themes, choose the darker color
        return header ? Qt.darker(darkerColor, 1.05) : darkerColor;
    }

}

    function centerViewOnCursor() {
        scrollView.contentX = Math.max(0, root.consumerPosition * root.timeScale - (scrollView.width / 2))
    }

    function clearDropData() {
        clipBeingDroppedId = -1
        droppedPosition = -1
        droppedTrack = -1
        clipDropArea.lastDragUuid = ""
        scrollTimer.running = false
        scrollTimer.stop()
        sameTrackIndicator.visible = false
    }

    function isDragging() {
        return dragInProgress
    }

    function initDrag(itemObject, itemCoord, itemId, itemPos, itemTrack, isComposition) {
        dragProxy.x = itemObject.modelStart * timeScale
        dragProxy.y = itemCoord.y
        dragProxy.width = itemObject.clipDuration * timeScale
        dragProxy.height = itemCoord.height
        dragProxy.masterObject = itemObject
        dragProxy.draggedItem = itemId
        dragProxy.sourceTrack = itemTrack
        dragProxy.sourceFrame = itemPos
        dragProxy.isComposition = isComposition
        dragProxy.verticalOffset = isComposition ? itemObject.displayHeight : 0
    }

    function endDragIfFocused(itemId) {
        if (dragProxy.draggedItem == itemId) {
            endDrag()
        }
    }

    function endDrag() {
        dragProxy.draggedItem = -1
        dragProxy.x = 0
        dragProxy.y = 0
        dragProxy.width = 0
        dragProxy.height = 0
        dragProxy.verticalOffset = 0
        doubleClickTimer.stop()
        root.blockAutoScroll = false
    }

    function regainFocus(mousePos) {
        var currentMouseTrack = Logic.getTrackIdFromPos(mousePos.y - ruler.height - subtitleTrack.height + scrollView.contentY)
        // Try to find correct item
        var sourceTrack = Logic.getTrackById(currentMouseTrack)
        var mouseYPos = (mousePos.y - ruler.height + scrollView.contentY) - sourceTrack.y
        var allowComposition = mouseYPos > sourceTrack.height / 2
        var tentativeClip = undefined
        if (allowComposition) {
            tentativeClip = getItemAtPos(currentMouseTrack, (mousePos.x - trackHeaders.width + scrollView.contentX), true)
            if (tentativeClip) {
                // Ensure mouse is really over the composition
                if (!tentativeClip.doesContainMouse(root.mapToItem(tentativeClip, mousePos.x, mousePos.y))) {
                    tentativeClip = undefined
                }
            }
        }
        if (!tentativeClip) {
            tentativeClip = getItemAtPos(currentMouseTrack, (mousePos.x - trackHeaders.width + scrollView.contentX), false)
        }

        if (tentativeClip && tentativeClip.clipId && tentativeClip.doesContainMouse(root.mapToItem(tentativeClip, mousePos.x, mousePos.y)) && root.activeTool !== K.ToolType.SpacerTool) {
            dragProxy.draggedItem = tentativeClip.clipId
            var tk = controller.getItemTrackId(tentativeClip.clipId)
            dragProxy.x = tentativeClip.x
            dragProxy.y = sourceTrack.y + (tentativeClip.isComposition ? tentativeClip.displayHeight : tentativeClip.y)
            //+ Logic.getTrackYFromId(tk)
            dragProxy.width = tentativeClip.width
            dragProxy.height = tentativeClip.itemHeight()
            dragProxy.masterObject = tentativeClip
            dragProxy.sourceTrack = tk
            dragProxy.sourceFrame = tentativeClip.modelStart
            dragProxy.isComposition = tentativeClip.isComposition
            dragProxy.verticalOffset = tentativeClip.isComposition ? tentativeClip.displayHeight : 0
            //console.log('missing item', tentativeClip.clipId, ', COORDS: ', dragProxy.x, 'x', dragProxy.y,'-',dragProxy.width,'x',dragProxy.height, ', TK id: ', tk, ', TKY: ', Logic.getTrackYFromId(tk),' STARTFRAME: ', dragProxy.sourceFrame)
        } else {
            console.log('item not found')
            if (dragProxy.draggedItem > -1) {
                endDrag()
            }
        }
        root.blockAutoScroll = false
    }

    function getAudioTracksCount(){
        var audioCount = 0;
        for (var i = 0; i < trackHeaderRepeater.count; i++) {
            if(trackHeaderRepeater.itemAt(i).isAudio) {
                audioCount++;
            }
        }
        return audioCount;
    }

    function animateLockButton(trackId){
        // TODO: fix then multiple subtitles track is implemented
        if (trackId == -2) {
            subtitleTrackHeader.animateLock()
        } else {
            Logic.getTrackHeaderById(trackId).animateLock()
        }
    }

    function getItemAtPos(tk, posx, compositionWanted) {
        var track = Logic.getTrackById(tk)
        if (track == undefined || track.children == undefined) {
            return undefined
        }
        var container = track.children[0]
        var tentativeClip = undefined
        for (var i = 0 ; i < container.children.length; i++) {
            if (container.children[i].children.length === 0 || container.children[i].children[0].children.length === 0) {
                continue
            }
            tentativeClip = container.children[i].children[0].childAt(posx, compositionWanted ? 5 : 0)
            if (tentativeClip && tentativeClip.clipId && (tentativeClip.isComposition === compositionWanted)) {
                break
            }
        }
        return tentativeClip
    }
    Keys.onDownPressed: {
        root.moveSelectedTrack(1)
    }
    Keys.onUpPressed: {
        root.moveSelectedTrack(-1)
    }
    Keys.onShortcutOverride: event => {event.accepted = focus && event.key === Qt.Key_F2}
    Keys.onPressed: event => {
        if (event.key == Qt.Key_F2) {
            Logic.getTrackHeaderById(timeline.activeTrack).editName()
            event.accepted = true;
        }
    }

    property int activeTool: K.ToolType.SelectTool
    property int baseUnit: Math.max(12, fontMetrics.font.pixelSize)
    property int minClipWidthForViews: 1.5 * baseUnit
    property real fontUnit: fontMetrics.font.pointSize
    property int collapsedHeight: Math.max(28, baseUnit * 1.8)
    property int minHeaderWidth: 6 * collapsedHeight
    property int headerWidth: Math.max(minHeaderWidth, timeline.headerWidth())
    property bool autoTrackHeight: timeline.autotrackHeight
    property color selectedTrackColor: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.2)
    property color frameColor: Qt.rgba(activePalette.shadow.r, activePalette.shadow.g, activePalette.shadow.b, 0.5)
    property bool autoScrolling: timeline.autoScroll
    property bool blockAutoScroll: false
    property int duration: timeline.duration
    property color audioColor: Utils.mixColors(activePalette.base, K.KdenliveSettings.thumbColor1, 0.3)
    property color videoColor: timeline.videoColor
    property color titleColor: timeline.titleColor
    property color imageColor: timeline.imageColor
    property color slideshowColor: timeline.slideshowColor
    property color lockedColor: timeline.lockedColor
    property color selectionColor: timeline.selectionColor
    property color groupColor: timeline.groupColor
    property int doubleClickInterval: timeline.doubleClickInterval()
    property int mainItemId: -1
    property int clickFrame: -1
    property int clipBeingDroppedId: -1
    property string clipBeingDroppedData
    property int droppedPosition: -1
    property int droppedTrack: -1
    property int clipBeingMovedId: -1
    property int consumerPosition: proxy ? proxy.position : -1
    property int spacerGroup: -1
    property int spacerTrack: -1
    property int spacerFrame: -1
    property int finalSpacerFrame: -1
    property int spacerClickFrame: -1
    property bool spacerGuides: false
    property real timeScale: timeline.scaleFactor
    property int snapping: (K.KdenliveSettings.snaptopoints && (root.timeScale < 2 * baseUnit)) ? Math.floor(baseUnit / (root.timeScale > 3 ? root.timeScale / 2 : root.timeScale)) : -1
    property var timelineSelection: timeline.selection
    property int selectedMix: timeline.selectedMix
    property var selectedGuides: []
    property int trackHeight
    property int copiedClip: -1
    property int zoomOnMouse: -1
    property bool zoomOnBar: false // Whether the scaling was done with the zoombar
    property string addedSequenceName : controller.visibleSequenceName
    property int viewActiveTrack: timeline.activeTrack
    property int wheelAccumulatedDelta: 0
    readonly property int defaultDeltasPerStep: 120
    property bool seekingFinished : proxy ? proxy.seekFinished : true
    property int scrollMin: scrollView.contentX / root.timeScale
    property int scrollMax: scrollMin + scrollView.contentItem.width / root.timeScale
    property double dar: 16/9
    property bool paletteUnchanged: true
    property int maxLabelWidth: 20 * root.baseUnit * Math.sqrt(root.timeScale)
    property bool showSubtitles: false
    property bool subtitlesWarning: timeline.subtitlesWarning
    property bool subtitlesLocked: timeline.subtitlesLocked
    property bool subtitlesDisabled: timeline.subtitlesDisabled
    property int maxSubLayer: timeline.maxSubLayer
    property int trackTagWidth: fontMetrics.boundingRect("M").width * ((getAudioTracksCount() > 9) || (trackHeaderRepeater.count - getAudioTracksCount() > 9)  ? 3 : 2)
    property int spacerMinPos: 0
    property int spacerMaxPos: -1

    onMaxSubLayerChanged: {
        subtitleTrack.height = showSubtitles? root.baseUnit * 2.5 * ((maxSubLayer == 0)? 2: (maxSubLayer + 1)) : 0
    }

    onAutoTrackHeightChanged: {
        trackHeightTimer.stop()
        if (root.autoTrackHeight) {
            timeline.autofitTrackHeight(scrollView.height - subtitleTrack.height, root.collapsedHeight)
        }
    }

    onSeekingFinishedChanged : {
        playhead.opacity = seekingFinished ? 1 : 0.5
    }

    onShowSubtitlesChanged: {
        subtitleTrack.height = showSubtitles? root.baseUnit * 2.5 * ((maxSubLayer == 0)? 2: (maxSubLayer + 1)) : 0
        if (root.autoTrackHeight) {
            timeline.autofitTrackHeight(scrollView.height - subtitleTrack.height, root.collapsedHeight)
        }
    }
    Timer {
        id: trackHeightTimer
        interval: 300; running: false; repeat: false
        onTriggered: timeline.autofitTrackHeight(scrollView.height - subtitleTrack.height, root.collapsedHeight)
    }

    onHeightChanged: {
        if (root.autoTrackHeight) {
            trackHeightTimer.restart()
        }
    }

    //onCurrentTrackChanged: timeline.selection = []

    onTimeScaleChanged: {
        if (timeline.fullDuration * root.timeScale < scrollView.width) {
            scrollView.contentX = 0
            root.zoomOnMouse = -1
        } else if (K.KdenliveSettings.centeredplayhead) {
            scrollView.contentX = Math.max(0, root.consumerPosition * root.timeScale - (scrollView.width / 2))
            root.zoomOnMouse = -1
        } else if (root.zoomOnMouse >= 0) {
            scrollView.contentX = Math.max(0, root.zoomOnMouse * root.timeScale - getMouseX())
            root.zoomOnMouse = -1
        } else if (root.zoomOnBar) {
            root.zoomOnBar = false
        } else {
            scrollView.contentX = Math.max(0, root.consumerPosition * root.timeScale - (scrollView.width / 2))
        }
        ruler.adjustStepSize()
        if (dragProxy.draggedItem > -1 && dragProxy.masterObject) {
            // update dragged item pos
            dragProxy.masterObject.updateDrag()
        }
        timeline.setTimelineMouseOffset(scrollView.contentX - root.headerWidth)
    }

    onConsumerPositionChanged: {
        if (!root.blockAutoScroll && root.consumerPosition > -1) {
            if (K.KdenliveSettings.centeredplayhead) {
                scrollView.contentX = Math.max(0, root.consumerPosition * root.timeScale - (scrollView.width / 2))
            } else if (root.autoScrolling) {
                Logic.scrollIfNeeded()
            }
        }
    }

    onViewActiveTrackChanged: {
        if (controller.isSubtitleTrack(timeline.activeTrack)) {
            // subtitle track
            scrollView.contentY = 0
            return
        }
        var tk = Logic.getTrackById(timeline.activeTrack)
        if (tk.y + subtitleTrack.height < scrollView.contentY) {
            scrollView.contentY = Math.max(0, tk.y + subtitleTrack.height)
        } else if (tk.y + tk.height + subtitleTrack.height > scrollView.contentY + scrollView.height) {
            var newY = Math.min(trackHeaders.height + subtitleTrack.height - scrollView.height, tk.y + tk.height - scrollView.height + subtitleTrack.height)
            if (newY >= 0) {
                scrollView.contentY = newY
            }
        }
    }

    onActiveToolChanged: {
        if (root.activeTool === K.ToolType.SpacerTool) {
            // Spacer activated
            endDrag()
        } else if (root.activeTool === K.ToolType.SelectTool) {
            var tk = getMouseTrack()
            if (tk < 0) {
                return
            }
            var pos = getMousePos()
            var sourceTrack = Logic.getTrackById(tk)
            var allowComposition = tracksArea.mouseY- sourceTrack.y > sourceTrack.height / 2
            var tentativeItem = undefined
            if (allowComposition) {
                tentativeItem = getItemAtPos(tk, pos, true)
            }
            if (!tentativeItem) {
                tentativeItem = getItemAtPos(tk, pos, false)
            }
            if (tentativeItem) {
                tentativeItem.updateDrag()
            }
        }
    }

    DropArea { //Drop area for compositions
        id: compoArea
        width: root.width - root.headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: root.headerWidth
        property bool isAudioDrag
        property int sameCutPos: -1
        property int fakeFrame: -1
        property int fakeTrack: -1
        // Tracks mouse move up, used to prevent scrolling up once entering the area
        property int lastYPos: -1
        property int upMove: 0
        keys: ['kdenlive/composition']
        function moveDrop(offset, voffset)
        {
            if (clipBeingDroppedId >= 0) {
                var track = Logic.getTrackIdFromPos(drag.y + voffset + scrollView.contentY - subtitleTrack.height)
                if (track !== -1) {
                    var frame = Math.floor((drag.x + scrollView.contentX + offset) / root.timeScale)
                    if (controller.isAudioTrack(track) != isAudioDrag) {
                        // Don't allow moving composition to an audio track
                        track = controller.getCompositionTrackId(clipBeingDroppedId)
                    }
                    var frameData = controller.suggestCompositionMove(clipBeingDroppedId, track, frame, root.consumerPosition, root.snapping)
                    fakeTrack = frameData[1]
                    timeline.activeTrack = fakeTrack
                    sameCutPos = timeline.isOnCut(clipBeingDroppedId)
                    if (sameCutPos > -1) {
                        var sourceTrack = Logic.getTrackById(fakeTrack)
                        if ((drag.y > sourceTrack.y + sourceTrack.height / 2) || isAudioDrag) {
                            sameTrackIndicator.x = sameCutPos * root.timeScale - sameTrackIndicator.width / 2
                            sameTrackIndicator.y = sourceTrack.y
                            sameTrackIndicator.height = sourceTrack.height
                            sameTrackIndicator.visible = true
                        } else {
                            sameTrackIndicator.visible = false
                        }
                    } else {
                        sameTrackIndicator.visible = false
                    }
                    continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY, upMove)
                }
                if (offset != 0) {
                    timeline.setTimelineMouseOffset(scrollView.contentX - root.headerWidth)
                }
            }
        }
        onEntered: drag => {
            upMove = 0
            lastYPos = -1
            if (clipBeingMovedId == -1 && clipBeingDroppedId == -1) {
                var yOffset = 0
                if (root.showSubtitles) {
                    yOffset = subtitleTrack.height
                }
                var track = Logic.getTrackIdFromPos(drag.y + scrollView.contentY - yOffset)
                var frame = Math.round((drag.x + scrollView.contentX) / root.timeScale)
                droppedPosition = frame
                isAudioDrag = drag.getDataAsString('type') == "audio"
                if (track >= 0 && controller.isAudioTrack(track) == isAudioDrag) {
                    clipBeingDroppedData = drag.getDataAsString('kdenlive/composition')
                    clipBeingDroppedId = timeline.insertComposition(track, frame, clipBeingDroppedData, false)
                    continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY, upMove)
                }
                drag.acceptProposedAction()
            }
        }
        onPositionChanged: drag => {
            var frame = Math.floor((drag.x + scrollView.contentX) / root.timeScale)
            root.updateTimelineMousePos(frame, timeline.duration)
            if (lastYPos == -1) {
                lastYPos = drag.y
            } else if (drag.y <= lastYPos) {
                upMove += lastYPos - drag.y
                lastYPos = drag.y
            } else {
                lastYPos = drag.y
                upMove = 0
            }

            if (clipBeingMovedId == -1) {
                if (clipBeingDroppedId >= 0) {
                    moveDrop(0, 0)
                } else {
                    var yOffset = 0
                    if (root.showSubtitles) {
                        yOffset = subtitleTrack.height
                    }
                    var track = Logic.getTrackIdFromPos(drag.y + scrollView.contentY - yOffset)
                    if (track !== -1 && controller.isAudioTrack(track) == isAudioDrag) {
                        frame = controller.suggestSnapPoint(frame, root.snapping)
                        clipBeingDroppedData = drag.getDataAsString('kdenlive/composition')
                        clipBeingDroppedId = timeline.insertComposition(track, frame, clipBeingDroppedData , false)
                        continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY, lastPos, upMove)
                    } else {
                        drag.accepted = false
                    }
                }
            }
        }
        onExited:{
            upMove = 0
            lastYPos = -1
            if (clipBeingDroppedId != -1) {
                // If we exit, remove composition
                controller.requestItemDeletion(clipBeingDroppedId, false)
                clearDropData()
            }
        }
        onDropped: drag => {
            lastYPos = -1
            upMove = 0
            if (clipBeingDroppedId != -1) {
                var frame = controller.getCompositionPosition(clipBeingDroppedId)
                var track = controller.getCompositionTrackId(clipBeingDroppedId)
                // we simulate insertion at the final position so that stored undo has correct value
                controller.requestItemDeletion(clipBeingDroppedId, false)
                if (sameTrackIndicator.visible) {
                    // We want a same track composition
                    timeline.insertNewMix(track, sameCutPos, clipBeingDroppedData)
                } else if (!isAudioDrag) {
                    timeline.insertNewCompositionAtPos(track, frame, clipBeingDroppedData)
                } else {
                    // Cannot insert an audio mix composition
                    // TODO: show warning
                }
            }
            clearDropData()
            regainFocus(clipDropArea.mapToItem(root, drag.x, drag.y))
        }
    }
    DropArea {
        //Drop area for bin/clips
        id: clipDropArea
        property string lastDragUuid
        property var lastDragPos
        /** @brief local helper function to handle the insertion of multiple dragged items */
        function insertAndMaybeGroup(track, frame, droppedData) {
            var binIds = droppedData.split(";")
            if (binIds.length === 0) {
                return -1
            }

            var id = -1
            if (binIds.length === 1) {
                id = timeline.insertClip(timeline.activeTrack, frame, clipBeingDroppedData, false, true, false)
            } else {
                var ids = timeline.insertClips(timeline.activeTrack, frame, binIds, false, true, false)

                // if the clip insertion succeeded, request the clips to be grouped
                if (ids.length > 0) {
                    timeline.selectItems(ids)
                    id = ids[0]
                }
            }
            return id
        }

        property int fakeFrame: -1
        property int fakeTrack: -1
        width: root.width - root.headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: root.headerWidth
        // Tracks mouse move up, used to prevent scrolling up once entering the area
        property int lastYPos: -1
        property int upMove: 0
        keys: ['text/producerslist']
        enabled: !compoArea.containsDrag
        function moveDrop(offset, voffset)
        {
            if (clipBeingDroppedId > -1) {
                var yOffset = 0
                if (root.showSubtitles) {
                    yOffset = subtitleTrack.height
                }
                var track = Logic.getTrackIndexFromPos(drag.y + voffset + scrollView.contentY - yOffset)
                if (track >= 0  && track < tracksRepeater.count) {
                    var targetTrack = tracksRepeater.itemAt(track).trackInternalId
                    var frame = Math.floor((drag.x + scrollView.contentX + offset) / root.timeScale)
                    var moveData = controller.suggestClipMove(clipBeingDroppedId, targetTrack, frame, root.consumerPosition, root.snapping)
                    fakeFrame = moveData[0]
                    fakeTrack = moveData[1]
                    timeline.activeTrack = fakeTrack
                    if (!controller.normalEdit()) {
                        controller.requestFakeClipMove(clipBeingDroppedId, fakeTrack, fakeFrame, true, false, false)
                    }
                    continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY, upMove)
                }
                if (offset != 0) {
                    timeline.setTimelineMouseOffset(scrollView.contentX - root.headerWidth)
                }
            }
        }
        function processDrop()
        {
            // Process the drop event, useful if drop event happens outside of drop area
            if (clipBeingDroppedId != -1) {
                var cid = clipBeingDroppedId
                clearDropData()
                var frame = controller.getClipPosition(cid)
                var track = controller.getClipTrackId(cid)
                if (!controller.normalEdit()) {
                    frame = fakeFrame
                    track = fakeTrack
                }
                /* We simulate insertion at the final position so that stored undo has correct value
                 * NOTE: even if dropping multiple clips, requesting the deletion of the first one is
                 * enough as internally it will request the group deletion
                 */
                controller.requestItemDeletion(cid, false)

                var binIds = clipBeingDroppedData.split(";")
                if (binIds.length == 1) {
                    if (controller.normalEdit()) {
                        timeline.insertClip(track, frame, clipBeingDroppedData, true, true, false)
                    } else {
                        timeline.insertClipZone(clipBeingDroppedData, track, frame)
                    }
                } else {
                    if (controller.normalEdit()) {
                        timeline.insertClips(track, frame, binIds, true, true)
                    } else {
                        // TODO
                        console.log('multiple clips insert/overwrite not supported yet')
                    }
                }
                fakeTrack = -1
                fakeFrame = -1
                if (clipDropArea.containsDrag) {
                    regainFocus(clipDropArea.mapToItem(root, drag.x, drag.y))
                }
                //root.updateTimelineMousePos(frame, timeline.duration)
            }
        }
        onEntered: drag => {
            lastYPos = -1
            upMove = 0
            if (clipBeingDroppedId > -1 && lastDragUuid != drag.getDataAsString('text/dragid') && timeline.exists(clipBeingDroppedId)) {
                // We are re-entering drop zone with another drag operation, ensure the previous drop operation is complete
                processDrop()
            }
            timeline.keepAudioTargets(true)
            lastDragPos = Qt.point(drag.x, drag.y)
            if (clipBeingMovedId == -1 && clipBeingDroppedId == -1) {
                var yOffset = 0
                if (root.showSubtitles) {
                    yOffset = subtitleTrack.height
                }
                var track = Logic.getTrackIndexFromPos(drag.y + scrollView.contentY - yOffset)
                clipBeingDroppedData = drag.getDataAsString('text/producerslist')
                lastDragUuid = drag.getDataAsString('text/dragid')
                if (track >= 0  && track < tracksRepeater.count) {
                    var frame = Math.round((drag.x + scrollView.contentX) / root.timeScale)
                    droppedPosition = frame
                    timeline.activeTrack = tracksRepeater.itemAt(track).trackInternalId
                    if (controller.normalEdit()) {
                        clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, frame, clipBeingDroppedData)
                    } else {
                        // we want insert/overwrite mode, make a fake insert at end of timeline, then move to position
                        frame = controller.adjustFrame(frame, timeline.activeTrack)
                        clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, frame, clipBeingDroppedData)
                        if (clipBeingDroppedId > -1) {
                            var moveData = controller.suggestClipMove(clipBeingDroppedId, timeline.activeTrack, frame, root.consumerPosition, root.snapping)
                            fakeFrame = moveData[0]
                            fakeTrack = moveData[1]
                        }
                    }
                    continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY, upMove)
                }
                drag.acceptProposedAction()
            }
        }
        onExited: {
            lastYPos = -1
            upMove = 0
            timeline.keepAudioTargets(false)
            if (clipBeingDroppedId != -1 && (lastDragPos.y < lastDragPos.x || (clipDropArea.height - lastDragPos.y < lastDragPos.x))) {
                // If we exit on top or bottom, remove clip
                controller.requestItemDeletion(clipBeingDroppedId, false)
                clearDropData()
            } else if (clipBeingDroppedId > -1 && fakeTrack > -1) {
                // Clip is dropped
                var moveData = controller.suggestClipMove(clipBeingDroppedId, fakeTrack, 0, root.consumerPosition, root.snapping)
                fakeFrame = moveData[0]
                fakeTrack = moveData[1]
                timeline.activeTrack = fakeTrack
            }
        }
        onPositionChanged: drag => {
            lastDragPos = Qt.point(drag.x, drag.y)
            var frame = Math.floor((drag.x + scrollView.contentX) / root.timeScale)
            if (lastYPos == -1) {
                lastYPos = drag.y
            } else if (drag.y <= lastYPos) {
                upMove += lastYPos - drag.y
                lastYPos = drag.y
            } else {
                lastYPos = drag.y
                upMove = 0
            }
            root.updateTimelineMousePos(frame, timeline.duration)
            if (clipBeingMovedId == -1) {
                if (clipBeingDroppedId > -1) {
                    moveDrop(0, 0)
                } else {
                    var yOffset = 0
                    if (root.showSubtitles) {
                        yOffset = subtitleTrack.height
                    }
                    var track = Logic.getTrackIndexFromPos(drag.y + scrollView.contentY - yOffset)
                    if (track >= 0  && track < tracksRepeater.count) {
                        var targetTrack = tracksRepeater.itemAt(track).trackInternalId
                        frame = controller.suggestSnapPoint(frame, root.snapping)
                        if (controller.normalEdit()) {
                            timeline.activeTrack = targetTrack
                            clipBeingDroppedId = insertAndMaybeGroup(targetTrack, frame, drag.getDataAsString('text/producerslist'), false, true)
                        } else {
                            // we want insert/overwrite mode, make a fake insert at end of timeline, then move to position
                            clipBeingDroppedId = insertAndMaybeGroup(targetTrack, timeline.fullDuration, clipBeingDroppedData)
                            if (clipBeingDroppedId > -1) {
                                var moveData = controller.suggestClipMove(clipBeingDroppedId, targetTrack, frame, root.consumerPosition, root.snapping)
                                fakeFrame = moveData[0]
                                fakeTrack = moveData[1]
                                timeline.activeTrack = fakeTrack
                            }
                        }
                        continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY, upMove)
                    } else {
                        drag.accepted = false
                    }
                }
            }
        }
        onDropped: {
            lastYPos = -1
            upMove = 0
            processDrop()
            timeline.keepAudioTargets(false)
        }
    }
    DropArea { //Drop area for urls (direct drop from file manager)
        /** @brief local helper function to handle the insertion of multiple dragged items */
        property int fakeFrame: -1
        property var droppedUrls: []
        enabled: !clipDropArea.containsDrag && !compoArea.containsDrag
        width: root.width - root.headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: root.headerWidth
        keys: ['text/uri-list']
        onEntered: drag => {
            drag.accepted = true
            droppedUrls.length = 0
            if (dragProxy.draggedItem > -1) {
                endDrag()
            }
            for(var i in drag.urls){
                var url = drag.urls[i]
                droppedUrls.push(Qt.resolvedUrl(url))
            }
        }
        onExited:{
            if (clipBeingDroppedId != -1) {
                controller.requestItemDeletion(clipBeingDroppedId, false)
            }
            clearDropData()
        }
        onPositionChanged: drag => {
            var frame = Math.floor((drag.x + scrollView.contentX) / root.timeScale)
            root.updateTimelineMousePos(frame, timeline.duration)
            if (clipBeingMovedId == -1) {
                var yOffset = 0
                if (root.showSubtitles) {
                    yOffset = subtitleTrack.height
                }
                var track = Logic.getTrackIndexFromPos(drag.y + scrollView.contentY - yOffset)
                if (track >= 0  && track < tracksRepeater.count) {
                    timeline.activeTrack = tracksRepeater.itemAt(track).trackInternalId
                    continuousScrolling(drag.x + scrollView.contentX, drag.y + scrollView.contentY)
                }
            }
        }
        onDropped: drag => {
            var frame = Math.floor((drag.x + scrollView.contentX) / root.timeScale)
            var track = timeline.activeTrack
            if (controller.normalEdit()) {
                timeline.urlDropped(droppedUrls, frame, track)
            }
            clearDropData()
        }
    }

    Row {
        Column {
            id: headerContainer
            width: root.headerWidth
            z: 1
            Item {
                // Padding between toolbar and track headers.
                width: parent.width
                height: ruler.height
                ToolButton {
                    text: metrics.elidedText
                    font: miniFont
                    flat: true
                    icon.name: 'tools-wizard'
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: i18n("Show sequence effects")
                    TextMetrics {
                        id: metrics
                        font: miniFont
                        elide: Text.ElideRight
                        elideWidth: root.headerWidth * 0.8
                        text: root.addedSequenceName.length == 0 ? i18n("Sequence") : root.addedSequenceName
                    }
                    onClicked: {
                        timeline.showMasterEffects()
                    }
                    DropArea { //Drop area for tracks
                        id: trackEffectDrop
                        anchors.fill: parent
                        keys: ['kdenlive/effect']
                        property string dropData
                        property string dropSource
                        onEntered: drag => {
                            dropData = drag.getDataAsString('kdenlive/effect')
                            dropSource = drag.getDataAsString('kdenlive/effectsource')
                            drag.acceptProposedAction()
                        }
                        onDropped: drag => {
                            console.log("Add effect: ", trackEffectDrop.dropData)
                            if (dropSource == '') {
                                // drop from effects list
                                controller.addTrackEffect(-1, trackEffectDrop.dropData);
                            } else {
                                controller.copyTrackEffect(-1, trackEffectDrop.dropSource);
                            }
                            dropSource = ''
                            drag.acceptProposedAction()
                        }
                    }
                }
            }
            Flickable {
                // Non-slider scroll area for the track headers.
                id: headerFlick
                contentY: scrollView.contentY
                width: parent.width
                y: ruler.height
                height: root.height - ruler.height
                interactive: false
                clip: true

                MouseArea {
                    width: trackHeaders.width
                    height: trackHeaders.height + subtitleTrackHeader.height
                    acceptedButtons: Qt.NoButton
                    onWheel: wheel => {
                        root.verticalScroll(wheel)
                        wheel.accepted = true
                    }
                }
                K.SubtitleTrackHead {
                    id: subtitleTrackHeader
                    width: trackHeaders.width
                    height: subtitleTrack.height
                    isDisabled: root.subtitlesDisabled
                    isLocked: root.subtitlesLocked
                    collapsedHeight: root.collapsedHeight
                    collapsed: subtitleTrack.height === subtitleTrackHeader.collapsedHeight
                }
                Column {
                    id: trackHeaders
                    y: subtitleTrack.height
                    spacing: 0
                    Repeater {
                        id: trackHeaderRepeater
                        model: multitrack
                        property int tracksCount: count
                        onTracksCountChanged: {
                            if (root.autoTrackHeight) {
                                trackHeightTimer.restart()
                            }
                        }
                        TrackHead {
                            trackName: model.name
                            thumbsFormat: model.thumbsFormat
                            trackTag: model.trackTag
                            isDisabled: model.disabled
                            isComposite: model.composite
                            isLocked: model.locked
                            isActive: model.trackActive
                            isAudio: model.audio
                            showAudioRecord: model.audioRecord
                            effectNames: model.effectNames
                            isStackEnabled: model.isStackEnabled
                            width: root.headerWidth
                            current: item === timeline.activeTrack
                            trackId: item
                            height: model.trackHeight
                            collapsed: height <= root.collapsedHeight
                            Component.onCompleted: {
                                root.collapsedHeight = collapsedHeight
                            }
                            onHeightChanged: {
                                collapsed = height <= root.collapsedHeight
                            }
                        }
                    }
                }
                Column {
                    id: trackHeadersResizer
                    spacing: 0
                    width: Math.round(root.baseUnit/3)
                    Rectangle {
                        id: resizer
                        height: trackHeaders.height + subtitleTrackHeader.height
                        width: parent.width
                        x: root.headerWidth - width
                        color: 'red'
                        opacity: 0
                        Drag.active: headerMouseArea.drag.active
                        Drag.proposedAction: Qt.MoveAction

                        MouseArea {
                            id: headerMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: tracksArea.isCursorHidden ? Qt.BlankCursor : Qt.SizeHorCursor
                            drag.target: parent
                            drag.axis: Drag.XAxis
                            drag.minimumX: root.minHeaderWidth
                            property double startX
                            property double originalX
                            drag.smoothed: false

                            onPressed: {
                                root.blockAutoScroll = true
                            }
                            onReleased: {
                                root.blockAutoScroll = false
                                parent.opacity = 0
                                timeline.setTimelineMouseOffset(scrollView.contentX - root.headerWidth)
                            }
                            onEntered: parent.opacity = 0.5
                            onExited: parent.opacity = 0
                            onPositionChanged: mouse => {
                                if (mouse.buttons === Qt.LeftButton) {
                                    parent.opacity = 0.5
                                    root.headerWidth = Math.max( root.minHeaderWidth, mapToItem(null, x, y).x + 2)
                                    timeline.setHeaderWidth(headerWidth)
                                }
                            }
                        }
                    }
                }
            }
        }
        MouseArea {
            id: tracksArea
            property real clickX
            property real clickY
            property point lastGlobalPos
            property point panStartGlobalPos
            property bool isWarping: false
            property bool isCursorHidden: false
            width: root.width - root.headerWidth
            height: root.height
            x: root.headerWidth
            property bool shiftPress: false
            // This provides continuous scrubbing and scimming at the left/right edges.
            hoverEnabled: true
            preventStealing: true
            acceptedButtons: Qt.AllButtons
            cursorShape: {
                if (isCursorHidden) {
                    return Qt.BlankCursor
                }
                switch(root.activeTool) {
                case K.ToolType.SelectTool:
                case K.ToolType.RollTool:
                    return Qt.ArrowCursor;
                case K.ToolType.RazorTool:
                    return Qt.IBeamCursor;
                case K.ToolType.RippleTool:
                    return Qt.SplitHCursor;
                default:
                    return Qt.SizeHorCursor;
                }
            }
            onWheel: wheel => {
                if (wheel.modifiers & Qt.AltModifier || wheel.modifiers & Qt.ControlModifier || mouseY > trackHeaders.height) {
                    zoomByWheel(wheel)
                } else if (root.activeTool !== K.ToolType.SlipTool) {
                    var delta = wheel.modifiers & Qt.ShiftModifier ? timeline.fps() : 1
                    proxy.position = wheel.angleDelta.y > 0 ? Math.max(root.consumerPosition - delta, 0) : Math.min(root.consumerPosition + delta, timeline.fullDuration - 1)
                }
            }
            onPressed: mouse => {
                focus = true
                shiftPress = (mouse.modifiers & Qt.ShiftModifier) && (mouse.y > ruler.height) && !(mouse.modifiers & Qt.AltModifier)
                let selectLikeTool = root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool
                if (mouse.buttons === Qt.MiddleButton || (selectLikeTool && (mouse.modifiers & Qt.ControlModifier) && !shiftPress)) {
                    clickX = mouseX
                    clickY = mouseY
                    panStartGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    lastGlobalPos = panStartGlobalPos
                    isWarping = false
                    timeline.hideCursor(true)
                    isCursorHidden = true
                    return
                }
                if (selectLikeTool && shiftPress && mouse.y > ruler.height) {
                        // rubber selection
                        rubberSelect.x = mouse.x + scrollView.contentX
                        rubberSelect.y = mouse.y - ruler.height + scrollView.contentY
                        rubberSelect.clickX = rubberSelect.x
                        rubberSelect.clickY = rubberSelect.y
                        rubberSelect.originX = rubberSelect.clickX
                        rubberSelect.originY = rubberSelect.clickY
                        rubberSelect.width = 0
                        rubberSelect.height = 0
                } else if (mouse.button & Qt.LeftButton) {
                    if (root.activeTool === K.ToolType.RazorTool) {
                        // razor tool
                        var y = mouse.y - ruler.height + scrollView.contentY - subtitleTrack.height
                        if (y >= 0) {
                            timeline.cutClipUnderCursor((scrollView.contentX + mouse.x) / root.timeScale, tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y)).trackInternalId)
                        } else if (subtitleTrack.height > 0) {
                            timeline.cutClipUnderCursor((scrollView.contentX + mouse.x) / root.timeScale, -2)
                        }
                    } else if (root.activeTool === K.ToolType.SlipTool) {
                        //slip tool
                        if (mouse.y > ruler.height) {
                            var tk = getMouseTrack()
                            if (tk < 0) {
                                return
                            }
                            var pos = getMousePos()
                            var sourceTrack = Logic.getTrackById(tk)
                            var mainClip = undefined
                            mainClip = getItemAtPos(tk, pos, false)
                            trimmingClickFrame = Math.round((scrollView.contentX + mouse.x) / root.timeScale)
                            timeline.requestStartTrimmingMode(mainClip.clipId, shiftPress)
                            endDrag()
                        }
                    }
                    if (dragProxy.draggedItem > -1 && mouse.y > ruler.height) {
                        // Check if the mouse exit event was not correctly triggered on the draggeditem
                        regainFocus(tracksArea.mapToItem(root,mouseX, mouseY))
                        if (dragProxy.draggedItem > -1) {
                            mouse.accepted = false
                            return
                        }
                    }
                    if (root.activeTool === K.ToolType.SpacerTool && mouse.y > ruler.height) {
                        // spacer tool
                        var y = mouse.y - ruler.height + scrollView.contentY
                        var frame = (scrollView.contentX + mouse.x) / root.timeScale
                        // Default to all tracks
                        spacerTrack = -1
                        if (mouse.modifiers & Qt.ControlModifier) {
                            if (subtitleTrack.height > 0) {
                                if (y < subtitleTrack.height) {
                                    // Activate spacer on subtitle track only
                                    spacerTrack = -2
                                } else {
                                    spacerTrack = tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y - subtitleTrack.height)).trackInternalId
                                }
                            } else {
                                spacerTrack = tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y)).trackInternalId
                            }
                        }

                        if((mouse.modifiers & Qt.ShiftModifier) || !K.KdenliveSettings.lockedGuides) {
                            //spacer tool and shift modifier
                            spacerGuides = true;
                        }

                        spacerGroup = timeline.requestSpacerStartOperation(spacerTrack, frame)
                        spacerMinPos = timeline.spacerMinPos()
                        spacerMaxPos = timeline.spacerMaxPos()
                        if (spacerGroup > -1 || spacerGuides) {
                            drag.axis = Drag.XAxis
                            Drag.active = true
                            Drag.proposedAction = Qt.MoveAction
                            spacerClickFrame = frame
                            spacerFrame = spacerGroup > -1 ? controller.getItemPosition(spacerGroup) : frame
                            finalSpacerFrame = spacerFrame
                            if (spacerGuides) {
                                selectedGuides = timeline.spacerSelection(Math.min(spacerClickFrame, spacerFrame))
                                if (selectedGuides.length > 0) {
                                    var firstGuidePos = timeline.getGuidePosition(selectedGuides[0])
                                    if (spacerGroup > -1 && firstGuidePos < spacerFrame) {
                                        // Don't allow moving guide below 0
                                        spacerMinPos = Math.max(spacerMinPos, spacerFrame - firstGuidePos + 1)
                                    }
                                }
                            }
                        }
                    } else if (selectLikeTool || mouse.y <= ruler.height) {
                        if (mouse.y > ruler.height) {
                            controller.requestClearSelection();
                            proxy.position = Math.min((scrollView.contentX + mouse.x) / root.timeScale, timeline.fullDuration - 1)
                        } else if (mouse.y > ruler.guideLabelHeight) {
                            proxy.position = Math.min((scrollView.contentX + mouse.x) / root.timeScale, timeline.fullDuration - 1)
                        }

                    }
                } else if (mouse.button & Qt.RightButton) {
                    if (mouse.y > ruler.height) {
                        if (mouse.y > ruler.height + subtitleTrack.height) {
                            timeline.activeTrack = tracksRepeater.itemAt(Logic.getTrackIndexFromPos(mouse.y - ruler.height + scrollView.contentY - subtitleTrack.height)).trackInternalId
                        } else {
                            timeline.activeTrack = -2
                            timeline.activeSubLayer = (mouse.y - ruler.height) / (subtitleTrack.height / (maxSubLayer + 1))
                        }
                        root.clickFrame = Math.floor((mouse.x + scrollView.contentX) / root.timeScale)
                        root.showTimelineMenu()
                    } else {
                        // ruler menu
                        proxy.position = (scrollView.contentX + mouse.x) / root.timeScale
                        root.showRulerMenu()
                    }
                }
            }
            property bool scim: false
            onExited: {
                scim = false
                if (isCursorHidden) {
                    timeline.hideCursor(false)
                    isCursorHidden = false
                }
                timeline.showTimelineToolInfo(false)
            }
            onEntered: {
                timeline.showTimelineToolInfo(true)
            }
            onDoubleClicked: mouse => {
                if (mouse.buttons === Qt.LeftButton && root.activeTool === K.ToolType.SelectTool && mouse.y > ruler.height) {
                    if (root.showSubtitles && mouse.y < (ruler.height + subtitleTrack.height)) {
                        subtitleModel.addSubtitle((scrollView.contentX + mouseX) / root.timeScale, (mouse.y - ruler.height) / (subtitleTrack.height / (maxSubLayer + 1)))
                        timeline.activeTrack = -2
                        timeline.activeSubLayer = (mouse.y - ruler.height) / (subtitleTrack.height / (maxSubLayer + 1))
                    } else {
                        timeline.activeTrack = tracksRepeater.itemAt(Logic.getTrackIndexFromPos(mouse.y - ruler.height + scrollView.contentY - subtitleTrack.height)).trackInternalId
                    }
                } else if (mouse.y < ruler.guideLabelHeight) {
                    timeline.switchGuide((scrollView.contentX + mouseX) / root.timeScale, false)
                }
            }
            onPositionChanged: mouse => {
                let selectLikeTool = root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool
                if (pressed && ((mouse.buttons === Qt.MiddleButton) || (mouse.buttons === Qt.LeftButton && selectLikeTool && (mouse.modifiers & Qt.ControlModifier) && !shiftPress))) {
                    // Pan view
                    if (!isCursorHidden) {
                        timeline.hideCursor(true)
                        isCursorHidden = true
                    }
                    let globalPos = mapToGlobal(mouse.x, mouse.y)
                    
                    if (isWarping) {
                        // After warp, skip frames until the mouse has reached the center area.
                        // This handles cases where multiple "old" mouse events are in flight
                        // from before the warp was processed by the windowing system.
                        let dist = Math.abs(globalPos.x - lastGlobalPos.x) + Math.abs(globalPos.y - lastGlobalPos.y)
                        if (dist > 100) {
                            return
                        }
                        isWarping = false
                    }

                    let deltaX = globalPos.x - lastGlobalPos.x
                    let deltaY = globalPos.y - lastGlobalPos.y
                    
                    if (deltaX === 0 && deltaY === 0) {
                        return
                    }
                    
                    var maxScrollX = timeline.fullDuration * root.timeScale - scrollView.width
                    var maxScrollY = trackHeaders.height + subtitleTrackHeader.height - scrollView.height + horZoomBar.height
                    
                    scrollView.contentX = Math.max(0, Math.min(scrollView.contentX - deltaX, maxScrollX))
                    scrollView.contentY = Math.max(0, Math.min(scrollView.contentY - deltaY, maxScrollY))
                    
                    lastGlobalPos = globalPos
                    clickX = mouse.x
                    clickY = mouse.y

                    // Check for screen or window edge and warp to center of tracks area
                    let margin = 20
                    let edges = screenEdges(globalPos)
                    let atScreenEdge = edges && (globalPos.x <= edges.left + margin || globalPos.x >= edges.right - margin
                                                || globalPos.y <= edges.top + margin || globalPos.y >= edges.bottom - margin)
                    let atWindowEdge = mouse.x <= margin || mouse.x >= tracksArea.width - margin
                                    || mouse.y <= margin || mouse.y >= tracksArea.height - margin

                    if (atScreenEdge || atWindowEdge) {
                        let center = mapToGlobal(tracksArea.width / 2, tracksArea.height / 2)
                        isWarping = true
                        lastGlobalPos = center
                        timeline.warpCursor(Qt.point(center.x, center.y))
                    }
                    return
                }
                if (root.activeTool === K.ToolType.SlipTool && pressed && mouse.y > ruler.height) {
                    var frame = getMouseFrame()
                    trimmingOffset = frame - trimmingClickFrame
                    timeline.slipPosChanged(trimmingOffset);
                }
                if (!pressed && !rubberSelect.visible && root.activeTool === K.ToolType.RazorTool) {
                    var mouseXPos = getMouseFrame()
                    cutLine.x = mouseXPos * root.timeScale - scrollView.contentX
                    if (mouse.modifiers & Qt.ShiftModifier) {
                        // Seek
                        proxy.position = mouseXPos
                    }
                }
                ruler.showZoneLabels = mouse.y < ruler.height
                if (shiftPress && mouse.buttons === Qt.LeftButton && selectLikeTool && !rubberSelect.visible && rubberSelect.y > 0) {
                    // rubber selection, check if mouse move was enough
                    var dx = rubberSelect.originX - (mouseX + scrollView.contentX)
                    var dy = rubberSelect.originY - (mouseY - ruler.height + scrollView.contentY)
                    if ((Math.abs(dx) + Math.abs(dy)) > Qt.styleHints.startDragDistance) {
                        rubberSelect.visible = true
                    }
                }
                if (rubberSelect.visible) {
                    var newX = mouse.x + scrollView.contentX
                    var newY = mouse.y + scrollView.contentY - ruler.height
                    if (newX < rubberSelect.originX) {
                        rubberSelect.x = newX
                        rubberSelect.width = rubberSelect.originX - newX
                    } else {
                        rubberSelect.x = rubberSelect.originX
                        rubberSelect.width = newX - rubberSelect.originX
                    }
                    if (newY < rubberSelect.originY) {
                        rubberSelect.y = newY
                        rubberSelect.height = rubberSelect.originY - newY
                    } else {
                        rubberSelect.y = rubberSelect.originY
                        rubberSelect.height = newY - rubberSelect.originY
                    }
                    continuousScrolling(newX, newY)
                } else if ((pressedButtons & Qt.LeftButton) && (!shiftPress || spacerGuides)) {
                    if (selectLikeTool || (mouse.y < ruler.height && root.activeTool !== K.ToolType.SlipTool && (root.activeTool !== K.ToolType.SpacerTool || spacerGroup == -1))) {
                        proxy.position = Math.max(0, Math.min((scrollView.contentX + mouse.x) / root.timeScale, timeline.fullDuration - 1))
                    } else if (root.activeTool === K.ToolType.SpacerTool && spacerGroup > -1) {
                        // Spacer tool, move group
                        var track = controller.getItemTrackId(spacerGroup)
                        var lastPos = 0;
                        if (spacerGuides) {
                            lastPos = controller.getItemFakePosition(spacerGroup)
                            if (lastPos == -1) {
                                lastPos = controller.getItemPosition(spacerGroup)
                            }
                        }
                        var frame = Math.round((mouse.x + scrollView.contentX) / root.timeScale) + spacerFrame - spacerClickFrame
                        frame = Math.max(spacerMinPos, frame)
                        if (spacerMaxPos > -1) {
                            frame = Math.min(spacerMaxPos, frame)
                        }
                        finalSpacerFrame = controller.suggestItemMove(spacerGroup, track, frame, root.consumerPosition, (mouse.modifiers & Qt.ShiftModifier) ? 0 : root.snapping, true)[0]
                        if (spacerGuides && finalSpacerFrame > -1) {
                            timeline.spacerMoveGuides(selectedGuides, finalSpacerFrame - lastPos)
                        }
                        continuousScrolling(mouse.x + scrollView.contentX, mouse.y + scrollView.contentY - ruler.height)
                    } else if (spacerGuides) {
                        var frame = Math.round((mouse.x + scrollView.contentX) / root.timeScale)
                        frame = Math.max(spacerMinPos, frame)
                        timeline.spacerMoveGuides(selectedGuides, frame - spacerFrame)
                        spacerFrame = frame;
                    }

                    scim = true
                } else {
                    scim = false
                }
            }
            onCanceled: {
                if (isCursorHidden) {
                    timeline.hideCursor(false)
                    isCursorHidden = false
                    timeline.warpCursor(panStartGlobalPos)
                }
            }
            onReleased: mouse => {
                isWarping = false
                if (isCursorHidden) {
                    timeline.hideCursor(false)
                    isCursorHidden = false
                    timeline.warpCursor(panStartGlobalPos)
                }
                if((mouse.button & Qt.LeftButton) && root.activeTool === K.ToolType.SlipTool) {
                    // slip tool
                    controller.requestSlipSelection(trimmingOffset, true)
                    trimmingOffset = 0;
                    mouse.accepted = false
                }
                if (rubberSelect.visible) {
                    rubberSelect.visible = false
                    var y = rubberSelect.y
                    var selectSubs = false
                    var selectOnlySubs = false
                    var selectionHeight = rubberSelect.height
                    if (showSubtitles) {
                        selectSubs = y < subtitleTrack.height
                        var bottomRubber = y + rubberSelect.height
                        if (bottomRubber > subtitleTrack.height) {
                            y = Math.max(0, y - subtitleTrack.height)
                            if (selectSubs) {
                                selectionHeight = bottomRubber - subtitleTrack.height
                            }
                        } else {
                            y -= subtitleTrack.height
                            selectOnlySubs = true
                        }
                    }
                    var topTrack = Logic.getTrackIndexFromPos(Math.max(0, y))
                    var bottomTrack = Logic.getTrackIndexFromPos(Math.max(0, y) + selectionHeight)
                    // Check if bottom of rubber selection covers the last track compositions
                    console.log('Got rubber bottom: ', y, ' - height: ', selectionHeight, ', TK y: ', Logic.getTrackYFromId(tracksRepeater.itemAt(bottomTrack).trackInternalId), ', SCROLLVIEWY: ', scrollView.contentY)
                    var selectBottomCompositions = ((y + selectionHeight) - Logic.getTrackYFromId(tracksRepeater.itemAt(bottomTrack).trackInternalId)) > (Logic.getTrackHeightByPos(bottomTrack) * 0.6)
                    if (bottomTrack >= topTrack) {
                        var t = []
                        if (!selectOnlySubs) {
                            for (var i = topTrack; i <= bottomTrack; i++) {
                                t.push(tracksRepeater.itemAt(i).trackInternalId)
                            }
                        }
                        var startFrame = Math.round(rubberSelect.x / root.timeScale)
                        var endFrame = Math.round((rubberSelect.x + rubberSelect.width) / root.timeScale)
                        timeline.selectItems(t, startFrame, endFrame, mouse.modifiers & Qt.ControlModifier, selectBottomCompositions, selectSubs);
                    }
                    rubberSelect.y = -1
                } else if (shiftPress && !spacerGuides) {
                    if (root.activeTool === K.ToolType.RazorTool) {
                        // Shift click, process seek
                        proxy.position = Math.min((scrollView.contentX + mouse.x) / root.timeScale, timeline.fullDuration - 1)
                    } else if (dragProxy.draggedItem > -1) {
                        // Select item
                        if (timeline.selection.indexOf(dragProxy.draggedItem) === -1) {
                            controller.requestAddToSelection(dragProxy.draggedItem)
                        } else {
                            controller.requestRemoveFromSelection(dragProxy.draggedItem)
                        }
                    } else if (!rubberSelect.visible) {
                        // Mouse release with shift press and no rubber select, seek
                        proxy.position = Math.min((scrollView.contentX + mouse.x) / root.timeScale, timeline.fullDuration - 1)
                    }
                    return
                }

                if (finalSpacerFrame > -1) {
                    if (spacerGroup > -1) {
                        var frame = controller.getItemPosition(spacerGroup)
                        timeline.requestSpacerEndOperation(spacerGroup, spacerFrame, finalSpacerFrame, spacerTrack, selectedGuides, spacerGuides ? spacerClickFrame : -1);
                    } else if (spacerGuides) {
                        // Move back guides to original pos
                        timeline.spacerMoveGuides(selectedGuides, spacerClickFrame - spacerFrame)
                        timeline.moveGuidesInRange(spacerClickFrame, -1, spacerFrame - finalSpacerFrame)
                    }
                }

                if (spacerGroup > -1 || spacerGuides) {
                    spacerClickFrame = -1
                    spacerFrame = -1
                    spacerGroup = -1
                    spacerMinPos = -1
                    spacerMaxPos = -1
                    selectedGuides = []
                    spacerGuides = false
                }

                scim = false
            }

            Item {
                // Guide zone delimiter
                Rectangle {
                    width: rulercontainer.width
                    height: 1
                    anchors.top: parent.top
                    anchors.topMargin: ruler.guideLabelHeight
                    color: activePalette.dark
                    visible: ruler.guideLabelHeight > 0
                }

                // monitor zone
                Rectangle {
                    width: rulercontainer.width
                    height: 1
                    anchors.top: parent.top
                    anchors.topMargin: ruler.height - ruler.zoneHeight
                    color: activePalette.dark
                    Rectangle {
                        width: rulercontainer.width
                        height: 1
                        anchors.top: parent.bottom
                        color: activePalette.light
                    }
                }
                Flickable {
                    // Non-slider scroll area for the Ruler.
                    id: rulercontainer
                    width: root.width - root.headerWidth
                    height: Math.round(root.baseUnit * 2.5) + ruler.guideLabelHeight
                    contentX: scrollView.contentX
                    contentWidth: Math.max(parent.width, timeline.fullDuration * timeScale)
                    interactive: false
                    clip: true
                    onWidthChanged: {
                        ruler.adjustStepSize()
                    }
                    Ruler {
                        id: ruler
                        width: rulercontainer.contentWidth
                        height: parent.height
                        K.TimelinePlayhead {
                            id: playhead
                            height: Math.round(root.baseUnit * .8)
                            width: Math.round(root.baseUnit * 1.2)
                            fillColor: activePalette.windowText
                            visible: cursor.visible
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: ruler.zoneHeight - 1
                            anchors.horizontalCenter: rulerCursor.horizontalCenter
                            // bottom line on zoom
                        }
                        Rectangle {
                            // Vertical line over ruler zone
                            id: rulerCursor
                            color: root.textColor
                            width: 1
                            height: ruler.zoneHeight - 1
                            visible: cursor.visible
                            x: cursor.x
                            anchors.bottom: parent.bottom
                            Rectangle {
                                color: ruler.dimmedColor
                                width: Math.max(1, root.timeScale)
                                height: 1
                                visible: width > playhead.width
                            }
                        }
                    }
                }
                MouseArea {
                    anchors.top: parent.top
                    height: rulercontainer.height
                    width: rulercontainer.width
                    acceptedButtons: Qt.NoButton
                                cursorShape: tracksArea.isCursorHidden ? Qt.BlankCursor : ruler.cursorShape
                }

                Item {
                    id: baseContainer
                    width: root.width - root.headerWidth
                    height: root.height - ruler.height
                    y: ruler.height
                    clip: true
                    // These make the striped background for the tracks.
                    // It is important that these are not part of the track visual hierarchy;
                    // otherwise, the clips will be obscured by the Track's background.
                    Column {
                        topPadding: -scrollView.contentY
                        Repeater {
                            model: maxSubLayer + 1
                            Rectangle {
                                width: scrollView.width
                                border.width: 1
                                border.color: root.frameColor
                                height: subtitleTrack.height / (maxSubLayer + 1)
                                color: (controller && controller.isSubtitleTrack(timeline.activeTrack) && (timeline.activeSubLayer == index)) ? Qt.tint(getTrackColor(false, false), selectedTrackColor) : getTrackColor(false, false)
                            }
                        }
                    }
                    Column {
                        y: subtitleTrack.height
                        topPadding: -scrollView.contentY
                        Repeater {
                            model: multitrack
                            id: trackBaseRepeater
                            delegate: Rectangle {
                                width: scrollView.width
                                border.width: 1
                                border.color: root.frameColor
                                height: model.trackHeight
                                color: (model.item === timeline.activeTrack) ? Qt.tint(getTrackColor(model.audio, false), selectedTrackColor) : getTrackColor(model.audio, false)
                            }
                        }
                    }
                    Flickable {
                        id: scrollView
                        anchors.fill: parent
                        anchors.rightMargin: vertScroll.visible ? vertScroll.width : 0
                        anchors.bottomMargin: horZoomBar.visible ? horZoomBar.height : 0
                        // Click and drag should seek, not scroll the timeline view
                        //flickableItem.interactive: false
                        clip: true
                        interactive: false
                        pixelAligned: true
                        property int firstVisibleFrame: Math.floor(scrollView.contentX / root.timeScale)
                        property int lastVisibleFrame: firstVisibleFrame + Math.ceil(scrollView.width / root.timeScale)
                        onContentXChanged: {
                            timeline.setTimelineMouseOffset(scrollView.contentX - root.headerWidth)
                        }
                        /*
                         // Replaced by our custom ZoomBar
                         ScrollBar.horizontal: ScrollBar {
                            id: horScroll
                            parent: scrollView.parent
                            anchors.top: scrollView.top
                            anchors.left: scrollView.left
                            anchors.right: scrollView.right
                        }*/
                        ScrollBar.vertical: ScrollBar {
                            id: vertScroll
                            parent: scrollView.parent
                            anchors.top: scrollView.top
                            anchors.left: scrollView.right
                            anchors.bottom: scrollView.bottom
                        }
                        contentWidth: tracksContainerArea.width
                        contentHeight: tracksContainerArea.height
                        Item {
                            id: subtitleTrack
                            width: tracksContainerArea.width
                            height: 0
                            MouseArea {
                                id: subtitleMouseArea
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                hoverEnabled: true
                                onWheel: wheel => zoomByWheel(wheel)
                                onEntered: {
                                    if (root.activeTool === K.ToolType.SelectTool) {
                                        timeline.showKeyBinding(i18n("<b>Double click</b> to add a subtitle"))
                                    }
                                }
                                onPositionChanged: mouse => {
                                    tracksArea.positionChanged(mouse)
                                }
                                onExited: {
                                    timeline.showKeyBinding()
                                }
                            }

                            Repeater { id: subtitlesRepeater; model: subtitleDelegateModel }
                        }
                        Item {
                            id: tracksContainerArea
                            width: Math.max(scrollView.width - vertScroll.width, timeline.fullDuration * timeScale)
                            height: trackHeaders.height + subtitleTrackHeader.height
                            y: subtitleTrack.height
                            //Math.max(trackHeaders.height, scrollView.contentHeight - scrollView.__horizontalScrollBar.height)
                            //color: root.color
                            Item {
                                // Drag proxy, responsible for clip / composition move
                                id: dragProxy
                                x: 0
                                y: 0
                                width: 0
                                height: 0
                                property int draggedItem: -1
                                property int sourceTrack
                                property int sourceFrame
                                property bool isComposition
                                property int verticalOffset
                                property var masterObject
                                // opacity: 0.8
                                MouseArea {
                                    id: dragProxyArea
                                    anchors.fill: parent
                                    drag.target: parent
                                    drag.axis: Drag.XAxis
                                    drag.smoothed: false
                                    property int dragFrame
                                    property int snapping: root.snapping
                                    property bool moveMirrorTracks: true
                                    property point dragStartGlobalPos
                                    cursorShape: {
                                        if (root.activeTool === K.ToolType.SelectTool) {
                                            return dragProxyArea.drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        }
                                        if (tracksArea.isCursorHidden) {
                                            return Qt.BlankCursor
                                        }
                                        return tracksArea.cursorShape
                                    }
                                    enabled: root.activeTool === K.ToolType.SelectTool || root.activeTool === K.ToolType.RippleTool
                                    onPressed: mouse => {
                                        if (mouse.modifiers & Qt.ControlModifier || (mouse.modifiers & Qt.ShiftModifier && !(mouse.modifiers & Qt.AltModifier))) {
                                            mouse.accepted = false
                                            return
                                        }
                                        dragStartGlobalPos = mapToGlobal(mouse.x, mouse.y)
                                        tracksArea.lastGlobalPos = dragStartGlobalPos
                                        if (!timeline.exists(dragProxy.draggedItem)) {
                                            endDrag()
                                            mouse.accepted = false
                                            return
                                        }
                                        dragFrame = -1
                                        moveMirrorTracks = !(mouse.modifiers & Qt.MetaModifier) && (Qt.platform.os != "windows" || !(mouse.modifiers & Qt.AltModifier))
                                        timeline.activeTrack = dragProxy.sourceTrack
                                        var singleSelection = mouse.modifiers & Qt.AltModifier
                                        if (singleSelection || timeline.selection.indexOf(dragProxy.draggedItem) === -1) {
                                            doubleClickTimer.start()
                                            controller.requestAddToSelection(dragProxy.draggedItem, /*clear=*/ !(mouse.modifiers & Qt.ShiftModifier), /*single item selection */ singleSelection)
                                        }
                                        timeline.showAsset(dragProxy.draggedItem)
                                        root.blockAutoScroll = true
                                        clipBeingMovedId = dragProxy.draggedItem
                                        if (dragProxy.draggedItem > -1) {
                                            var tk = controller.getItemTrackId(dragProxy.draggedItem)
                                            var x = controller.getItemPosition(dragProxy.draggedItem)
                                            var posx = Math.round((parent.x)/ root.timeScale)
                                            var clickAccepted = true
                                            var currentMouseTrack = Logic.getTrackIdFromPos(parent.y)
                                            if (controller.normalEdit() && (tk !== currentMouseTrack || x !== posx)) {
                                                console.log('incorrect drag, Trying to recover item', parent.y,'xpos',x,'=',posx,'track',tk)
                                                // Try to find correct item
                                                var tentativeClip = getItemAtPos(currentMouseTrack, mouseX + parent.x, dragProxy.isComposition)
                                                if (tentativeClip && tentativeClip.clipId) {
                                                    console.log('missing item', tentativeClip.clipId)
                                                    clickAccepted = true
                                                    dragProxy.draggedItem = tentativeClip.clipId
                                                    dragProxy.x = tentativeClip.x
                                                    dragProxy.y = currentMouseTrack.y + tentativeClip.isComposition ? tentativeClip.displayHeight : tentativeClip.y
                                                    dragProxy.height = tentativeClip.itemHeight()
                                                    dragProxy.width = tentativeClip.width
                                                    dragProxy.masterObject = tentativeClip
                                                    dragProxy.sourceTrack = tk
                                                    dragProxy.isComposition = tentativeClip.isComposition
                                                    dragProxy.verticalOffset = tentativeClip.isComposition ? tentativeClip.displayHeight : 0
                                                } else {
                                                    console.log('item not found')
                                                    clickAccepted = false
                                                    mouse.accepted = false
                                                    dragProxy.draggedItem = -1
                                                    dragProxy.masterObject = undefined
                                                    dragProxy.sourceFrame = -1
                                                    parent.x = 0
                                                    parent.y = 0
                                                    parent.width = 0
                                                    parent.height = 0
                                                }
                                            }
                                            if (clickAccepted && dragProxy.draggedItem != -1) {
                                                focus = true;
                                                root.mainItemId = dragProxy.draggedItem
                                                dragProxy.masterObject.originalX = dragProxy.masterObject.x
                                                dragProxy.masterObject.originalTrackId = dragProxy.masterObject.trackId
                                                dragProxy.sourceFrame = dragProxy.masterObject.modelStart
                                                dragProxy.masterObject.forceActiveFocus();
                                            } else {
                                                root.mainItemId = -1
                                            }
                                        } else {
                                            mouse.accepted = false
                                            parent.x = 0
                                            parent.y = 0
                                            parent.width = 0
                                            parent.height = 0
                                        }
                                    }
                                    onPositionChanged: mouse => {
                                        // we have to check item validity in the controller, because they could have been deleted since the beginning of the drag
                                        if (dragProxy.draggedItem > -1 && !timeline.exists(dragProxy.draggedItem)) {
                                            endDrag()
                                            return
                                        }
                                        
                                        let globalPos = mapToGlobal(mouse.x, mouse.y)
                                        if (tracksArea.isWarping) {
                                            // After warp, skip this frame and reset tracking to current position
                                            tracksArea.isWarping = false
                                            tracksArea.lastGlobalPos = globalPos
                                            return
                                        }

                                        if (dragProxy.draggedItem > -1 && mouse.buttons === Qt.LeftButton &&  (controller.isClip(dragProxy.draggedItem) || controller.isComposition(dragProxy.draggedItem))) {
                                            let deltaX = globalPos.x - tracksArea.lastGlobalPos.x
                                            let deltaY = globalPos.y - tracksArea.lastGlobalPos.y
                                            
                                            if (deltaX !== 0 || deltaY !== 0) {
                                                // If we have movement, process it
                                                let posInTracks = dragProxyArea.mapToItem(tracksArea, dragProxyArea.mouseX, dragProxyArea.mouseY)
                                                // Clamp y to tracks area height to avoid jumping tracks when mouse is far outside
                                                let clampedY = Math.max(0, Math.min(posInTracks.y, tracksArea.height))
                                                continuousScrolling(dragProxyArea.mouseX + parent.x, clampedY + scrollView.contentY - ruler.height)
                                                snapping = (mouse.modifiers & Qt.ShiftModifier) ? 0 : root.snapping
                                                moveItem()
                                                
                                                tracksArea.lastGlobalPos = globalPos
                                            }
                                        }
                                    }

                                    function moveItem() {
                                        if (dragProxy.draggedItem > -1 && !rubberSelect.visible) {
                                            var posx = Math.round((parent.x)/ root.timeScale)
                                            let posInTracks = dragProxyArea.mapToItem(tracksArea, dragProxyArea.mouseX, dragProxyArea.mouseY)
                                            var posy = Math.min(Math.max(0, posInTracks.y + scrollView.contentY - ruler.height), tracksContainerArea.height)
                                            var tId = Logic.getTrackIdFromPos(posy)
                                            if (dragProxy.masterObject && tId === dragProxy.masterObject.trackId) {
                                                if (posx == dragProxyArea.dragFrame && controller.normalEdit()) {
                                                    return
                                                }
                                            }
                                            var moveData
                                            if (dragProxy.isComposition) {
                                                moveData = controller.suggestCompositionMove(dragProxy.draggedItem, tId, posx, root.consumerPosition, dragProxyArea.snapping)
                                                dragProxyArea.dragFrame = moveData[0]
                                                timeline.activeTrack = moveData[1]
                                            } else {
                                                moveData = controller.suggestClipMove(dragProxy.draggedItem, tId, posx, root.consumerPosition, dragProxyArea.snapping, moveMirrorTracks)
                                                dragProxyArea.dragFrame = moveData[0]
                                                timeline.activeTrack = moveData[1]
                                                if (!controller.normalEdit()) {
                                                    controller.requestFakeClipMove(dragProxy.draggedItem, timeline.activeTrack, dragProxyArea.dragFrame, true, false, false)
                                                }
                                            }
                                            var targetTrack = Logic.getTrackById(timeline.activeTrack)
                                            if (targetTrack) {
                                                dragProxy.y = targetTrack.y + dragProxy.verticalOffset
                                            }
                                            var delta = dragProxyArea.dragFrame - dragProxy.sourceFrame
                                            if (delta != 0) {
                                                var s = timeline.simplifiedTC(Math.abs(delta))
                                                s = i18n("Offset: %1, Position: %2", (delta < 0 ? '-' : '+') + s, timeline.simplifiedTC(dragProxyArea.dragFrame))
                                                timeline.showToolTip(s);
                                            } else {
                                                timeline.showToolTip()
                                                //bubbleHelp.hide()
                                            }
                                        }
                                    }
                                    onCanceled: {
                                    }
                                    onReleased: {
                                        clipBeingMovedId = -1
                                        root.blockAutoScroll = false
                                        var itemId = dragProxy.draggedItem
                                        var sourceTrack = dragProxy.sourceTrack
                                        var sourceFrame = dragProxy.sourceFrame
                                        var isComposition = dragProxy.isComposition

                                        if (itemId > -1 && dragFrame > -1 && (controller.isClip(itemId) || controller.isComposition(itemId))) {
                                            var tId = controller.getItemTrackId(itemId)
                                            if (isComposition) {
                                                if (controller.normalEdit()) {
                                                    // Move composition back to original position
                                                    controller.requestCompositionMove(itemId, sourceTrack, sourceFrame, true, false)
                                                    // Move composition to final pos
                                                    controller.requestCompositionMove(itemId, tId, dragFrame , true, true)
                                                } else {
                                                    // Fake move, only process final move
                                                    timeline.endFakeMove(itemId, dragFrame, true, true, true)
                                                }
                                            } else {
                                                if (controller.normalEdit()) {
                                                    // Move clip back to original position
                                                    controller.requestClipMove(itemId, sourceTrack, sourceFrame, moveMirrorTracks, true, false, false, true)
                                                    // Move clip to final pos
                                                    controller.requestClipMove(itemId, tId, dragFrame , moveMirrorTracks, true, true, true)
                                                } else {
                                                    // Fake move, only process final move
                                                    timeline.endFakeMove(itemId, dragFrame, true, true, true)
                                                }
                                            }
                                            if (dragProxy.masterObject && dragProxy.masterObject.isGrabbed) {
                                                dragProxy.masterObject.grabItem()
                                            }
                                            dragProxy.x = controller.getItemPosition(itemId) * root.timeScale
                                            timeline.showToolTip()
                                            //bubbleHelp.hide()
                                            tracksArea.focus = true
                                            if (!dragProxyArea.containsMouse) {
                                                regainFocus(dragProxyArea.mapToItem(root, dragProxyArea.mouseX, dragProxyArea.mouseY))
                                            }
                                        }
                                    }
                                    onDoubleClicked: {
                                        if (dragProxy.masterObject.keyframeModel && dragProxy.masterObject.showKeyframes && !doubleClickTimer.running) {
                                            var newVal = (dragProxy.height - mouseY) / dragProxy.height
                                            var newPos = Math.round(mouseX / timeScale) + dragProxy.masterObject.inPoint
                                            timeline.addEffectKeyframe(dragProxy.draggedItem, newPos, newVal)
                                        } else {
                                            clipBeingMovedId = -1
                                            timeline.ungrabHack()
                                            if(dragProxy.masterObject.itemType === K.ClipType.Timeline) {
                                                timeline.focusTimelineSequence(dragProxy.draggedItem)
                                            } else if(dragProxy.masterObject.itemType === K.ClipType.Text || dragProxy.masterObject.itemType === K.ClipType.TextTemplate) {
                                                timeline.editTitleClip(dragProxy.draggedItem)
                                            } else if (dragProxy.masterObject.itemType === K.ClipType.Animation) {
                                                timeline.editAnimationClip(dragProxy.draggedItem)
                                            } else {
                                                timeline.editItemDuration()
                                            }
                                        }
                                    }
                                    onClicked: {
                                        if (dragProxy.masterObject.keyframeModel && dragProxy.masterObject.showKeyframes) {
                                            dragProxy.masterObject.resetSelection()
                                        }
                                    }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                onWheel: wheel => zoomByWheel(wheel)
                                cursorShape: (dragProxyArea.drag.active ? Qt.ClosedHandCursor : (tracksArea.isCursorHidden ? Qt.BlankCursor : tracksArea.cursorShape))
                            }
                            Column {
                                id: tracksContainer
                                Repeater { id: tracksRepeater; model: trackDelegateModel }
                            }
                            Item {
                                id: dragContainer
                                anchors.fill: tracksContainer
                                z: 100
                            }
                            Rectangle {
                                id: sameTrackIndicator
                                color: 'red'
                                opacity: 0.5
                                visible: false
                                width: root.baseUnit
                                height: width
                            }
                        }
                        Rectangle {
                            id: rubberSelect
                            // Used to determine if drag start should trigger an event
                            property int originX
                            // Used to determine if drag start should trigger an event
                            property int originY
                            // Absolute position of the click event
                            property int clickX
                            property int clickY
                            y: -1
                            color: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.4)
                            border.color: activePalette.highlight
                            border.width: 1
                            visible: false
                        }
                        Item {
                            id: recordStartPlaceHolder
                            x: 0
                            width: 0
                        }
                        
                        Item {
                            id: recordPlaceHolder
                            // Used to determine if drag start should trigger an event
                            property int startTime: 0
                            property double currentLevel
                            property var recModel: []
                            property int channels: 1
                            property int maxWidth: 2048
                            property int totalChunks: 0
                            visible: false
                            clip: true
                            anchors.left: recordStartPlaceHolder.left
                            width: 0
                            Repeater {
                                id: recWaveformRepeater
                                model: Math.ceil(recordPlaceHolder.width / recordPlaceHolder.maxWidth)
                                property bool repaintNodes: false
                                anchors.fill: parent
                                K.TimelineRecWaveform {
                                    id: recWave
                                    width: recordPlaceHolder.maxWidth < recordPlaceHolder.width ? index == recordPlaceHolder.totalChunks - 1 ? recordPlaceHolder.width % recordPlaceHolder.maxWidth : recordPlaceHolder.maxWidth : Math.round(recordPlaceHolder.width)
                                    height: recordPlaceHolder.height
                                    ix: index
                                    channels: recordPlaceHolder.channels
                                    isFirstChunk: index == 0
                                    isOpaque: true
                                    scaleFactor: root.timeScale
                                    format: K.KdenliveSettings.displayallchannels
                                    waveInPoint: Math.round((index * recordPlaceHolder.maxWidth / root.timeScale) * recordPlaceHolder.channels)
                                    waveOutPoint: waveInPoint + Math.round(width / root.timeScale) * recordPlaceHolder.channels
                                    fillColor0: Qt.rgba(1, 0, 0, 0.3)
                                    fillColor1: Qt.rgba(1, 0, 0)
                                    fillColor2: Qt.rgba(1, .5, 0)
                                    enforceRepaint: false
                                }
                            }
                            Text {
                                property int recState: audiorec.recordState
                                text: i18n("Recording")
                                anchors.right: parent.right
                                anchors.rightMargin: 2
                                anchors.top: parent.top
                                font: miniFont
                                color: '#FFF'
                                onRecStateChanged: {
                                    if (recState == 1) {
                                        // Recording
                                        text = i18n("Recording")
                                    } else if (recState == 2) {
                                        text = i18n("Paused")
                                    }
                                }
                            }
                        }
                        Repeater { 
                            id: guidesRepeater
                            model: guidesDelegateModel
                        }
                        Rectangle {
                            id: cursor
                            visible: root.consumerPosition > -1
                            color: root.textColor
                            width: 1
                            opacity: 1
                            height: tracksContainerArea.height
                            x: Math.round(root.consumerPosition * root.timeScale)
                        }
                    }
                    K.ZoomBar {
                        id: horZoomBar
                        visible: scrollView.visibleArea.widthRatio < 1
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: scrollView.bottom
                        }
                        height: Math.round(root.baseUnit * 0.7)
                        barMinWidth: root.baseUnit
                        fitsZoom: timeline.scaleFactor === root.fitZoom() && root.scrollPos() === 0
                        zoomFactor: scrollView.visibleArea.widthRatio
                        onProposeZoomFactor: (proposedValue) => {
                            timeline.scaleFactor = scrollView.width / Math.round(proposedValue * scrollView.contentWidth / root.timeScale)
                            zoomOnBar = true
                        }
                        contentPos: scrollView.contentX / scrollView.contentWidth
                        onProposeContentPos: (proposedValue) => { scrollView.contentX = Math.max(0, proposedValue * scrollView.contentWidth) }
                        onZoomByWheel: wheel => root.zoomByWheel(wheel)
                        onFitZoom: {
                            timeline.scaleFactor = root.fitZoom()
                            scrollView.contentX = 0
                            zoomOnBar = true
                        }
                    }
                }
            }
            Rectangle {
                id: cutLine
                visible: root.activeTool === K.ToolType.RazorTool && (tracksArea.mouseY > ruler.height || subtitleMouseArea.containsMouse)
                color: 'red'
                width: 1
                opacity: 1
                height: tracksContainerArea.height
                x: 0
                onVisibleChanged: {
                    if (visible) {
                        cutLine.x = getMousePos()
                    }
                }
                //x: root.consumerPosition * root.timeScale - scrollView.contentX
                y: ruler.height
                Rectangle {
                    color: 'red'
                    width: Math.max(0, 1 * root.timeScale - 1)
                    visible: width > 1
                    opacity: 0.2
                    anchors.left:parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                }
            }
            Rectangle {
                id: multicamLine
                visible: root.activeTool === K.ToolType.MulticamTool && timeline.multicamIn > -1
                color: 'purple'
                width: 3
                opacity: 1
                height: tracksContainerArea.height
                x: timeline.multicamIn * root.timeScale - scrollView.contentX
                y: ruler.height
                Rectangle {
                    // multicam in label
                    width: multilabel.contentWidth + 4
                    height: multilabel.contentHeight + 2
                    radius: height / 4
                    color: 'purple'
                    anchors {
                        top: parent.top
                        left: parent.left
                    }
                    Text {
                        id: multilabel
                        text: i18n("Multicam In")
                        bottomPadding: 2
                        leftPadding: 2
                        rightPadding: 2
                        font: miniFont
                        color: '#FFF'
                    }
                }
            }
        }
    }

    Rectangle {
        id: bubbleHelp
        property alias text: bubbleHelpLabel.text
        color: root.color //application.toolTipBaseColor
        width: bubbleHelpLabel.width + 6
        height: bubbleHelpLabel.height + 6
        radius: 3
        states: [
            State { name: 'invisible'; PropertyChanges { bubbleHelp.opacity: 0} },
            State { name: 'visible'; PropertyChanges { bubbleHelp.opacity: 0.8} }
        ]
        state: 'invisible'
        transitions: [
            Transition {
                from: 'invisible'
                to: 'visible'
                OpacityAnimator { target: bubbleHelp; duration: 200; easing.type: Easing.InOutQuad }
            },
            Transition {
                from: 'visible'
                to: 'invisible'
                OpacityAnimator { target: bubbleHelp; duration: 200; easing.type: Easing.InOutQuad }
            }
        ]
        Label {
            id: bubbleHelpLabel
            color: activePalette.text //application.toolTipTextColor
            anchors.centerIn: parent
            font: miniFont
        }
        function show(x, y, text) {
            bubbleHelp.text = text
            bubbleHelp.x = x + tracksArea.x - scrollView.contentX - bubbleHelp.width
            bubbleHelp.y = y + tracksArea.y - scrollView.contentY - bubbleHelp.height + ruler.height - 3
            if (bubbleHelp.state !== 'visible')
                bubbleHelp.state = 'visible'
        }
        function hide() {
            bubbleHelp.state = 'invisible'
            bubbleHelp.opacity = 0
        }
    }
    /*DropShadow {
        source: bubbleHelp
        anchors.fill: bubbleHelp
        opacity: bubbleHelp.opacity
        horizontalOffset: 3
        verticalOffset: 3
        radius: 8
        color: '#80000000'
        transparentBorder: true
        fast: true
    }*/

    DelegateModel {
        id: trackDelegateModel
        model: multitrack
        delegate: Track {
            trackModel: multitrack
            rootIndex: trackDelegateModel.modelIndex(index)
            width: tracksContainerArea.width
            height: trackHeight
            isAudio: audio
            isLocked: model.locked
            trackThumbsFormat: thumbsFormat
            trackInternalId: item
            effectZones: model.effectZones
            z: tracksRepeater.count - index
        }
    }


    DelegateModel {
        id: guidesDelegateModel
        model: guidesModel
        Item {
            id: guideRoot
            z: 20
            Rectangle {
                id: guideBase
                width: 1
                height: tracksContainerArea.height
                x: Math.round(model.frame * root.timeScale);
                color: model.color
            }
        }
    }


    DelegateModel {
        id: subtitleDelegateModel
        model: subtitleModel
        delegate: SubTitle {
            subId: model.id
            selected: model.selected
            startFrame: model.startframe
            fakeStartFrame: model.fakeStart
            endFrame: model.endframe
            subtitle: model.subtitle
            isGrabbed: model.grabbed
            subLayer: model.layer
        }
    }

    Connections {
        target: timeline
        function onFrameFormatChanged() {
            ruler.adjustFormat()
        }

        function onSelectionChanged() {
            if (dragProxy.draggedItem > -1 && !timeline.exists(dragProxy.draggedItem)) {
                endDrag()
            }
        }
    }

    // This provides continuous scrolling at the left/right edges.
    Timer {
        id: scrollTimer
        interval: 80
        repeat: true
        triggeredOnStart: true
        property int horizontal: 0
        property int vertical: 0
        onTriggered: {
            if (vertical != 0) {
                scrollView.contentY += vertical
                if (dragProxyArea.pressed && dragProxy.draggedItem > -1) {
                    dragProxyArea.moveItem()
                }
                if (scrollView.contentY <= 0) {
                    scrollView.contentY = 0
                    vertical = 0
                    stop()
                } else {
                    if ((clipBeingMovedId == -1 && clipBeingDroppedId == -1 && !rubberSelect.visible && spacerGroup == -1)) {
                        vertical = 0
                        stop()
                    } else {
                        var maxScroll = trackHeaders.height + subtitleTrack.height - scrollView.height
                        if (scrollView.contentY > maxScroll) {
                            scrollView.contentY = Math.max(0, maxScroll)
                            vertical = 0
                            stop()
                        }
                    }
                }
            }
            if (horizontal != 0) {
                if (scrollView.contentX < -horizontal) {
                    horizontal = - scrollView.contentX
                    scrollView.contentX = 0
                } else {
                    scrollView.contentX += horizontal
                }
                if (clipBeingDroppedId > -1) {
                    if (clipDropArea.containsDrag) {
                        clipDropArea.moveDrop(horizontal, vertical)
                    } else if (compoArea.containsDrag) {
                        compoArea.moveDrop(horizontal, vertical)
                    }
                } else if (dragProxy.draggedItem > -1) {
                    dragProxy.x += horizontal
                    dragProxyArea.moveItem()
                }
                if (scrollView.contentX == 0 || (clipBeingMovedId == -1 && clipBeingDroppedId == -1 && !rubberSelect.visible)) {
                    if (root.subtitleMoving) {
                        root.subtitleItem.checkOffset(horizontal)
                    } else {
                        horizontal = 0
                        stop()
                    }
                }
            }
            if (rubberSelect.visible) {
                if (horizontal != 0) {
                    if (rubberSelect.x < rubberSelect.originX) {
                        if (horizontal < 0) {
                            // Expanding left
                            rubberSelect.x += horizontal
                            rubberSelect.width -= horizontal
                        } else if (horizontal < rubberSelect.width) {
                            // Expanding right
                            rubberSelect.x -= horizontal
                            rubberSelect.width -= horizontal
                        } else {
                            // Switching direction
                            rubberSelect.width = rubberSelect.x + rubberSelect.width + horizontal - rubberSelect.originX
                            rubberSelect.x = rubberSelect.originX
                        }
                    } else {
                        rubberSelect.x = rubberSelect.originX
                        rubberSelect.width += horizontal
                    }
                }
                if (vertical != 0) {
                    if (rubberSelect.y < rubberSelect.originY) {
                         if (vertical < 0) {
                            // Expanding up
                            rubberSelect.y += vertical
                            rubberSelect.height = rubberSelect.originY - rubberSelect.y
                        } else if (vertical < rubberSelect.height) {
                            // Expanding bottom
                            rubberSelect.y += vertical
                            rubberSelect.height = rubberSelect.originY - rubberSelect.y
                        } else {
                            // Switching direction
                            rubberSelect.height = rubberSelect.y + rubberSelect.height + horizontal - rubberSelect.originY
                            rubberSelect.y = rubberSelect.originY
                        }
                    } else {
                        rubberSelect.y = rubberSelect.originY
                        rubberSelect.height += vertical
                    }
                }
            }
        }
    }
}

