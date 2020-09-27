import QtQuick 2.11
import QtQuick.Controls 2.4
import Kdenlive.Controls 1.0
import QtQml.Models 2.11
import com.enums 1.0

Row {
    id: waveform
    opacity: clipStatus == ClipState.Disabled ? 0.2 : 1
    property int maxWidth: 500 + 100 * timeline.scaleFactor
    anchors.fill: parent

    Timer {
        id: waveTimer
        interval: 50; running: false; repeat: false
        onTriggered: processReload()
    }

    function reload(reset) {
        if (reset == 0) {
            waveformRepeater.model = 0
        }
        waveTimer.start()
    }
    onMaxWidthChanged: {
        waveTimer.start();
    }

    function processReload() {
        // This is needed to make the model have the correct count.
        // Model as a property expression is not working in all cases.
        if (!waveform.visible || !timeline.showAudioThumbnails) {
            return;
        }
        var chunks = Math.ceil(waveform.width / waveform.maxWidth)
        if (waveformRepeater.model == undefined || chunks != waveformRepeater.model) {
            waveformRepeater.model = chunks
        }
    }

    Repeater {
        id: waveformRepeater
        TimelineWaveform {
            width: Math.min(waveform.width, waveform.maxWidth)
            height: waveform.height
            channels: clipRoot.audioChannels
            binId: clipRoot.binId
            audioStream: clipRoot.audioStream
            isFirstChunk: index == 0
            showItem: waveform.visible && (index * waveform.maxWidth < (clipRoot.scrollStart + scrollView.width)) && ((index * waveform.maxWidth + width) > clipRoot.scrollStart)
            format: timeline.audioThumbFormat
            drawInPoint: Math.max(0, clipRoot.scrollStart - (index * waveform.maxWidth))
            drawOutPoint: (clipRoot.scrollStart + scrollView.width - (index * waveform.maxWidth))
            waveInPoint: clipRoot.speed < 0 ? (Math.round((clipRoot.maxDuration - 1 - clipRoot.inPoint) * Math.abs(clipRoot.speed)  - (index * waveform.maxWidth / clipRoot.timeScale) * Math.abs(clipRoot.speed)) * channels) : (Math.round((clipRoot.inPoint + (index * waveform.maxWidth / clipRoot.timeScale)) * clipRoot.speed) * channels)
            waveOutPoint: clipRoot.speed < 0 ? (waveInPoint - Math.ceil(width / clipRoot.timeScale * Math.abs(clipRoot.speed)) * channels) : (waveInPoint + Math.round(width / clipRoot.timeScale * clipRoot.speed) * channels)
            fillColor1: root.thumbColor1
            fillColor2: root.thumbColor2
        }
    }
}
