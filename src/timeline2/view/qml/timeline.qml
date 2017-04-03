import QtQuick 2.4
import QtQml.Models 2.1
import QtQuick.Controls 1.4
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

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }

    function zoomByWheel(wheel) {
        if (wheel.modifiers & Qt.ControlModifier) {
            //TODO
            timeline.setScaleFactor(timeline.scaleFactor + 0.2 * wheel.angleDelta.y / 120);
        } else {
            scrollView.flickableItem.contentX = Math.max(0, scrollView.flickableItem.contentX + wheel.angleDelta.y)
        }
        //Logic.scrollIfNeeded()
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

    property int headerWidth: 140
    property real baseUnit: fontMetrics.font.pointSize
    property int currentTrack: 0
    property color selectedTrackColor: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.4)
    property alias trackCount: tracksRepeater.count
    property bool stopScrolling: false
    property int seekPos: 0
    property int duration: timeline.duration
    property color shotcutBlue: Qt.rgba(23/255, 92/255, 118/255, 1.0)
    property int clipBeingDroppedId: -1
    property string clipBeingDroppedData
    property int droppedPosition: -1
    property int droppedTrack: -1
    property int clipBeingMovedId: -1
    property real timeScale: timeline.scaleFactor
    property int trackHeight
    //property alias ripple: toolbar.ripple

    //onCurrentTrackChanged: timeline.selection = []
    onTimeScaleChanged: {
        scrollView.flickableItem.contentX = Math.max(0, root.seekPos * timeline.scaleFactor - (scrollView.width / 2))
        ruler.adjustStepSize()
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
            console.log('======================== ON POS CHANGED ========================================')
            if (clipBeingMovedId == -1) {
                var track = Logic.getTrackIdFromPos(drag.y)
                var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
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
        onExited:{
            if (clipBeingDroppedId != -1) {
                controller.requestCompositionDeletion(clipBeingDroppedId, false)
            }
            clipBeingDroppedId = -1
            droppedPosition = -1
            droppedTrack = -1
            scrollTimer.running = false
        }
        onDropped: {
            if (clipBeingDroppedId != -1) {
                var frame = controller.getCompositionPosition(clipBeingDroppedId)
                var track = controller.getCompositionTrackId(clipBeingDroppedId)
                // we simulate insertion at the final position so that stored undo has correct value
                controller.requestItemDeletion(clipBeingDroppedId, false)
                timeline.insertComposition(track, frame, clipBeingDroppedData, true)
            }
            clipBeingDroppedId = -1
            droppedPosition = -1
            droppedTrack = -1
            scrollTimer.running = false
        }
    }
    DropArea { //Drop area for bin/clips
        width: root.width - headerWidth
        height: root.height - ruler.height
        y: ruler.height
        x: headerWidth
        keys: 'kdenlive/producerslist'
        onEntered: {
            if (clipBeingMovedId == -1) {
                var track = Logic.getTrackIdFromPos(drag.y)
                var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                droppedPosition = frame
                if (track >= 0) {
                    //drag.acceptProposedAction()
                    clipBeingDroppedData = drag.getDataAsString('kdenlive/producerslist')
                    clipBeingDroppedId = timeline.insertClip(track, frame, clipBeingDroppedData, false)
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
            clipBeingDroppedId = -1
            droppedPosition = -1
            droppedTrack = -1
            scrollTimer.running = false
        }
        onPositionChanged: {
            console.log('======================== ON POS CHANGED ========================================')
            if (clipBeingMovedId == -1) {
                var track = Logic.getTrackIdFromPos(drag.y)
                var frame = Math.round((drag.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
                if (clipBeingDroppedId >= 0){
                    controller.requestClipMove(clipBeingDroppedId, track, frame, true, false)
                    continuousScrolling(drag.x + scrollView.flickableItem.contentX)

                } else {
                    clipBeingDroppedId = timeline.insertClip(track, frame, drag.getDataAsString('kdenlive/producerslist'), false)
                    continuousScrolling(drag.x + scrollView.flickableItem.contentX)
                }
            }
        }
        onDropped: {
            if (clipBeingDroppedId != -1) {
                var frame = controller.getClipPosition(clipBeingDroppedId)
                var track = controller.getClipTrackId(clipBeingDroppedId)
                // we simulate insertion at the final position so that stored undo has correct value
                controller.requestItemDeletion(clipBeingDroppedId, false)
                timeline.insertClip(track, frame, clipBeingDroppedData, true)
            }
            clipBeingDroppedId = -1
            droppedPosition = -1
            droppedTrack = -1
            scrollTimer.running = false
        }
    }
    Menu {
        id: menu
        property int clickedX
        property int clickedY
        MenuItem {
            text: i18n('Add Audio Track')
            shortcut: 'Ctrl+U'
            onTriggered: timeline.addAudioTrack();
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
    }
    MessageDialog {
        id: compositionFail
        title: i18n("Timeline error")
        icon: StandardIcon.Warning
        text: i18n("Impossible to add a composition at that position. There might not be enough space")
        standardButtons: StandardButton.Ok
    }
    Menu {
        id: headerMenu
        MenuItem {
            text: i18n('Add Track')
            shortcut: 'Ctrl+U'
            onTriggered: timeline.addTrack(currentTrack);
        }
        MenuItem {
            text: i18n('Delete Track')
            //shortcut: 'Ctrl+U'
            onTriggered: timeline.deleteTrack(currentTrack);
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
                color: selected? shotcutBlue : activePalette.window
                border.color: selected? 'red' : 'transparent'
                border.width: selected? 1 : 0
                z: 1
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        timeline.selectMultitrack()
                    }
                }
            }
            Flickable {
                // Non-slider scroll area for the track headers.
                contentY: scrollView.flickableItem.contentY
                width: headerWidth
                height: trackHeaders.height
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
                            current: index === currentTrack
                            trackId: item
                            onIsLockedChanged: tracksRepeater.itemAt(index).isLocked = isLocked
                            onMyTrackHeightChanged: {
                                model.trackHeight = myTrackHeight
                                trackBaseRepeater.itemAt(index).height = myTrackHeight
                                tracksRepeater.itemAt(index).height = myTrackHeight
                                height = myTrackHeight
                                // hack: change property to trigger transition adjustment
                                root.trackHeight = root.trackHeight === 1 ? 0 : 1
                            }
                            onClicked: {
                                currentTrack = index
                                console.log('track name: ',index, ' = ', model.name)
                                //timeline.selectTrackHead(currentTrack)
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

            // This provides continuous scrubbing and scimming at the left/right edges.
            hoverEnabled: true
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            onWheel: {
                root.seekPos += (wheel.angleDelta.y > 0 ? 1 : -1)
                timeline.position = root.seekPos
            }
            onClicked: {
                if (mouse.button & Qt.RightButton) {
                    menu.clickedX = mouse.x
                    menu.clickedY = mouse.y
                    menu.popup()
                } else {
                    console.log("Position changed: ",timeline.position)
                    root.seekPos = (scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor
                    timeline.position = root.seekPos
                }
            }
            property bool scim: false
            onReleased: scim = false
            onExited: scim = false
            onPositionChanged: {
                if (/*mouse.modifiers === Qt.ShiftModifier ||*/ mouse.buttons === Qt.LeftButton) {
                    root.seekPos = (scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor
                    timeline.position = root.seekPos
                    scim = true
                }
                else
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
                        root.seekPos = timeline.position - 10
                    else
                        root.seekPos = timeline.position + 10
                    timeline.position = root.seekPos
                }
            }

            Column {
                Flickable {
                    // Non-slider scroll area for the Ruler.
                    contentX: scrollView.flickableItem.contentX
                    width: root.width - headerWidth
                    height: ruler.height
                    interactive: false

                    Ruler {
                        id: ruler
                        width: root.duration * timeScale
                        index: index
                    }
                }
                ScrollView {
                    id: scrollView
                    width: root.width - headerWidth
                    height: root.height - ruler.height
                    // Click and drag should seek, not scroll the timeline view
                    flickableItem.interactive: false
                    Rectangle {
                        width: ruler.width
                        height: trackHeaders.height
                        color: activePalette.window
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: zoomByWheel(wheel)
                        }
                        Column {
                            // These make the striped background for the tracks.
                            // It is important that these are not part of the track visual hierarchy;
                            // otherwise, the clips will be obscured by the Track's background.
                            Repeater {
                                model: multitrack
                                id: trackBaseRepeater
                                delegate: Rectangle {
                                    width: ruler.width
                                    border.width: 1
                                    border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.1)
                                    //Layout.fillWidth: true
                                    height: model.trackHeight
                                    color: tracksRepeater.itemAt(index) ? ((index === currentTrack) ? Qt.tint(getTrackColor(tracksRepeater.itemAt(index).isAudio, false), selectedTrackColor) : getTrackColor(tracksRepeater.itemAt(index).isAudio, false)) : 'red'
                                }
                            }
                        }
                        Column {
                            id: tracksContainer
                            Repeater { id: tracksRepeater; model: trackDelegateModel }
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
                id: cursor
                visible: timeline.position > -1
                color: activePalette.text
                width: Math.max(1, 1 * timeline.scaleFactor)
                opacity: (width > 2) ? 0.5 : 1
                height: root.height - scrollView.__horizontalScrollBar.height - ruler.height
                x: timeline.position * timeline.scaleFactor - scrollView.flickableItem.contentX
                y: ruler.height
            }
            Rectangle {
                id: seekCursor
                visible: timeline.position != root.seekPos
                color: activePalette.highlight
                width: 4
                height: ruler.height
                opacity: 0.5
                x: root.seekPos * timeline.scaleFactor - scrollView.flickableItem.contentX
                y: 0
            }
            TimelinePlayhead {
                id: playhead
                visible: timeline.position > -1
                height: baseUnit
                width: baseUnit * 1.5
                y: ruler.height - height
                x: timeline.position * timeline.scaleFactor - scrollView.flickableItem.contentX - (width / 2)
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
            width: root.duration * timeScale
            isAudio: audio
            isCurrentTrack: currentTrack === index
            trackId: item
            selection: timeline.selection
            onClipClicked: {
                currentTrack = track.DelegateModel.itemsIndex
                if (shiftClick === 1) {
                    timeline.addSelection(clip.clipId)
                } else {
                    timeline.selection = [ clip.clipId ]
                }
                //root.clipClicked()
            }
            onClipDragged: {
                continuousScrolling(x)
                // Show distance moved as time in a "bubble" help.
                var track = tracksRepeater.itemAt(clip.trackIndex)
                var delta = Math.round((clip.x - clip.originalX) / timeline.scaleFactor)
                var s = timeline.timecode(Math.abs(delta))
                // remove leading zeroes
                if (s.substring(0, 3) === '00:')
                    s = s.substring(3)
                s = ((delta < 0)? '-' : (delta > 0)? '+' : '') + s
                bubbleHelp.show(x, track.y + height/2, s)
                clipBeingMovedId = clip.clipId
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
                var track = tracksRepeater.itemAt(clip.trackIndex)
                clip.y = track.height / 2
            }
            onClipDraggedToTrack: {
                var y = pos - ruler.height
                currentTrack = Logic.getTrackIndexFromPos(y)
                var frame = Math.round(clip.x / timeScale)
                if (currentTrack >= 0  && currentTrack < tracksRepeater.count) {
                    var track = tracksRepeater.itemAt(currentTrack)
                    if (controller.requestClipMove(clip.clipId, track.trackId, frame, false, false)) {
                        clip.reparent(track)
                        clip.trackIndex = track.DelegateModel.itemsIndex
                        clip.trackId = track.trackId
                    }
                }
            }
            onCompositionDraggedToTrack: {
                var y = pos - ruler.height
                currentTrack = Logic.getTrackIndexFromPos(y)
                var frame = Math.round(composition.x / timeScale)
                if (currentTrack >= 0  && currentTrack < tracksRepeater.count) {
                    var track = tracksRepeater.itemAt(currentTrack)
                    if (controller.requestCompositionMove(composition.clipId, track.trackId, frame, false, false)) {
                        composition.reparent(track)
                        composition.trackIndex = track.DelegateModel.itemsIndex
                        composition.trackId = track.trackId
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


    Connections {
        target: timeline
        onPositionChanged: if (!stopScrolling) Logic.scrollIfNeeded()
        /*onDragging: Logic.dragging(pos, duration)
        onDropped: Logic.dropped()
        onDropAccepted: Logic.acceptDrop(xml)*/
        onSelectionChanged: {
            cornerstone.selected = timeline.isMultitrackSelected()
            var selectedTrack = timeline.selectedTrack()
            for (var i = 0; i < trackHeaderRepeater.count; i++)
                trackHeaderRepeater.itemAt(i).selected = (i === selectedTrack)
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
