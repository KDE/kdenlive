import QtQuick 2.6
import QtQuick.Controls 2.2
import Kdenlive.Controls 1.0
import QtQml.Models 2.2
import com.enums 1.0

Row {
    id: waveform
    visible: clipStatus != ClipState.VideoOnly && parentTrack.isAudio && timeline.showAudioThumbnails  && !parentTrack.isMute
    opacity: clipStatus == ClipState.Disabled ? 0.2 : 1
    property int maxWidth: 1000
    property int innerWidth: clipRoot.width - clipRoot.border.width * 2
    anchors.fill: parent
    property int scrollStart: Math.max(0, scrollView.flickableItem.contentX / timeline.scaleFactor - clipRoot.modelStart)
    property int scrollEnd: scrollStart + scrollView.viewport.width / timeline.scaleFactor

    function reload() {
        // This is needed to make the model have the correct count.
        // Model as a property expression is not working in all cases.
        if (timeline.showAudioThumbnails) {
            waveformRepeater.model = Math.ceil(waveform.innerWidth / waveform.maxWidth)
            for (var i = 0; i < waveformRepeater.count; i++) {
                waveformRepeater.itemAt(i).update();
            }
        }
    }

    Repeater {
        id: waveformRepeater
        TimelineWaveform {
            width: Math.min(waveform.innerWidth, waveform.maxWidth)
            height: waveform.height
            showItem: waveform.visible && (index * waveform.maxWidth) < waveform.scrollEnd && (index * waveform.maxWidth + width) > waveform.scrollStart
            format: timeline.audioThumbFormat
            property int channels: 2
            inPoint: Math.round((clipRoot.inPoint + (index * waveform.maxWidth / clipRoot.timeScale)) * clipRoot.speed) * channels
            outPoint: inPoint + Math.round(width / clipRoot.timeScale * clipRoot.speed) * channels
            levels: clipRoot.audioLevels
        }
    }
}
