/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQml.Models 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: trackRoot
    required property K.TimelineController timeline
    required property K.TimelineItemModel controller
    required property int snapping
    required property bool isDisabled
    required property bool isAudio
    required property bool isLocked
    required property int trackInternalId
    required property int trackThumbsFormat
    required property var effectZones
    required property bool isPanning

    signal blockAutoScroll(bool enabled)

    property alias trackModel: trackModel.model
    property alias rootIndex : trackModel.rootIndex

    property int itemType: 0

    opacity: isDisabled ? 0.4 : 1

    SystemPalette { id: activePalette }

    function clipAt(index) {
        return repeater.itemAt(index)
    }

    function isClip(type) {
        return type != K.ClipType.Composition && type != K.ClipType.Track;
    }

    function clipTrimming(clip, newDuration, shiftTrim, controlTrim, right) {
        if (K.Core.activeTool === K.ToolType.SelectTool && controlTrim) {
            if (!speedController.visible) {
                // Store original speed
                speedController.originalSpeed = clip.speed
            }
            speedController.x = clip.x + clip.border.width
            newDuration = trackRoot.controller.requestItemSpeedChange(clip.clipId, newDuration, right, trackRoot.snapping)
            if (!right) {
                clip.x += clip.width - (newDuration * trackRoot.timeline.scaleFactor)
            }
            clip.width = newDuration * trackRoot.timeline.scaleFactor
            speedController.width = Math.max(0, clip.width - 2 * clip.border.width)
            speedController.lastValidDuration = newDuration
            clip.speed = clip.originalDuration * speedController.originalSpeed / newDuration
            speedController.visible = true
            speedController.updatedSpeed = Math.round(clip.speed*100)
            speedController.resizeRight = right
            // var delta = newDuration - clip.originalDuration
            // var s = trackRoot.timeline.simplifiedTC(Math.abs(delta))
            let s = '%1:%2\%, %3:%4'.arg(KI18n.i18n("Speed"))
                .arg(Math.round(speedController.updatedSpeed))
                .arg(KI18n.i18n("Duration"))
                .arg(trackRoot.timeline.simplifiedTC(newDuration))
            trackRoot.timeline.showToolTip(s)
            return
        }
        var new_duration = 0;
        if (K.Core.activeTool === K.ToolType.RippleTool) {
            console.log("Trimming request for " + newDuration + " right: " + right)
            new_duration = trackRoot.timeline.requestItemRippleResize(clip.clipId, newDuration, right, false, trackRoot.snapping, shiftTrim)
            trackRoot.timeline.requestStartTrimmingMode(clip.clipId, false, right);
            trackRoot.timeline.ripplePosChanged(new_duration, right);
        } else {
            new_duration = trackRoot.controller.requestItemResize(clip.clipId, newDuration, right, false, trackRoot.snapping, shiftTrim)
        }
        if (new_duration > 0) {
            clip.lastValidDuration = new_duration
            if (!right) {
                clip.originalX = clip.draggedX
            }

            // Show amount trimmed as a time in a "bubble" help.
            let s;
            if (right) {
                let delta = clip.originalDuration - new_duration
                s = '%1%2, %3:%4'.arg((delta <= 0)? '+' : '-')
                    .arg(delta)
                    .arg(KI18n.i18n("Duration"))
                    .arg(trackRoot.timeline.simplifiedTC(new_duration))
            } else {
                let delta = new_duration - clip.originalDuration
                s = '%1%2, %3:%4, %5:%6'.arg((delta <= 0)? '+' : '-')
                    .arg(delta)
                    .arg(KI18n.i18n("In"))
                    .arg(trackRoot.timeline.simplifiedTC(clip.inPoint))
                    .arg(KI18n.i18n("Duration"))
                    .arg(trackRoot.timeline.simplifiedTC(new_duration))
            }
            trackRoot.timeline.showToolTip(s);
        }
    }

    function trimedClip(clip, shiftTrim, controlTrim, right) {
        //bubbleHelp.hide()
        trackRoot.timeline.showToolTip();
        if (shiftTrim || (root.groupTrimData == undefined/*TODO > */ || K.Core.activeTool === K.ToolType.RippleTool /* < TODO*/) || controlTrim) {
            // We only resize one element
            if (K.Core.activeTool === K.ToolType.RippleTool) {
                trackRoot.timeline.requestItemRippleResize(clip.clipId, clip.originalDuration, right, false, 0, shiftTrim)
            } else {
                trackRoot.controller.requestItemResize(clip.clipId, clip.originalDuration, right, false, 0, shiftTrim)
            }

            if (K.Core.activeTool === K.ToolType.SelectTool && controlTrim) {
                // Update speed
                speedController.visible = false
                trackRoot.controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, right, trackRoot.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                speedController.originalSpeed = 1
            } else {
                if (K.Core.activeTool === K.ToolType.RippleTool) {
                    trackRoot.timeline.requestItemRippleResize(clip.clipId, clip.lastValidDuration, right, true, 0, shiftTrim)
                    trackRoot.timeline.requestEndTrimmingMode();
                } else {
                    trackRoot.controller.requestItemResize(clip.clipId, clip.lastValidDuration, right, true, 0, shiftTrim)
                }
            }
        } else {
            var updatedGroupData = trackRoot.controller.getGroupData(clip.clipId)
            trackRoot.controller.processGroupResize(root.groupTrimData, updatedGroupData, right)
        }
        root.groupTrimData = undefined
    }

    width: clipRow.width

    DelegateModel {
        id: trackModel
        delegate: Item {
            id: itemOnTrack
            required property var model
            function calculateZIndex() {
                // Z order indicates the items that will be drawn on top.
                if (model.clipType == K.ClipType.Composition) {
                    // Compositions should be top, then clips
                    return 50000
                }

                if (model.mixDuration > 0) {
                    // Clips with mix should be ordered related to their position so that the right clip of a clip mix is always on top (the mix UI is drawn over the right clip)
                    return Math.round(model.start / 25) + 1;
                }

                if (K.Core.activeTool === K.ToolType.SlipTool && model.selected) {
                    return model.item === trackRoot.timeline.trimmingMainClip ? 2 : 1;
                }

                if (K.Core.activeTool === K.ToolType.RippleTool && model.item === trackRoot.timeline.trimmingMainClip) {
                    return 1;
                }
                return 0;
            }
            z: calculateZIndex()
            Loader {
                id: clipLoader
                active: trackRoot.isClip(itemOnTrack.model.clipType)
                sourceComponent: Clip {
                    height: trackRoot.height

                    parentTrack: trackRoot
                    timeline: trackRoot.timeline
                    controller: trackRoot.controller

                    clipId: itemOnTrack.model.item
                    speed: itemOnTrack.model.speed
                    timeScale: trackRoot.timeline.scaleFactor
                    fakeTid: itemOnTrack.model.fakeTrackId
                    tagColor: itemOnTrack.model.tag
                    fakePosition: itemOnTrack.model.fakePosition
                    mixDuration: itemOnTrack.model.mixDuration
                    mixCut: itemOnTrack.model.mixCut
                    mixEndDuration: itemOnTrack.model.mixEndDuration
                    selected: itemOnTrack.model.selected
                    mltService: itemOnTrack.model.mlt_service
                    modelStart: itemOnTrack.model.start
                    fadeIn: itemOnTrack.model.fadeIn
                    fadeInMethod: itemOnTrack.model.fadeInMethod
                    positionOffset: itemOnTrack.model.positionOffset
                    effectNames: itemOnTrack.model.effectNames
                    isStackEnabled: itemOnTrack.model.isStackEnabled
                    clipStatus: itemOnTrack.model.clipStatus
                    fadeOut: itemOnTrack.model.fadeOut
                    fadeOutMethod: itemOnTrack.model.fadeOutMethod
                    showKeyframes: itemOnTrack.model.showKeyframes
                    isGrabbed: itemOnTrack.model.isGrabbed
                    keyframeModel: itemOnTrack.model.keyframeModel
                    clipDuration: itemOnTrack.model.duration
                    inPoint: itemOnTrack.model.in
                    outPoint: itemOnTrack.model.out
                    grouped: itemOnTrack.model.grouped
                    clipName: itemOnTrack.model.name
                    clipResource: itemOnTrack.model.resource
                    clipState: itemOnTrack.model.clipState
                    maxDuration: itemOnTrack.model.maxDuration
                    clipThumbId: itemOnTrack.model.clipThumbId
                    forceReloadAudioThumb: itemOnTrack.model.reloadAudioThumb
                    binId: itemOnTrack.model.binId
                    timeremap: itemOnTrack.model.timeremap
                    audioChannels: itemOnTrack.model.audioChannels
                    audioStream: itemOnTrack.model.audioStream
                    multiStream: itemOnTrack.model.multiStream
                    aStreamIndex: itemOnTrack.model.audioStreamIndex
                    isAudio: itemOnTrack.model.audio
                    markers: itemOnTrack.model.markers
                    hasAudio: itemOnTrack.model.hasAudio
                    canBeAudio: itemOnTrack.model.canBeAudio
                    canBeVideo: itemOnTrack.model.canBeVideo
                    itemType: itemOnTrack.model.clipType
                    trackId: itemOnTrack.model.trackId
                    isPanning: trackRoot.isPanning

                    onInitGroupTrim: clipId => {
                        // We are resizing a group, remember coordinates of all elements
                        root.groupTrimData = trackRoot.controller.getGroupData(clipId)
                    }
                    onTrimmingIn: (clip, newDuration, shiftTrim, controlTrim) => { trackRoot.clipTrimming(clip, newDuration, shiftTrim, controlTrim, false) }
                    onTrimmedIn: (clip, shiftTrim, controlTrim) => { trackRoot.trimedClip(clip, shiftTrim, controlTrim, false) }
                    onTrimmingOut: (clip, newDuration, shiftTrim, controlTrim) => { trackRoot.clipTrimming(clip, newDuration, shiftTrim, controlTrim, true) }
                    onTrimmedOut: (clip, shiftTrim, controlTrim) => { trackRoot.trimedClip(clip, shiftTrim, controlTrim, true) }
                    onIsUserInteractingChanged: { trackRoot.blockAutoScroll(isUserInteracting) }
                    onTrimInProgressChanged: { root.trimInProgress = trimInProgress }
                }
                onLoaded: {
                    console.log('loaded clip: ', itemOnTrack.model.start, ', ID: ', itemOnTrack.model.item, ', index: ', trackRoot.DelegateModel.itemsIndex,', TYPE:', itemOnTrack.model.clipType)
                    console.log('loaded clip with Astream: ', itemOnTrack.model.audioStream)
                }
            }
            Loader {
                id: compositionLoader
                active: itemOnTrack.model.clipType == K.ClipType.Composition
                sourceComponent: Composition {
                    opacity: 0.8

                    parentTrack: trackRoot
                    timeScale: trackRoot.timeline.scaleFactor
                    displayHeight: Math.max(trackRoot.height / 2, trackRoot.height - (K.UiUtils.baseSizeMedium * 2))

                    timeline: trackRoot.timeline
                    controller: trackRoot.controller

                    clipId: itemOnTrack.model.item
                    trackId: itemOnTrack.model.trackId
                    fakeTid: itemOnTrack.model.fakeTrackId
                    fakePosition: itemOnTrack.model.fakePosition
                    mltService: itemOnTrack.model.mlt_service
                    modelStart: itemOnTrack.model.start
                    showKeyframes: itemOnTrack.model.showKeyframes
                    isGrabbed: itemOnTrack.model.isGrabbed
                    keyframeModel: itemOnTrack.model.keyframeModel
                    aTrack: itemOnTrack.model.a_track
                    visible: !itemOnTrack.model.hideItem
                    clipDuration: itemOnTrack.model.duration
                    inPoint: itemOnTrack.model.in
                    outPoint: itemOnTrack.model.out
                    grouped: itemOnTrack.model.grouped
                    clipName: itemOnTrack.model.name
                    selected: itemOnTrack.model.selected
                    isPanning: trackRoot.isPanning

                    onTrimmingIn: (clip, newDuration) => {
                        var new_duration = trackRoot.controller.requestItemResize(clip.clipId, newDuration, false, false, trackRoot.snapping)
                        if (new_duration > 0) {
                            clip.lastValidDuration = newDuration
                            clip.originalX = clip.draggedX
                            // Show amount trimmed as a time in a "bubble" help.
                            var delta = clip.originalDuration - new_duration
                            var s = trackRoot.timeline.simplifiedTC(Math.abs(delta))
                            s = KI18n.i18n("%1%2, Duration = %3", ((delta <= 0)? '+' : '-')
                                , s, trackRoot.timeline.simplifiedTC(new_duration))
                            trackRoot.timeline.showToolTip(s)
                        }
                    }
                    onTrimmedIn: clip => {
                        trackRoot.timeline.showToolTip()
                        //bubbleHelp.hide()
                        trackRoot.controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, trackRoot.snapping)
                        trackRoot.controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, trackRoot.snapping)
                    }
                    onTrimmingOut: (clip, newDuration) => {
                        var new_duration = trackRoot.controller.requestItemResize(clip.clipId, newDuration, true, false, trackRoot.snapping)
                        if (new_duration > 0) {
                            clip.lastValidDuration = newDuration
                            // Show amount trimmed as a time in a "bubble" help.
                            var delta = clip.originalDuration - new_duration
                            var s = trackRoot.timeline.simplifiedTC(Math.abs(delta))
                            s = KI18n.i18n("%1%2, Duration = %3", ((delta <= 0)? '+' : '-')
                                , s, trackRoot.timeline.simplifiedTC(new_duration))
                            trackRoot.timeline.showToolTip(s)
                        }
                    }
                    onTrimmedOut: clip => {
                        trackRoot.timeline.showToolTip()
                        //bubbleHelp.hide()
                        trackRoot.controller.requestItemResize(clip.clipId, clip.originalDuration, true, false, trackRoot.snapping)
                        trackRoot.controller.requestItemResize(clip.clipId, clip.lastValidDuration, true, true, trackRoot.snapping)
                    }

                    onIsUserInteractingChanged: { trackRoot.blockAutoScroll(isUserInteracting) }
                    onTrimInProgressChanged: { root.trimInProgress = trimInProgress }
                }
                onLoaded: {
                    console.log('loaded composition: ', itemOnTrack.model.start, ', ID: ', itemOnTrack.model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                }
            }
        }
    }

    Item {
        id: clipRow
        height: trackRoot.height
        Repeater { id: repeater; model: trackModel }
    }

    Rectangle {
        id: speedController
        anchors.bottom: parent.bottom
        color: activePalette.highlight //'#cccc0000'
        visible: false
        clip: true
        height: K.UiUtils.baseSizeMedium * 1.5
        property int lastValidDuration: 0
        property real originalSpeed: 1
        property real updatedSpeed: 100
        property bool resizeRight: true
        Text {
            id: speedLabel
            text: KI18n.i18n("%1% | Adjusting speed", speedController.updatedSpeed)
            font: K.UiUtils.smallestReadableFont
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: speedController.resizeRight ? (speedLabel.implicitWidth > speedController.width ? Text.AlignLeft : Text.AlignRight) : Text.AlignLeft
            color: activePalette.highlightedText
        }
        transitions: [ Transition {
            NumberAnimation { property: "opacity"; duration: 300}
        } ]
    }
    Repeater {
        model: trackRoot.effectZones
        Rectangle {
            required property int index
            x: trackRoot.effectZones[index].x * trackRoot.timeline.scaleFactor
            height: 2
            width: (trackRoot.effectZones[index].y - trackRoot.effectZones[index].x) * trackRoot.timeline.scaleFactor
            color: 'blueviolet'
            opacity: 1
            anchors.top: parent.top
        }
    }
}
