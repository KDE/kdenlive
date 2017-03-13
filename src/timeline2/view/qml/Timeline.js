/*
 * Copyright (c) 2013-2015 Meltytech, LLC
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

function scrollIfNeeded() {
    var x = timeline.position * timeline.scaleFactor;
    if (!scrollView) return;
    if (x > scrollView.flickableItem.contentX + scrollView.width - 50)
        scrollView.flickableItem.contentX = x - scrollView.width + 50;
    else if (x < 50)
        scrollView.flickableItem.contentX = 0;
    else if (x < scrollView.flickableItem.contentX + 50)
        scrollView.flickableItem.contentX = x - 50;
}

function getTrackIndexFromPos(pos) {
    if (tracksRepeater.count > 0) {
        for (var i = 0; i < tracksRepeater.count; i++) {
            var trackY = tracksRepeater.itemAt(i).y - scrollView.flickableItem.contentY
            var trackH = tracksRepeater.itemAt(i).height
            if (pos >= trackY && pos < trackY + trackH) {
                return i
            }
        }
    }
    return -1
}

function getTrackIdFromPos(pos) {
    var index = getTrackIndexFromPos(pos);
    if (index != -1) {
        return tracksRepeater.itemAt(index).trackId
    }
    return -1
}

function dropped() {
    dropTarget.visible = false
    scrollTimer.running = false
}

function acceptDrop(xml) {
    var position = Math.round((dropTarget.x + scrollView.flickableItem.contentX - headerWidth) / timeline.scaleFactor)
    timeline.insertClip(currentTrack, position, xml)
    /*if (timeline.ripple)
        timeline.insert(currentTrack, position, xml)
    else
        timeline.overwrite(currentTrack, position, xml)*/
}

function trackHeight(isAudio) {
    return isAudio? Math.max(40, timeline.trackHeight) : timeline.trackHeight * 2
}

