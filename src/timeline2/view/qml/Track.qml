/*
 * Copyright (c) 2013-2016 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.11
import QtQml.Models 2.11
import com.enums 1.0

Item{
    id: trackRoot
    property alias trackModel: trackModel.model
    property alias rootIndex : trackModel.rootIndex
    property bool isAudio
    property real timeScale: 1.0
    property bool isCurrentTrack: false
    property bool isLocked: false
    property int trackInternalId : -42
    property int trackThumbsFormat
    property int itemType: 0
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
            z: model.clipType == ProducerType.Composition ? 5 : 0
            Loader {
                id: loader
                Binding {
                    target: loader.item
                    property: "timeScale"
                    value: trackRoot.timeScale
                    when: loader.status == Loader.Ready && loader.item
                }
                Binding {
                    target: loader.item
                    property: "fakeTid"
                    value: model.fakeTrackId
                    when: loader.status == Loader.Ready && loader.item && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "fakePosition"
                    value: model.fakePosition
                    when: loader.status == Loader.Ready && loader.item && isClip(model.clipType)
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
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "positionOffset"
                    value: model.positionOffset
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "effectNames"
                    value: model.effectNames
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "clipStatus"
                    value: model.clipStatus
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "fadeOut"
                    value: model.fadeOut
                    when: loader.status == Loader.Ready && isClip(model.clipType)
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
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "isProxy"
                    value: model.isProxy
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "maxDuration"
                    value: model.maxDuration
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "forceReloadThumb"
                    value: model.reloadThumb
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                Binding {
                    target: loader.item
                    property: "binId"
                    value: model.binId
                    when: loader.status == Loader.Ready && isClip(model.clipType)
                }
                sourceComponent: {
                    if (isClip(model.clipType)) {
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
                    if (isClip(model.clipType)) {
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
                        // Speed change triggers a new clip insert so no binding necessary
                        item.speed = model.speed
                    } else if (model.clipType == ProducerType.Composition) {
                        console.log('loaded composition: ', model.start, ', ID: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                        //item.aTrack = model.a_track
                    } else {
                        console.log('loaded unwanted element: ', model.item, ', index: ', trackRoot.DelegateModel.itemsIndex)
                    }
                    item.trackId = model.trackId
                    //item.selected= trackRoot.selection.indexOf(item.clipId) !== -1
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
                clip.groupTrimData = controller.getGroupData(clip.clipId)
            }
            onTrimmingIn: {
                if (controlTrim) {
                    newDuration = controller.requestItemSpeedChange(clip.clipId, newDuration, false, root.snapping)
                    if (!speedController.visible) {
                        // Store original speed
                        speedController.originalSpeed = clip.speed
                    }
                    clip.x += clip.width - (newDuration * trackRoot.timeScale)
                    clip.width = newDuration * root.timeScale
                    speedController.x = clip.x + clip.border.width
                    speedController.width = clip.width - 2 * clip.border.width
                    speedController.lastValidDuration = newDuration
                    clip.speed = clip.originalDuration * speedController.originalSpeed / newDuration
                    speedController.visible = true
                    return
                }
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, false, false, root.snapping, shiftTrim)
                if (new_duration > 0) {
                    clip.lastValidDuration = new_duration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = new_duration - clip.originalDuration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = '%1%2\n%3:%4'.arg((delta <= 0)? '+' : '-')
                        .arg(s)
                        .arg(i18n("In"))
                        .arg(timeline.simplifiedTC(clip.inPoint))
                    bubbleHelp.show(clip.x - 20, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                bubbleHelp.hide()
                if (shiftTrim || clip.groupTrimData == undefined || controlTrim) {
                    // We only resize one element
                    controller.requestItemResize(clip.clipId, clip.originalDuration, false, false, 0, shiftTrim)
                    if (controlTrim) {
                        // Update speed
                        speedController.visible = false
                        controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, false, root.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                        speedController.originalSpeed = 1
                    } else {
                        controller.requestItemResize(clip.clipId, clip.lastValidDuration, false, true, 0, shiftTrim)
                    }
                } else {
                    var updatedGroupData = controller.getGroupData(clip.clipId)
                    controller.processGroupResize(clip.groupTrimData, updatedGroupData, false)
                }
                clip.groupTrimData = undefined
            }
            onTrimmingOut: {
                if (controlTrim) {
                    if (!speedController.visible) {
                        // Store original speed
                        speedController.originalSpeed = clip.speed
                    }
                    speedController.x = clip.x + clip.border.width
                    newDuration = controller.requestItemSpeedChange(clip.clipId, newDuration, true, root.snapping)
                    clip.width = newDuration * trackRoot.timeScale
                    speedController.width = clip.width - 2 * clip.border.width
                    speedController.lastValidDuration = newDuration
                    clip.speed = clip.originalDuration * speedController.originalSpeed / newDuration
                    speedController.visible = true
                    return
                }
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, true, false, root.snapping, shiftTrim)
                if (new_duration > 0) {
                    clip.lastValidDuration = new_duration
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = clip.originalDuration - new_duration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = '%1%2\n%3:%4'.arg((delta <= 0)? '+' : '-')
                        .arg(s)
                        .arg(i18n("Duration"))
                        .arg(timeline.simplifiedTC(new_duration))
                    bubbleHelp.show(clip.x + clip.width - 20, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
                if (shiftTrim || clip.groupTrimData == undefined || controlTrim) {
                    controller.requestItemResize(clip.clipId, clip.originalDuration, true, false, 0, shiftTrim)
                    if (controlTrim) {
                        speedController.visible = false
                        // Update speed
                        controller.requestClipResizeAndTimeWarp(clip.clipId, speedController.lastValidDuration, true, root.snapping, shiftTrim, clip.originalDuration * speedController.originalSpeed / speedController.lastValidDuration)
                        speedController.originalSpeed = 1
                    } else {
                        controller.requestItemResize(clip.clipId, clip.lastValidDuration, true, true, 0, shiftTrim)
                    }
                } else {
                    var updatedGroupData = controller.getGroupData(clip.clipId)
                    controller.processGroupResize(clip.groupTrimData, updatedGroupData, true)
                }
                clip.groupTrimData = undefined
            }
        }
    }
    Component {
        id: compositionDelegate
        Composition {
            displayHeight: Math.max(trackRoot.height / 2, trackRoot.height - (root.baseUnit * 2))
            opacity: 0.8
            selected: root.timelineSelection.indexOf(clipId) !== -1
            onTrimmingIn: {
                var new_duration = controller.requestItemResize(clip.clipId, newDuration, false, false, root.snapping)
                if (new_duration > 0) {
                    clip.lastValidDuration = newDuration
                    clip.originalX = clip.draggedX
                    // Show amount trimmed as a time in a "bubble" help.
                    var delta = clip.originalDuration - new_duration
                    var s = timeline.simplifiedTC(Math.abs(delta))
                    s = '%1%2 = %3'.arg((delta <= 0)? '+' : '-')
                        .arg(s)
                        .arg(timeline.simplifiedTC(new_duration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedIn: {
                bubbleHelp.hide()
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
                    s = '%1%2 = %3'.arg((delta <= 0)? '+' : '-')
                        .arg(s)
                        .arg(timeline.simplifiedTC(new_duration))
                    bubbleHelp.show(clip.x + clip.width, trackRoot.y + trackRoot.height, s)
                }
            }
            onTrimmedOut: {
                bubbleHelp.hide()
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
}
