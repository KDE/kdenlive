import QtQuick 2.2
import QtQml.Models 2.1
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import 'Timeline.js' as Logic

Rectangle {
    id: root
    objectName: "timelineview"

    SystemPalette { id: activePalette }
    color: activePalette.window

    signal clipClicked()

    function zoomByWheel(wheel) {
        if (wheel.modifiers & Qt.ControlModifier) {
            //TODO
            timeline.setScaleFactor(timeline.scaleFactor + 0.2 * wheel.angleDelta.y / 120);
        } else {
            scrollView.flickableItem.contentX += wheel.angleDelta.y
        }
        //Logic.scrollIfNeeded()
    }

    property int headerWidth: 140
    property int currentTrack: 0
    property color selectedTrackColor: Qt.rgba(0.8, 0.8, 0, 0.3);
    property alias trackCount: tracksRepeater.count
    property bool stopScrolling: false
    property color shotcutBlue: Qt.rgba(23/255, 92/255, 118/255, 1.0)
    //property alias ripple: toolbar.ripple

    onCurrentTrackChanged: timeline.selection = []

    DropArea {
        anchors.fill: parent
        onEntered: {
            if (drag.formats.indexOf('application/mlt+xml') >= 0)
                drag.acceptProposedAction()
        }
        onExited: Logic.dropped()
        onPositionChanged: {
            if (drag.formats.indexOf('application/mlt+xml') >= 0)
                Logic.dragging(drag, drag.text)
        }
        onDropped: {
            if (drop.formats.indexOf('application/mlt+xml') >= 0) {
                if (currentTrack >= 0) {
                    Logic.acceptDrop(drop.getDataAsString('application/mlt+xml'))
                    drop.acceptProposedAction()
                }
            }
            Logic.dropped()
        }
    }
    Menu {
        id: menu
        MenuItem {
            text: qsTr('Add Audio Track')
            shortcut: 'Ctrl+U'
            onTriggered: timeline.addAudioTrack();
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
                    onClicked: timeline.selectMultitrack()
                }
            }
            Flickable {
                // Non-slider scroll area for the track headers.
                //contentY: scrollView.flickableItem.contentY
                width: headerWidth
                height: trackHeaders.height
                interactive: false

                Column {
                    id: trackHeaders
                    Repeater {
                        id: trackHeaderRepeater
                        model: multitrack
                        TrackHead {
                            trackName: model.name
                            isMute: model.mute
                            isHidden: model.hidden
                            isComposite: model.composite
                            isLocked: model.locked
                            isVideo: !model.audio
                            width: headerWidth
                            //height: Logic.trackHeight(model.audio)
                            height: timeline.trackHeight[index]
                            selected: false
                            current: index === currentTrack
                            onIsLockedChanged: tracksRepeater.itemAt(index).isLocked = isLocked
                            onClicked: {
                                currentTrack = index
                                timeline.selectTrackHead(currentTrack)
                            }
                    }
                }
            }
            Rectangle {
                    // thin dividing line between headers and tracks
                    //color: activePalette.windowText
                    width: 1
                    x: parent.x + parent.width
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
            }
            }
        }
        MouseArea {
            id: tracksArea
            width: root.width - headerWidth
            height: root.height

            // This provides continuous scrubbing and scimming at the left/right edges.
            focus: true
            hoverEnabled: true
            onClicked: timeline.position = (scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor
            property bool scim: false
            onReleased: scim = false
            onExited: scim = false
            onPositionChanged: {
                if (mouse.modifiers === Qt.ShiftModifier || mouse.buttons === Qt.LeftButton) {
                    timeline.position = (scrollView.flickableItem.contentX + mouse.x) / timeline.scaleFactor
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
                        timeline.position -= 10
                    else
                        timeline.position += 10
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
                        width: tracksContainer.width
                        index: index
                        timeScale: timeline.scaleFactor
                    }
                }
                ScrollView {
                    id: scrollView
                    width: root.width - headerWidth
                    height: root.height - ruler.height

                    Item {
                        width: tracksContainer.width + headerWidth
                        height: trackHeaders.height + 30 // 30 is padding
                        Column {
                            // These make the striped background for the tracks.
                            // It is important that these are not part of the track visual hierarchy;
                            // otherwise, the clips will be obscured by the Track's background.
                            Repeater {
                                model: multitrack
                                delegate: Rectangle {
                                    width: tracksContainer.width
                                    color: (index === currentTrack)? selectedTrackColor : (index % 2)? activePalette.alternateBase : activePalette.base
                                    //height: Logic.trackHeight(audio)
                                    height: timeline.trackHeight[index]
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
        }
    }

    Rectangle {
        id: bubbleHelp
        property alias text: bubbleHelpLabel.text
        //color: application.toolTipBaseColor
        width: bubbleHelpLabel.width + 8
        height: bubbleHelpLabel.height + 8
        radius: 4
        states: [
            State { name: 'invisible'; PropertyChanges { target: bubbleHelp; opacity: 0} },
            State { name: 'visible'; PropertyChanges { target: bubbleHelp; opacity: 1} }
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
            //color: application.toolTipTextColor
            anchors.centerIn: parent
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
    DropShadow {
        source: bubbleHelp
        anchors.fill: bubbleHelp
        opacity: bubbleHelp.opacity
        horizontalOffset: 3
        verticalOffset: 3
        radius: 8
        color: '#80000000'
        transparentBorder: true
        fast: true
    }

    DelegateModel {
        id: trackDelegateModel
        model: multitrack
        Track {
            model: multitrack
            rootIndex: trackDelegateModel.modelIndex(index)
            //height: Logic.trackHeight(audio)
            height: timeline.trackHeight[index]
            isAudio: audio
            isCurrentTrack: currentTrack === index
            timeScale: timeline.scaleFactor
            selection: timeline.selection
            onClipClicked: {
                currentTrack = track.DelegateModel.itemsIndex
                timeline.selection = [ clip.DelegateModel.itemsIndex ];
                root.clipClicked()
            }
            onClipDragged: {
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
                // Show distance moved as time in a "bubble" help.
                var track = tracksRepeater.itemAt(clip.trackIndex)
                var delta = Math.round((clip.x - clip.originalX) / timeline.scaleFactor)
                var s = timeline.timecode(Math.abs(delta))
                // remove leading zeroes
                if (s.substring(0, 3) === '00:')
                    s = s.substring(3)
                s = ((delta < 0)? '-' : (delta > 0)? '+' : '') + s
                bubbleHelp.show(x, track.y + height, s)
            }
            onClipDropped: {
                scrollTimer.running = false
                bubbleHelp.hide()
            }
            onClipDraggedToTrack: {
                var i = clip.trackIndex + direction
                if (i >= 0  && i < tracksRepeater.count) {
                    var track = tracksRepeater.itemAt(i)
                    clip.reparent(track)
                    clip.trackIndex = track.DelegateModel.itemsIndex
                }
            }
            onCheckSnap: {
                for (var i = 0; i < tracksRepeater.count; i++)
                    tracksRepeater.itemAt(i).snapClip(clip)
            }
            Image {
                anchors.fill: parent
                source: "qrc:///icons/light/16x16/track-locked.png"
                fillMode: Image.Tile
                opacity: parent.isLocked
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
}
