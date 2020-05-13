import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.11
import com.enums 1.0


Row {
    id: thumbRow
    anchors.fill: parent
    visible: !isAudio
    opacity: clipStatus == ClipState.Disabled ? 0.2 : 1
    property bool fixedThumbs: clipRoot.itemType == ProducerType.Image || clipRoot.itemType == ProducerType.Text || clipRoot.itemType == ProducerType.TextTemplate
    property int thumbWidth: container.height * root.dar
    property bool enableCache: clipRoot.itemType == ProducerType.Video || clipRoot.itemType == ProducerType.AV

    function reload(reset) {
        //console.log('+++++\n\ntriggered ML thumb reload\n\n++++++++++++++')
        clipRoot.baseThumbPath = clipRoot.variableThumbs ? '' : 'image://thumbnail/' + clipRoot.binId + '/' + Math.random() + '/#'
    }

    Repeater {
        id: thumbRepeater
        // switching the model allows one to have different view modes:
        // 2: will display start / end thumbs
        // container.width / thumbRow.thumbWidth will display all frames showThumbnails
        // 1: only show first thumbnail
        // 0: will disable thumbnails
        model: parentTrack.trackThumbsFormat == 0 ? 2 : parentTrack.trackThumbsFormat == 1 ? Math.ceil(container.width / thumbRow.thumbWidth) : parentTrack.trackThumbsFormat == 2 ? 1 : 0
        property int startFrame: clipRoot.inPoint
        property int endFrame: clipRoot.outPoint
        property real imageWidth: Math.max(thumbRow.thumbWidth, container.width / thumbRepeater.count)
        property int thumbStartFrame: fixedThumbs ? 0 : (clipRoot.speed >= 0) ? Math.round(clipRoot.inPoint * clipRoot.speed) : Math.round((clipRoot.maxDuration - clipRoot.inPoint) * -clipRoot.speed - 1)
        property int thumbEndFrame: fixedThumbs ? 0 : (clipRoot.speed >= 0) ? Math.round(clipRoot.outPoint * clipRoot.speed) : Math.round((clipRoot.maxDuration - clipRoot.outPoint) * -clipRoot.speed - 1)

        Image {
            width: thumbRepeater.imageWidth
            height: container.height
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: enableCache
            property int currentFrame: fixedThumbs ? 0 : thumbRepeater.count < 3 ? (index == 0 ? thumbRepeater.thumbStartFrame : thumbRepeater.thumbEndFrame) : Math.floor(clipRoot.inPoint + Math.round((index) * width / timeline.scaleFactor)* clipRoot.speed)
            horizontalAlignment: thumbRepeater.count < 3 ? (index == 0 ? Image.AlignLeft : Image.AlignRight) : Image.AlignLeft
            source: thumbRepeater.count < 3 ? (clipRoot.baseThumbPath + currentFrame) : (index * width < clipRoot.scrollStart - width || index * width > clipRoot.scrollStart + scrollView.width) ? '' : clipRoot.baseThumbPath + currentFrame
            onStatusChanged: {
                if (thumbRepeater.count < 3) {
                    if (status === Image.Ready) {
                        thumbPlaceholder.source = source
                    }
                }
            }
            BusyIndicator {
                running: parent.status != Image.Ready
                anchors.left: parent.left
                anchors.leftMargin: index < thumbRepeater.count - 1 ? 0 : parent.width - thumbRow.thumbWidth - 1
                implicitWidth: thumbRepeater.imageWidth
                implicitHeight: container.height
                hoverEnabled: false
                visible: running
                contentItem:
                Image {
                    id: thumbPlaceholder
                    visible: parent.running
                    anchors.fill: parent
                    horizontalAlignment: Image.AlignLeft
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
            }
            Rectangle {
                visible: thumbRepeater.count < 3
                anchors.left: parent.left
                anchors.leftMargin: index == 0 ? thumbRow.thumbWidth : parent.width - thumbRow.thumbWidth - 1
                color: "#ffffff"
                opacity: 0.3
                width: 1
                height: parent.height
            }
        }
    }
}
