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
import 'Track.js' as Logic

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
    signal clipDraggedToTrack(var clip, int pos)
    signal checkSnap(var clip)

    function redrawWaveforms() {
        for (var i = 0; i < repeater.count; i++)
            repeater.itemAt(i).generateWaveform()
    }

    function snapClip(clip) {
        Logic.snapClip(clip, repeater)
    }

    function snapDrop(clip) {
        Logic.snapDrop(clip, repeater)
    }

    function clipAt(index) {
        return repeater.itemAt(index)
    }

    width: clipRow.width

    DelegateModel {
        id: trackModel
        Clip {
            timeScale: timeline.scaleFactor
            clipName: model.name
            clipResource: model.resource
            clipDuration: model.duration
            mltService: model.mlt_service
            inPoint: model.in
            outPoint: model.out
            isBlank: model.blank
            clipId: model.item
            binId: model.binId
            isAudio: false //model.audio
            isTransition: false //model.isTransition
            audioLevels: model.audioLevels
            width: model.duration * timeScale
            height: parent.height
            modelStart: model.start
            x: modelStart * timeScale
            grouped: model.grouped
            borderColor: (model.grouped > 0 ? 'yellow' : 'black')
            trackIndex: trackRoot.DelegateModel.itemsIndex
            trackId: trackRoot.trackId
            fadeIn: 0 //model.fadeIn
            fadeOut: 0 //model.fadeOut
            //hash: model.hash
            speed: 1 //model.speed
            selected: trackRoot.selection.indexOf(clipId) !== -1

            onClicked: {
                console.log("Clip clicked",clip.clipId)
                trackRoot.clipClicked(clip, trackRoot, shiftClick);
                clip.draggedX = clip.x
            }
            onMoved: {
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                var frame = Math.round(clip.x / timeScale)
                var origFrame = Math.round(clip.originalX / timeScale)

                console.log("Asking move ",toTrack, cIndex, frame)
                timeline.moveClip(cIndex, clip.originalTrackId, origFrame, false)
                var val = timeline.moveClip(cIndex, toTrack, frame, true)
                console.log("RESULT", val)
            }
            onDragged: {
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                clip.x = Math.max(0, clip.x)
                var frame = Math.round(clip.x / timeScale)

                frame = timeline.suggestClipMove(cIndex, toTrack, frame);

                //console.log("dragging clip x: ", clip.x, " ID: "<<clip.originalClipIndex)
                if (!timeline.allowMoveClip(cIndex, toTrack, frame)) {
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
                //if (!(mouse.modifiers & Qt.AltModifier) && timeline.snap && !timeline.ripple)
                //    delta = Logic.snapTrimIn(clip, delta)
                if (timeline.resizeClip(clip.clipId, newDuration, false, false)) {
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
                timeline.resizeClip(clip.clipId, clip.originalDuration, false, false)
                timeline.resizeClip(clip.clipId, clip.lastValidDuration, false, true)
            }
            onTrimmingOut: {
               // if (!(mouse.modifiers & Qt.AltModifier) && timeline.snap && !timeline.ripple)
               //     delta = Logic.snapTrimOut(clip, delta)
                if (timeline.resizeClip(clip.clipId, newDuration, true, false)) {
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
                timeline.resizeClip(clip.clipId, clip.originalDuration, true, false)
                timeline.resizeClip(clip.clipId, clip.lastValidDuration, true, true)
            }

            Component.onCompleted: {
                moved.connect(trackRoot.clipDropped)
                dropped.connect(trackRoot.clipDropped)
                draggedToTrack.connect(trackRoot.clipDraggedToTrack)
            }
        }
    }

    Item {
        id: clipRow
        height: trackRoot.height
        Repeater { id: repeater; model: trackModel }
    }
}
