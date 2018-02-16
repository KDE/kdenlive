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

import QtQuick 2.6
import QtQml.Models 2.2

Column{
    id: trackRoot
    property alias model: trackModel.model
    property alias rootIndex: trackModel.rootIndex
    property bool isAudio
    property bool isMute
    property bool isHidden
    property real timeScale: 1.0
    property bool isCurrentTrack: false
    property bool isLocked: false
    property int trackId : -42
    height: parent.height

    SystemPalette { id: activePalette }

    signal clipClicked(var clip, var track, int shiftClick)
    signal clipDragged(var clip, int x, int y)
    signal clipDropped(var clip)
    signal compositionDropped(var clip)
    signal clipDraggedToTrack(var clip, int pos, int xpos)
    signal compositionDraggedToTrack(var composition, int pos, int xpos)

    /*function redrawWaveforms() {
        for (var i = 0; i < repeater.count; i++)
            repeater.itemAt(i).generateWaveform()
    }*/

    function clipAt(index) {
        return repeater.itemAt(index)
    }

    width: clipRow.width

    DelegateModel {
        id: trackModel
        delegate: Item{
            property var itemModel : model
            z: model.isComposition ? 1 : 0
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
                    value: root.timelineSelection.indexOf(loader.item.clipId) !== -1
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "mltService"
                    value: model.mlt_service
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "modelStart"
                    value: model.start
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "scrollX"
                    value: scrollView.flickableItem.contentX
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "fadeIn"
                    value: model.fadeIn
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "groupDrag"
                    value: model.groupDrag
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "clipStatus"
                    value: model.clipStatus
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "fadeOut"
                    value: model.fadeOut
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "audioLevels"
                    value: model.audioLevels
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "showKeyframes"
                    value: model.showKeyframes
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "keyframeModel"
                    value: model.keyframeModel
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "aTrack"
                    value: model.a_track
                    when: loader.status == Loader.Ready && loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "trackHeight"
                    value: root.trackHeight
                    when: loader.status == Loader.Ready && loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "clipDuration"
                    value: model.duration
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "inPoint"
                    value: model.in
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "outPoint"
                    value: model.out
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "grouped"
                    value: model.grouped
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "clipName"
                    value: model.name
                    when: loader.status == Loader.Ready
                }
                Binding {
                    target: loader.item
                    property: "clipResource"
                    value: model.resource
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "speed"
                    value: model.speed
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "forceReloadThumb"
                    value: model.reloadThumb
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                Binding {
                    target: loader.item
                    property: "binId"
                    value: model.binId
                    when: loader.status == Loader.Ready && !loader.item.isComposition
                }
                sourceComponent: {
                    if (model.isComposition) {
                        return compositionDelegate
                    } else {
                        return clipDelegate
                    }
                }
                onLoaded: {
                    item.clipId= model.item
                    if (loader.item.isComposition === false) {
                        console.log('loaded clip: ', model.start, ', ID: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                        item.isAudio= model.audio
                        item.markers= model.markers
                        item.hasAudio = model.hasAudio
                        //item.binId= model.binId
                    } else {
                        console.log('loaded composition: ', model.start, ', ID: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                        //item.aTrack = model.a_track
                    }
                    item.trackId= trackRoot.trackId
                    //item.selected= trackRoot.selection.indexOf(item.clipId) !== -1
                    //console.log(width, height);
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
            height: trackRoot.height

            onClicked: {
                console.log("Clip clicked",clip.clipId)
                trackRoot.clipClicked(clip, clip.parentTrack, shiftClick);
                clip.draggedX = clip.x
            }
            onMoved: { //called when the movement is finished
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                var frame = clip.currentFrame
                var origFrame = clip.modelStart
                if (frame > -1) {
                    controller.requestClipMove(cIndex, clip.originalTrackId, origFrame, false, false, false)
                    controller.requestClipMove(cIndex, toTrack, frame, true, true, true)
                }
                clip.currentFrame = -1
            }
            onDropped: { //called when the movement is finished
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                var frame = clip.currentFrame
                var origFrame = clip.modelStart
                if (frame != origFrame && frame > -1) {
                    controller.requestClipMove(cIndex, toTrack, origFrame, false, false, false)
                    controller.requestClipMove(cIndex, toTrack, frame, true, true, true)
                }
                clip.currentFrame = -1
            }
            onDragged: { //called when the move is in process
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                clip.x = Math.max(0, clip.x)
                if (Math.round(clip.currentFrame * timeScale) == clip.x) {
                    // No move to perform
                } else {
                    var frame = Math.round(clip.x / timeScale)
                    frame = controller.suggestClipMove(cIndex, toTrack, frame, root.snapping);
                    if (clip.currentFrame == frame) {
                        // Abort move
                        clip.x = frame * timeScale
                    } else {
                        clip.x = frame * timeScale
                        clip.currentFrame = frame
                        var mapped = trackRoot.mapFromItem(clip, mouse.x, mouse.y)
                        trackRoot.clipDragged(clip, mapped.x, mapped.y)
                        clip.draggedX = clip.x
                    }
                }
            }
            onTrimmingIn: {
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, false, false, root.snapping, shiftTrim)
                if (new_duration > 0) {
                    clip.lastValidDuration = new_duration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = new_duration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s)
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x - 20, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, root.snapping, shiftTrim)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, root.snapping, shiftTrim)
            }
            onTrimmingOut: {
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, true, false, root.snapping, shiftTrim)
                if (new_duration > 0) {
                    clip.lastValidDuration = new_duration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = clip.originalDuration - new_duration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s)
                        .arg(timeline.timecode(new_duration))
                    bubbleHelp.show(clip.x + clip.width - 20, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, true, false, root.snapping, shiftTrim)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, true, true, root.snapping, shiftTrim)
            }

            Component.onCompleted: {
                moved.connect(trackRoot.clipDropped)
                dropped.connect(trackRoot.clipDropped)
                draggedToTrack.connect(trackRoot.clipDraggedToTrack)
                //console.log('BUILDING CLIP item ', model.clipId, 'name', model.name, ' service: ',mltService)
            }
        }
    }
    Component {
        id: compositionDelegate
        Composition {
            displayHeight: trackRoot.height / 2
            opacity: 0.8
            selected: root.timelineSelection.indexOf(clipId) !== -1

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

                frame = controller.suggestCompositionMove(cIndex, toTrack, frame, root.snapping);

                if (!controller.requestCompositionMove(cIndex, toTrack, frame, false, false)) {
                    // Abort move
                    clip.x = clip.draggedX
                } else {
                    clip.x = frame * timeScale
                    var mapped = trackRoot.mapFromItem(clip, mouse.x, mouse.y)
                    trackRoot.clipDragged(clip, mapped.x, mapped.y)
                    clip.draggedX = clip.x
                }
            }
            onTrimmingIn: {
                if (controller.requestItemResize(clip.clipId, newDuration, false, false, root.snapping)) {
                    clip.lastValidDuration = newDuration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = newDuration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s)
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, root.snapping)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, root.snapping)
            }
            onTrimmingOut: {
                if (controller.requestItemResize(clip.clipId, newDuration, true, false, root.snapping)) {
                    clip.lastValidDuration = newDuration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = newDuration - clip.originalDuration
                    var s = timeline.timecode(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta < 0)? '+' : (delta > 0)? '-' : '')
                        .arg(s)
                        .arg(timeline.timecode(clipDuration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, true, false, root.snapping)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, true, true, root.snapping)
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
