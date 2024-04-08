/*
    SPDX-FileCopyrightText: 2013-2015 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2015 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

function scrollIfNeeded() {
    if (!scrollView) return;
    var x = root.consumerPosition * timeline.scaleFactor;
    if (x > scrollView.contentX + scrollView.width - 50)
        scrollView.contentX = x - scrollView.width + 50;
    else if (x < 50)
        scrollView.contentX = 0;
    else if (x < scrollView.contentX + 50)
        scrollView.contentX = x - 50;
}

function scrollToPosIfNeeded(xpos) {
    if (!scrollView) return;
    if (xpos > scrollView.contentX + scrollView.width - 50)
        scrollView.contentX = x - scrollView.width + 50;
    else if (xpos < 50)
        scrollView.contentX = 0;
    else if (xpos < scrollView.contentX + 50)
        scrollView.contentX = x - 50;
}

function getTrackIndexFromPos(pos) {
    if (tracksRepeater.count > 0) {
        for (var i = 0; i < tracksRepeater.count; i++) {
            var trackY = tracksRepeater.itemAt(i).y
            var trackH = tracksRepeater.itemAt(i).height
            if (pos >= trackY && (pos < trackY + trackH || i == tracksRepeater.count - 1)) {
                return i
            }
        }
    }
    return -1
}

function getTrackIdFromPos(pos) {
    var index = getTrackIndexFromPos(pos);
    if (index !== -1) {
        return tracksRepeater.itemAt(index).trackInternalId
    }
    return -1
}

function getTrackYFromId(id) {
    var result = 0
    for (var i = 0; i < trackHeaderRepeater.count; i++) {
        if (trackHeaderRepeater.itemAt(i).trackId === id) {
            break;
        }
        result += trackHeaderRepeater.itemAt(i).height
    }
    return result
}

function getTrackIndexFromId(id) {
    var i = 0;
    for (; i < trackHeaderRepeater.count; i++) {
        if (trackHeaderRepeater.itemAt(i).trackId === id) {
            break;
        }
    }
    return i
}

function getTrackHeaderById(id) {
    var i = 0;
    for (; i < trackHeaderRepeater.count; i++) {
        if (trackHeaderRepeater.itemAt(i).trackId === id) {
            return trackHeaderRepeater.itemAt(i);
        }
    }
    return 0
}

function getTrackById(id) {
    var i = 0;
    for (; i < tracksRepeater.count; i++) {
        if (tracksRepeater.itemAt(i).trackInternalId === id) {
            return tracksRepeater.itemAt(i);
        }
    }
    return 0
}

function getTrackHeightByPos(pos) {
    if (pos >= 0 && pos < tracksRepeater.count) {
        return tracksRepeater.itemAt(pos).height
    }
    return 0
}

function getTrackYFromMltIndex(id) {
    if (id <= 0) {
        return 0
    }
    var result = - scrollView.contentY
    for (var i = 0; i < trackHeaderRepeater.count - id; i++) {
        result += trackHeaderRepeater.itemAt(i).height
    }
    return result
}

function getTracksList() {
    var result = new Array(2);
    var aTracks = 0
    var vTracks = 0
    for (var i = 0; i < trackHeaderRepeater.count; i++) {
        if (trackHeaderRepeater.itemAt(i).isAudio) {
            aTracks ++;
        } else {
            vTracks ++;
        }
    }
    result[0] = aTracks;
    result[1] = vTracks;
    return result
}

function acceptDrop(xml) {
    var position = Math.round((dropTarget.x + scrollView.contentX - headerWidth) / timeline.scaleFactor)
    timeline.insertClip(currentTrack, position, xml)
    /*if (timeline.ripple)
        timeline.insert(currentTrack, position, xml)
    else
        timeline.overwrite(currentTrack, position, xml)*/
}

function trackHeight(isAudio) {
    return isAudio? Math.max(40, timeline.trackHeight) : timeline.trackHeight * 2
}

