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
import QtQuick.Controls 2.4
import Kdenlive.Controls 1.0
import QtQml.Models 2.11
import QtQuick.Window 2.2
import 'Timeline.js' as Logic
import com.enums 1.0

Rectangle {
    id: clipRoot
    property real timeScale: 1.0
    property string clipName: ''
    property string clipResource: ''
    property string mltService: ''
    property string effectNames
    property int modelStart
    property real scrollX: 0
    property int inPoint: 0
    property int outPoint: 0
    property int clipDuration: 0
    property int maxDuration: 0
    property bool isAudio: false
    property int audioChannels
    property bool showKeyframes: false
    property bool isGrabbed: false
    property bool grouped: false
    property var markers
    property var keyframeModel
    property int clipStatus: 0
    property int itemType: 0
    property int fadeIn: 0
    property int fadeOut: 0
    property int binId: 0
    property int positionOffset: 0
    property var parentTrack
    property int trackIndex //Index in track repeater
    property int clipId     //Id of the clip in the model
    property int trackId: -1 // Id of the parent track in the model
    property int fakeTid: -1
    property int fakePosition: 0
    property int originalTrackId: -1
    property int originalX: x
    property int originalDuration: clipDuration
    property int lastValidDuration: clipDuration
    property int draggedX: x
    property bool selected: false
    property bool isLocked: parentTrack && parentTrack.isLocked == true
    property bool hasAudio
    property bool canBeAudio
    property bool canBeVideo
    property double speed: 1.0
    property color borderColor: 'black'
    property bool forceReloadThumb
    property bool isComposition: false
    property bool hideClipViews
    property var groupTrimData
    property int scrollStart: scrollView.flickableItem.contentX - clipRoot.modelStart * timeline.scaleFactor
    width : clipDuration * timeScale;
    opacity: dragProxyArea.drag.active && dragProxy.draggedItem == clipId ? 0.8 : 1.0

    signal trimmingIn(var clip, real newDuration, var mouse, bool shiftTrim, bool controlTrim)
    signal trimmedIn(var clip, bool shiftTrim, bool controlTrim)
    signal initGroupTrim(var clip)
    signal trimmingOut(var clip, real newDuration, var mouse, bool shiftTrim, bool controlTrim)
    signal trimmedOut(var clip, bool shiftTrim, bool controlTrim)
    
    onScrollStartChanged: {
        clipRoot.hideClipViews = scrollStart > width || scrollStart + scrollView.viewport.width < 0
    }

    onIsGrabbedChanged: {
        if (clipRoot.isGrabbed) {
            grabItem()
        } else {
            mouseArea.focus = false
        }
    }

    function grabItem() {
        clipRoot.forceActiveFocus()
        mouseArea.focus = true
    }

    function clearAndMove(offset) {
        controller.requestClearSelection()
        controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true)
        controller.requestAddToSelection(clipRoot.clipId)
    }

    onClipResourceChanged: {
        if (itemType == ProducerType.Color) {
            color: Qt.darker(getColor(), 1.5)
        }
    }
    ToolTip {
        visible: mouseArea.containsMouse && !dragProxyArea.pressed
        delay: 1000
        timeout: 5000
        background: Rectangle {
            color: activePalette.alternateBase
            border.color: activePalette.light
        }
        contentItem: Label {
            color: activePalette.text
            font.pointSize: root.fontUnit
            text: label.text + ' (' + timeline.simplifiedTC(clipRoot.inPoint) + '-' + timeline.simplifiedTC(clipRoot.outPoint) + ')'
        }
    }

    onKeyframeModelChanged: {
        if (effectRow.keyframecanvas) {
            console.log('keyframe model changed............')
            effectRow.keyframecanvas.requestPaint()
        }
    }

    onClipDurationChanged: {
        width = clipDuration * timeScale;
        if (parentTrack && parentTrack.isAudio && thumbsLoader.item) {
            // Duration changed, we may need a different number of repeaters
            thumbsLoader.item.reload()
        }
    }

    onModelStartChanged: {
        x = modelStart * timeScale;
    }
    onFakePositionChanged: {
        x = fakePosition * timeScale;
    }
    onFakeTidChanged: {
        if (clipRoot.fakeTid > -1 && parentTrack) {
            if (clipRoot.parent != dragContainer) {
                var pos = clipRoot.mapToGlobal(clipRoot.x, clipRoot.y);
                clipRoot.parent = dragContainer
                pos = clipRoot.mapFromGlobal(pos.x, pos.y)
                clipRoot.x = pos.x
                clipRoot.y = pos.y
            }
            clipRoot.y = Logic.getTrackById(clipRoot.fakeTid).y
        }
    }

    onForceReloadThumbChanged: {
        // TODO: find a way to force reload of clip thumbs
        if (thumbsLoader.item) {
            thumbsLoader.item.reload()
        }
    }

    onTimeScaleChanged: {
        x = modelStart * timeScale;
        width = clipDuration * timeScale;
        labelRect.x = scrollX > modelStart * timeScale ? scrollX - modelStart * timeScale : 0
    }
    onScrollXChanged: {
        labelRect.x = scrollX > modelStart * timeScale ? scrollX - modelStart * timeScale : 0
    }

    border.color: selected ? root.selectionColor : grouped ? root.groupColor : borderColor
    border.width: isGrabbed ? 8 : 1.5

    function updateDrag() {
        var itemPos = mapToItem(tracksContainerArea, 0, 0, clipRoot.width, clipRoot.height)
        initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
    }

    function getColor() {
        if (clipStatus == ClipState.Disabled) {
            return 'grey'
        }
        if (itemType == ProducerType.Color) {
            var color = clipResource.substring(clipResource.length - 9)
            if (color[0] == '#') {
                return color
            }
            return '#' + color.substring(color.length - 8, color.length - 2)
        }
        return isAudio? root.audioColor : root.videoColor
    }

