import QtQuick 2.6
import QtQuick.Controls 2.2
import Kdenlive.Controls 1.0
import QtQml.Models 2.2
import com.enums 1.0

Row {
    id: waveform
    visible: clipStatus != ClipState.VideoOnly && parentTrack.isAudio && !parentTrack.isMute
    opacity: clipStatus == ClipState.Disabled ? 0.2 : 1
    property int maxWidth: 1000
    anchors.fill: parent
    property int scrollStart: scrollView.flickableItem.contentX - clipRoot.modelStart * timeline.scaleFactor
    property int scrollEnd: scrollStart + scrollView.viewport.width
    property int scrollMin: scrollView.flickableItem.contentX / timeline.scaleFactor
    property int scrollMax: scrollMin + scrollView.viewport.width / timeline.scaleFactor

    Timer {
        id: waveTimer
        interval: 50; running: false; repeat: false
        onTriggered: processReload()
    }

    onScrollStartChanged: {
        waveTimer.start()
    }

    function reload() {
        waveTimer.start()
    }

    function processReload() {
        // This is needed to make the model have the correct count.
        // Model as a property expression is not working in all cases.
        if (!waveform.visible || !timeline.showAudioThumbnails || (waveform.scrollMin > clipRoot.modelStart + clipRoot.clipDuration) || (clipRoot.modelStart > waveform.scrollMax)) {
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
            isFirstChunk: index == 0
            showItem: waveform.visible && (index * width) < waveform.scrollEnd && (index * width + width) > waveform.scrollStart
            format: timeline.audioThumbFormat
            waveInPoint: clipRoot.speed < 0 ? (Math.round(clipRoot.outPoint - (index * waveform.maxWidth / clipRoot.timeScale) * Math.abs(clipRoot.speed)) * channels) : (Math.round(clipRoot.inPoint + (index * waveform.maxWidth / clipRoot.timeScale) * clipRoot.speed) * channels)
            waveOutPoint: clipRoot.speed < 0 ? (waveInPoint - Math.ceil(width / clipRoot.timeScale * Math.abs(clipRoot.speed)) * channels) : (waveInPoint + Math.round(width / clipRoot.timeScale * clipRoot.speed) * channels)
            fillColor: activePalette.text
            onShowItemChanged: {
                if (showItem) {
                    update();
                }
            }
        }
    }
}
