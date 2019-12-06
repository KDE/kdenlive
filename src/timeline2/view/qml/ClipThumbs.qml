import QtQuick 2.6
import QtQuick.Controls 2.2
import QtQml.Models 2.2
import com.enums 1.0


Row {
    id: thumbRow
    anchors.fill: parent
    visible: !isAudio
    opacity: parentTrack.isAudio || parentTrack.isHidden || clipStatus == ClipState.Disabled ? 0.2 : 1
    property int thumbWidth: container.height * 16.0/9.0
    property bool enableCache: clipRoot.itemType == ProducerType.Video || clipRoot.itemType == ProducerType.AV

    function reload() {
        console.log('+++++\n\ntriggered ML thumb reload\n\n++++++++++++++')
        clipRoot.baseThumbPath = clipRoot.variableThumbs ? '' : 'image://thumbnail/' + clipRoot.binId + '/' + Math.random() + '/' + (clipRoot.isImage ? '#0' : '#')
    }

    Repeater {
        id: thumbRepeater
        // switching the model allows to have different view modes:
        // 2: will display start / end thumbs
        // container.width / thumbRow.thumbWidth will display all frames showThumbnails
        // 1: only show first thumbnail
        // 0: will disable thumbnails
        model: parentTrack.trackThumbsFormat == 0 ? 2 : parentTrack.trackThumbsFormat == 1 ? Math.ceil(container.width / thumbRow.thumbWidth) : parentTrack.trackThumbsFormat == 2 ? 1 : 0
        property int startFrame: clipRoot.inPoint
        property int endFrame: clipRoot.outPoint
        property real imageWidth: Math.max(thumbRow.thumbWidth, container.width / thumbRepeater.count)
        property int thumbStartFrame: (clipRoot.speed >= 0) ? Math.round(clipRoot.inPoint * clipRoot.speed) : Math.round((clipRoot.maxDuration - clipRoot.inPoint) * -clipRoot.speed - 1)
        property int thumbEndFrame: (clipRoot.speed >= 0) ? Math.round(clipRoot.outPoint * clipRoot.speed) : Math.round((clipRoot.maxDuration - clipRoot.outPoint) * -clipRoot.speed - 1)

        Image {
            width: thumbRepeater.imageWidth
            height: container.height
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: enableCache
            property int currentFrame: Math.floor(clipRoot.inPoint + Math.round((index) * width / timeline.scaleFactor)* clipRoot.speed)
            horizontalAlignment: thumbRepeater.count < 3 ? (index == 0 ? Image.AlignLeft : Image.AlignRight) : Image.AlignLeft
            source: thumbRepeater.count < 3 ? (index == 0 ? clipRoot.baseThumbPath + thumbRepeater.thumbStartFrame : clipRoot.baseThumbPath + thumbRepeater.thumbEndFrame) : (index * width < clipRoot.scrollStart - width || index * width > clipRoot.scrollStart + scrollView.viewport.width) ? '' : clipRoot.baseThumbPath + currentFrame
        }
    }
}
