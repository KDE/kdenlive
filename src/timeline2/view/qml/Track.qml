/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQml.Models 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: trackRoot
    required property K.TimelineController timeline
    required property K.TimelineItemModel controller
    required property var activeTool
    required property bool snapping
    required property bool isDisabled
    required property bool isAudio
    required property bool isLocked
    required property int trackInternalId
    required property int trackThumbsFormat
    required property var effectZones

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
        if (trackRoot.activeTool === K.ToolType.SelectTool && controlTrim) {
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
        if (trackRoot.activeTool === K.ToolType.RippleTool) {
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
        if (shiftTrim || (root.groupTrimData == undefined/*TODO > */ || trackRoot.activeTool === K.ToolType.RippleTool /* < TODO*/) || controlTrim) {
            // We only resize one element
            if (trackRoot.activeTool === K.ToolType.RippleTool) {
                trackRoot.timeline.requestItemRippleResize(clip.clipId, clip.originalDuration, right, false, 0, shiftTrim)
            } else {
                trackRoot.controller.requestItemResize(clip.clipId, clip.originalDuration, right, false, 0, shiftTrim)
            }

            if (trackRoot.activeTool === K.ToolType.SelectTool && controlTrim) {
                // Update speed
                speedController.visible = false
                trackRoot.controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, right, trackRoot.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                speedController.originalSpeed = 1
            } else {
                if (trackRoot.activeTool === K.ToolType.RippleTool) {
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
            required property var model
            property var itemModel : model
            property bool clipItem: trackRoot.isClip(model.clipType)
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

                if (trackRoot.activeTool === K.ToolType.SlipTool && model.selected) {
                    return model.item === trackRoot.timeline.trimmingMainClip ? 2 : 1;
                }

                if (trackRoot.activeTool === K.ToolType.RippleTool && model.item === trackRoot.timeline.trimmingMainClip) {
                    return 1;
                }
                return 0;
            }
            z: calculateZIndex()
            Loader {
                id: loader
                Binding {
                    target: loader.item
                    property: "speed"
                    value: model.speed
                    when: loader.status == Loader.Ready && loader.item && clipItem
                }
                Binding {
                    target: loader.item
                    property: "timeScale"
                    value: trackRoot.timeline.scaleFactor
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "fakeTid"
                    value: model.fakeTrackId
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "tagColor"
                    value: model.tag
                    when: loader.status == Loader.Ready && loader.item && clipItem
                }
                Binding {
                    target: loader.item
                    property: "fakePosition"
                    value: model.fakePosition
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "mixDuration"
                    value: model.mixDuration
                    when: loader.status == Loader.Ready && loader.item && clipItem
                }
                Binding {
                    target: loader.item
                    property: "mixCut"
                    value: model.mixCut
                    when: loader.status == Loader.Ready && loader.item && clipItem
                }
                Binding {
                    target: loader.item
                    property: "mixEndDuration"
                    value: model.mixEndDuration
                    when: loader.status == Loader.Ready && loader.item && clipItem
                }
                Binding {
                    target: loader.item
                    property: "selected"
                    value: model.selected
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "mltService"
                    value: model.mlt_service
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "modelStart"
                    value: model.start
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "fadeIn"
                    value: model.fadeIn
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "fadeInMethod"
                    value: model.fadeInMethod
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "positionOffset"
                    value: model.positionOffset
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "effectNames"
                    value: model.effectNames
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "isStackEnabled"
                    value: model.isStackEnabled
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "clipStatus"
                    value: model.clipStatus
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "fadeOut"
                    value: model.fadeOut
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "fadeOutMethod"
                    value: model.fadeOutMethod
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "showKeyframes"
                    value: model.showKeyframes
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "isGrabbed"
                    value: model.isGrabbed
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "keyframeModel"
                    value: model.keyframeModel
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "aTrack"
                    value: model.a_track
                    when: loader.status == Loader.Ready && model.clipType === K.ClipType.Composition
                }
                Binding {
                    target: loader.item
                    property: "visible"
                    value: !model.hideItem
                    when: loader.status == Loader.Ready && model.clipType === K.ClipType.Composition
                }
                Binding {
                    target: loader.item
                    property: "trackHeight"
                    value: root.trackHeight
                    when: loader.status == Loader.Ready && model.clipType == K.ClipType.Composition
                }
                Binding {
                    target: loader.item
                    property: "clipDuration"
                    value: model.duration
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "inPoint"
                    value: model.in
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "outPoint"
                    value: model.out
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "grouped"
                    value: model.grouped
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "clipName"
                    value: model.name
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "clipResource"
                    value: model.resource
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "clipState"
                    value: model.clipState
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "maxDuration"
                    value: model.maxDuration
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "clipThumbId"
                    value: model.clipThumbId
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "forceReloadAudioThumb"
                    value: model.reloadAudioThumb
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "binId"
                    value: model.binId
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "timeremap"
                    value: model.timeremap
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "audioChannels"
                    value: model.audioChannels
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "audioStream"
                    value: model.audioStream
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "multiStream"
                    value: model.multiStream
                    when: loader.status == Loader.Ready && clipItem
                }
                Binding {
                    target: loader.item
                    property: "aStreamIndex"
                    value: model.audioStreamIndex
                    when: loader.status == Loader.Ready && clipItem
                }
                sourceComponent: {
                    if (clipItem) {
                        return clipDelegate
                    } else if (model.clipType == K.ClipType.Composition) {
                        return compositionDelegate
                    } else {
                        // Track
                        return undefined
                    }
                }
                onLoaded: {
                    item.clipId = model.item
                    item.parentTrack = trackRoot
                    if (clipItem) {
                        console.log('loaded clip: ', model.start, ', ID: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex,', TYPE:', model.clipType)
                        item.isAudio= model.audio
                        item.markers= model.markers
                        item.hasAudio = model.hasAudio
                        item.canBeAudio = model.canBeAudio
                        item.canBeVideo = model.canBeVideo
                        item.itemType = model.clipType
                        console.log('loaded clip with Astream: ', model.audioStream)                       
                    } else if (model.clipType == K.ClipType.Composition) {
                        console.log('loaded composition: ', model.start, ', ID: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                        //item.aTrack = model.a_track
                    } else {
                        console.log('loaded unwanted element: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                    }
                    item.trackId = model.trackId
                    //item.selected= trackRoot.selection.indexOf(item.clipId) != -1
                    //console.log(width, height);
                }
            }
        }
    }

    Item {
        id: clipRow
        height: trackRoot.height
        Repeater { id: repeater; model: trackModel }
    }

    Component {
        id: clipDelegate
        Clip {
            height: trackRoot.height
            timeline: trackRoot.timeline
            controller: trackRoot.controller
            onInitGroupTrim: clipId => {
                // We are resizing a group, remember coordinates of all elements
                root.groupTrimData = trackRoot.controller.getGroupData(clipId)
            }
            onTrimmingIn: (clip, newDuration, shiftTrim, controlTrim) => { trackRoot.clipTrimming(clip, newDuration, shiftTrim, controlTrim, false) }
            onTrimmedIn: (clip, shiftTrim, controlTrim) => { trackRoot.trimedClip(clip, shiftTrim, controlTrim, false) }
            onTrimmingOut: (clip, newDuration, shiftTrim, controlTrim) => { trackRoot.clipTrimming(clip, newDuration, shiftTrim, controlTrim, true) }
            onTrimmedOut: (clip, shiftTrim, controlTrim) => { trackRoot.trimedClip(clip, shiftTrim, controlTrim, true) }
        }
    }
    Component {
        id: compositionDelegate
        Composition {
            displayHeight: Math.max(trackRoot.height / 2, trackRoot.height - (K.UiUtils.baseSizeMedium * 2))
            opacity: 0.8
            selected: root.timelineSelection.indexOf(clipId) != -1
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
        }
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
