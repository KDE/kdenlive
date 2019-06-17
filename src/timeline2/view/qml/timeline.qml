import QtQuick 2.6
import QtQml.Models 2.2
import QtQuick.Controls 1.4 as OLD
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import Kdenlive.Controls 1.0
import QtQuick.Window 2.2
import 'Timeline.js' as Logic

Rectangle {
    id: root
    objectName: "timelineview"

    SystemPalette { id: activePalette }
    color: activePalette.window
    property bool validMenu: false
    property color textColor: activePalette.text
    property bool dragInProgress: dragProxyArea.pressed || dragProxyArea.drag.active

    signal clipClicked()
    signal mousePosChanged(int position)
    signal zoomIn(bool onMouse)
    signal zoomOut(bool onMouse)
    signal processingDrag(bool dragging)

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }
    ClipMenu {
        id: clipMenu
    }
    CompositionMenu {
        id: compositionMenu
    }

    onDragInProgressChanged: {
        processingDrag(!root.dragInProgress)
    }

    function fitZoom() {
        return scrollView.width / (timeline.duration * 1.1)
    }

    function scrollPos() {
        return scrollView.flickableItem.contentX
    }

    function goToStart(pos) {
        scrollView.flickableItem.contentX = pos
    }

    function updatePalette() {
        root.color = activePalette.window
        root.textColor = activePalette.text
        playhead.fillColor = activePalette.windowText
        ruler.repaintRuler()
    }

    function moveSelectedTrack(offset) {
        var cTrack = Logic.getTrackIndexFromId(timeline.activeTrack)
        var newTrack = cTrack + offset
        var max = tracksRepeater.count;
        if (newTrack < 0) {
            newTrack = max - 1;
        } else if (newTrack >= max) {
            newTrack = 0;
        }
        console.log('Setting curr tk: ', newTrack, 'MAX: ',max)
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
            // Vertical scroll
            var newScroll = Math.min(scrollView.flickableItem.contentY - wheel.angleDelta.y, trackHeaders.height - tracksArea.height + scrollView.__horizontalScrollBar.height + cornerstone.height)
            scrollView.flickableItem.contentY = Math.max(newScroll, 0)
        } else {
            // Horizontal scroll
            var newScroll = Math.min(scrollView.flickableItem.contentX - wheel.angleDelta.y, timeline.fullDuration * root.timeScale - (scrollView.width - scrollView.__verticalScrollBar.width))
            scrollView.flickableItem.contentX = Math.max(newScroll, 0)
        }
        wheel.accepted = true
    }

    function continuousScrolling(x) {
        // This provides continuous scrolling at the left/right edges.
        if (x > scrollView.flickableItem.contentX + scrollView.width - 50) {
            scrollTimer.item = clip
            scrollTimer.backwards = false
            scrollTimer.start()
        } else if (x < 50) {
            scrollView.flickableItem.contentX = 0;
            scrollTimer.stop()
        } else if (x < scrollView.flickableItem.contentX + 50) {
            scrollTimer.item = clip
            scrollTimer.backwards = true
            scrollTimer.start()
        } else {
            scrollTimer.stop()
        }
    }
    function getTrackYFromId(a_track) {
        return Logic.getTrackYFromId(a_track)
    }

    function getTrackYFromMltIndex(a_track) {
        return Logic.getTrackYFromMltIndex(a_track)
    }

    function getTracksCount() {
        return Logic.getTracksList()
    }

    function getMousePos() {
        return (scrollView.flickableItem.contentX + tracksArea.mouseX) / timeline.scaleFactor
    }

    function getScrollPos() {
        return scrollView.flickableItem.contentX
    }

    function setScrollPos(pos) {
        return scrollView.flickableItem.contentX = pos
    }

    function getCopiedItemId() {
        return copiedClip
    }

    function getMouseTrack() {
        return Logic.getTrackIdFromPos(tracksArea.mouseY - ruler.height + scrollView.flickableItem.contentY)
    }

    function getTrackColor(audio, header) {
        var col = activePalette.alternateBase
        if (audio) {
            col = Qt.tint(col, "#06FF00CC")
        }
        if (header) {
            col = Qt.darker(col, 1.05)
        }
        return col
    }

    function clearDropData() {
        clipBeingDroppedId = -1
        droppedPosition = -1
        droppedTrack = -1
        scrollTimer.running = false
        scrollTimer.stop()
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
    function endDrag() {
        dragProxy.draggedItem = -1
        dragProxy.x = 0
        dragProxy.y = 0
        dragProxy.width = 0
        dragProxy.height = 0
        dragProxy.verticalOffset = 0
    }

    function getItemAtPos(tk, posx, isComposition) {
        var track = Logic.getTrackById(tk)
        var container = track.children[0]
        var tentativeClip = undefined
        //console.log('TESTING ITMES OK TK: ', tk, ', POS: ', posx, ', CHILREN: ', container.children.length, ', COMPO: ', isComposition)
        for (var i = 0 ; i < container.children.length; i++) {
            if (container.children[i].children.length == 0 || container.children[i].children[0].children.length == 0) {
                continue
            }
            tentativeClip = container.children[i].children[0].childAt(posx, 1)
            if (tentativeClip && tentativeClip.clipId && (tentativeClip.isComposition == isComposition)) {
                //console.log('found item with id: ', tentativeClip.clipId, ' IS COMPO: ', tentativeClip.isComposition)
                break
            }
        }
        return tentativeClip
    }

    property int headerWidth: timeline.headerWidth()
    property int activeTool: 0
    property real baseUnit: fontMetrics.font.pointSize
    property color selectedTrackColor: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.2)
    property color frameColor: Qt.rgba(activePalette.shadow.r, activePalette.shadow.g, activePalette.shadow.b, 0.3)
    property bool stopScrolling: false
    property int duration: timeline.duration
    property color audioColor: timeline.audioColor
    property color videoColor: timeline.videoColor
    property color lockedColor: timeline.lockedColor
    property color selectionColor: timeline.selectionColor
    property color groupColor: timeline.groupColor
    property int clipBeingDroppedId: -1
    property string clipBeingDroppedData
    property int droppedPosition: -1
    property int droppedTrack: -1
    property int clipBeingMovedId: -1
    property int spacerGroup: -1
    property int spacerFrame: -1
    property int spacerClickFrame: -1
    property real timeScale: timeline.scaleFactor
    property real snapping: (timeline.snap && (timeScale < 2 * baseUnit)) ? 10 / Math.sqrt(timeScale) - 0.5 : -1
    property var timelineSelection: timeline.selection
    property int trackHeight
    property int copiedClip: -1
    property int zoomOnMouse: -1
    property int viewActiveTrack: timeline.activeTrack
    property int wheelAccumulatedDelta: 0
    readonly property int defaultDeltasPerStep: 120

    //onCurrentTrackChanged: timeline.selection = []
    onTimeScaleChanged: {
        if (root.zoomOnMouse >= 0) {
            scrollView.flickableItem.contentX = Math.max(0, root.zoomOnMouse * timeline.scaleFactor - tracksArea.mouseX)
            root.zoomOnMouse = -1
        } else {
            scrollView.flickableItem.contentX = Math.max(0, (timeline.seekPosition > -1 ? timeline.seekPosition : timeline.position) * timeline.scaleFactor - (scrollView.width / 2))
        }
        //root.snapping = timeline.snap ? 10 / Math.sqrt(root.timeScale) : -1
        ruler.adjustStepSize()
        if (dragProxy.draggedItem > -1 && dragProxy.masterObject) {
            // update dragged item pos
            dragProxy.masterObject.updateDrag()
        }
    }

    onViewActiveTrackChanged: {
        var tk = Logic.getTrackById(timeline.activeTrack)
        if (tk.y < scrollView.flickableItem.contentY) {
            scrollView.flickableItem.contentY = Math.max(0, tk.y - scrollView.height / 3)
        } else if (tk.y + tk.height > scrollView.flickableItem.contentY + scrollView.viewport.height) {
            scrollView.flickableItem.contentY = Math.min(trackHeaders.height - scrollView.height + scrollView.__horizontalScrollBar.height, tk.y - scrollView.height / 3)
        }
    }

    onActiveToolChanged: {
        if (root.activeTool == 2) {
            // Spacer activated
            endDrag()
        } else if (root.activeTool == 0) {
            var tk = getMouseTrack()
            if (tk < 0) {
                console.log('........ MOUSE OUTSIDE TRAKS\n\n.........')
                return
            }
            var pos = getMousePos() * timeline.scaleFactor
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
        width: root.width - headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: headerWidth
        keys: 'kdenlive/composition'
        onEntered: {
            console.log("Trying to drop composition")
            if (clipBeingMovedId == -1) {
                console.log("No clip being moved")
                var track = Logic.getTrackIdFromPos(drag.y + scrollView.flickableItem.contentY)
                var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                droppedPosition = frame
                if (track >= 0 && !controller.isAudioTrack(track)) {
                    clipBeingDroppedData = drag.getDataAsString('kdenlive/composition')
                    console.log("Trying to insert",track, frame, clipBeingDroppedData)
                    clipBeingDroppedId = timeline.insertComposition(track, frame, clipBeingDroppedData, false)
                    console.log("id",clipBeingDroppedId)
                    continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    drag.acceptProposedAction()
                } else {
                    drag.accepted = false
                }
            }
        }
        onPositionChanged: {
            if (clipBeingMovedId == -1) {
                var track = Logic.getTrackIdFromPos(drag.y + scrollView.flickableItem.contentY)
                if (track !=-1) {
                    var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                    frame = controller.suggestSnapPoint(frame, Math.floor(root.snapping))
                    if (clipBeingDroppedId >= 0){
                        if (controller.isAudioTrack(track)) {
                            // Don't allow moving composition to an audio track
                            track = controller.getCompositionTrackId(clipBeingDroppedId)
                        }
                        controller.requestCompositionMove(clipBeingDroppedId, track, frame, true, false)
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    } else if (!controller.isAudioTrack(track)) {
                        clipBeingDroppedData = drag.getDataAsString('kdenlive/composition')
                        clipBeingDroppedId = timeline.insertComposition(track, frame, clipBeingDroppedData , false)
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    }
                }
            }
        }
        onExited:{
            if (clipBeingDroppedId != -1) {
                controller.requestItemDeletion(clipBeingDroppedId, false)
            }
            clearDropData()
        }
        onDropped: {
            if (clipBeingDroppedId != -1) {
                var frame = controller.getCompositionPosition(clipBeingDroppedId)
                var track = controller.getCompositionTrackId(clipBeingDroppedId)
                // we simulate insertion at the final position so that stored undo has correct value
                controller.requestItemDeletion(clipBeingDroppedId, false)
                timeline.insertNewComposition(track, frame, clipBeingDroppedData, true)
            }
            clearDropData()
        }
    }
    DropArea { //Drop area for bin/clips
        /** @brief local helper function to handle the insertion of multiple dragged items */
        function insertAndMaybeGroup(track, frame, droppedData) {
            var binIds = droppedData.split(";")
            if (binIds.length == 0) {
                return -1
            }

            var id = -1
            if (binIds.length == 1) {
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
        width: root.width - headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: headerWidth
        keys: 'kdenlive/producerslist'
        onEntered: {
            if (clipBeingMovedId == -1) {
                //var track = Logic.getTrackIdFromPos(drag.y)
                var track = Logic.getTrackIndexFromPos(drag.y + scrollView.flickableItem.contentY)
                if (track >= 0  && track < tracksRepeater.count) {
                    var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                    droppedPosition = frame
                    timeline.activeTrack = tracksRepeater.itemAt(track).trackInternalId
                    //drag.acceptProposedAction()
                    clipBeingDroppedData = drag.getDataAsString('kdenlive/producerslist')
                    console.log('dropped data: ', clipBeingDroppedData)
                    if (controller.normalEdit()) {
                        clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, frame, clipBeingDroppedData)
                    } else {
                        // we want insert/overwrite mode, make a fake insert at end of timeline, then move to position
                        clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, timeline.fullDuration, clipBeingDroppedData)
                        fakeFrame = controller.suggestClipMove(clipBeingDroppedId, timeline.activeTrack, frame, timeline.position, Math.floor(root.snapping))
                        fakeTrack = timeline.activeTrack
                    }
                    continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                } else {
                    drag.accepted = false
                }
            }
        }
        onExited:{
            if (clipBeingDroppedId != -1) {
                controller.requestItemDeletion(clipBeingDroppedId, false)
            }
            clearDropData()
        }
        onPositionChanged: {
            if (clipBeingMovedId == -1) {
                var track = Logic.getTrackIndexFromPos(drag.y + scrollView.flickableItem.contentY)
                if (track >= 0  && track < tracksRepeater.count) {
                    timeline.activeTrack = tracksRepeater.itemAt(track).trackInternalId
                    var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                    if (clipBeingDroppedId >= 0){
                        fakeFrame = controller.suggestClipMove(clipBeingDroppedId, timeline.activeTrack, frame, timeline.position, Math.floor(root.snapping))
                        fakeTrack = timeline.activeTrack
                        //controller.requestClipMove(clipBeingDroppedId, timeline.activeTrack, frame, true, false, false)
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    } else {
                        frame = controller.suggestSnapPoint(frame, Math.floor(root.snapping))
                        if (controller.normalEdit()) {
                            clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, frame, drag.getDataAsString('kdenlive/producerslist'), false, true)
                        } else {
                            // we want insert/overwrite mode, make a fake insert at end of timeline, then move to position
                            clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, timeline.fullDuration, clipBeingDroppedData)
                            fakeFrame = controller.suggestClipMove(clipBeingDroppedId, timeline.activeTrack, frame, timeline.position, Math.floor(root.snapping))
                            fakeTrack = timeline.activeTrack
                        }
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    }
                }
            }
        }
        onDropped: {
            if (clipBeingDroppedId != -1) {
                var frame = controller.getClipPosition(clipBeingDroppedId)
                var track = controller.getClipTrackId(clipBeingDroppedId)
                if (!controller.normalEdit()) {
                    frame = fakeFrame
                    track = fakeTrack
                }
                /* We simulate insertion at the final position so that stored undo has correct value
                 * NOTE: even if dropping multiple clips, requesting the deletion of the first one is
                 * enough as internally it will request the group deletion
                 */
                controller.requestItemDeletion(clipBeingDroppedId, false)

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
            }
            clearDropData()
        }
    }
    OLD.Menu {
        id: menu
        property int clickedX
        property int clickedY
        onAboutToHide: {
            timeline.ungrabHack()
            editGuideMenu.visible = false
        }
        OLD.MenuItem {
            text: i18n("Paste")
            iconName: 'edit-paste'
            visible: copiedClip != -1
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height + scrollView.flickableItem.contentY)
                var frame = Math.floor((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.pasteItem(frame, track)
            }
        }
        OLD.MenuItem {
            text: i18n("Insert Space")
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height + scrollView.flickableItem.contentY)
                var frame = Math.floor((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.insertSpace(track, frame);
            }
        }
        OLD.MenuItem {
            text: i18n("Remove Space On Active Track")
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height + scrollView.flickableItem.contentY)
                var frame = Math.floor((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.removeSpace(track, frame);
            }
        }
        OLD.MenuItem {
            text: i18n("Remove Space")
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height + scrollView.flickableItem.contentY)
                var frame = Math.floor((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.removeSpace(track, frame, true);
            }
        }
        OLD.MenuItem {
            id: addGuideMenu
            text: i18n("Add Guide")
            onTriggered: {
                timeline.switchGuide(timeline.position);
            }
        }
        GuidesMenu {
            title: i18n("Go to guide...")
            menuModel: guidesModel
            enabled: guidesDelegateModel.count > 0
            onGuideSelected: {
                timeline.seekPosition = assetFrame
                timeline.position = timeline.seekPosition
            }
        }
        OLD.MenuItem {
            id: editGuideMenu
            text: i18n("Edit Guide")
            visible: false
            onTriggered: {
                timeline.editGuide(timeline.position);
            }
        }
        AssetMenu {
            title: i18n("Insert a composition...")
            menuModel: transitionModel
            isTransition: true
            onAssetSelected: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height + scrollView.flickableItem.contentY)
                var frame = Math.round((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                var id = timeline.insertComposition(track, frame, assetId, true)
                if (id == -1) {
                    compositionFail.open()
                }
            }
        }
        onAboutToShow: {
            if (guidesModel.hasMarker(timeline.position)) {
                // marker at timeline position
                addGuideMenu.text = i18n("Remove Guide")
                editGuideMenu.visible = true
            } else {
                addGuideMenu.text = i18n("Add Guide")
            }
            console.log("pop menu")
        }
    }
    OLD.Menu {
        id: rulermenu
        property int clickedX
        property int clickedY
        onAboutToHide: {
            timeline.ungrabHack()
            editGuideMenu2.visible = false
        }
        OLD.MenuItem {
            id: addGuideMenu2
            text: i18n("Add Guide")
            onTriggered: {
                timeline.switchGuide(timeline.position);
            }
        }
        GuidesMenu {
            title: i18n("Go to guide...")
            menuModel: guidesModel
            enabled: guidesDelegateModel.count > 0
            onGuideSelected: {
                timeline.seekPosition = assetFrame
                timeline.position = timeline.seekPosition
            }
        }
        OLD.MenuItem {
            id: editGuideMenu2
            text: i18n("Edit Guide")
            visible: false
            onTriggered: {
                timeline.editGuide(timeline.position);
            }
        }
        OLD.MenuItem {
            id: addProjectNote
            text: i18n("Add Project Note")
            onTriggered: {
                timeline.triggerAction('add_project_note')
            }
        }
        onAboutToShow: {
            if (guidesModel.hasMarker(timeline.position)) {
                // marker at timeline position
                addGuideMenu2.text = i18n("Remove Guide")
                editGuideMenu2.visible = true
            } else {
                addGuideMenu2.text = i18n("Add Guide")
            }
            console.log("pop menu")
        }
    }
    MessageDialog {
        id: compositionFail
        title: i18n("Timeline error")
        icon: StandardIcon.Warning
        text: i18n("Impossible to add a composition at that position. There might not be enough space")
        standardButtons: StandardButton.Ok
    }
    OLD.Menu {
        id: headerMenu
        property int trackId: -1
        property int thumbsFormat: 0
        property bool audioTrack: false
        property bool recEnabled: false
        onAboutToHide: {
            timeline.ungrabHack()
        }
        OLD.MenuItem {
            text: i18n("Add Track")
            onTriggered: {
                timeline.addTrack(timeline.activeTrack)
            }
        }
        OLD.MenuItem {
            text: i18n("Delete Track")
            onTriggered: {
                timeline.deleteTrack(timeline.activeTrack)
            }
        }
        OLD.MenuItem {
            visible: headerMenu.audioTrack
            id: showRec
            text: i18n("Show Record Controls")
            onTriggered: {
                controller.setTrackProperty(headerMenu.trackId, "kdenlive:audio_rec", showRec.checked ? '1' : '0')
            }
            checkable: true
            checked: headerMenu.recEnabled
        }
        OLD.MenuItem {
            visible: headerMenu.audioTrack
            id: configRec
            text: i18n("Configure Recording")
            onTriggered: {
                timeline.showConfig(4,2)
            }
        }
        OLD.Menu {
            title: i18n("Track thumbnails")
            visible: !headerMenu.audioTrack
                    OLD.ExclusiveGroup { id: thumbStyle }
                    OLD.MenuItem {
                        text: i18n("In frame")
                        id: inFrame
                        onTriggered:controller.setTrackProperty(headerMenu.trackId, "kdenlive:thumbs_format", 2)
                        checkable: true
                        exclusiveGroup: thumbStyle
                    }
                    OLD.MenuItem {
                        text: i18n("In / out frames")
                        id: inOutFrame
                        onTriggered:controller.setTrackProperty(headerMenu.trackId, "kdenlive:thumbs_format", 0)
                        checkable: true
                        checked: true
                        exclusiveGroup: thumbStyle
                    }
                    OLD.MenuItem {
                        text: i18n("All frames")
                        id: allFrame
                        onTriggered:controller.setTrackProperty(headerMenu.trackId, "kdenlive:thumbs_format", 1)
                        checkable: true
                        exclusiveGroup: thumbStyle
                    }
                    OLD.MenuItem {
                        text: i18n("No thumbnails")
                        id: noFrame
                        onTriggered:controller.setTrackProperty(headerMenu.trackId, "kdenlive:thumbs_format", 3)
                        checkable: true
                        exclusiveGroup: thumbStyle
                    }
                onAboutToShow: {
                        switch(headerMenu.thumbsFormat) {
                            case 3:
                                noFrame.checked = true
                                break
                            case 2:
                                inFrame.checked = true
                                break
                            case 1:
                                allFrame.checked = true
                                break
                            default:
                                inOutFrame.checked = true
                                break
                        }
                }
        }
    }

    Row {
        Column {
            id: headerContainer
            z: 1
            Rectangle {
                id: cornerstone
                property bool selected: false
                // Padding between toolbar and track headers.
                width: headerWidth
                height: ruler.height
                color: 'transparent' //selected? shotcutBlue : activePalette.window
                border.color: selected? 'red' : 'transparent'
                border.width: selected? 1 : 0
                z: 1
            }
            Flickable {
                // Non-slider scroll area for the track headers.
                id: headerFlick
                contentY: scrollView.flickableItem.contentY
                width: headerWidth
                height: 100
                interactive: false

                MouseArea {
                    width: trackHeaders.width
                    height: trackHeaders.height
                    acceptedButtons: Qt.NoButton
                    onWheel: {
                        var newScroll = Math.min(scrollView.flickableItem.contentY - wheel.angleDelta.y, height - tracksArea.height + scrollView.__horizontalScrollBar.height + cornerstone.height)
                        scrollView.flickableItem.contentY = Math.max(newScroll, 0)
                    }
                }
                Column {
                    id: trackHeaders
                    spacing: 0
                    Repeater {
                        id: trackHeaderRepeater
                        model: multitrack
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
                            width: headerWidth
                            current: item === timeline.activeTrack
                            trackId: item
                            height: model.trackHeight
                            onIsLockedChanged: tracksRepeater.itemAt(index).isLocked = isLocked
                            collapsed: height <= collapsedHeight
                            onMyTrackHeightChanged: {
                                collapsed = myTrackHeight <= collapsedHeight
                                if (!collapsed) {
                                    controller.setTrackProperty(trackId, "kdenlive:trackheight", myTrackHeight)
                                    controller.setTrackProperty(trackId, "kdenlive:collapsed", "0")
                                } else {
                                    controller.setTrackProperty(trackId, "kdenlive:collapsed", collapsedHeight)
                                }
                                // hack: change property to trigger transition adjustment
                                root.trackHeight = root.trackHeight === 1 ? 0 : 1
                            }
                            onClicked: {
                                timeline.activeTrack = tracksRepeater.itemAt(index).trackInternalId
                                console.log('track name: ',index, ' = ', model.name,'/',tracksRepeater.itemAt(index).trackInternalId)
                                //timeline.selectTrackHead(currentTrack)
                            }
                        }
                    }
                }
                Column {
                    id: trackHeadersResizer
                    spacing: 0
                    width: 5
                    Rectangle {
                        id: resizer
                        height: trackHeaders.height
                        width: 3
                        x: root.headerWidth - 2
                        color: 'red'
                        opacity: 0
                        Drag.active: headerMouseArea.drag.active
                        Drag.proposedAction: Qt.MoveAction

                        MouseArea {
                            id: headerMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.SizeHorCursor
                            drag.target: parent
                            drag.axis: Drag.XAxis
                            drag.minimumX: 2 * baseUnit
                            property double startX
                            property double originalX
                            drag.smoothed: false

                            onPressed: {
                                root.stopScrolling = true
                            }
                            onReleased: {
                                root.stopScrolling = false
                                parent.opacity = 0
                            }
                            onEntered: parent.opacity = 0.5
                            onExited: parent.opacity = 0
                            onPositionChanged: {
                                if (mouse.buttons === Qt.LeftButton) {
                                    parent.opacity = 0.5
                                    headerWidth = Math.max(10, mapToItem(null, x, y).x + 2)
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
            width: root.width - headerWidth
            height: root.height
            Keys.onDownPressed: {
                root.moveSelectedTrack(1)
            }
            Keys.onUpPressed: {
                root.moveSelectedTrack(-1)
            }
            // This provides continuous scrubbing and scimming at the left/right edges.
            hoverEnabled: true
            acceptedButtons: Qt.RightButton | Qt.LeftButton | Qt.MidButton
            cursorShape: tracksArea.mouseY < ruler.height || root.activeTool === 0 ? Qt.ArrowCursor : root.activeTool === 1 ? Qt.IBeamCursor : Qt.SplitHCursor
            onWheel: {
                if (wheel.modifiers & Qt.AltModifier) {
                    // Alt + wheel = seek to next snap point
                    if (wheel.angleDelta.x > 0) {
                        timeline.triggerAction('monitor_seek_snap_backward')
                    } else {
                        timeline.triggerAction('monitor_seek_snap_forward')
                    }
                } else {
                    var delta = wheel.modifiers & Qt.ShiftModifier ? timeline.fps() : 1
                    if (timeline.seekPosition > -1) {
                        timeline.seekPosition = Math.min(timeline.seekPosition - (wheel.angleDelta.y > 0 ? delta : -delta), timeline.fullDuration - 1)
                    } else {
                        timeline.seekPosition = Math.min(timeline.position - (wheel.angleDelta.y > 0 ? delta : -delta), timeline.fullDuration - 1)
                    }
                    timeline.position = timeline.seekPosition
                }
            }
            onPressed: {
                focus = true
                if (mouse.buttons === Qt.MidButton || (root.activeTool == 0 && mouse.modifiers & Qt.ControlModifier)) {
                    clickX = mouseX
                    clickY = mouseY
                    return
                }
                if (root.activeTool === 0 && mouse.modifiers & Qt.ShiftModifier && mouse.y > ruler.height) {
                        // rubber selection
                        rubberSelect.x = mouse.x + tracksArea.x
                        rubberSelect.y = mouse.y
                        rubberSelect.originX = mouse.x
                        rubberSelect.originY = rubberSelect.y
                        rubberSelect.width = 0
                        rubberSelect.height = 0
                } else if (mouse.button & Qt.LeftButton) {
                    if (root.activeTool === 1) {
                        // razor tool
                        var y = mouse.y - ruler.height + scrollView.flickableItem.contentY
                        timeline.cutClipUnderCursor((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y)).trackInternalId)
                    }
                    if (dragProxy.draggedItem > -1) {
                        mouse.accepted = false
                        return
                    }
                    if (root.activeTool === 2 && mouse.y > ruler.height) {
                        // spacer tool
                        var y = mouse.y - ruler.height + scrollView.flickableItem.contentY
                        var frame = (scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor
                        var track = (mouse.modifiers & Qt.ControlModifier) ? tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y)).trackInternalId : -1
                        spacerGroup = timeline.requestSpacerStartOperation(track, frame)
                        if (spacerGroup > -1) {
                            drag.axis = Drag.XAxis
                            Drag.active = true
                            Drag.proposedAction = Qt.MoveAction
                            spacerClickFrame = frame
                            spacerFrame = controller.getItemPosition(spacerGroup)
                        }
                    } else if (root.activeTool === 0 || mouse.y <= ruler.height) {
                        if (mouse.y > ruler.height) {
                            controller.requestClearSelection();
                        }
                        timeline.seekPosition = Math.min((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, timeline.fullDuration - 1)
                        timeline.position = timeline.seekPosition
                    }
                } else if (mouse.button & Qt.RightButton) {
                    menu.clickedX = mouse.x
                    menu.clickedY = mouse.y
                    if (mouse.y > ruler.height) {
                        timeline.activeTrack = tracksRepeater.itemAt(Logic.getTrackIndexFromPos(mouse.y - ruler.height + scrollView.flickableItem.contentY)).trackInternalId
                        menu.popup()
                    } else {
                        // ruler menu
                        rulermenu.popup()
                    }
                }
            }
            property bool scim: false
            onExited: {
                scim = false
            }
            onPositionChanged: {
                if (pressed && ((mouse.buttons === Qt.MidButton) || (mouse.buttons === Qt.LeftButton && root.activeTool == 0 && mouse.modifiers & Qt.ControlModifier))) {
                    var newScroll = Math.min(scrollView.flickableItem.contentX - (mouseX - clickX), timeline.fullDuration * root.timeScale - (scrollView.width - scrollView.__verticalScrollBar.width))
                    var vertScroll = Math.min(scrollView.flickableItem.contentY - (mouseY - clickY), trackHeaders.height - scrollView.height + scrollView.__horizontalScrollBar.height)
                    scrollView.flickableItem.contentX = Math.max(newScroll, 0)
                    scrollView.flickableItem.contentY = Math.max(vertScroll, 0)
                    clickX = mouseX
                    clickY = mouseY
                    return
                }
                if (!pressed && !rubberSelect.visible && root.activeTool === 1) {
                    cutLine.x = Math.floor((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor) * timeline.scaleFactor - scrollView.flickableItem.contentX
                    if (mouse.modifiers & Qt.ShiftModifier) {
                        timeline.position = Math.floor((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor)
                    }
                }
                if (dragProxy.draggedItem > -1) {
                    mouse.accepted = false
                    return
                }
                var mousePos = Math.max(0, Math.round((mouse.x + scrollView.flickableItem.contentX) / timeline.scaleFactor))
                root.mousePosChanged(mousePos)
                ruler.showZoneLabels = mouse.y < ruler.height
                if (mouse.modifiers & Qt.ShiftModifier && mouse.buttons === Qt.LeftButton && root.activeTool === 0 && !rubberSelect.visible && rubberSelect.y > 0) {
                    // rubber selection
                    rubberSelect.visible = true
                }
                if (rubberSelect.visible) {
                    var newX = mouse.x
                    var newY = mouse.y
                    if (newX < rubberSelect.originX) {
                        rubberSelect.x = newX + tracksArea.x
                        rubberSelect.width = rubberSelect.originX - newX
                    } else {
                        rubberSelect.x = rubberSelect.originX + tracksArea.x
                        rubberSelect.width = newX - rubberSelect.originX
                    }
                    if (newY < rubberSelect.originY) {
                        rubberSelect.y = newY
                        rubberSelect.height = rubberSelect.originY - newY
                    } else {
                        rubberSelect.y = rubberSelect.originY
                        rubberSelect.height= newY - rubberSelect.originY
                    }
                } else if (mouse.buttons === Qt.LeftButton) {
                    if (root.activeTool === 0 || mouse.y < ruler.height) {
                        timeline.seekPosition = Math.max(0, Math.min((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, timeline.fullDuration - 1))
                        timeline.position = timeline.seekPosition
                    } else if (root.activeTool === 2 && spacerGroup > -1) {
                        // Move group
                        var track = controller.getItemTrackId(spacerGroup)
                        var frame = Math.round((mouse.x + scrollView.flickableItem.contentX) / timeline.scaleFactor) + spacerFrame - spacerClickFrame
                        frame = controller.suggestItemMove(spacerGroup, track, frame, timeline.position, Math.floor(root.snapping))
                        continuousScrolling(mouse.x + scrollView.flickableItem.contentX)
                    }
                    scim = true
                } else {
                    scim = false
                }
            }
            onReleased: {
                if (rubberSelect.visible) {
                    rubberSelect.visible = false
                    var y = rubberSelect.y - ruler.height + scrollView.flickableItem.contentY
                    var topTrack = Logic.getTrackIndexFromPos(Math.max(0, y))
                    var bottomTrack = Logic.getTrackIndexFromPos(y + rubberSelect.height)
                    if (bottomTrack >= topTrack) {
                        var t = []
                        for (var i = topTrack; i <= bottomTrack; i++) {
                            t.push(tracksRepeater.itemAt(i).trackInternalId)
                        }
                        var startFrame = (scrollView.flickableItem.contentX - tracksArea.x + rubberSelect.x) / timeline.scaleFactor
                        var endFrame = (scrollView.flickableItem.contentX - tracksArea.x + rubberSelect.x + rubberSelect.width) / timeline.scaleFactor
                        timeline.selectItems(t, startFrame, endFrame, mouse.modifiers & Qt.ControlModifier);
                    }
                    rubberSelect.y = -1
                } else if (mouse.modifiers & Qt.ShiftModifier) {
                    // Shift click, process seek
                    timeline.seekPosition = Math.min((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, timeline.fullDuration - 1)
                    timeline.position = timeline.seekPosition
                }
                if (spacerGroup > -1) {
                    var frame = controller.getItemPosition(spacerGroup)
                    timeline.requestSpacerEndOperation(spacerGroup, spacerFrame, frame);
                    spacerClickFrame = -1
                    spacerFrame = -1
                    spacerGroup = -1
                }
                scim = false
            }

            Column {
                Flickable {
                    // Non-slider scroll area for the Ruler.
                    id: rulercontainer
                    width: root.width - headerWidth
                    height: fontMetrics.font.pixelSize * 2
                    contentX: scrollView.flickableItem.contentX
                    contentWidth: Math.max(parent.width, timeline.fullDuration * timeScale)
                    interactive: false
                    clip: true
                    Ruler {
                        id: ruler
                        width: rulercontainer.contentWidth
                        height: parent.height
                        Rectangle {
                            id: seekCursor
                            visible: timeline.seekPosition > -1
                            color: activePalette.highlight
                            width: 4
                            height: ruler.height
                            opacity: 0.5
                            x: timeline.seekPosition * timeline.scaleFactor
                        }
                        TimelinePlayhead {
                            id: playhead
                            visible: timeline.position > -1
                            height: baseUnit
                            width: baseUnit * 1.5
                            fillColor: activePalette.windowText
                            anchors.bottom: parent.bottom
                            x: timeline.position * timeline.scaleFactor - (width / 2)
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: zoomByWheel(wheel)
                            cursorShape: dragProxyArea.drag.active ? Qt.ClosedHandCursor : tracksArea.cursorShape
                        }
                    }
                }
                OLD.ScrollView {
                    id: scrollView
                    width: root.width - headerWidth
                    height: root.height - ruler.height
                    y: ruler.height
                    // Click and drag should seek, not scroll the timeline view
                    flickableItem.interactive: false
                    clip: true
                    Rectangle {
                        id: tracksContainerArea
                        width: Math.max(scrollView.width - scrollView.__verticalScrollBar.width, timeline.fullDuration * timeScale)
                        height: trackHeaders.height
                        //Math.max(trackHeaders.height, scrollView.contentHeight - scrollView.__horizontalScrollBar.height)
                        color: root.color
                        Rectangle {
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
                            color: 'transparent'
                            //opacity: 0.8
                            MouseArea {
                                id: dragProxyArea
                                anchors.fill: parent
                                drag.target: parent
                                drag.axis: Drag.XAxis
                                drag.smoothed: false
                                drag.minimumX: 0
                                property int dragFrame
                                property bool shiftClick: false
                                enabled: root.activeTool == 0
                                onPressed: {
                                    console.log('+++++++++++++++++++ DRAG CLICKED +++++++++++++')
                                    if (mouse.modifiers & Qt.ControlModifier) {
                                        mouse.accepted = false
                                        return
                                    }
                                    if (!timeline.exists(dragProxy.draggedItem)) {
                                        endDrag()
                                        mouse.accepted = false
                                        return
                                    }
                                    dragFrame = -1
                                    timeline.activeTrack = dragProxy.sourceTrack
                                    if (mouse.modifiers & Qt.ShiftModifier) {
                                        if (timeline.selection.indexOf(dragProxy.draggedItem) == -1) {
                                            console.log('ADD SELECTION: ', dragProxy.draggedItem)
                                            controller.requestAddToSelection(dragProxy.draggedItem)
                                        } else {
                                            console.log('REMOVE SELECTION: ', dragProxy.draggedItem)
                                            controller.requestRemoveFromSelection(dragProxy.draggedItem)
                                            //endDrag()
                                            shiftClick = true
                                            return
                                        }
                                        shiftClick = true
                                    } else {
                                        if (timeline.selection.indexOf(dragProxy.draggedItem) == -1) {
                                            controller.requestAddToSelection(dragProxy.draggedItem, /*clear=*/ true)
                                        }
                                        shiftClick = false
                                    }
                                    timeline.showAsset(dragProxy.draggedItem)
                                    root.stopScrolling = true
                                    clipBeingMovedId = dragProxy.draggedItem
                                    if (dragProxy.draggedItem > -1) {
                                        var tk = controller.getItemTrackId(dragProxy.draggedItem)
                                        var x = controller.getItemPosition(dragProxy.draggedItem)
                                        var posx = Math.round((parent.x)/ root.timeScale)
                                        var clickAccepted = true
                                        if (controller.normalEdit() && (tk != Logic.getTrackIdFromPos(parent.y) || x != posx)) {
                                            console.log('INCORRECT DRAG, Trying to recover item: ', parent.y,' XPOS: ',x,'=',posx,'on track: ',tk ,'\n!!!!!!!!!!')
                                            // Try to find correct item
                                            var tentativeClip = getItemAtPos(tk, mouseX + parent.x, dragProxy.isComposition)
                                            if (tentativeClip && tentativeClip.clipId) {
                                                clickAccepted = true
                                                dragProxy.draggedItem = tentativeClip.clipId
                                                dragProxy.x = tentativeClip.x
                                                dragProxy.y = tentativeClip.y
                                                dragProxy.width = tentativeClip.width
                                                dragProxy.height = tentativeClip.height
                                                dragProxy.masterObject = tentativeClip
                                                dragProxy.sourceTrack = tk
                                                dragProxy.sourceFrame = tentativeClip.modelStart
                                                dragProxy.isComposition = tentativeClip.isComposition
                                            } else {
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
                                            dragProxy.masterObject.originalX = dragProxy.masterObject.x
                                            dragProxy.masterObject.originalTrackId = dragProxy.masterObject.trackId
                                            dragProxy.masterObject.forceActiveFocus();
                                        }
                                    } else {
                                        mouse.accepted = false
                                        parent.x = 0
                                        parent.y = 0
                                        parent.width = 0
                                        parent.height = 0
                                    }
                                }
                                onPositionChanged: {
                                    // we have to check item validity in the controller, because they could have been deleted since the beginning of the drag
                                    if (dragProxy.draggedItem > -1 && !timeline.exists(dragProxy.draggedItem)) {
                                        endDrag()
                                        return
                                    }
                                    if (!shiftClick && dragProxy.draggedItem > -1 && mouse.buttons === Qt.LeftButton &&  (controller.isClip(dragProxy.draggedItem) || controller.isComposition(dragProxy.draggedItem))) {
                                        continuousScrolling(mouse.x + parent.x)
                                        var mapped = tracksContainerArea.mapFromItem(dragProxy, mouse.x, mouse.y).x
                                        root.mousePosChanged(Math.round(mapped / timeline.scaleFactor))
                                        var posx = Math.round((parent.x)/ root.timeScale)
                                        var posy = Math.min(Math.max(0, mouse.y + parent.y - dragProxy.verticalOffset), tracksContainerArea.height)
                                        var tId = Logic.getTrackIdFromPos(posy)
                                        if (dragProxy.masterObject && tId == dragProxy.masterObject.trackId) {
                                            if (posx == dragFrame) {
                                                return
                                            }
                                        }
                                        if (dragProxy.isComposition) {
                                            dragFrame = controller.suggestCompositionMove(dragProxy.draggedItem, tId, posx, timeline.position, Math.floor(root.snapping))
                                            timeline.activeTrack = timeline.getItemMovingTrack(dragProxy.draggedItem)
                                        } else {
                                            if (!controller.normalEdit() && dragProxy.masterObject.parent != dragContainer) {
                                                var pos = dragProxy.masterObject.mapToGlobal(dragProxy.masterObject.x, dragProxy.masterObject.y);
                                                dragProxy.masterObject.parent = dragContainer
                                                pos = dragProxy.masterObject.mapFromGlobal(pos.x, pos.y)
                                                dragProxy.masterObject.x = pos.x
                                                dragProxy.masterObject.y = pos.y
                                                //console.log('bringing item to front')
                                            }
                                            dragFrame = controller.suggestClipMove(dragProxy.draggedItem, tId, posx, timeline.position, Math.floor(root.snapping))
                                            timeline.activeTrack = timeline.getItemMovingTrack(dragProxy.draggedItem)
                                        }
                                        var delta = dragFrame - dragProxy.sourceFrame
                                        if (delta != 0) {
                                            var s = timeline.timecode(Math.abs(delta))
                                            // remove leading zeroes
                                            if (s.substring(0, 3) === '00:')
                                                s = s.substring(3)
                                            s = ((delta < 0)? '-' : (delta > 0)? '+' : '') + s
                                            bubbleHelp.show(parent.x, ruler.height, s)
                                        } else bubbleHelp.hide()
                                    }
                                }
                                onReleased: {
                                    clipBeingMovedId = -1
                                    root.stopScrolling = false
                                    if (!shiftClick && dragProxy.draggedItem > -1 && dragFrame > -1 && (controller.isClip(dragProxy.draggedItem) || controller.isComposition(dragProxy.draggedItem))) {
                                        var tId = controller.getItemTrackId(dragProxy.draggedItem)
                                        if (dragProxy.isComposition) {
                                            controller.requestCompositionMove(dragProxy.draggedItem, dragProxy.sourceTrack, dragProxy.sourceFrame, true, false, false)
                                            controller.requestCompositionMove(dragProxy.draggedItem, tId, dragFrame , true, true, true)
                                        } else {
                                            if (controller.normalEdit()) {
                                                controller.requestClipMove(dragProxy.draggedItem, dragProxy.sourceTrack, dragProxy.sourceFrame, true, false, false)
                                                controller.requestClipMove(dragProxy.draggedItem, tId, dragFrame , true, true, true)
                                            } else {
                                                // Fake move, only process final move
                                                timeline.endFakeMove(dragProxy.draggedItem, dragFrame, true, true, true)
                                            }
                                        }
                                        dragProxy.x = controller.getItemPosition(dragProxy.draggedItem) * timeline.scaleFactor
                                        dragProxy.sourceFrame = dragFrame
                                        bubbleHelp.hide()
                                    }
                                }
                                onDoubleClicked: {
                                    if (dragProxy.masterObject.keyframeModel) {
                                        var newVal = (dragProxy.height - mouseY) / dragProxy.height
                                        var newPos = Math.round(mouseX / timeScale) + dragProxy.masterObject.inPoint
                                        timeline.addEffectKeyframe(dragProxy.draggedItem, newPos, newVal)
                                    }
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: zoomByWheel(wheel)
                            cursorShape: dragProxyArea.drag.active ? Qt.ClosedHandCursor : tracksArea.cursorShape
                        }
                        Column {
                            // These make the striped background for the tracks.
                            // It is important that these are not part of the track visual hierarchy;
                            // otherwise, the clips will be obscured by the Track's background.
                            Repeater {
                                model: multitrack
                                id: trackBaseRepeater
                                delegate: Rectangle {
                                    width: tracksContainerArea.width
                                    border.width: 1
                                    border.color: root.frameColor
                                    height: model.trackHeight
                                    color: tracksRepeater.itemAt(index) ? ((tracksRepeater.itemAt(index).trackInternalId === timeline.activeTrack) ? Qt.tint(getTrackColor(tracksRepeater.itemAt(index).isAudio, false), selectedTrackColor) : getTrackColor(tracksRepeater.itemAt(index).isAudio, false)) : 'red'
                                }
                            }
                        }
                        Column {
                            id: tracksContainer
                            Repeater { id: tracksRepeater; model: trackDelegateModel }
                            Item {
                                id: dragContainer
                                z: 100
                            }
                            Repeater { id: guidesRepeater; model: guidesDelegateModel }
                        }
                        Rectangle {
                            id: cursor
                            visible: timeline.position > -1
                            color: root.textColor
                            width: Math.max(1, 1 * timeline.scaleFactor)
                            opacity: (width > 2) ? 0.5 : 1
                            height: parent.height
                            x: timeline.position * timeline.scaleFactor
                        }
                    }
                }
            }
            /*CornerSelectionShadow {
                y: tracksRepeater.count ? tracksRepeater.itemAt(currentTrack).y + ruler.height - scrollView.flickableItem.contentY : 0
                clip: timeline.selection.length ?
                        tracksRepeater.itemAt(currentTrack).clipAt(timeline.selection[0]) : null
                opacity: clip && clip.x + clip.width < scrollView.flickableItem.contentX ? 1 : 0
            }

            CornerSelectionShadow {
                y: tracksRepeater.count ? tracksRepeater.itemAt(currentTrack).y + ruler.height - scrollView.flickableItem.contentY : 0
                clip: timeline.selection.length ?
                        tracksRepeater.itemAt(currentTrack).clipAt(timeline.selection[timeline.selection.length - 1]) : null
                opacity: clip && clip.x > scrollView.flickableItem.contentX + scrollView.width ? 1 : 0
                anchors.right: parent.right
                mirrorGradient: true
            }*/
            Rectangle {
                id: cutLine
                visible: root.activeTool == 1 && tracksArea.mouseY > ruler.height
                color: 'red'
                width: Math.max(1, 1 * timeline.scaleFactor)
                opacity: (width > 2) ? 0.5 : 1
                height: root.height - scrollView.__horizontalScrollBar.height - ruler.height
                x: 0
                //x: timeline.position * timeline.scaleFactor - scrollView.flickableItem.contentX
                y: ruler.height
            }
        }
    }

    Rectangle {
        id: bubbleHelp
        property alias text: bubbleHelpLabel.text
        color: root.color //application.toolTipBaseColor
        width: bubbleHelpLabel.width + 8
        height: bubbleHelpLabel.height + 8
        radius: 4
        states: [
            State { name: 'invisible'; PropertyChanges { target: bubbleHelp; opacity: 0} },
            State { name: 'visible'; PropertyChanges { target: bubbleHelp; opacity: 0.8} }
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
            font.pixelSize: root.baseUnit
        }
        function show(x, y, text) {
            bubbleHelp.x = x + tracksArea.x - scrollView.flickableItem.contentX - bubbleHelpLabel.width
            bubbleHelp.y = y + tracksArea.y - scrollView.flickableItem.contentY - bubbleHelpLabel.height
            bubbleHelp.text = text
            if (bubbleHelp.state !== 'visible')
                bubbleHelp.state = 'visible'
        }
        function hide() {
            bubbleHelp.state = 'invisible'
            bubbleHelp.opacity = 0
        }
    }

    Rectangle {
        id: rubberSelect
        property int originX
        property int originY
        y: -1
        color: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.4)
        border.color: activePalette.highlight
        border.width: 1
        visible: false
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
            timeScale: timeline.scaleFactor
            width: tracksContainerArea.width
            height: trackHeight
            isAudio: audio
            trackThumbsFormat: thumbsFormat
            isCurrentTrack: item === timeline.activeTrack
            trackInternalId: item
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
                    height: tracksContainer.height
                    x: model.frame * timeScale;
                    color: model.color
                }
                Rectangle {
                    visible: mlabel.visible
                    opacity: 0.7
                    x: guideBase.x
                    y: mlabel.y
                    radius: 2
                    width: mlabel.width + 4
                    height: mlabel.height
                    color: model.color
                    MouseArea {
                        z: 10
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        property int startX
                        drag.axis: Drag.XAxis
                        drag.target: guideRoot
                        onPressed: {
                            drag.target = guideRoot
                            startX = guideRoot.x
                        }
                        onReleased: {
                            if (startX != guideRoot.x) {
                                timeline.moveGuide(model.frame,  model.frame + guideRoot.x / timeline.scaleFactor)
                            }
                            drag.target = undefined
                        }
                        onPositionChanged: {
                            if (pressed) {
                                var frame = Math.round(model.frame + guideRoot.x / timeline.scaleFactor)
                                frame = controller.suggestSnapPoint(frame, root.snapping)
                                guideRoot.x = (frame - model.frame) * timeline.scaleFactor
                            }
                        }
                        drag.smoothed: false
                        onDoubleClicked: {
                            timeline.editGuide(model.frame)
                            drag.target = undefined
                        }
                        onClicked: timeline.position = guideBase.x / timeline.scaleFactor
                    }
                }
                Text {
                    id: mlabel
                    visible: timeline.showMarkers
                    text: model.comment
                    font.pixelSize: root.baseUnit
                    x: guideBase.x + 2
                    y: scrollView.flickableItem.contentY
                    color: 'white'
                }
            }
        }


    Connections {
        target: timeline
        onPositionChanged: if (!stopScrolling) Logic.scrollIfNeeded()
        onFrameFormatChanged: ruler.adjustFormat()
        onSelectionChanged: {
            if (dragProxy.draggedItem > -1 && !timeline.exists(dragProxy.draggedItem)) {
                endDrag()
            }
        }
    }

    // This provides continuous scrolling at the left/right edges.
    Timer {
        id: scrollTimer
        interval: 25
        repeat: true
        triggeredOnStart: true
        property var item
        property bool backwards
        onTriggered: {
            var delta = backwards? -10 : 10
            if (item) item.x += delta
            scrollView.flickableItem.contentX += delta
            if (scrollView.flickableItem.contentX <= 0 || clipBeingMovedId == -1)
                stop()
        }
    }
}

