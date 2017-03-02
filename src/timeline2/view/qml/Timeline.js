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

function getTrackFromPos(pos) {
    if (tracksRepeater.count > 0) {
        for (var i = 0; i < tracksRepeater.count; i++) {
            var trackY = tracksRepeater.itemAt(i).y - scrollView.flickableItem.contentY
            var trackH = tracksRepeater.itemAt(i).height
            if (pos >= trackY && pos < trackY + trackH) {
                return tracksRepeater.itemAt(i).trackId
            }
        }
    }
    return -1
}

function dragging(pos, duration) {
    console.log("clip drag pos:", pos.x)
    if (tracksRepeater.count > 0) {
        var oldPosX = dropTarget.x
        var oldPosY = dropTarget.y
        var headerHeight = ruler.height
        dropTarget.x = pos.x + headerWidth
        dropTarget.width = duration * timeline.scaleFactor

        for (var i = 0; i < tracksRepeater.count; i++) {
            var trackY = tracksRepeater.itemAt(i).y - scrollView.flickableItem.contentY
            var trackH = tracksRepeater.itemAt(i).height
            if (pos.y >= trackY && pos.y < trackY + trackH) {
                currentTrack = i
                if (pos.x > 0) {
                    dropTarget.height = trackH
                    dropTarget.y = trackY + headerHeight
                    if (dropTarget.y < headerHeight) {
                        dropTarget.height -= headerHeight - dropTarget.y
                        dropTarget.y = headerHeight
                    }
                    dropTarget.visible = true
                }
                break
            }
        }
        if (i === tracksRepeater.count || pos.x <= 0)
            dropTarget.visible = false
        console.log("clip drag pos2:", dropTarget.x)
        // Scroll tracks if at edges.
        if (pos.x > scrollView.width - 50) {
            // Right edge
            scrollTimer.backwards = false
            scrollTimer.start()
        } else if (pos.x >= 0 && pos.x < 50) {
            // Left edge
            if (scrollView.flickableItem.contentX < 50) {
                scrollTimer.stop()
                scrollView.flickableItem.contentX = 0;
            } else {
                scrollTimer.backwards = true
                scrollTimer.start()
            }
        } else {
            scrollTimer.stop()
        }

        if (timeline.scrub) {
            timeline.position = Math.round(
                (pos.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
        }
        if (timeline.snap) {
            var frame = Math.round((pos.x + scrollView.flickableItem.contentX) / timeline.scaleFactor)
            var snapped = timeline.requestBestSnapPos(frame, duration)
            if (snapped > 0) {
                dropTarget.x = snapped * timeline.scaleFactor - scrollView.flickableItem.contentX + headerWidth;
            }
        }
        var frame = Math.round((dropTarget.x - headerWidth + scrollView.flickableItem.contentX ) / timeline.scaleFactor)
        if (!timeline.allowMove(currentTrack, frame, duration)) {
            dropTarget.x = oldPosX
            dropTarget.y = oldPosY
        }
    }
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

