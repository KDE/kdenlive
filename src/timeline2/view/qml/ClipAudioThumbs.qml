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
        processReload()
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
            // Having too many chunks causes major slowdowns. In this case, we use an offset and only allow up to 10 chunks
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
        }
    }
    Item {
        // Placeholder item for the offset
        width: waveform.usesOffset ? waveform.offset * waveform.maxWidth : 0
        height: waveform.height
    }

    Repeater {
        id: waveformRepeater
        TimelineWaveform {
            width: waveform.maxWidth < waveform.width ? (index + waveform.offset == waveform.totalChunks - 1 ? waveform.width % waveform.maxWidth : waveform.maxWidth) : Math.round(waveform.width)
            height: waveform.height
            channels: clipRoot.audioChannels
            binId: clipRoot.binId
            audioStream: clipRoot.audioStream
            isOpaque: true
            scaleFactor: waveform.timeScale
            format: timeline.audioThumbFormat
            normalize: timeline.audioThumbNormalize
            speed: clipRoot.speed
            property int aWaveInPoint: Math.round((clipRoot.inPoint + ((index + waveform.offset) * waveform.maxWidth / scaleFactor)) * Math.abs(clipRoot.speed))
            waveInPoint: aWaveInPoint
            waveOutPoint: aWaveInPoint + Math.round(width / scaleFactor * Math.abs(clipRoot.speed))
            bgColorEven: root.thumbColor1.darker(5)
            bgColorOdd: root.thumbColor2.darker(5)
            fgColorEven: root.thumbColor1
            fgColorOdd: root.thumbColor2
            drawChannelNames: (index + waveform.offset) == 0
        }
    }
}
