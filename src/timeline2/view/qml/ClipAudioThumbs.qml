/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import Kdenlive.Controls 1.0
import QtQml.Models 2.15
import com.enums 1.0

Row {
    id: waveform
    opacity: clipState === ClipState.Disabled ? 0.2 : 1
    property int maxWidth: 2048
    property int totalChunks: 0
    property bool usesOffset: false
    property int offset: -1
    property real timeScale: root.timeScale
    anchors.fill: parent

    Timer {
        id: waveTimer
        interval: 50; running: false; repeat: false
        onTriggered: processReload()
    }
    
    onHeightChanged: {
        waveTimer.start()
    }

    function reload(reset) {
        if (reset === 0) {
            waveform.offset = 0
            waveform.totalChunks = 0
            waveformRepeater.model = 0
        }
        waveTimer.start()
    }

    onTimeScaleChanged: {
        waveTimer.start()
    }

    function processReload() {
        // This is needed to make the model have the correct count.
        // Model as a property expression is not working in all cases.
        if (!waveform.visible || !timeline.showAudioThumbnails) {
            return;
        }
        var total = Math.ceil(waveform.width / waveform.maxWidth)
        var chunks = total
        var updatedOffset = 0
        if (chunks > 10) {
            // Having too many chunks causes major slowdowns. In this case, we use an offset and only allow up to 20 chunks
            waveform.usesOffset = true
            updatedOffset = Math.max(0, Math.floor(clipRoot.scrollStart / waveform.maxWidth - 2))
            if (updatedOffset < waveform.offset || updatedOffset > (waveform.offset + 5) || total != waveform.totalChunks) {
                // Enforce repaint
                //waveformRepeater.model = 0
            } else {
                // All required audio thumbs chunks are already painted
                return
            }
            chunks = Math.min(10, total - updatedOffset)
        } else {
            waveform.usesOffset = false
            updatedOffset = 0
        }
        waveform.offset = updatedOffset
        waveform.totalChunks = total
        if (waveformRepeater.model === undefined || chunks !== waveformRepeater.model) {
            waveformRepeater.model = chunks
        } else {
            // Enforce repaint
            waveformRepeater.repaintNodes = !waveformRepeater.repaintNodes
        }
    }
    Item {
        width: waveform.offset * waveform.maxWidth
        height: parent.height
    }

    Repeater {
        id: waveformRepeater
        property bool repaintNodes: false
        TimelineWaveform {
            width: waveform.maxWidth < waveform.width ? (index + waveform.offset == waveform.totalChunks - 1 ? waveform.width % waveform.maxWidth : waveform.maxWidth) : Math.round(waveform.width)
            height: waveform.height
            ix: index
            channels: clipRoot.audioChannels
            binId: clipRoot.binId
            audioStream: clipRoot.audioStream
            isFirstChunk: index + waveform.offset == 0
            isOpaque: true
            scaleFactor: waveform.timeScale
            format: timeline.audioThumbFormat
            normalize: timeline.audioThumbNormalize
            speed: clipRoot.speed
            waveInPoint: clipRoot.speed < 0 ? (Math.ceil((clipRoot.maxDuration - 1 - clipRoot.inPoint) * Math.abs(clipRoot.speed)  - ((index + waveform.offset) * waveform.maxWidth / waveform.timeScale) * Math.abs(clipRoot.speed)) * clipRoot.audioChannels) : (Math.round((clipRoot.inPoint + ((index + waveform.offset) * waveform.maxWidth / waveform.timeScale)) * clipRoot.speed) * clipRoot.audioChannels)
            waveOutPoint: clipRoot.speed < 0 ? Math.max(0, (waveInPoint - Math.round(width / waveform.timeScale * Math.abs(clipRoot.speed)) * clipRoot.audioChannels)) : (waveInPoint + Math.round(width / waveform.timeScale * clipRoot.speed) * clipRoot.audioChannels)
            fillColor0: clipRoot.color
            fillColor1: root.thumbColor1
            fillColor2: root.thumbColor2
            enforceRepaint: waveformRepeater.repaintNodes
        }
    }
}
