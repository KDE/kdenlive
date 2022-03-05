/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.11
import QtQml.Models 2.11
import com.enums 1.0

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
        return type != ProducerType.Composition && type != ProducerType.Track;
    }

    width: clipRow.width

    DelegateModel {
        id: trackModel
        delegate: Item {
            property var itemModel : model
            property bool clipItem: isClip(model.clipType)
            function calculateZIndex() {
                // Z order indicates the items that will be drawn on top.
                if (model.clipType == ProducerType.Composition) {
                    // Compositions should be top, then clips
                    return 50000;
                }

                if (model.mixDuration > 0) {
                    // Clips with mix should be ordered related to their position so that the right clip of a clip mix is always on top (the mix UI is drawn over the right clip)
                    return model.start / 25;
                }

                if (root.activeTool === ProjectTool.SlipTool && model.selected) {
                    return model.item === timeline.trimmingMainClip ? 2 : 1;
                }

                if (root.activeTool === ProjectTool.RippleTool && model.item === timeline.trimmingMainClip) {
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
                    when: loader.status == Loader.Ready && loader.item && clipItem
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
                    when: loader.status == Loader.Ready && loader.item && clipItem
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
                    property: "selected"
                    value: model.selected
                    when: loader.status == Loader.Ready && model.clipType != ProducerType.Track
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
                    property: "scrollX"
                    value: scrollView.contentX
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
                    when: loader.status == Loader.Ready && model.clipType == ProducerType.Composition
                }
                Binding {
                    target: loader.item
                    property: "trackHeight"
                    value: root.trackHeight
                    when: loader.status == Loader.Ready && model.clipType == ProducerType.Composition
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
                sourceComponent: {
                    if (clipItem) {
                        return clipDelegate
                    } else if (model.clipType == ProducerType.Composition) {
                        return compositionDelegate
                    } else {
                        // Track
                        return undefined
                    }
                }
                onLoaded: {
                    item.clipId= model.item
                    item.parentTrack = trackRoot
                    if (clipItem) {
                        console.log('loaded clip: ', model.start, ', ID: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex,', TYPE:', model.clipType)
                        item.isAudio= model.audio
                        item.markers= model.markers
                        item.hasAudio = model.hasAudio
                        item.canBeAudio = model.canBeAudio
                        item.canBeVideo = model.canBeVideo
                        item.itemType = model.clipType
                        item.audioChannels = model.audioChannels
                        item.audioStream = model.audioStream
                        item.multiStream = model.multiStream
                        item.aStreamIndex = model.audioStreamIndex
                        console.log('loaded clip with Astream: ', model.audioStream)
                        
                    } else if (model.clipType == ProducerType.Composition) {
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
            onInitGroupTrim: {
                // We are resizing a group, remember coordinates of all elements
                root.groupTrimData = controller.getGroupData(clip.clipId)
            }
            onTrimmingIn: {
                if (root.activeTool === ProjectTool.SelectTool && controlTrim) {
                    newDuration = controller.requestItemSpeedChange(clip.clipId, newDuration, false, root.snapping)
                    if (!speedController.visible) {
                        // Store original speed
                        speedController.originalSpeed = clip.speed
                    }
                    clip.x += clip.width - (newDuration * root.timeScale)
                    clip.width = newDuration * root.timeScale
                    speedController.x = clip.x + clip.border.width
                    speedController.width = Math.max(0, clip.width - 2 * clip.border.width)
                    speedController.lastValidDuration = newDuration
                    clip.speed = clip.originalDuration * speedController.originalSpeed / newDuration
                    speedController.visible = true
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = '%1:%2, %3:%4'.arg(i18n("Speed"))
                        .arg(clip.speed)
                        .arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(newDuration))
                    timeline.showToolTip(s)
                    return
                }
                var new_duration = 0;
                if (root.activeTool === ProjectTool.RippleTool) {
                    console.log("In: Request for " + newDuration)
                    new_duration = timeline.requestItemRippleResize(clip.clipId, newDuration, false, false, root.snapping, shiftTrim)
                    timeline.requestStartTrimmingMode(clip.clipId, false, false);
                    timeline.ripplePosChanged(new_duration, false);
                } else {
                    new_duration = controller.requestItemResize(clip.clipId, newDuration, false, false, root.snapping, shiftTrim)
                }

                if (new_duration > 0) {
                    clip.lastValidDuration = new_duration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = new_duration - clip.originalDuration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = '%1%2, %3:%4'.arg((delta <= 0)? '+' : '-')
                        .arg(s)
                        .arg(i18n("In"))
                        .arg(timeline.simplifiedTC(clip.inPoint))
                    timeline.showToolTip(s)
                    //bubbleHelp.show(clip.x - 20, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                //bubbleHelp.hide()
                timeline.showToolTip();
                if (shiftTrim || (root.groupTrimData == undefined/*TODO > */ || root.activeTool === ProjectTool.RippleTool /* < TODO*/) || controlTrim) {
                    // We only resize one element
                    if (root.activeTool === ProjectTool.RippleTool) {
                        timeline.requestItemRippleResize(clip.clipId, clip.originalDuration, false, false, 0, shiftTrim)
                    } else {
                        controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, 0, shiftTrim)
                    }

                    if (root.activeTool === ProjectTool.SelectTool && controlTrim) {
                        // Update speed
                        speedController.visible = false
                        controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, false, root.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                        speedController.originalSpeed = 1
                    } else {
                        if (root.activeTool === ProjectTool.RippleTool) {
                            timeline.requestItemRippleResize(clip.clipId, clip.lastValidDuration, false, true, 0, shiftTrim)
                            timeline.requestEndTrimmingMode();
                        } else {
                            controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, 0, shiftTrim)
                        }
                    }
                } else {
                    var updatedGroupData = controller.getGroupData(clip.clipId)
                    controller.processGroupResize(root.groupTrimData, updatedGroupData, false)
                }
                root.groupTrimData = undefined
            }
            onTrimmingOut: {
                if (root.activeTool === ProjectTool.SelectTool && controlTrim) {
                    if (!speedController.visible) {
                        // Store original speed
                        speedController.originalSpeed = clip.speed
                    }
                    speedController.x = clip.x + clip.border.width
                    newDuration = controller.requestItemSpeedChange(clip.clipId, newDuration, true, root.snapping)
                    clip.width = newDuration * root.timeScale
                    speedController.width = Math.max(0, clip.width - 2 * clip.border.width)
                    speedController.lastValidDuration = newDuration
                    clip.speed = clip.originalDuration * speedController.originalSpeed / newDuration
                    speedController.visible = true
                    var s = '%1:%2\%, %3:%4'.arg(i18n("Speed"))
                        .arg(Math.round(clip.speed*100))
                        .arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(newDuration))
                    timeline.showToolTip(s)
                    return
                }
                var new_duration = 0;
                if (root.activeTool === ProjectTool.RippleTool) {
                    console.log("Out: Request for " + newDuration)
                    new_duration = timeline.requestItemRippleResize(clip.clipId, newDuration, true, false, root.snapping, shiftTrim)
                    timeline.requestStartTrimmingMode(clip.clipId, false, true);
                    timeline.ripplePosChanged(new_duration, true);
                } else {
                    new_duration = controller.requestItemResize(clip.clipId, newDuration, true, false, root.snapping, shiftTrim)
                }
                if (new_duration > 0) {
                    clip.lastValidDuration = new_duration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = clip.originalDuration - new_duration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = '%1%2, %3:%4'.arg((delta <= 0)? '+' : '-')
                        .arg(s)
                        .arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(new_duration))
                    timeline.showToolTip(s);
                    //bubbleHelp.show(clip.x + clip.width - 20, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                timeline.showToolTip();
                //bubbleHelp.hide()
                if (shiftTrim || (root.groupTrimData == undefined/*TODO > */ || root.activeTool === ProjectTool.RippleTool /* < TODO*/) || controlTrim) {
                    if (root.activeTool === ProjectTool.RippleTool) {
                        timeline.requestItemRippleResize(clip.clipId, clip.originalDuration, true, false, 0, shiftTrim)
                    } else {
                        controller.requestItemResize(clip.clipId, clip.originalDuration, true, false, 0, shiftTrim)
                    }

                    if (root.activeTool === ProjectTool.SelectTool && controlTrim) {
                        speedController.visible = false
                        // Update speed
                        controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, true, root.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                        speedController.originalSpeed = 1
                    } else {
                        if (root.activeTool === ProjectTool.RippleTool) {
                            timeline.requestItemRippleResize(clip.clipId, clip.lastValidDuration, true, true, 0, shiftTrim)
                            timeline.requestEndTrimmingMode();
                        } else {
                            controller.requestItemResize(clip.clipId, clip.lastValidDuration, true, true, 0, shiftTrim)
                        }
                    }
                } else {
                    var updatedGroupData = controller.getGroupData(clip.clipId)
                    controller.processGroupResize(root.groupTrimData, updatedGroupData, true)
                }
                root.groupTrimData = undefined
            }
        }
    }
    Component {
        id: compositionDelegate
        Composition {
            displayHeight: Math.max(trackRoot.height / 2, trackRoot.height - (root.baseUnit * 2))
            opacity: 0.8
            selected: root.timelineSelection.indexOf(clipId) != -1
            onTrimmingIn: {
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
            onTrimmedIn: {
                timeline.showToolTip()
                //bubbleHelp.hide()
                controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, root.snapping)
                controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, root.snapping)
            }
            onTrimmingOut: {
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
            onTrimmedOut: {
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
        Text {
            id: speedLabel
            text: i18n("Adjusting speed")
            font: miniFont
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
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
