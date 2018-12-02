import QtQuick 2.6
import QtQuick.Controls 2.2
import Kdenlive.Controls 1.0
import QtQml.Models 2.2
import com.enums 1.0

Row {
    id: waveform
    visible: clipStatus != ClipState.VideoOnly && parentTrack.isAudio && timeline.showAudioThumbnails  && !parentTrack.isMute
    opacity: clipStatus == ClipState.Disabled ? 0.2 : 1
    property int maxWidth: 1500
    property int innerWidth: clipRoot.width - clipRoot.border.width * 2
    anchors.fill: parent
    property int scrollStart: scrollView.flickableItem.contentX - clipRoot.modelStart * timeline.scaleFactor
    property int scrollEnd: scrollStart + scrollView.viewport.width
    property int scrollMin: scrollView.flickableItem.contentX / timeline.scaleFactor
    property int scrollMax: scrollMin + scrollView.viewport.width / timeline.scaleFactor

    onScrollStartChanged: {
        reload()
    }

    function reload() {
        // This is needed to make the model have the correct count.
        // Model as a property expression is not working in all cases.
            if (!timeline.showAudioThumbnails || (waveform.scrollMin > clipRoot.modelStart + clipRoot.clipDuration) || (clipRoot.modelStart > waveform.scrollMax) || clipRoot.audioLevels == '') {
                return;
            }
            //var t0 = new Date();
            waveformRepeater.model = Math.ceil(waveform.innerWidth / waveform.maxWidth)
            var firstWaveRepeater = Math.max(0, Math.floor((waveform.scrollMin - clipRoot.modelStart) / (waveform.maxWidth / timeline.scaleFactor)))
            var lastWaveRepeater = Math.min(waveformRepeater.count - 1, firstWaveRepeater + Math.ceil((waveform.scrollMax - waveform.scrollMin) / (waveform.maxWidth / timeline.scaleFactor)))
            for (var i = firstWaveRepeater; i <= lastWaveRepeater; i++) {
                waveformRepeater.itemAt(i).update();
            }
            /*var t1 = new Date();
            console.log("Took: " + (t1.valueOf() - t0.valueOf()) + " millisecs for " + waveformRepeater.count + " iterations");*/
    }

    Repeater {
        id: waveformRepeater
        TimelineWaveform {
            width: Math.min(waveform.innerWidth, waveform.maxWidth)
            height: waveform.height
            channels: clipRoot.audioChannels
            showItem: waveform.visible && (index * width) < waveform.scrollEnd && (index * width + width) > waveform.scrollStart
            format: timeline.audioThumbFormat
            inPoint: Math.round((clipRoot.inPoint + (index * waveform.maxWidth / clipRoot.timeScale)) * clipRoot.speed) * channels
            outPoint: inPoint + Math.round(width / clipRoot.timeScale * clipRoot.speed) * channels
            levels: clipRoot.audioLevels
        }
    }
}
