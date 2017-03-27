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

import QtQuick 2.0
import QtQml.Models 2.1

Column{
    id: trackRoot
    property alias model: trackModel.model
    property alias rootIndex: trackModel.rootIndex
    property bool isAudio
    property real timeScale: 1.0
    property bool isCurrentTrack: false
    property bool isLocked: false
    property var selection
    property int trackId : -42
    height: parent.height

    SystemPalette { id: activePalette }

    signal clipClicked(var clip, var track, int shiftClick)
    signal clipDragged(var clip, int x, int y)
    signal clipDropped(var clip)
    signal compositionDropped(var clip)
    signal clipDraggedToTrack(var clip, int pos)
    signal compositionDraggedToTrack(var composition, int pos)

    function redrawWaveforms() {
        for (var i = 0; i < repeater.count; i++)
            repeater.itemAt(i).generateWaveform()
    }

    function clipAt(index) {
        return repeater.itemAt(index)
    }

    width: clipRow.width

    DelegateModel {
        id: trackModel
        delegate: Item{
            property var itemModel : model
            Loader {
                id: loader
                Binding {
                    target: loader.item
                    property: "timeScale"
                    value: trackRoot.timeScale
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "selected"
                    value: trackRoot.selection.indexOf(loader.item.clipId) !== -1
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "a_track"
                    value: loader.item.a_track
                    when: loader.item.isComposition && loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "trackHeight"
                    value: root.trackHeight
                    when: loader.item.isComposition && loader.status == Loader.Ready
                }
                sourceComponent: {
                    if (model.isComposition) {
                        return compositionDelegate
                    } else {
                        return clipDelegate
                    }
                }
                onLoaded: {
                    item.clipName= model.name
                    item.clipResource= model.resource
                    item.clipDuration= model.duration
                    item.mltService= model.mlt_service
                    item.inPoint= model.in
                    item.outPoint= model.out
                    item.isBlank= model.blank
                    item.clipId= model.item
                    item.binId= model.binId
                    item.isAudio= false //model.audio
                    item.isComposition= model.isComposition
                    if (!model.isComposition) {
                        item.audioLevels= model.audioLevels
                        item.markers= model.markers
                        item.fadeIn= 0 //model.fadeIn
                        item.fadeOut= 0 //model.fadeOut
                    } else {
                        item.a_track = model.a_track
                    }
                    item.width= model.duration * timeScale
                    item.modelStart= model.start
                    item.x= model.start * timeScale
                    item.grouped= model.grouped
                    item.borderColor= (model.grouped ? 'yellow' : 'black')
                    item.trackIndex= trackRoot.DelegateModel.itemsIndex
                    item.trackId= trackRoot.trackId
                    //hash= model.hash
                    item.speed= 1 //model.speed
                    item.selected= trackRoot.selection.indexOf(item.clipId) !== -1
                    console.log(width, height);
                }
            }
        }
    }

    Item {
        id: clipRow
        height: trackRoot.height
        Repeater { id: repeater; model: trackModel }
    }

    Component {
        id: clipDelegate
        Clip {
            z: 10
            height: trackRoot.height
            onGroupedChanged: {
                console.log('Clip ', clipId, ' is grouped : ', grouped)
                flashclip.start()
            }

            SequentialAnimation on color {
                id: flashclip
                loops: 2
                running: false
                ColorAnimation { from: Qt.darker(getColor()); to: "#ff3300"; duration: 100 }
                ColorAnimation { from: "#ff3300"; to: Qt.darker(getColor()); duration: 100 }
            }

            onClicked: {
                console.log("Clip clicked",clip.clipId)
                trackRoot.clipClicked(clip, trackRoot, shiftClick);
                clip.draggedX = clip.x
            }
            onMoved: { //called when the movement is finished
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                var frame = Math.round(clip.x / timeScale)
                var origFrame = Math.round(clip.originalX / timeScale)

                console.log("Asking move ",toTrack, cIndex, frame)
                controller.requestClipMove(cIndex, clip.originalTrackId, origFrame, false, false)
                var val = controller.requestClipMove(cIndex, toTrack, frame, true, true)
                console.log("RESULT", val)
            }
            onDragged: { //called when the move is in process
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                clip.x = Math.max(0, clip.x)
                var frame = Math.round(clip.x / timeScale)

                frame = controller.suggestClipMove(cIndex, toTrack, frame);

                if (!controller.requestClipMove(cIndex, toTrack, frame, false, false)) {
                    // Abort move
                    clip.x = clip.draggedX
                } else {
                    clip.x = frame * timeScale
                }
                var mapped = trackRoot.mapFromItem(clip, mouse.x, mouse.y)
                trackRoot.clipDragged(clip, mapped.x, mapped.y)
                clip.draggedX = clip.x
            }
            onTrimmingIn: {
                if (controller.requestClipResize(clip.clipId, newDuration, false, false, true)) {
                    clip.lastValidDuration = newDuration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = newDuration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s.substring(3))
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                bubbleHelp.hide()
                controller.requestClipResize(clip.clipId, clip.originalDuration, false, false, true)
                controller.requestClipResize(clip.clipId, clip.lastValidDuration, false, true, true)
            }
            onTrimmingOut: {
                if (controller.requestClipResize(clip.clipId, newDuration, true, false, true)) {
                    clip.lastValidDuration = newDuration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = newDuration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s.substring(3))
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
                controller.requestClipResize(clip.clipId, clip.originalDuration, true, false, true)
                controller.requestClipResize(clip.clipId, clip.lastValidDuration, true, true, true)
            }

            Component.onCompleted: {
                moved.connect(trackRoot.clipDropped)
                dropped.connect(trackRoot.clipDropped)
                draggedToTrack.connect(trackRoot.clipDraggedToTrack)
                console.log('Showing CLIP item ', model.clipId, 'name', model.name, ' service: ',mltService)
            }
        }
    }
    Component {
        id: compositionDelegate
        Composition {
            z: 20
            y: trackRoot.height / 2
            height: trackRoot.height / 2
            opacity: 0.6
            selected: trackRoot.selection.indexOf(clipId) !== -1

            onGroupedChanged: {
                console.log('Composition ', clipId, ' is grouped : ', grouped)
                flashclip.start()
            }

            SequentialAnimation on color {
                id: flashclip
                loops: 2
                running: false
                ColorAnimation { from: Qt.darker(getColor()); to: "#ff3300"; duration: 100 }
                ColorAnimation { from: "#ff3300"; to: Qt.darker(getColor()); duration: 100 }
            }

            onClicked: {
                console.log("Composition clicked",clip.clipId)
                trackRoot.clipClicked(clip, trackRoot, shiftClick);
                clip.draggedX = clip.x
            }
            onMoved: { //called when the movement is finished
                console.log("Composition released",clip.clipId)
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                var frame = Math.round(clip.x / timeScale)
                var origFrame = Math.round(clip.originalX / timeScale)

                console.log("Asking move ",toTrack, cIndex, frame)
                controller.requestCompositionMove(cIndex, clip.originalTrackId, origFrame, false, false)
                var val = controller.requestCompositionMove(cIndex, toTrack, frame, true, true)
                console.log("RESULT", val)
            }
            onDragged: { //called when the move is in process
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                clip.x = Math.max(0, clip.x)
                var frame = Math.round(clip.x / timeScale)

                frame = controller.suggestCompositionMove(cIndex, toTrack, frame);

                if (!controller.requestCompositionMove(cIndex, toTrack, frame, false, false)) {
                    // Abort move
                    clip.x = clip.draggedX
                } else {
                    clip.x = frame * timeScale
                }
                var mapped = trackRoot.mapFromItem(clip, mouse.x, mouse.y)
                trackRoot.clipDragged(clip, mapped.x, mapped.y)
                clip.draggedX = clip.x
            }
            onTrimmingIn: {
                if (controller.requestClipResize(clip.clipId, newDuration, false, false, true)) {
                    clip.lastValidDuration = newDuration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = newDuration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s.substring(3))
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                bubbleHelp.hide()
                controller.requestClipResize(clip.clipId, clip.originalDuration, false, false, true)
                controller.requestClipResize(clip.clipId, clip.lastValidDuration, false, true, true)
            }
            onTrimmingOut: {
                if (controller.requestClipResize(clip.clipId, newDuration, true, false, true)) {
                    clip.lastValidDuration = newDuration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = newDuration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s.substring(3))
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
                controller.requestClipResize(clip.clipId, clip.originalDuration, true, false, true)
                controller.requestClipResize(clip.clipId, clip.lastValidDuration, true, true, true)
            }

            Component.onCompleted: {
                moved.connect(trackRoot.compositionDropped)
                dropped.connect(trackRoot.compositionDropped)
                draggedToTrack.connect(trackRoot.compositionDraggedToTrack)
                // console.log('Showing item ', model.item, 'name', model.name, ' service: ',mltService)
            }
        }
    }
}
