import QtQuick 2.6
import QtQml.Models 2.2
import QtQuick.Controls 1.4 as OLD
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import Kdenlive.Controls 1.0
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import 'Timeline.js' as Logic

Rectangle {
    id: root
    objectName: "timelineview"

    SystemPalette { id: activePalette }
    color: activePalette.window

    signal clipClicked()
    signal mousePosChanged(int position)

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
        timeline.activeTrack = tracksRepeater.itemAt(newTrack).trackId
    }

    function zoomByWheel(wheel) {
        if (wheel.modifiers & Qt.ControlModifier) {
            timeline.setScaleFactor(timeline.scaleFactor + 0.2 * wheel.angleDelta.y / 120);
        } else {
            var newScroll = Math.min(scrollView.flickableItem.contentX + wheel.angleDelta.y, timeline.duration * root.timeScale - (scrollView.width - scrollView.__verticalScrollBar.width) + root.projectMargin)
            scrollView.flickableItem.contentX = Math.max(newScroll, 0)
        }
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

    function getMouseTrack() {
        return Logic.getTrackIdFromPos(tracksArea.mouseY - ruler.height)
    }

    function getTrackColor(audio, header) {
        var col = activePalette.alternateBase
        if (audio) {
            col = Qt.tint(col, "#1000cc00")
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
    }

    property int headerWidth: timeline.headerWidth()
    property int activeTool: 0
    property int projectMargin: 200
    property real baseUnit: fontMetrics.font.pointSize
    property color selectedTrackColor: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.4)
    property bool stopScrolling: false
    property int duration: timeline.duration
    property color shotcutBlue: Qt.rgba(23/255, 92/255, 118/255, 1.0)
    property int clipBeingDroppedId: -1
    property string clipBeingDroppedData
    property int droppedPosition: -1
    property int droppedTrack: -1
    property int clipBeingMovedId: -1
    property int spacerGroup: -1
    property int spacerFrame: -1
    property int spacerClickFrame: -1
    property real timeScale: timeline.scaleFactor
    property real snapping: timeline.snap ? 10 / Math.sqrt(timeScale) : -1
    property var timelineSelection: timeline.selection
    property int trackHeight
    property int copiedClip: -1
    property var dragList: []
    property int zoomOnMouse: -1

    //onCurrentTrackChanged: timeline.selection = []
    onTimeScaleChanged: {
        if (root.zoomOnMouse >= 0) {
            var mouseFraction = 
            scrollView.flickableItem.contentX = Math.max(0, root.zoomOnMouse * timeline.scaleFactor - tracksArea.mouseX)
            root.zoomOnMouse = -1
        } else {
            scrollView.flickableItem.contentX = Math.max(0, (timeline.seekPosition > -1 ? timeline.seekPosition : timeline.position) * timeline.scaleFactor - (scrollView.width / 2))
        }
        root.snapping = timeline.snap ? 10 / Math.sqrt(root.timeScale) : -1
        ruler.adjustStepSize()
    }
    
    onTimelineSelectionChanged: {
        if (root.timelineSelection.length == 0) {
            root.dragList = []
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
                var track = Logic.getTrackIdFromPos(drag.y)
                var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                droppedPosition = frame
                if (track >= 0) {
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
                var track = Logic.getTrackIdFromPos(drag.y)
                if (track !=-1) {
                    var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                    frame = controller.suggestSnapPoint(frame, root.snapping)
                    if (clipBeingDroppedId >= 0){
                        controller.requestCompositionMove(clipBeingDroppedId, track, frame, true, false)
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    } else {
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
                timeline.insertComposition(track, frame, clipBeingDroppedData, true)
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
                id = timeline.insertClip(timeline.activeTrack, frame, clipBeingDroppedData, false, true)
            } else {
                var ids = timeline.insertClips(timeline.activeTrack, frame, binIds, false, true)

                // if the clip insertion succeeded, request the clips to be grouped
                if (ids.length > 0) {
                    timeline.groupClips(ids)
                    id = ids[0]
                }
            }
            return id
        }

        width: root.width - headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: headerWidth
        keys: 'kdenlive/producerslist'
        onEntered: {
            if (clipBeingMovedId == -1) {
                //var track = Logic.getTrackIdFromPos(drag.y)
                var track = Logic.getTrackIndexFromPos(drag.y)
                if (track >= 0  && track < tracksRepeater.count) {
                    var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                    droppedPosition = frame
                    timeline.activeTrack = tracksRepeater.itemAt(track).trackId
                    //drag.acceptProposedAction()
                    clipBeingDroppedData = drag.getDataAsString('kdenlive/producerslist')
                    console.log('dropped data: ', clipBeingDroppedData)
                    clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, frame, clipBeingDroppedData)
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
                var track = Logic.getTrackIndexFromPos(drag.y)
                if (track >= 0  && track < tracksRepeater.count) {
                    timeline.activeTrack = tracksRepeater.itemAt(track).trackId
                    var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                    frame = controller.suggestSnapPoint(frame, root.snapping)
                    if (clipBeingDroppedId >= 0){
                        controller.requestClipMove(clipBeingDroppedId, timeline.activeTrack, frame, true, false, false)
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    } else {
                        clipBeingDroppedId = insertAndMaybeGroup(timeline.activeTrack, frame, drag.getDataAsString('kdenlive/producerslist'), false, true)
                        continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                    }
                }
            }
        }
        onDropped: {
            if (clipBeingDroppedId != -1) {
                var frame = controller.getClipPosition(clipBeingDroppedId)
                var track = controller.getClipTrackId(clipBeingDroppedId)
                /* We simulate insertion at the final position so that stored undo has correct value
                 * NOTE: even if dropping multiple clips, requesting the deletion of the first one is
                 * enough as internally it will request the group deletion
                 */
                controller.requestItemDeletion(clipBeingDroppedId, false)

                var binIds = clipBeingDroppedData.split(";")
                if (binIds.length == 1) {
                    timeline.insertClip(track, frame, clipBeingDroppedData, true, true)
                } else {
                    timeline.insertClips(track, frame, binIds, true, true)
                }
            }
            clearDropData()
        }
    }
    OLD.Menu {
        id: menu
        property int clickedX
        property int clickedY
        OLD.MenuItem {
            text: i18n('Paste')
            visible: copiedClip != -1
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height)
                var frame = Math.round((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.pasteItem(copiedClip, track, frame)
            }
        }
        OLD.MenuItem {
            text: i18n('Insert Space')
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height)
                var frame = Math.round((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.insertSpace(track, frame);
            }
        }
        OLD.MenuItem {
            text: i18n('Remove Space On Active Track')
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height)
                var frame = Math.round((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.removeSpace(track, frame);
            }
        }
        OLD.MenuItem {
            text: i18n('Remove Space')
            onTriggered: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height)
                var frame = Math.round((menu.clickedX + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                timeline.removeSpace(track, frame, true);
            }
        }
        OLD.MenuItem {
            id: addGuideMenu
            text: i18n('Add Guide')
            onTriggered: {
                timeline.switchGuide(timeline.position);
            }
        }
        OLD.MenuItem {
            id: editGuideMenu
            text: i18n('Edit Guide')
            visible: false
            onTriggered: {
                timeline.editGuide(timeline.position);
            }
        }
        AssetMenu {
            title: i18n('Insert a composition...')
            menuModel: transitionModel
            onAssetSelected: {
                var track = Logic.getTrackIdFromPos(menu.clickedY - ruler.height)
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
                addGuideMenu.text = i18n('Remove Guide')
                editGuideMenu.visible = true
            } else {
                addGuideMenu.text = i18n('Add Guide')
                editGuideMenu.visible = false
            }
            console.log("pop menu")
        }
    }
    OLD.Menu {
        id: rulermenu
        property int clickedX
        property int clickedY
        OLD.MenuItem {
            id: addGuideMenu2
            text: i18n('Add Guide')
            onTriggered: {
                timeline.switchGuide(timeline.position);
            }
        }
        OLD.MenuItem {
            id: editGuideMenu2
            text: i18n('Edit Guide')
            visible: false
            onTriggered: {
                timeline.editGuide(timeline.position);
            }
        }
        onAboutToShow: {
            if (guidesModel.hasMarker(timeline.position)) {
                // marker at timeline position
                addGuideMenu2.text = i18n('Remove Guide')
                editGuideMenu2.visible = true
            } else {
                addGuideMenu2.text = i18n('Add Guide')
                editGuideMenu2.visible = false
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
        OLD.MenuItem {
            text: i18n('Add Track')
            onTriggered: timeline.addTrack(timeline.activeTrack);
        }
        OLD.MenuItem {
            text: i18n('Delete Track')
            onTriggered: timeline.deleteTrack(timeline.activeTrack);
        }
    }

    Row {
        Column {
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
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        //timeline.selectMultitrack()
                    }
                }
            }
            Flickable {
                // Non-slider scroll area for the track headers.
                contentY: scrollView.flickableItem.contentY
                width: headerWidth
                height: 100
                interactive: false

                Column {
                    id: trackHeaders
                    spacing: 0
                    Repeater {
                        id: trackHeaderRepeater
                        model: multitrack
                        TrackHead {
                            trackName: model.name
                            isMute: model.mute
                            isHidden: model.hidden
                            isComposite: model.composite
                            isLocked: model.locked
                            isAudio: model.audio
                            width: headerWidth
                            height: model.trackHeight
                            selected: false
                            current: item === timeline.activeTrack
                            trackId: item
                            onIsLockedChanged: tracksRepeater.itemAt(index).isLocked = isLocked
                            onMyTrackHeightChanged: {
                                trackBaseRepeater.itemAt(index).height = myTrackHeight
                                tracksRepeater.itemAt(index).height = myTrackHeight
                                height = myTrackHeight
                                collapsed = height <= collapsedHeight
                                if (!collapsed) {
                                    controller.setTrackProperty(trackId, "kdenlive:trackheight", myTrackHeight)
                                }
                                // hack: change property to trigger transition adjustment
                                root.trackHeight = root.trackHeight === 1 ? 0 : 1
                            }
                            onClicked: {
                                timeline.activeTrack = tracksRepeater.itemAt(index).trackId
                                console.log('track name: ',index, ' = ', model.name,'/',tracksRepeater.itemAt(index).trackId)
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
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            cursorShape: tracksArea.mouseY < ruler.height || root.activeTool === 0 ? Qt.ArrowCursor : root.activeTool === 1 ? Qt.IBeamCursor : Qt.SplitHCursor
            onWheel: {
                timeline.seekPosition = Math.min(timeline.position + (wheel.angleDelta.y > 0 ? 1 : -1), timeline.duration - 1)
                timeline.position = timeline.seekPosition
            }
            onPressed: {
                focus = true
                if (mouse.modifiers & Qt.ShiftModifier) {
                        // rubber selection
                        rubberSelect.x = mouse.x + tracksArea.x
                        rubberSelect.y = mouse.y
                        rubberSelect.originX = mouse.x
                        rubberSelect.originY = rubberSelect.y
                        rubberSelect.width = 0
                        rubberSelect.height = 0
                } else if (mouse.button & Qt.LeftButton) {
                    if (root.activeTool === 2 && mouse.y > ruler.height) {
                        // spacer tool
                        var y = mouse.y - ruler.height
                        var frame = (scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor
                        var track = (mouse.modifiers & Qt.ControlModifier) ? tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y)).trackId : -1
                        spacerGroup = timeline.requestSpacerStartOperation(track, frame)
                        if (spacerGroup > -1) {
                            drag.axis = Drag.XAxis
                            Drag.active = true
                            Drag.proposedAction = Qt.MoveAction
                            spacerClickFrame = frame
                            spacerFrame = controller.getClipPosition(spacerGroup)
                        }
                    } else if (root.activeTool === 0 || mouse.y <= ruler.height) {
                        if (mouse.y > ruler.height) {
                            timeline.selection = []
                        }
                        timeline.seekPosition = Math.min((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, timeline.duration - 1)
                        timeline.position = timeline.seekPosition
                    } else if (root.activeTool === 1) {
                        // razor tool
                        var y = mouse.y - ruler.height
                        timeline.cutClipUnderCursor((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, tracksRepeater.itemAt(Logic.getTrackIndexFromPos(y)).trackId)
                    }
                }
            }
            onClicked: {
                if (mouse.button & Qt.RightButton) {
                    menu.clickedX = mouse.x
                    menu.clickedY = mouse.y
                    if (mouse.y > ruler.height) {
                        timeline.activeTrack = tracksRepeater.itemAt(Logic.getTrackIndexFromPos(mouse.y - ruler.height)).trackId
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
                root.mousePosChanged(Math.round((mouse.x + scrollView.flickableItem.contentX) / timeline.scaleFactor))
                if ((mouse.modifiers & Qt.ShiftModifier) && mouse.buttons === Qt.LeftButton && !rubberSelect.visible) {
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
                        timeline.seekPosition = Math.min((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, timeline.duration - 1)
                        timeline.position = timeline.seekPosition
                    } else if (root.activeTool === 2 && spacerGroup > -1) {
                        // Move group
                        var track = controller.getClipTrackId(spacerGroup)
                        var frame = Math.round((mouse.x + scrollView.flickableItem.contentX) / timeline.scaleFactor) + spacerFrame - spacerClickFrame
                        frame = controller.suggestClipMove(spacerGroup, track, frame, root.snapping);
                        controller.requestClipMove(spacerGroup, track, frame, true, false, false)
                        continuousScrolling(mouse.x + scrollView.flickableItem.contentX)
                    }
                    scim = true
                } else {
                    scim = false
                    if (root.activeTool === 1) {
                        cutLine.x = Math.floor((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor) * timeline.scaleFactor - scrollView.flickableItem.contentX
                    }
                }
            }
            onReleased: {
                if (rubberSelect.visible) {
                    rubberSelect.visible = false
                    var topTrack = Logic.getTrackIndexFromPos(Math.max(0, rubberSelect.y - ruler.height))
                    var bottomTrack = Logic.getTrackIndexFromPos(rubberSelect.y - ruler.height + rubberSelect.height)
                    if (bottomTrack >= topTrack) {
                        var t = []
                        for (var i = topTrack; i <= bottomTrack; i++) {
                            t.push(tracksRepeater.itemAt(i).trackId)
                        }
                        var startFrame = (scrollView.flickableItem.contentX - tracksArea.x + rubberSelect.x) / timeline.scaleFactor
                        var endFrame = (scrollView.flickableItem.contentX - tracksArea.x + rubberSelect.x + rubberSelect.width) / timeline.scaleFactor
                        timeline.selectItems(t, startFrame, endFrame, mouse.modifiers & Qt.ControlModifier);
                    }
                } else if (mouse.modifiers & Qt.ShiftModifier) {
                    // Shift click, process seek
                    timeline.seekPosition = Math.min((scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor, timeline.duration - 1)
                    timeline.position = timeline.seekPosition
                }
                if (spacerGroup > -1) {
                    var frame = controller.getClipPosition(spacerGroup)
                    timeline.requestSpacerEndOperation(spacerGroup, spacerFrame, frame);
                    spacerClickFrame = -1
                    spacerFrame = -1
                    spacerGroup = -1
                }
                scim = false
            }
            Timer {
                id: scrubTimer
                interval: 25
                repeat: true
                running: parent.scim && parent.containsMouse
                         && (parent.mouseX < 50 || parent.mouseX > parent.width - 50)
                         && (timeline.position * timeline.scaleFactor >= 50)
                onTriggered: {
                    if (parent.mouseX < 50)
                        timeline.seekPosition = Math.max(0, timeline.position - 10)
                    else
                        timeline.seekPosition = Math.min(timeline.position + 10, timeline.duration - 1)
                }
            }

            Column {
                Flickable {
                    // Non-slider scroll area for the Ruler.
                    contentX: scrollView.flickableItem.contentX
                    width: root.width - headerWidth
                    height: ruler.height
                    interactive: false
                    clip: true
                    Ruler {
                        id: ruler
                        width: Math.max(root.width - headerWidth, timeline.duration * timeScale + root.projectMargin)
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
                            anchors.bottom: parent.bottom
                            x: timeline.position * timeline.scaleFactor - (width / 2)
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
                        width: Math.max(root.width - headerWidth - scrollView.__verticalScrollBar.width, timeline.duration * timeScale + root.projectMargin)
                        height: trackHeaders.height
                        color: activePalette.window
                        id: tracksContainerArea
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: zoomByWheel(wheel)
                            cursorShape: tracksArea.cursorShape
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
                                    border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.1)
                                    height: model.trackHeight
                                    color: tracksRepeater.itemAt(index) ? ((tracksRepeater.itemAt(index).trackId === timeline.activeTrack) ? Qt.tint(getTrackColor(tracksRepeater.itemAt(index).isAudio, false), selectedTrackColor) : getTrackColor(tracksRepeater.itemAt(index).isAudio, false)) : 'red'
                                }
                            }
                        }
                        Column {
                            id: tracksContainer
                            Repeater { id: tracksRepeater; model: trackDelegateModel }
                            Repeater { id: guidesRepeater; model: guidesDelegateModel }
                        }
                        Rectangle {
                            id: cursor
                            visible: timeline.position > -1
                            color: activePalette.text
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
        color: activePalette.window //application.toolTipBaseColor
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
        Track {
            model: multitrack
            rootIndex: trackDelegateModel.modelIndex(index)
            height: trackHeight
            timeScale: timeline.scaleFactor
            width: tracksContainerArea.width
            isAudio: audio
            isMute: mute
            isHidden: hidden
            isCurrentTrack: item === timeline.activeTrack
            trackId: item
            onClipClicked: {
                timeline.activeTrack = track.trackId
                if (shiftClick === 1) {
                    timeline.addSelection(clip.clipId)
                } else {
                    timeline.selection = [ clip.clipId ]
                }
                timeline.showAsset(clip.clipId)
            }
            onClipDragged: {
                continuousScrolling(x)
                // Show distance moved as time in a "bubble" help.
                var delta = Math.round((clip.x / timeline.scaleFactor) - Math.round(clip.originalX) / timeline.scaleFactor)
                if (delta != 0) {
                    var track = tracksRepeater.itemAt(clip.trackIndex)
                    var s = timeline.timecode(Math.abs(delta))
                    // remove leading zeroes
                    if (s.substring(0, 3) === '00:')
                        s = s.substring(3)
                    s = ((delta < 0)? '-' : (delta > 0)? '+' : '') + s
                    bubbleHelp.show(x, track.y + height/2, s)
                    clipBeingMovedId = clip.clipId
                } else bubbleHelp.hide()
            }
            onClipDropped: {
                console.log(" + + + ++ + DROPPED  + + + + + + +");
                scrollTimer.running = false
                bubbleHelp.hide()
                clipBeingMovedId = -1
            }
            onCompositionDropped: {
                console.log(" + + + ++ + COMPOSITION DROPPED  + + + + + + +");
                scrollTimer.running = false
                bubbleHelp.hide()
                clipBeingMovedId = -1
            }
            onClipDraggedToTrack: {
                var y = Math.max(0, pos - ruler.height)
                var activeTrack = Logic.getTrackIndexFromPos(y)
                var frame = Math.max(0, Math.round(clip.x / timeScale))
                if (activeTrack >= 0  && activeTrack < tracksRepeater.count) {
                    var track = tracksRepeater.itemAt(activeTrack)
                    //console.log('Dragging clip ',clip.clipId,' to track: ', activeTrack, ' - ', y)
                    if (controller.requestClipMove(clip.clipId, track.trackId, frame, false, false, false)) {
                        timeline.activeTrack = track.trackId
                        //clip.reparent(track)
                        clip.height = track.height
                        clip.y = track.y - Logic.getTrackById(clip.originalTrackId).y
                        clip.trackId = track.trackId
                        clip.currentFrame = frame
                        if (root.dragList.length > 1) {
                            var currentMasterY = track.y
                            var sourceTrack = Logic.getTrackIndexFromId(clip.originalTrackId)
                            var destTrack = Logic.getTrackIndexFromId(clip.trackId)
                            var trackOffset = destTrack - sourceTrack
                            for (var i = 0; i < root.dragList.length; i++) {
                                if (root.dragList[i] != clip) {
                                    if (root.dragList[i].originalTrackId == clip.originalTrackId) {
                                        if (root.dragList[i].isComposition == false) {
                                            root.dragList[i].height = clip.height
                                        } else {
                                            root.dragList[i].displayHeight = clip.height / 2
                                        }
                                    } else {
                                        var newTrackIndex = Logic.getTrackIndexFromId(root.dragList[i].originalTrackId) + trackOffset
                                        var newTrack = tracksRepeater.itemAt(newTrackIndex)
                                        if (root.dragList[i].isComposition == false) {
                                            root.dragList[i].height = newTrack.height
                                            root.dragList[i].y = newTrack.y - currentMasterY
                                        } else {
                                            root.dragList[i].displayHeight = newTrack.height / 2
                                            root.dragList[i].y = newTrack.y - currentMasterY
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (track.trackId != clip.trackId) {
                            // check if we can move on existing track
                            if (!controller.requestClipMove(clip.clipId, clip.trackId, frame, false, false, false)) {
                                // Abort move
                                clip.x = clip.currentFrame * timeScale
                            } else {
                                clip.x = frame * timeScale
                                clip.currentFrame = frame
                                var delta = Math.round((clip.x - clip.originalX) / timeline.scaleFactor)
                                var s = timeline.timecode(Math.abs(delta))
                                // remove leading zeroes
                                if (s.substring(0, 3) === '00:')
                                s = s.substring(3)
                                s = ((delta < 0)? '-' : (delta > 0)? '+' : '') + s
                                bubbleHelp.show(xpos, activeTrack.y + height/2, s)
                            }
                        } else {
                            // Abort move
                            clip.x = clip.draggedX
                        }
                    }
                } else {
                    console.log('+ + + + +\nWARNING CLIP DRAGGED TO INVALID TRACK: ',activeTrack,'\n+ + + +');
                }
            }
            onCompositionDraggedToTrack: {
                var y = pos - ruler.height
                var tk = Logic.getTrackIndexFromPos(y)
                var frame = Math.round(composition.x / timeScale)
                if (tk >= 0  && tk < tracksRepeater.count) {
                    var track = tracksRepeater.itemAt(tk)
                    timeline.activeTrack = track.trackId
                    if (controller.requestCompositionMove(composition.clipId, track.trackId, frame, false, false)) {
                        composition.reparent(track)
                        composition.trackIndex = track.DelegateModel.itemsIndex
                        composition.trackId = track.trackId
                        if (root.dragList.length > 1) {
                            var currentMasterY = track.y
                            var sourceTrack = Logic.getTrackIndexFromId(composition.originalTrackId)
                            var destTrack = Logic.getTrackIndexFromId(composition.trackId)
                            var trackOffset = destTrack - sourceTrack
                            for (var i = 0; i < root.dragList.length; i++) {
                                if (root.dragList[i] != composition) {
                                    if (root.dragList[i].originalTrackId == composition.originalTrackId) {
                                        if (root.dragList[i].isComposition == false) {
                                            root.dragList[i].height = track.height
                                        } else {
                                            root.dragList[i].displayHeight = composition.displayHeight
                                        }
                                    } else {
                                        var newTrackIndex = Logic.getTrackIndexFromId(root.dragList[i].originalTrackId) + trackOffset
                                        var newTrack = tracksRepeater.itemAt(newTrackIndex)
                                        if (root.dragList[i].isComposition == false) {
                                            root.dragList[i].height = newTrack.height
                                            root.dragList[i].y = newTrack.y - currentMasterY
                                        } else {
                                            root.dragList[i].displayHeight = newTrack.height / 2
                                            root.dragList[i].y = newTrack.y - currentMasterY
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.left: parent.left
                height: parent.height
                //source: "qrc:///pics/kdenlive-lock.svgz"
                //fillMode: Image.Tile
                opacity: parent.isLocked
                color: '#33ff3300'
                visible: opacity
                Behavior on opacity { NumberAnimation {} }
                MouseArea {
                    anchors.fill: parent
                    onPressed: {
                        mouse.accepted = true;
                        trackHeaderRepeater.itemAt(index).pulseLockButton()
                    }
                }
            }
        }
    }


    DelegateModel {
        id: guidesDelegateModel
        model: guidesModel
            Item {
                id: guideRoot
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
                    y: scrollView.flickableItem.contentY + 20
                    color: 'white'
                }
            }
        }


    Connections {
        target: timeline
        onPositionChanged: if (!stopScrolling) Logic.scrollIfNeeded()
        onFrameFormatChanged: ruler.adjustFormat()
        /*onDragging: Logic.dragging(pos, duration)
        onDropped: Logic.dropped()
        onDropAccepted: Logic.acceptDrop(xml)*/
        onSelectionChanged: {
            //cornerstone.selected = timeline.isMultitrackSelected()
            var selectedTrack = timeline.selectedTrack()
            for (var i = 0; i < trackHeaderRepeater.count; i++) {
                trackHeaderRepeater.itemAt(i).selected = (i === selectedTrack)
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
            if (scrollView.flickableItem.contentX <= 0)
                stop()
        }
    }
}

