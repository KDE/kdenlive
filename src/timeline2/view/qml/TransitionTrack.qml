/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *   Based on work by Dan Dennedy <dan@dennedy.org> (Shotcut)              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

import QtQuick 2.0
import QtQml.Models 2.1

Column{
    id: transitionTrackRoot
    property alias model: transTackModel.model
    property alias rootIndex: transTackModel.rootIndex
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
    signal transitionDraggedToTrack(var clip, int pos)

    function clipAt(index) {
        return repeater.itemAt(index)
    }

    width: transitionRow.width

    DelegateModel {
        id: transTackModel
        MltTransition {
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
            isTransition: true
            width: model.duration * timeScale
            height: parent.height / 2
            modelStart: model.start
            x: modelStart * timeScale
            y: parent.height / 2
            grouped: model.grouped
            borderColor: (model.grouped ? 'yellow' : 'black')
            trackIndex: transitionTrackRoot.DelegateModel.itemsIndex
            trackId: transitionTrackRoot.trackId
            opacity: 0.6
            selected: transitionTrackRoot.selection.indexOf(clipId) !== -1

            onGroupedChanged: {
                console.log('Transition ', clipId, ' is grouped : ', grouped)
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
                console.log("Transition clicked",clip.clipId)
                transitionTrackRoot.clipClicked(clip, transitionTrackRoot, shiftClick);
                clip.draggedX = clip.x
            }
            onMoved: { //called when the movement is finished
                console.log("Transition released",clip.clipId)
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                var frame = Math.round(clip.x / timeScale)
                var origFrame = Math.round(clip.originalX / timeScale)

                console.log("Asking move ",toTrack, cIndex, frame)
                controller.requestTransitionMove(cIndex, clip.originalTrackId, origFrame, false, false)
                var val = controller.requestTransitionMove(cIndex, toTrack, frame, true, true)
                console.log("RESULT", val)
            }
            onDragged: { //called when the move is in process
                var toTrack = clip.trackId
                var cIndex = clip.clipId
                clip.x = Math.max(0, clip.x)
                var frame = Math.round(clip.x / timeScale)

                frame = controller.suggestTransitionMove(cIndex, toTrack, frame);

                if (!controller.requestTransitionMove(cIndex, toTrack, frame, false, false)) {
                    // Abort move
                    clip.x = clip.draggedX
                } else {
                    clip.x = frame * timeScale
                }
                var mapped = transitionTrackRoot.mapFromItem(clip, mouse.x, mouse.y)
                transitionTrackRoot.clipDragged(clip, mapped.x, mapped.y)
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
                    bubbleHelp.show(clip.x + clip.width, transitionTrackRoot.y + transitionTrackRoot.height, s)
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
                    bubbleHelp.show(clip.x + clip.width, transitionTrackRoot.y + transitionTrackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
                controller.requestClipResize(clip.clipId, clip.originalDuration, true, false, true)
                controller.requestClipResize(clip.clipId, clip.lastValidDuration, true, true, true)
            }

            Component.onCompleted: {
                moved.connect(transitionTrackRoot.clipDropped)
                dropped.connect(transitionTrackRoot.clipDropped)
                draggedToTrack.connect(transitionTrackRoot.transitionDraggedToTrack)
                //console.log('Showing item ', model.item, 'name', model.name, ' service: ',mltService)
            }
        }
    }

    Item {
        id: transitionRow
        height: transitionTrackRoot.height
        Repeater { id: repeater; model: transTackModel }
    }
}
