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
    id: rulerRoot
    property real timeScale: timeline.scaleFactor
    property int labelSize: fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
    property real stepSize: labelSize
    property real frameSize: labelSize
    property real fontUnit: root.baseUnit * 0.9
    property alias rulerZone : zone
    property int workingPreview : timeline.workingPreview
    SystemPalette { id: activePalette }

    function adjustStepSize() {
        if (rulerRoot.timeScale > 19) {
            rulerRoot.frameSize = rulerRoot.timeScale
            rulerRoot.stepSize = rulerRoot.timeScale > rulerRoot.labelSize * 1.3 ? rulerRoot.timeScale : Math.floor(rulerRoot.labelSize/rulerRoot.timeScale+1) * rulerRoot.timeScale
        } else {
            rulerRoot.frameSize = rulerRoot.labelSize / 3
            rulerRoot.stepSize = rulerRoot.labelSize * 4 / 3
        }
    }

    function adjustFormat() {
        rulerRoot.labelSize = fontMetrics.tightBoundingRect(timeline.timecode(36000)).width
        adjustStepSize()
    }
    enabled: false
    height: fontMetrics.font.pixelSize * 2
    color: activePalette.window

    // Timeline preview stuff
    Repeater {
        model: timeline.dirtyChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: scrollView.flickableItem.contentX / rulerRoot.stepSize + modelData * rulerRoot.timeScale
            y: 0
            width: 25 * rulerRoot.timeScale
            height: parent.height / 4
            color: 'darkred'
        }
    }

    Repeater {
        model: timeline.renderedChunks
        anchors.fill: parent
        delegate: Rectangle {
            x: scrollView.flickableItem.contentX / rulerRoot.stepSize + modelData * rulerRoot.timeScale
            y: 0
            width: 25 * rulerRoot.timeScale
            height: parent.height / 4
            color: 'darkgreen'
        }
    }
    Rectangle {
        id: working
        x: scrollView.flickableItem.contentX / rulerRoot.stepSize + rulerRoot.workingPreview * rulerRoot.timeScale
        y: 0
        width: 25 * rulerRoot.timeScale
        height: parent.height / 4
        color: 'orange'
        visible: rulerRoot.workingPreview > -1
    }

    // Ruler marks
    Repeater {
        model: scrollView.width / rulerRoot.frameSize + 2
        Rectangle {
            property int realPos: scrollView.flickableItem.contentX / rulerRoot.frameSize + index
            x: realPos * rulerRoot.frameSize
            anchors.bottom: parent.bottom
            height: (realPos % 4)? ((realPos % 2) ? 3 : 7) : 12
            width: 1
            color: activePalette.windowText
            opacity: 0.5
        }
    }
    // Ruler labels
    Repeater {
        model: scrollView.width / rulerRoot.stepSize + 2
        Label {
            property int realPos: scrollView.flickableItem.contentX / rulerRoot.stepSize + index
            anchors.top: parent.top
            anchors.topMargin: 2
            x: realPos * rulerRoot.stepSize
            text: timeline.timecode(realPos * rulerRoot.stepSize / rulerRoot.timeScale)
            font.pointSize: rulerRoot.fontUnit
            color: activePalette.windowText
        }
    }

    // monitor zone
    Rectangle {
        id: zone
        visible: timeline.zoneOut > timeline.zoneIn
        color: activePalette.highlight
        x: timeline.zoneIn * rulerRoot.timeScale
        width: (timeline.zoneOut - timeline.zoneIn) * rulerRoot.timeScale
        anchors.bottom: parent.bottom
        height: parent.height / 3
        opacity: 0.4
    }
}