/*    function reparent(track) {
        console.log('TrackId: ',trackId)
        parent = track
        height = track.height
        parentTrack = track
        trackId = parentTrack.trackId
        console.log('Reparenting clip to Track: ', trackId)
        //generateWaveform()
    }
*/
    property bool variableThumbs: (isAudio || itemType == ProducerType.Color || mltService === '')
    property bool isImage: itemType == ProducerType.Image
    property string baseThumbPath: variableThumbs ? '' : 'image://thumbnail/' + binId + '/' + documentId + '/' + (isImage ? '#0' : '#')

    DropArea { //Drop area for clips
        anchors.fill: clipRoot
        keys: 'kdenlive/effect'
        property string dropData
        property string dropSource
        property int dropRow: -1
        onEntered: {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
        }
        onDropped: {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                controller.addClipEffect(clipRoot.clipId, dropData);
            } else {
                controller.copyClipEffect(clipRoot.clipId, dropSource);
            }
            dropSource = ''
            dropRow = -1
            drag.acceptProposedAction
        }
    }
    MouseArea {
        id: mouseArea
        enabled: root.activeTool === 0
        anchors.fill: clipRoot
        acceptedButtons: Qt.RightButton
        hoverEnabled: root.activeTool === 0
        cursorShape: (trimInMouseArea.drag.active || trimOutMouseArea.drag.active)? Qt.SizeHorCursor : dragProxyArea.cursorShape
        onPressed: {
            root.autoScrolling = false
            if (mouse.button == Qt.RightButton) {
                if (timeline.selection.indexOf(clipRoot.clipId) == -1) {
                    controller.requestAddToSelection(clipRoot.clipId, true)
                }
                clipMenu.clipId = clipRoot.clipId
                clipMenu.clipStatus = clipRoot.clipStatus
                clipMenu.clipFrame = Math.round(mouse.x / timeline.scaleFactor)
                clipMenu.grouped = clipRoot.grouped
                clipMenu.trackId = clipRoot.trackId
                clipMenu.canBeAudio = clipRoot.canBeAudio
                clipMenu.canBeVideo = clipRoot.canBeVideo
                clipMenu.popup()
            }
        }
        Keys.onShortcutOverride: event.accepted = clipRoot.isGrabbed && (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Escape)
        Keys.onLeftPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart - offset, true, true, true);
        }
        Keys.onRightPressed: {
            var offset = event.modifiers === Qt.ShiftModifier ? timeline.fps() : 1
            controller.requestClipMove(clipRoot.clipId, clipRoot.trackId, clipRoot.modelStart + offset, true, true, true);
        }
        Keys.onUpPressed: {
            controller.requestClipMove(clipRoot.clipId, controller.getNextTrackId(clipRoot.trackId), clipRoot.modelStart, true, true, true);
        }
        Keys.onDownPressed: {
            controller.requestClipMove(clipRoot.clipId, controller.getPreviousTrackId(clipRoot.trackId), clipRoot.modelStart, true, true, true);
        }
        Keys.onEscapePressed: {
            timeline.grabCurrent()
            //focus = false
        }
        onPositionChanged: {
            var mapped = parentTrack.mapFromItem(clipRoot, mouse.x, mouse.y).x
            root.mousePosChanged(Math.round(mapped / timeline.scaleFactor))
        }
        onEntered: {
            var itemPos = mapToItem(tracksContainerArea, 0, 0, width, height)
            initDrag(clipRoot, itemPos, clipRoot.clipId, clipRoot.modelStart, clipRoot.trackId, false)
        }

        onExited: {
            endDrag()
        }
        onWheel: zoomByWheel(wheel)

        Item {
            // Thumbs container
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: clipRoot.border.width
            anchors.bottomMargin: clipRoot.border.width
            clip: true
            Loader {
                id: thumbsLoader
                asynchronous: true
                visible: status == Loader.Ready
                anchors.fill: parent
                source: clipRoot.hideClipViews ? "" : parentTrack.isAudio ? (timeline.showAudioThumbnails ? "ClipAudioThumbs.qml" : "") : itemType == ProducerType.Color ? "" : timeline.showThumbnails ? "ClipThumbs.qml" : ""
                onLoaded: {
                    item.reload()
                }
            }
        }

        Item {
            // Clipping container
            id: container
            anchors.fill: parent
            anchors.margins: 1.5
            clip: true

            Rectangle {
                // text background
                id: labelRect
                color: clipRoot.selected ? 'darkred' : '#66000000'
                width: label.width + 2
                height: label.height
                visible: clipRoot.width > width / 2
                Text {
                    id: label
                    text: clipName + (clipRoot.speed != 1.0 ? ' [' + Math.round(clipRoot.speed*100) + '%]': '')
                    font.pointSize: root.fontUnit
                    anchors {
                        top: labelRect.top
                        left: labelRect.left
                        topMargin: 1
                        leftMargin: 1
                    }
                    color: 'white'
                    style: Text.Outline
                    styleColor: 'black'
                }
            }
            Rectangle {
                // Offset info
                id: offsetRect
                color: 'darkgreen'
                width: offsetLabel.width + radius
                height: offsetLabel.height
                radius: height/3
                x: labelRect.width + 4
                visible: labelRect.visible && positionOffset != 0
                MouseArea {
                    id: offsetArea
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    anchors.fill: parent
                    onClicked: {
                        clearAndMove(positionOffset)
                    }
                    ToolTip {
                        visible: offsetArea.containsMouse
                        delay: 1000
                        timeout: 5000
                        background: Rectangle {
                            color: activePalette.alternateBase
                            border.color: activePalette.light
                        }
                        contentItem: Label {
                            color: activePalette.text
                            font.pointSize: root.fontUnit
                            text: positionOffset < 0 ? i18n("Offset: -%1", timeline.simplifiedTC(-positionOffset)) : i18n("Offset: %1", timeline.simplifiedTC(positionOffset))
                        }
                    }
                    Text {
                        id: offsetLabel
                        text: positionOffset
                        font.pointSize: root.fontUnit
                        anchors {
                            horizontalCenter: parent.horizontalCenter
                            topMargin: 1
                            leftMargin: 1
                        }
                        color: 'white'
                        style: Text.Outline
                        styleColor: 'black'
                    }
                }
            }
            Rectangle {
                // effects
                id: effectsRect
                color: '#555555'
                width: effectLabel.width + 2
                height: effectLabel.height
                x: labelRect.x
                anchors.top: labelRect.bottom
                visible: labelRect.visible && clipRoot.effectNames != ''
                Text {
                    id: effectLabel
                    text: clipRoot.effectNames
                    font.pointSize: root.fontUnit
                    anchors {
                        top: effectsRect.top
                        left: effectsRect.left
                        topMargin: 1
                        leftMargin: 1
                        // + ((isAudio || !settings.timelineShowThumbnails) ? 0 : inThumbnail.width) + 1
                    }
                    color: 'white'
                    //style: Text.Outline
                    styleColor: 'black'
                }
            }

            Repeater {
                model: markers
                delegate:
                Item {
                    anchors.fill: parent
                    Rectangle {
                        id: markerBase
                        width: 1
                        height: parent.height
                        x: clipRoot.speed < 0 ? clipRoot.clipDuration * timeScale + (Math.round(model.frame / clipRoot.speed) - (clipRoot.maxDuration - clipRoot.outPoint)) * timeScale : (Math.round(model.frame / clipRoot.speed) - clipRoot.inPoint) * timeScale;
                        color: model.color
                    }
                    Rectangle {
                        visible: mlabel.visible
                        opacity: 0.7
                        x: markerBase.x
                        radius: 2
                        width: mlabel.width + 4
                        height: mlabel.height
                        anchors {
                            bottom: parent.verticalCenter
                        }
                        color: model.color
                        MouseArea {
                            z: 10
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onDoubleClicked: timeline.editMarker(clipRoot.clipId, model.frame)
                            onClicked: proxy.position = (clipRoot.x + markerBase.x) / timeline.scaleFactor
                        }
                    }
                    Text {
                        id: mlabel
                        visible: timeline.showMarkers && parent.width > width * 1.5
                        text: model.comment
                        font.pointSize: root.fontUnit
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

            KeyframeView {
                id: effectRow
                visible: clipRoot.showKeyframes && clipRoot.keyframeModel
                selected: clipRoot.selected
                inPoint: clipRoot.inPoint
                outPoint: clipRoot.outPoint
                masterObject: clipRoot
                kfrModel: clipRoot.hideClipViews ? 0 : clipRoot.keyframeModel
            }
        }

        states: [
            State {
                name: 'locked'
                when: isLocked
                PropertyChanges {
                    target: clipRoot
                    color: root.lockedColor
                    opacity: 0.8
                    z: 0
                }
            },
            State {
                name: 'normal'
                when: clipRoot.selected === false
                PropertyChanges {
                    target: clipRoot
                    color: Qt.darker(getColor(), 1.5)
                    z: 0
                }
            },
            State {
                name: 'selected'
                when: clipRoot.selected === true
                PropertyChanges {
                    target: clipRoot
                    color: getColor()
                    z: 3
                }
            }
        ]

        Rectangle {
            id: compositionIn
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            anchors.leftMargin: 4
            width: root.baseUnit * 1.2
            height: width
            radius: 2
            color: Qt.darker('mediumpurple')
            border.width: 2
            border.color: 'green'
            opacity: 0
            enabled: !clipRoot.isAudio && dragProxy.draggedItem === clipRoot.clipId
            visible: clipRoot.width > 4 * width
            MouseArea {
                id: compInArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: parent.opacity = 0.7
                onExited: {
                    if (!pressed) {
                    parent.opacity = 0
                    }
                }
                onPressed: {
                    timeline.addCompositionToClip('', clipRoot.clipId, 0)
                }
                onReleased: {
                    parent.opacity = 0
                }
                ToolTip {
                    visible: compInArea.containsMouse && !dragProxyArea.pressed
                    delay: 1000
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        font.pointSize: root.fontUnit
                        text: i18n("Click to add composition")
                    }
                }
            }
        }
        Rectangle {
            id: compositionOut
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            anchors.rightMargin: 4
            width: root.baseUnit * 1.2
            height: width
            radius: 2
            color: Qt.darker('mediumpurple')
            border.width: 2
            border.color: 'green'
            opacity: 0
            enabled: !clipRoot.isAudio  && dragProxy.draggedItem == clipRoot.clipId
            visible: clipRoot.width > 4 * width
            MouseArea {
                id: compOutArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: {
                    parent.opacity = 0.7
                }
                onExited: {
                    if (!pressed) {
                        parent.opacity = 0
                    }
                }
                onPressed: {
                    timeline.addCompositionToClip('', clipRoot.clipId, clipRoot.clipDuration - 1)
                }
                onReleased: {
                    parent.opacity = 0
                }
                ToolTip {
                    visible: compOutArea.containsMouse && !dragProxyArea.pressed
                    delay: 1000
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        font.pointSize: root.fontUnit
                        text: i18n("Click to add composition")
                    }
                }
            }
        }


        TimelineTriangle {
            id: fadeInTriangle
            fillColor: 'green'
            width: Math.min(clipRoot.fadeIn * timeScale, clipRoot.width)
            height: clipRoot.height - clipRoot.border.width * 2
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: clipRoot.border.width
            opacity: 0.3
        }
        Rectangle {
            id: fadeInControl
            anchors.left: fadeInTriangle.right
            anchors.leftMargin: fadeInTriangle.width > root.baseUnit ? -root.baseUnit : 0
            anchors.top: fadeInTriangle.top
            anchors.topMargin: -10
            width: root.baseUnit * 2
            height: width
            radius: width / 2
            color: '#FF66FFFF'
            border.width: 2
            border.color: 'green'
            enabled: !isLocked && !dragProxy.isComposition
            opacity: 0
            visible : clipRoot.width > 3 * width
            Drag.active: fadeInMouseArea.drag.active
            MouseArea {
                id: fadeInMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                drag.target: parent
                drag.minimumX: -root.baseUnit
                drag.maximumX: container.width
                drag.axis: Drag.XAxis
                drag.smoothed: false
                property int startX
                property int startFadeIn
                onEntered: parent.opacity = 0.7
                onExited: {
                    if (!pressed) {
                    parent.opacity = 0
                    }
                }
                onPressed: {
                    root.autoScrolling = false
                    startX = Math.round(parent.x / timeScale)
                    startFadeIn = clipRoot.fadeIn
                    parent.anchors.left = undefined
                    parent.opacity = 1
                    fadeInTriangle.opacity = 0.5
                    // parentTrack.clipSelected(clipRoot, parentTrack) TODO
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    fadeInTriangle.opacity = 0.3
                    parent.opacity = 0
                    parent.anchors.left = fadeInTriangle.right
                    console.log('released fade: ', clipRoot.fadeIn)
                    timeline.adjustFade(clipRoot.clipId, 'fadein', clipRoot.fadeIn, startFadeIn)
                    bubbleHelp.hide()
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var delta = Math.round(parent.x / timeScale) - startX
                        var duration = Math.max(0, startFadeIn + delta)
                        duration = Math.min(duration, clipRoot.clipDuration - 1)
                        if (duration != clipRoot.fadeIn) {
                            timeline.adjustFade(clipRoot.clipId, 'fadein', duration, -1)
                            // Show fade duration as time in a "bubble" help.
                            var s = timeline.simplifiedTC(Math.max(duration, 0))
                            bubbleHelp.show(clipRoot.x, parentTrack.y + clipRoot.height, s)
                        }
                    }
                }
            }
            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: fadeInMouseArea.containsMouse && !fadeInMouseArea.pressed
                NumberAnimation {
                    from: 1.0
                    to: 0.7
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.7
                    to: 1.0
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }
        }

        TimelineTriangle {
            id: fadeOutCanvas
            fillColor: 'red'
            width: Math.min(clipRoot.fadeOut * timeScale, clipRoot.width)
            height: clipRoot.height - clipRoot.border.width * 2
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: clipRoot.border.width
            opacity: 0.3
            transform: Scale { xScale: -1; origin.x: fadeOutCanvas.width / 2}
        }
        Rectangle {
            id: fadeOutControl
            anchors.right: fadeOutCanvas.left
            anchors.rightMargin: fadeOutCanvas.width > root.baseUnit ? -root.baseUnit : 0
            anchors.top: fadeOutCanvas.top
            anchors.topMargin: -10
            width: root.baseUnit * 2
            height: width
            radius: width / 2
            color: '#66FFFFFF'
            border.width: 2
            border.color: 'red'
            opacity: 0
            enabled: !isLocked && !dragProxy.isComposition
            Drag.active: fadeOutMouseArea.drag.active
            visible : clipRoot.width > 3 * width
            MouseArea {
                id: fadeOutMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.minimumX: -root.baseUnit
                drag.maximumX: container.width
                property int startX
                property int startFadeOut
                onEntered: parent.opacity = 0.7
                onExited: {
                    if (!pressed) {
                    parent.opacity = 0
                    }
                }
                drag.smoothed: false
                onPressed: {
                    root.autoScrolling = false
                    startX = Math.round(parent.x / timeScale)
                    startFadeOut = clipRoot.fadeOut
                    parent.anchors.right = undefined
                    parent.opacity = 1
                    fadeOutCanvas.opacity = 0.5
                }
                onReleased: {
                    fadeOutCanvas.opacity = 0.3
                    parent.opacity = 0
                    root.autoScrolling = timeline.autoScroll
                    parent.anchors.right = fadeOutCanvas.left
                    var duration = clipRoot.fadeOut
                    if (duration > 0) {
                        duration += 1
                    }
                    timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, startFadeOut)
                    bubbleHelp.hide()
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var delta = startX - Math.round(parent.x / timeScale)
                        var duration = Math.max(0, startFadeOut + delta)
                        duration = Math.min(duration, clipRoot.clipDuration)
                        if (clipRoot.fadeOut != duration) {
                            timeline.adjustFade(clipRoot.clipId, 'fadeout', duration, -1)
                            // Show fade duration as time in a "bubble" help.
                            var s = timeline.simplifiedTC(Math.max(duration, 0))
                            bubbleHelp.show(clipRoot.x + clipRoot.width, parentTrack.y + clipRoot.height, s)
                        }
                    }
                }
            }
            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: fadeOutMouseArea.containsMouse && !fadeOutMouseArea.pressed
                NumberAnimation {
                    from: 1.0
                    to: 0.7
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.7
                    to: 1.0
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    Rectangle {
        id: trimIn
        anchors.left: clipRoot.left
        anchors.leftMargin: 0
        height: parent.height
        enabled: !isLocked
        width: root.baseUnit / 2
        color: isAudio? 'green' : 'lawngreen'
        opacity: 0
        Drag.active: trimInMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        visible: trimInMouseArea.pressed || (root.activeTool === 0 && !mouseArea.drag.active && clipRoot.width > 4 * width)

        ToolTip {
            visible: trimInMouseArea.containsMouse && !trimInMouseArea.pressed
            delay: 1000
            timeout: 5000
            background: Rectangle {
                color: activePalette.alternateBase
                border.color: activePalette.light
            }
            contentItem: Label {
                color: activePalette.text
                font.pointSize: root.fontUnit
                text: i18n("In:%1\nPosition:%2", timeline.simplifiedTC(clipRoot.inPoint),timeline.simplifiedTC(clipRoot.modelStart))
            }
        }

        MouseArea {
            id: trimInMouseArea
            anchors.fill: parent
            hoverEnabled: true
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false
            property bool shiftTrim: false
            property bool controlTrim: false
            property bool sizeChanged: false
            cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor);
            onPressed: {
                root.autoScrolling = false
                clipRoot.originalX = clipRoot.x
                clipRoot.originalDuration = clipDuration
                parent.anchors.left = undefined
                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                controlTrim = mouse.modifiers & Qt.ControlModifier
                if (!shiftTrim && clipRoot.grouped) {
                    clipRoot.initGroupTrim(clipRoot)
                }
                parent.opacity = 0
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
                parent.anchors.left = clipRoot.left
                if (sizeChanged) {
                    clipRoot.trimmedIn(clipRoot, shiftTrim, controlTrim)
                    sizeChanged = false
                }
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var delta = Math.round((trimIn.x) / timeScale)
                    if (delta !== 0) {
                        if (delta < -modelStart) {
                            delta = -modelStart
                        }
                        var newDuration =  clipDuration - delta
                        sizeChanged = true
                        clipRoot.trimmingIn(clipRoot, newDuration, mouse, shiftTrim, controlTrim)
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    parent.opacity = 0.5
                }
            }
            onExited: {
                parent.opacity = 0
            }
        }
    }
    Rectangle {
        id: trimOut
        anchors.right: clipRoot.right
        anchors.rightMargin: 0
        height: parent.height
        width: root.baseUnit / 2
        color: 'red'
        opacity: 0
        enabled: !isLocked
        Drag.active: trimOutMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        visible: trimOutMouseArea.pressed || (root.activeTool === 0 && !mouseArea.drag.active && clipRoot.width > 4 * width)

        ToolTip {
            visible: trimOutMouseArea.containsMouse && !trimOutMouseArea.pressed
            delay: 1000
            timeout: 5000
            background: Rectangle {
                color: activePalette.alternateBase
                border.color: activePalette.light
            }
            contentItem: Label {
                color: activePalette.text
                font.pointSize: root.fontUnit
                text: i18n("Out: ") + timeline.simplifiedTC(clipRoot.outPoint)
            }
        }

        MouseArea {
            id: trimOutMouseArea
            anchors.fill: parent
            hoverEnabled: true
            property bool shiftTrim: false
            property bool controlTrim: false
            property bool sizeChanged: false
            cursorShape: (containsMouse ? Qt.SizeHorCursor : Qt.ClosedHandCursor);
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false

            onPressed: {
                root.autoScrolling = false
                clipRoot.originalDuration = clipDuration
                parent.anchors.right = undefined
                shiftTrim = mouse.modifiers & Qt.ShiftModifier
                controlTrim = mouse.modifiers & Qt.ControlModifier
                if (!shiftTrim && clipRoot.grouped) {
                    clipRoot.initGroupTrim(clipRoot)
                }
                parent.opacity = 0
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
                parent.anchors.right = clipRoot.right
                if (sizeChanged) {
                    clipRoot.trimmedOut(clipRoot, shiftTrim, controlTrim)
                    sizeChanged = false
                }
            }
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    var newDuration = Math.round((parent.x + parent.width) / timeScale)
                    if (newDuration != clipDuration) {
                        sizeChanged = true
                        clipRoot.trimmingOut(clipRoot, newDuration, mouse, shiftTrim, controlTrim)
                    }
                }
            }
            onEntered: {
                if (!pressed) {
                    parent.opacity = 0.5
                }
            }
            onExited: parent.opacity = 0
        }
    }

        /*MenuItem {
            id: mergeItem
            text: i18n("Merge with next clip")
            onTriggered: timeline.mergeClipWithNext(trackIndex, index, false)
        }
        MenuItem {
            text: i18n("Rebuild Audio Waveform")
            onTriggered: timeline.remakeAudioLevels(trackIndex, index)
        }*/
        /*onPopupVisibleChanged: {
            if (visible && application.OS !== 'OS X' && __popupGeometry.height > 0) {
                // Try to fix menu running off screen. This only works intermittently.
                menu.__yOffset = Math.min(0, Screen.height - (__popupGeometry.y + __popupGeometry.height + 40))
                menu.__xOffset = Math.min(0, Screen.width - (__popupGeometry.x + __popupGeometry.width))
            }
        }*/
}
