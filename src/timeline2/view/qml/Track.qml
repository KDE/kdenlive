/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQml.Models 2.15

import org.kde.kdenlive as Kdenlive

Item{
    id: trackRoot
    property alias trackModel: trackModel.model
    property alias rootIndex : trackModel.rootIndex
    property bool isAudio
    property bool isLocked: false
    property int trackInternalId : -42
    property int trackThumbsFormat
    property int itemType: 0
    property var effectZones
    opacity: model.disabled ? 0.4 : 1

    function clipAt(index) {
        return repeater.itemAt(index)
    }

    function isClip(type) {
        return type != Kdenlive.ClipType.Composition && type != Kdenlive.ClipType.Track;
    }

    function clipTrimming(clip, newDuration, shiftTrim, controlTrim, right) {
        if (root.activeTool === Kdenlive.ToolType.SelectTool && controlTrim) {
            if (!speedController.visible) {
                // Store original speed
                speedController.originalSpeed = clip.speed
            }
            speedController.x = clip.x + clip.border.width
            newDuration = controller.requestItemSpeedChange(clip.clipId, newDuration, right, root.snapping)
            if (!right) {
                clip.x += clip.width - (newDuration * root.timeScale)
            }
            clip.width = newDuration * root.timeScale
            speedController.width = Math.max(0, clip.width - 2 * clip.border.width)
            speedController.lastValidDuration = newDuration
            clip.speed = clip.originalDuration * speedController.originalSpeed / newDuration
            speedController.visible = true
            speedController.updatedSpeed = Math.round(clip.speed*100)
            speedController.resizeRight = right
            // var delta = newDuration - clip.originalDuration
            // var s = timeline.simplifiedTC(Math.abs(delta))
            let s = '%1:%2\%, %3:%4'.arg(i18n("Speed"))
                .arg(Math.round(speedController.updatedSpeed))
                .arg(i18n("Duration"))
                .arg(timeline.simplifiedTC(newDuration))
            timeline.showToolTip(s)
            return
        }
        var new_duration = 0;
        if (root.activeTool === Kdenlive.ToolType.RippleTool) {
            console.log("Trimming request for " + newDuration + " right: " + right)
            new_duration = timeline.requestItemRippleResize(clip.clipId, newDuration, right, false, root.snapping, shiftTrim)
            timeline.requestStartTrimmingMode(clip.clipId, false, right);
            timeline.ripplePosChanged(new_duration, right);
        } else {
            new_duration = controller.requestItemResize(clip.clipId, newDuration, right, false, root.snapping, shiftTrim)
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
                    .arg(i18n("Duration"))
                    .arg(timeline.simplifiedTC(new_duration))
            } else {
                let delta = new_duration - clip.originalDuration
                s = '%1%2, %3:%4, %5:%6'.arg((delta <= 0)? '+' : '-')
                    .arg(delta)
                    .arg(i18n("In"))
                    .arg(timeline.simplifiedTC(clip.inPoint))
                    .arg(i18n("Duration"))
                    .arg(timeline.simplifiedTC(new_duration))
            }
            timeline.showToolTip(s);
        }
    }

    function trimedClip(clip, shiftTrim, controlTrim, right) {
        //bubbleHelp.hide()
        timeline.showToolTip();
        if (shiftTrim || (root.groupTrimData == undefined/*TODO > */ || root.activeTool === Kdenlive.ToolType.RippleTool /* < TODO*/) || controlTrim) {
            // We only resize one element
            if (root.activeTool === Kdenlive.ToolType.RippleTool) {
                timeline.requestItemRippleResize(clip.clipId, clip.originalDuration, right, false, 0, shiftTrim)
            } else {
                controller.requestItemResize(clip.clipId, clip.originalDuration, right, false, 0, shiftTrim)
            }

            if (root.activeTool === Kdenlive.ToolType.SelectTool && controlTrim) {
                // Update speed
                speedController.visible = false
                controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, right, root.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                speedController.originalSpeed = 1
            } else {
                if (root.activeTool === Kdenlive.ToolType.RippleTool) {
                    timeline.requestItemRippleResize(clip.clipId, clip.lastValidDuration, right, true, 0, shiftTrim)
                    timeline.requestEndTrimmingMode();
                } else {
                    controller.requestItemResize(clip.clipId, clip.lastValidDuration, right, true, 0, shiftTrim)
                }
            }
        } else {
            var updatedGroupData = controller.getGroupData(clip.clipId)
            controller.processGroupResize(root.groupTrimData, updatedGroupData, right)
        }
        root.groupTrimData = undefined
    }

    width: clipRow.width

    DelegateModel {
        id: trackModel
        delegate: Item {
            property var itemModel : model
            property bool clipItem: isClip(model.clipType)
            function calculateZIndex() {
                // Z order indicates the items that will be drawn on top.
                if (model.clipType == Kdenlive.ClipType.Composition) {
                    // Compositions should be top, then clips
                    return 50000;
                }

                if (model.mixDuration > 0) {
                    // Clips with mix should be ordered related to their position so that the right clip of a clip mix is always on top (the mix UI is drawn over the right clip)
                    return Math.round(model.start / 25) + 1;
                }

                if (root.activeTool === Kdenlive.ToolType.SlipTool && model.selected) {
                    return model.item === timeline.trimmingMainClip ? 2 : 1;
                }

                if (root.activeTool === Kdenlive.ToolType.RippleTool && model.item === timeline.trimmingMainClip) {
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
                    value: root.timeScale
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
                    when: loader.status == Loader.Ready && model.clipType == Kdenlive.ClipType.Composition
                }
                Binding {
                    target: loader.item
                    property: "trackHeight"
                    value: root.trackHeight
                    when: loader.status == Loader.Ready && model.clipType == Kdenlive.ClipType.Composition
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
                    } else if (model.clipType == Kdenlive.ClipType.Composition) {
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
                    } else if (model.clipType == Kdenlive.ClipType.Composition) {
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
            onInitGroupTrim: clipId => {
                // We are resizing a group, remember coordinates of all elements
                root.groupTrimData = controller.getGroupData(clipId)
            }
            onTrimmingIn: (clip, newDuration, shiftTrim, controlTrim) => { clipTrimming(clip, newDuration, shiftTrim, controlTrim, false) }
            onTrimmedIn: (clip, shiftTrim, controlTrim) => { trackRoot.trimedClip(clip, shiftTrim, controlTrim, false) }
            onTrimmingOut: (clip, newDuration, shiftTrim, controlTrim) => { clipTrimming(clip, newDuration, shiftTrim, controlTrim, true) }
            onTrimmedOut: (clip, shiftTrim, controlTrim) => { trackRoot.trimedClip(clip, shiftTrim, controlTrim, true) }
        }
    }
    Component {
        id: compositionDelegate
        Composition {
            displayHeight: Math.max(trackRoot.height / 2, trackRoot.height - (root.baseUnit * 2))
            opacity: 0.8
            selected: root.timelineSelection.indexOf(clipId) != -1
            onTrimmingIn: (clip, newDuration) => {
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, false, false, root.snapping)
                if (new_duration > 0) {
                    clip.lastValidDuration = newDuration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = clip.originalDuration - new_duration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = i18n("%1%2, Duration = %3", ((delta <= 0)? '+' : '-')
                        , s, timeline.simplifiedTC(new_duration))
                    timeline.showToolTip(s)
                }
            }
            onTrimmedIn: clip => {
                timeline.showToolTip()
                //bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, root.snapping)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, root.snapping)
            }
            onTrimmingOut: (clip, newDuration) => {
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, true, false, root.snapping)
                if (new_duration > 0) {
                    clip.lastValidDuration = newDuration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = clip.originalDuration - new_duration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = i18n("%1%2, Duration = %3", ((delta <= 0)? '+' : '-')
                        , s, timeline.simplifiedTC(new_duration))
                    timeline.showToolTip(s)
                }
            }
            onTrimmedOut: clip => {
                timeline.showToolTip()
                //bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, true, false, root.snapping)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, true, true, root.snapping)
            }
        }
    }
    Rectangle {
        id: speedController
        anchors.bottom: parent.bottom
        color: activePalette.highlight //'#cccc0000'
        visible: false
        clip: true
        height: root.baseUnit * 1.5
        property int lastValidDuration: 0
        property real originalSpeed: 1
        property real updatedSpeed: 100
        property bool resizeRight: true
        Text {
            id: speedLabel
            text: i18n("%1% | Adjusting speed", speedController.updatedSpeed)
            font: miniFont
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
        model: effectZones
        Rectangle {
            x: effectZones[index].x * timeline.scaleFactor
            height: 2
            width: (effectZones[index].y - effectZones[index].x) * timeline.scaleFactor
            color: 'blueviolet'
            opacity: 1
            anchors.top: parent.top
        }
    }
}
