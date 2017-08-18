/*
 * Copyright (c) 2013 Meltytech, LLC
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
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Rectangle {
    property int labelSize: fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
    property real stepSize: labelSize
    property real frameSize: labelSize
    property int index: 0
    property real timeScale: timeline.scaleFactor
    property real fontUnit: root.baseUnit * 0.9
    property alias rulerZone : zone
    property int workingPreview : timeline.workingPreview

    SystemPalette { id: activePalette }

    function adjustStepSize() {
        if (timeScale > 19) {
            frameSize = timeScale
            stepSize = timeScale > labelSize * 1.3 ? timeScale : Math.floor(labelSize/timeScale+1) * timeScale
        } else {
            frameSize = labelSize / 3
            stepSize = labelSize * 4 / 3
        }
    }

    function adjustFormat() {
        labelSize = fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
        adjustStepSize()
    }

    /*onDirtyChunksChanged: {
        console.log('new chunks RES: ' , dirtyChunks)
        //for (var i = 0; i < dirtyChunks.count; i++)
        //    console.log('new chunks: ' , dirtyChunks[i])
    }*/

    id: rulerTop
    enabled: false
    height: fontMetrics.font.pixelSize * 2
    color: activePalette.window

    Repeater {
        model: timeline.dirtyChunks
        anchors.fill: rulerTop
        delegate: Rectangle {
            x: scrollView.flickableItem.contentX / stepSize + modelData * rulerTop.timeScale
            y: 0
            width: 25 * rulerTop.timeScale
            height: rulerTop.height / 4
            color: 'darkred'
        }
    }

    Repeater {
        model: timeline.renderedChunks
        anchors.fill: rulerTop
        delegate: Rectangle {
            x: scrollView.flickableItem.contentX / stepSize + modelData * rulerTop.timeScale
            y: 0
            width: 25 * rulerTop.timeScale
            height: rulerTop.height / 4
            color: 'darkgreen'
        }
    }
    Rectangle {
        id: working
        x: scrollView.flickableItem.contentX / stepSize + rulerTop.workingPreview * rulerTop.timeScale
        y: 0
        width: 25 * rulerTop.timeScale
        height: rulerTop.height / 4
        color: 'orange'
        visible: rulerTop.workingPreview > -1
        SequentialAnimation on color {
            id: anim
            loops: Animation.Infinite
            ColorAnimation {
                to: 'transparent'
                duration: 1000
            }
            PropertyAnimation {
                to: 'orange'
                duration: 1000
            }
        }
    }

    Repeater {
        model: scrollView.width / frameSize + 2
        Rectangle {
            property int realPos: scrollView.flickableItem.contentX / frameSize + index
            x: realPos * frameSize
            anchors.bottom: rulerTop.bottom
            height: (realPos % 4)? ((realPos % 2) ? 3 : 7) : 12
            width: 1
            color: activePalette.windowText
            opacity: 0.5
        }
    }
    Repeater {
        model: scrollView.width / stepSize + 2
        Label {
            property int realPos: scrollView.flickableItem.contentX / stepSize + index
            anchors.top: rulerTop.top
            anchors.topMargin: 2
            x: realPos * stepSize
            text: timeline.timecode(realPos * stepSize / timeScale)
            font.pointSize: fontUnit
            color: activePalette.windowText
        }
    }

    // monitor zone
    Rectangle {
        id: zone
        visible: timeline.zoneOut > timeline.zoneIn
        color: activePalette.highlight
        x: timeline.zoneIn * timeScale
        width: (timeline.zoneOut - timeline.zoneIn) * timeScale
        anchors.bottom: parent.bottom
        height: ruler.height / 3
        opacity: 0.4
    }
}

