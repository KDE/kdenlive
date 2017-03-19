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

import QtQuick 2.2
import QtQuick.Controls 1.0
import Kdenlive.Controls 1.0
import QtGraphicalEffects 1.0
import QtQml.Models 2.2
import QtQuick.Window 2.2

Rectangle {
    id: clipRoot
    property real timeScale: 1.0
    property string clipName: ''
    property string clipResource: ''
    property string mltService: ''
    property int modelStart: x
    property int inPoint: 0
    property int outPoint: 0
    property int clipDuration: 0
    property bool isBlank: false
    property bool isAudio: false
    property bool isTransition: false
    property bool grouped: false
    property var audioLevels
    property var markers : []
    property int fadeIn: 0
    property int fadeOut: 0
    property int binId: 0
    property int trackIndex //Index in track repeater
    property int trackId: -42    //Id in the model
    property int clipId     //Id of the clip in the model
    property int originalTrackId: trackId
    property int originalX: x
    property int originalDuration: clipDuration
    property int lastValidDuration: clipDuration
    property int draggedX: x
    property bool selected: false
    property string hash: 'ccc' //TODO
    property double speed: 1.0
    property color borderColor: 'black'

    signal clicked(var clip, int shiftClick)
    signal moved(var clip)
    signal dragged(var clip, var mouse)
    signal dropped(var clip)
    signal draggedToTrack(var clip, int pos)
    signal trimmingIn(var clip, real newDuration, var mouse)
    signal trimmedIn(var clip)
    signal trimmingOut(var clip, real newDuration, var mouse)
    signal trimmedOut(var clip)

    onModelStartChanged: {
        console.log("MODEL START CHANGED !!!!!!", modelStart, clipId, x);
        x = modelStart * timeScale;
    }
    onTimeScaleChanged: {
        console.log("SCALE CHANGED !!!!!!", timeScale)
        x = modelStart * timeScale;
        width = clipDuration * timeScale;
    }

    SystemPalette { id: activePalette }
    color: Qt.darker(getColor())

    border.color: selected? 'red' : borderColor
    border.width: 1.5
    clip: true
    Drag.active: mouseArea.drag.active
    Drag.proposedAction: Qt.MoveAction
    opacity: Drag.active? 0.5 : 1.0

    function getColor() {
        if (mltService === 'color') {
            //console.log('clip color', clipResource, " / ", '#' + clipResource.substring(2, 8))
            if (clipResource.length == 10) {
                // 0xRRGGBBAA
                return '#' + clipResource.substring(2, 8)
            }
        }
        return isAudio? 'darkseagreen' : 'blue'
        //root.shotcutBlue
    }

    function reparent(track) {
        parent = track
        isAudio = track.isAudio
        height = track.height
        y = track.y
        generateWaveform()
    }

    function generateWaveform() {
        // This is needed to make the model have the correct count.
        // Model as a property expression is not working in all cases.
        waveformRepeater.model = Math.ceil(waveform.innerWidth / waveform.maxWidth)
        for (var i = 0; i < waveformRepeater.count; i++)
            waveformRepeater.itemAt(0).update()
    }

    function imagePath(time) {
        if (isAudio || isBlank || mltService === 'color') {
            return ''
        } else {
            return 'image://thumbnail/' + binId + '/' + mltService + '/' + clipResource + '#' + time
        }
    }

    onAudioLevelsChanged: generateWaveform()

    Image {
        id: outThumbnail
        visible: timeline.showThumbnails && mltService != 'color'
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: parent.border.width
        anchors.rightMargin: parent.border.width + 1
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.border.width + 1
        width: height * 16.0/9.0
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        source: imagePath(outPoint)
    }

    Image {
        id: inThumbnail
        visible: timeline.showThumbnails && mltService != 'color'
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: parent.border.width
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.border.width + 1
        anchors.leftMargin: parent.border.width
        width: height * 16.0/9.0
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        source: imagePath(inPoint)
    }

    /*TimelineTransition {
        visible: isTransition
        anchors.fill: parent
        property var color: isAudio? 'darkseagreen' : 'blue'
        //root.shotcutBlue
        colorA: color
        colorB: clipRoot.selected ? Qt.darker(color) : Qt.lighter(color)
    }*/

    Row {
        id: waveform
        visible: timeline.showAudioThumbnails
        height: isAudio? parent.height : parent.height / 2
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: parent.border.width
        opacity: 0.7
        property int maxWidth: 10000
        property int innerWidth: clipRoot.width - clipRoot.border.width * 2

        Repeater {
            id: waveformRepeater
            TimelineWaveform {
                width: Math.min(waveform.innerWidth, waveform.maxWidth)
                height: waveform.height
                fillColor: 'red'
                property int channels: 2
                inPoint: Math.round((clipRoot.inPoint + index * waveform.maxWidth / timeScale) * speed) * channels
                outPoint: inPoint + Math.round(width / timeScale * speed) * channels
                levels: audioLevels
            }
        }
    }

    Rectangle {
        // audio peak line
        width: parent.width - parent.border.width * 2
        visible: timeline.showAudioThumbnails
        height: 1
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: parent.border.width
        anchors.bottomMargin: waveform.height * 0.9
        color: Qt.darker(parent.color)
        opacity: 0.4
    }

    Rectangle {
        // text background
        color: 'lightgray'
        visible: !isBlank && !isTransition
        opacity: 0.7
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: parent.border.width
        anchors.leftMargin: parent.border.width
            // + ((isAudio || !settings.timelineShowThumbnails) ? 0 : inThumbnail.width)
        width: label.width + 2
        height: label.height
    }

    Text {
        id: label
        text: clipName
        visible: !isBlank && !isTransition
        font.pixelSize: root.baseUnit
        anchors {
            top: parent.top
            left: parent.left
            topMargin: parent.border.width + 1
            leftMargin: parent.border.width + 1
                // + ((isAudio || !settings.timelineShowThumbnails) ? 0 : inThumbnail.width) + 1
        }
        color: 'black'
    }

    Repeater {
        model: markers.length / 2
        delegate:
        Item {
            anchors.fill: parent
        Rectangle {
            id: markerBase
            width: 1 // modelDa
            height: parent.height
            x: (markers[2 * modelData] - clipRoot.inPoint) * timeScale;
            color: 'red'
        }
        Rectangle {
            opacity: 0.7
            x: markerBase.x
            radius: 2
            width: mlabel.width + 4
            height: mlabel.height
            anchors {
                bottom: parent.verticalCenter
            }
            color: 'red'
        }
        Text {
            id: mlabel
            text: markers[2 * modelData + 1]
            visible: !isBlank && !isTransition
            font.pixelSize: root.baseUnit
            x: markerBase.x
            anchors {
                bottom: parent.verticalCenter
                topMargin: 2
                leftMargin: 2
            }
            color: 'white'
        }
        }
    }

    states: [
        State {
            name: 'normal'
            when: !clipRoot.selected
            PropertyChanges {
                target: clipRoot
                z: 0
            }
        },
        State {
            name: 'selectedBlank'
            when: clipRoot.selected && clipRoot.isBlank
            PropertyChanges {
                target: clipRoot
                color: Qt.darker(getColor())
            }
        },
        State {
            name: 'selected'
            when: clipRoot.selected
            PropertyChanges {
                target: clipRoot
                z: 1
                color: getColor()
            }
        }
    ]

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: !isBlank
        acceptedButtons: Qt.LeftButton
        drag.target: parent
        drag.axis: Drag.XAxis
        property int startX

        onPressed: {
            root.stopScrolling = true
            originalX = parent.x
            originalTrackId = clipRoot.trackId
            startX = parent.x
            clipRoot.forceActiveFocus();
            clipRoot.clicked(clipRoot, mouse.modifiers === Qt.ShiftModifier)
        }
        onPositionChanged: {
            if (mouse.y < 0 || mouse.y > height) {
                parent.draggedToTrack(clipRoot, mapToItem(null, 0, mouse.y).y)
            } else {
                parent.dragged(clipRoot, mouse)
            }
        }
        onReleased: {
            root.stopScrolling = false
            parent.y = 0
            var delta = parent.x - startX
            if (Math.abs(delta) >= 1.0 || trackId !== originalTrackId) {
                parent.moved(clipRoot)
                originalX = parent.x
                originalTrackId = trackId
            } else {
                parent.dropped(clipRoot)
            }
        }
        onDoubleClicked: timeline.position = clipRoot.x / timeline.scaleFactor
        onWheel: zoomByWheel(wheel)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            propagateComposedEvents: true
            cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor :
                (fadeInMouseArea.drag.active || fadeOutMouseArea.drag.active)? Qt.PointingHandCursor :
                drag.active? Qt.ClosedHandCursor :
                isBlank? Qt.ArrowCursor : Qt.OpenHandCursor
            onPressed: {
                root.stopScrolling = true
                clipRoot.forceActiveFocus();
                if (!clipRoot.selected) {
                    clipRoot.clicked(clipRoot, false)
                }
            }
            onClicked: menu.show()
        }
    }

    TimelineTriangle {
        id: fadeInTriangle
        visible: !isBlank && !isTransition
        width: parent.fadeIn * timeScale
        height: parent.height - parent.border.width * 2
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: parent.border.width
        opacity: 0.5
    }
    Rectangle {
        id: fadeInControl
        enabled: !isBlank && !isTransition
        anchors.left: fadeInTriangle.width > radius? undefined : fadeInTriangle.left
        anchors.horizontalCenter: fadeInTriangle.width > radius? fadeInTriangle.right : undefined
        anchors.top: fadeInTriangle.top
        anchors.topMargin: -3
        width: 15
        height: 15
        radius: 7.5
        color: 'black'
        border.width: 2
        border.color: 'white'
        opacity: 0
        Drag.active: fadeInMouseArea.drag.active
        MouseArea {
            id: fadeInMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            property int startX
            property int startFadeIn
            onEntered: parent.opacity = 0.7
            onExited: parent.opacity = 0
            onPressed: {
                root.stopScrolling = true
                startX = parent.x
                startFadeIn = fadeIn
                parent.anchors.left = undefined
                parent.anchors.horizontalCenter = undefined
                parent.opacity = 1
                // trackRoot.clipSelected(clipRoot, trackRoot) TODO
            }
            onReleased: {
                root.stopScrolling = false
                if (fadeInTriangle.width > parent.radius)
                    parent.anchors.horizontalCenter = fadeInTriangle.right
                else
                    parent.anchors.left = fadeInTriangle.left
                bubbleHelp.hide()
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = Math.round((parent.x - startX) / timeScale)
                    var duration = startFadeIn + delta
                    timeline.fadeIn(trackIndex, index, duration)

                    // Show fade duration as time in a "bubble" help.
                    var s = timeline.timecode(Math.max(duration, 0))
                    bubbleHelp.show(clipRoot.x, trackRoot.y + clipRoot.height, s.substring(6))
                }
            }
        }
        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: fadeInMouseArea.containsMouse
            NumberAnimation {
                from: 1.0
                to: 0.5
                duration: 250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                from: 0.5
                to: 1.0
                duration: 250
                easing.type: Easing.InOutQuad
            }
        }
    }

    TimelineTriangle {
        id: fadeOutCanvas
        visible: !isBlank && !isTransition
        width: parent.fadeOut * timeScale
        height: parent.height - parent.border.width * 2
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: parent.border.width
        opacity: 0.5
        transform: Scale { xScale: -1; origin.x: fadeOutCanvas.width / 2}
    }
    Rectangle {
        id: fadeOutControl
        enabled: !isBlank && !isTransition
        anchors.right: fadeOutCanvas.width > radius? undefined : fadeOutCanvas.right
        anchors.horizontalCenter: fadeOutCanvas.width > radius? fadeOutCanvas.left : undefined
        anchors.top: fadeOutCanvas.top
        anchors.topMargin: -3
        width: 15
        height: 15
        radius: 7.5
        color: 'black'
        border.width: 2
        border.color: 'white'
        opacity: 0
        Drag.active: fadeOutMouseArea.drag.active
        MouseArea {
            id: fadeOutMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            property int startX
            property int startFadeOut
            onEntered: parent.opacity = 0.7
            onExited: parent.opacity = 0
            onPressed: {
                root.stopScrolling = true
                startX = parent.x
                startFadeOut = fadeOut
                parent.anchors.right = undefined
                parent.anchors.horizontalCenter = undefined
                parent.opacity = 1
            }
            onReleased: {
                root.stopScrolling = false
                if (fadeOutCanvas.width > parent.radius)
                    parent.anchors.horizontalCenter = fadeOutCanvas.left
                else
                    parent.anchors.right = fadeOutCanvas.right
                bubbleHelp.hide()
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = Math.round((startX - parent.x) / timeScale)
                    var duration = startFadeOut + delta
                    timeline.fadeOut(trackIndex, index, duration)

                    // Show fade duration as time in a "bubble" help.
                    var s = timeline.timecode(Math.max(duration, 0))
                    bubbleHelp.show(clipRoot.x + clipRoot.width, trackRoot.y + clipRoot.height, s.substring(6))
                }
            }
        }
        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: fadeOutMouseArea.containsMouse
            NumberAnimation {
                from: 1.0
                to: 0.5
                duration: 250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                from: 0.5
                to: 1.0
                duration: 250
                easing.type: Easing.InOutQuad
            }
        }
    }

    Rectangle {
        id: trimIn
        enabled: !isBlank
        anchors.left: parent.left
        anchors.leftMargin: 0
        height: parent.height
        width: 5
        color: isAudio? 'green' : 'lawngreen'
        opacity: 0
        Drag.active: trimInMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction

        MouseArea {
            id: trimInMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis

            onPressed: {
                root.stopScrolling = true
                clipRoot.originalX = mapToItem(null, x, y).x
                clipRoot.originalDuration = clipDuration
                parent.anchors.left = undefined
            }
            onReleased: {
                root.stopScrolling = false
                parent.anchors.left = clipRoot.left
                clipRoot.trimmedIn(clipRoot)
                parent.opacity = 0
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    clipRoot.draggedX = mapToItem(null, x, y).x
                    var delta = Math.round((draggedX - originalX) / timeScale)
                    if (delta !== 0) {
                        var newDuration = clipRoot.clipDuration - delta
                        clipRoot.trimmingIn(clipRoot, newDuration, mouse)
                    }
                }
            }
            onEntered: parent.opacity = 0.5
            onExited: parent.opacity = 0
        }
    }
    Rectangle {
        id: trimOut
        enabled: !isBlank
        anchors.right: parent.right
        anchors.rightMargin: 0
        height: parent.height
        width: 5
        color: 'red'
        opacity: 0
        Drag.active: trimOutMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction

        MouseArea {
            id: trimOutMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis

            onPressed: {
                root.stopScrolling = true
                clipRoot.originalDuration = clipDuration
                parent.anchors.right = undefined
            }
            onReleased: {
                root.stopScrolling = false
                parent.anchors.right = clipRoot.right
                clipRoot.trimmedOut(clipRoot)
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var newDuration = Math.round((parent.x + parent.width) / timeScale)
                    clipRoot.trimmingOut(clipRoot, newDuration, mouse)
                }
            }
            onEntered: parent.opacity = 0.5
            onExited: parent.opacity = 0
        }
    }
    Menu {
        id: menu
        function show() {
            //mergeItem.visible = timeline.mergeClipWithNext(trackIndex, index, true)
            popup()
        }
        MenuItem {
            visible: true // !isBlank && !isTransition
            text: i18n('Cut')
            onTriggered: {
                if (!trackRoot.isLocked) {
                    timeline.copyClip(trackIndex, index)
                    timeline.remove(trackIndex, index)
                } else {
                    root.pulseLockButtonOnTrack(currentTrack)
                }
            }
        }
        MenuItem {
            visible: !grouped && trackRoot.selection.length > 1
            text: i18n('Group')
            onTriggered: timeline.groupSelection()
        }
        MenuItem {
            visible: grouped
            text: i18n('Ungroup')
            onTriggered: timeline.unGroupSelection(clipId)
        }

        MenuItem {
            visible: true //!isBlank && !isTransition
            text: i18n('Copy')
            onTriggered: timeline.copyClip(trackIndex, index)
        }
        MenuSeparator {
            visible: !isBlank && !isTransition
        }
        MenuItem {
            text: i18n('Remove')
            onTriggered: timeline.triggerAction('delete_timeline_clip')
        }
        MenuItem {
            visible: true //!isBlank
            text: i18n('Lift')
            onTriggered: timeline.lift(trackIndex, index)
        }
        MenuSeparator {
            visible: true //!isBlank && !isTransition
        }
        MenuItem {
            visible: true //!isBlank && !isTransition
            text: i18n('Split At Playhead (S)')
            onTriggered: timeline.splitClip(trackIndex, index)
        }
        MenuItem {
            id: mergeItem
            text: i18n('Merge with next clip')
            onTriggered: timeline.mergeClipWithNext(trackIndex, index, false)
        }
        MenuItem {
            visible: !isBlank && !isTransition
            text: i18n('Rebuild Audio Waveform')
            onTriggered: timeline.remakeAudioLevels(trackIndex, index)
        }
        /*onPopupVisibleChanged: {
            if (visible && application.OS !== 'OS X' && __popupGeometry.height > 0) {
                // Try to fix menu running off screen. This only works intermittently.
                menu.__yOffset = Math.min(0, Screen.height - (__popupGeometry.y + __popupGeometry.height + 40))
                menu.__xOffset = Math.min(0, Screen.width - (__popupGeometry.x + __popupGeometry.width))
            }
        }*/
    }
}
