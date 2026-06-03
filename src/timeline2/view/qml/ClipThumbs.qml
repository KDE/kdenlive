/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15

import org.kde.kdenlive as K

Row {
    id: thumbRow
    anchors.fill: parent
    visible: !isAudio
    clip: true
    property real initialSpeed: 1
    opacity: clipState === K.PlaylistState.Disabled ? 0.2 : 1
    property bool fixedThumbs: clipRoot.itemType === K.ClipType.Image || clipRoot.itemType === K.ClipType.Text || clipRoot.itemType === K.ClipType.TextTemplate
    property int thumbWidth: container.height * K.Core.getCurrentDar()
    property bool enableCache: clipRoot.itemType === K.ClipType.Video || clipRoot.itemType === K.ClipType.AV

    Repeater {
        id: thumbRepeater
        // switching the model allows one to have different view modes.
        // We set the model to the number of frames we want to show
        model: switch (parentTrack.trackThumbsFormat) {
                   case 0:
                       // in/out
                       if (parent.width > thumbRow.thumbWidth) {
                           2 // 2: will display start / end thumbs
                       } else {
                           1 // 1: if the width of the container is to small, only show first thumbnail
                       }
                       break;
                   case 1:
                       // All frames
                       // display as many thumbnails as can fit into the container
                       Math.ceil(parent.width / thumbRow.thumbWidth)
                       break;
                   case 2:
                       // In frame only
                       1 // 1: only show first thumbnail
                       break;
                   case 3:
                   default:
                       // No thumbs
                       0 // 0: will disable thumbnails
               }
        property int startFrame: clipRoot.inPoint
        property int endFrame: clipRoot.outPoint
        property real imageWidth: Math.max(thumbRow.thumbWidth, parent.width / thumbRepeater.count)
        property int thumbStartFrame: thumbRow.fixedThumbs ? 0 :
                                                    (clipRoot.speed >= 0)
                                                    ? Math.round(clipRoot.inPoint * thumbRow.initialSpeed)
                                                    : Math.round((clipRoot.maxDuration - clipRoot.inPoint) * -thumbRow.initialSpeed - 1)
        property int thumbEndFrame: thumbRow.fixedThumbs ? 0 :
                                                  (clipRoot.speed >= 0)
                                                  ? Math.round(clipRoot.outPoint * thumbRow.initialSpeed)
                                                  : Math.round((clipRoot.maxDuration - clipRoot.outPoint) * -thumbRow.initialSpeed - 1)

        Image {
            id: thumbImage
            required property int index
            width: thumbRepeater.imageWidth
            height: container.height
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: thumbRow.enableCache
            //sourceSize.width: width
            //sourceSize.height: height
            property int currentThumbFrame: thumbRow.fixedThumbs
                                       ? 0
                                       : thumbRepeater.count < 3
                                         ? (index === 0 ? thumbRepeater.thumbStartFrame : thumbRepeater.thumbEndFrame)
                                         : Math.floor(clipRoot.inPoint * thumbRow.initialSpeed + Math.round((index) * width / timeline.scaleFactor)* clipRoot.speed)
            horizontalAlignment: thumbRepeater.count < 3
                                 ? (index === 0 ? Image.AlignLeft : Image.AlignRight)
                                 : Image.AlignLeft
            source: thumbRepeater.count < 3
                    ? (clipRoot.baseThumbPath + currentThumbFrame)
                    : (index * width < clipRoot.scrollStart - width || index * width > clipRoot.scrollStart + scrollView.width) ? '' : clipRoot.baseThumbPath + currentThumbFrame
            onStatusChanged: {
                if (status === Image.Ready && (index === 0  || index === thumbRepeater.count - 1)) {
                    thumbPlaceholder.source = source
                }
            }
            Image {
                id: thumbPlaceholder
                visible: parent.status != Image.Ready && (thumbImage.index === 0  || thumbImage.index === thumbRepeater.count - 1)
                anchors.left: parent.left
                anchors.leftMargin: thumbImage.index < thumbRepeater.count - 1 ? 0 : parent.width - thumbRow.thumbWidth - 1
                width: parent.width
                height: parent.height
                horizontalAlignment: Image.AlignLeft
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Rectangle {
                visible: thumbRepeater.count < 3
                anchors.left: parent.left
                anchors.leftMargin: thumbImage.index === 0 ? thumbRow.thumbWidth: parent.width - thumbRow.thumbWidth - 1
                color: clipRoot.color.darker()
                width: 2
                height: parent.height
            }
        }
    }
}
