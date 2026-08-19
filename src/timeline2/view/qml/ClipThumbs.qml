/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick 2.15

import org.kde.kdenlive as K

Row {
    id: thumbRow
    anchors.fill: parent

    clip: true
    // On Qt 6.10.x, using Clip type creates a dependency loop between Clip and ClipThumbs.
    // When increasing the Qt dependency, we can revert to use :
    // required property Clip parentClip
    required property var parentClip
    required property int thumbsFormat
    required property real timeScale
    required property int timelineScrollViewWidth
    readonly property bool fixedThumbs: parentClip.itemType === K.ClipType.Image || parentClip.itemType === K.ClipType.Text || parentClip.itemType === K.ClipType.TextTemplate
    readonly property int thumbWidth: parent.height * K.Core.getCurrentDar()
    readonly property bool enableCache: parentClip.itemType === K.ClipType.Video || parentClip.itemType === K.ClipType.AV

    Repeater {
        id: thumbRepeater
        // switching the model allows one to have different view modes.
        // We set the model to the number of frames we want to show
        model: {
            switch (thumbRow.thumbsFormat) {
                   case 0:
                       // in/out
                       if (parent.width > thumbRow.thumbWidth) {
                           return 2 // 2: will display start / end thumbs
                       } else {
                           return 1 // 1: if the width of the container is to small, only show first thumbnail
                       }
                   case 1:
                       // All frames
                       // display as many thumbnails as can fit into the container
                       return Math.ceil(parent.width / thumbRow.thumbWidth)
                   case 2:
                       // In frame only
                       return 1 // 1: only show first thumbnail
                   case 3:
                   default:
                       // No thumbs
                       return 0 // 0: will disable thumbnails
               }
        }
        property int startFrame: thumbRow.parentClip.inPoint
        property int endFrame: thumbRow.parentClip.outPoint
        property real imageWidth: Math.max(thumbRow.thumbWidth, parent.width / thumbRepeater.count)
        property int thumbStartFrame: thumbRow.fixedThumbs ? 0 :
                                                    (thumbRow.parentClip.speed >= 0)
                                                    ? Math.round(thumbRow.parentClip.inPoint * thumbRow.parentClip.speed)
                                                    : Math.round((thumbRow.parentClip.maxDuration - thumbRow.parentClip.inPoint) * -thumbRow.parentClip.speed - 1)
        property int thumbEndFrame: thumbRow.fixedThumbs ? 0 :
                                                  (thumbRow.parentClip.speed >= 0)
                                                  ? Math.round(thumbRow.parentClip.outPoint * thumbRow.parentClip.speed)
                                                  : Math.round((thumbRow.parentClip.maxDuration - thumbRow.parentClip.outPoint) * -thumbRow.parentClip.speed - 1)

        Image {
            id: thumbImage
            required property int index
            width: thumbRepeater.imageWidth
            height: thumbRow.height
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: thumbRow.enableCache
            //sourceSize.width: width
            //sourceSize.height: height
            readonly property string thumbPath: thumbRow.parentClip.baseThumbPath + currentThumbFrame
            property int currentThumbFrame: thumbRow.fixedThumbs
                                       ? 0
                                       : thumbRepeater.count < 3
                                         ? (index === 0 ? thumbRepeater.thumbStartFrame : thumbRepeater.thumbEndFrame)
                                         : Math.floor((thumbRow.parentClip.inPoint + Math.round(index * width / thumbRow.timeScale))* thumbRow.parentClip.speed)
            horizontalAlignment: thumbRepeater.count < 3
                                 ? (index === 0 ? Image.AlignLeft : Image.AlignRight)
                                 : Image.AlignLeft
            source: thumbRepeater.count < 3 ? thumbPath
                                            : (index * width < thumbRow.parentClip.scrollStart - width || index * width > thumbRow.parentClip.scrollStart + thumbRow.timelineScrollViewWidth) ? '' : thumbPath
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
                color: thumbRow.parentClip.color.darker()
                width: 2
                height: parent.height
            }
        }
    }
}
