/*
 *  SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
 *  SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

Rectangle {
    id: keyframe
    visible: K.Core.activeTool === K.ToolType.SelectTool
    required property var model
    required property int index

    required property K.TimelineController timeline
    required property var kfrModel
    required property double timeScale
    required property int keyframeModelOffset
    required property int parentInPoint
    required property int parentItemId
    required property bool allowUserInteraction
    required property int keyframeContainerHeight
    required property int keyframeContainerWidth
    required property color keyframeColor
    required property int consumerPosition

    readonly property bool isUserInteracting: kfMouseArea.pressed || kf1MouseArea.pressed
    readonly property bool isInsideVisibleAreaX: x > K.UiUtils.baseSizeMedium / 2 && x < parent.width - K.UiUtils.baseSizeMedium / 2

    signal requestRepaint()
    signal seek(int position)
    signal seekToIx(int ix)
    signal keyframeSelected(int index, bool add, bool setActive)
    signal resetSelection()
    signal focusKeyframeContainer()
    signal blockAutoScroll(bool block)

    property int frame : model.frame
    property int frameType : model.type
    property string realValue: model.value
    x: (model.frame - parentInPoint) * timeScale
    height: parent.height
    property int value: parent.height * model.normalizedValue
    property int tmpVal : keyframeVal.y + K.UiUtils.baseSizeMedium / 2
    property int tmpPos : x + keyframeVal.x + K.UiUtils.baseSizeMedium / 2
    property int dragPos : -1
    anchors.bottom: parent.bottom

    onFrameTypeChanged: { requestRepaint() }
    onValueChanged: { requestRepaint() }
    onFrameChanged: { requestRepaint() }

    onRealValueChanged: {
        kf1MouseArea.movingVal = kfrModel.realValue(model.normalizedValue)
    }
    width: Math.max(1, keyframe.timeScale / 2)
    color: kfMouseArea.containsMouse ? 'darkred' : 'transparent'

    SystemPalette { id: activePalette }

    MouseArea {
        id: kfMouseArea
        anchors.fill: parent
        anchors.leftMargin: - K.UiUtils.baseSizeMedium / 3
        anchors.rightMargin: - K.UiUtils.baseSizeMedium / 3


        hoverEnabled: keyframe.allowUserInteraction
        cursorShape: Qt.SizeHorCursor
        enabled: keyframe.allowUserInteraction && keyframe.isInsideVisibleAreaX
        drag.target: parent
        drag.smoothed: false
        drag.axis: Drag.XAxis

        onPressed: {
            keyframe.blockAutoScroll(true)
        }

        onReleased: mouse => {
            keyframe.blockAutoScroll(false)
            if (keyframe.frame != keyframe.parentInPoint && keyframe.dragPos != keyframe.frame) {
                if (mouse.modifiers & Qt.ShiftModifier) {
                    // offset all subsequent keyframes
                    keyframe.kfrModel.offsetKeyframes(keyframe.frame, keyframe.dragPos, true)
                } else {
                    keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, true)
                }
            }
            keyframe.dragPos = -1
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                if (keyframe.frame == keyframe.parentInPoint) {
                    parent.x = keyframe.parentInPoint * keyframe.timeScale
                    return
                }
                var newPos = Math.min(Math.round(parent.x / keyframe.timeScale), Math.round(keyframe.keyframeContainerWidth / keyframe.timeScale) - 1)
                if (newPos < 1) {
                    newPos = 1
                }
                if (newPos != keyframe.dragPos && (newPos == 0 || !keyframe.kfrModel.hasKeyframe(keyframe.frame + newPos))) {
                    keyframe.dragPos = newPos
                    parent.x = newPos * keyframe.timeScale
                    keyframe.requestRepaint()
                } else {
                    parent.x = keyframe.dragPos * keyframe.timeScale
                }
            }
        }
        onEntered: {
            if (keyframe.timeline) {
                keyframe.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to move selected keyframes position. <b>Shift drag</b> to move all keyframes after this one."))
            }
        }
        onExited: {
            if (keyframe.timeline) {
                keyframe.timeline.showKeyBinding()
            }
        }
    }
    Item {
        id: keyframeVal
        x: - K.UiUtils.baseSizeMedium / 2
        y: keyframe.keyframeContainerHeight - keyframe.value - K.UiUtils.baseSizeMedium / 2
        width: K.UiUtils.baseSizeMedium
        height: width
        Rectangle {
            anchors.fill: keyframeVal
            anchors.margins: keyframeVal.width / 4
            radius: width / 2
            color: keyframe.model.active ? 'red' : keyframe.model.selected ? 'orange' : (kf1MouseArea.containsMouse || kf1MouseArea.pressed) ? activePalette.text : keyframe.keyframeColor
            border.color: kf1MouseArea.containsMouse || kf1MouseArea.pressed ? activePalette.highlight : activePalette.text
        }

        MouseArea {
            id: kf1MouseArea
            anchors.fill: parent
            hoverEnabled: keyframe.allowUserInteraction
            enabled: keyframe.allowUserInteraction
            cursorShape: shiftPressed ? Qt.SizeVerCursor : Qt.PointingHandCursor
            drag.target: parent
            drag.smoothed: false
            drag.threshold: 1
            property string movingVal: keyframe.kfrModel.realValue(keyframe.model.normalizedValue)
            property double newVal: NaN
            property bool shiftPressed: false
            onPressed: mouse => {
                keyframe.blockAutoScroll(true)
                drag.axis = keyframe.model.moveOnly ? Drag.XAxis : (mouse.modifiers & Qt.ShiftModifier) ? Drag.YAxis : Drag.XAndYAxis
            }
            onClicked: mouse => {
                keyframe.focusKeyframeContainer()
                if (mouse.modifiers & Qt.ControlModifier && keyframe.model.selected) {
                    keyframe.keyframeSelected(keyframe.index, true, false)
                } else {
                    keyframe.keyframeSelected(keyframe.index, mouse.modifiers & Qt.ControlModifier, true)
                }
                var ix = keyframe.kfrModel.activeKeyframe()
                if (ix > -1) {
                    keyframe.seekToIx(ix)
                }
            }
            onReleased: {
                keyframe.blockAutoScroll(false)
                parent.x = - K.UiUtils.baseSizeMedium / 2
                if (isNaN(newVal)) {
                    keyframe.dragPos = -1
                    return
                }
                if (keyframe.dragPos == keyframe.frame && keyframe.value == keyframe.height - parent.y - K.UiUtils.baseSizeMedium / 2) {
                    var pos = keyframe.keyframeModelOffset + keyframe.frame - keyframe.parentInPoint
                    if (keyframe.consumerPosition !== pos) {
                        keyframe.seek(pos)
                    }
                    return
                }
                if (newVal > 1.5 || newVal < -0.5) {
                    if (keyframe.frame != keyframe.parentInPoint) {
                        keyframe.resetSelection()
                        keyframe.kfrModel.removeKeyframe(keyframe.frame);
                    } else {
                        if (newVal < 0) {
                            newVal = 0;
                        } else if (newVal > 1) {
                            newVal = 1;
                        }
                        keyframe.kfrModel.updateKeyframe(keyframe.frame, newVal)
                    }
                } else {
                    if (newVal < 0) {
                        newVal = 0;
                    } else if (newVal > 1) {
                        newVal = 1;
                    }
                    if (keyframe.model.moveOnly) {
                        keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, true)
                    } else {
                        console.log('MOVING KFR TO NEW POS: ', keyframe.dragPos)
                        keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.frame == keyframe.parentInPoint ? keyframe.frame : keyframe.dragPos, newVal, true)
                    }
                }
                keyframe.dragPos = -1
            }
            onPositionChanged: mouse => {
                shiftPressed = (mouse.modifiers & Qt.ShiftModifier)
                if (mouse.buttons === Qt.LeftButton) {
                    if (keyframe.frame == keyframe.parentInPoint) {
                        parent.x = - K.UiUtils.baseSizeMedium / 2
                    } else {
                        var newPos = Math.min(Math.round((parent.x + K.UiUtils.baseSizeMedium / 2) / keyframe.timeScale), Math.round(keyframe.keyframeContainerWidth / keyframe.timeScale) - keyframe.frame + keyframe.parentInPoint - 1) + keyframe.frame
                        if (newPos <= keyframe.parentInPoint) {
                            newPos = keyframe.parentInPoint + 1
                        }

                        if (newPos != keyframe.dragPos && !keyframe.kfrModel.hasKeyframe(newPos)) {
                            keyframe.dragPos = newPos
                            parent.x = (keyframe.dragPos - keyframe.frame) * keyframe.timeScale - K.UiUtils.baseSizeMedium / 2
                        } else {
                            parent.x = (keyframe.dragPos - keyframe.frame) * keyframe.timeScale - K.UiUtils.baseSizeMedium / 2
                        }
                    }
                    keyframe.requestRepaint()
                    newVal = (keyframe.keyframeContainerHeight - (parent.y + mouse.y)) / keyframe.keyframeContainerHeight
                    movingVal = keyframe.kfrModel.realValue(Math.min(Math.max(newVal, 0), 1))
                }
            }
            onDoubleClicked: {
                keyframe.resetSelection()
                keyframe.kfrModel.removeKeyframe(keyframe.frame);
            }
            onEntered: {
                if (keyframe.timeline) {
                    keyframe.timeline.showKeyBinding(KI18n.i18n("<b>Shift drag</b> to change value of selected keyframes, <b>Ctrl click</b> for multiple keyframe selection."))
                }
            }
            onExited: {
                if (keyframe.timeline) {
                    keyframe.timeline.showKeyBinding()
                }
            }
            ToolTip.visible: (containsMouse || pressed) && movingVal != ""
            ToolTip.text: movingVal
        }
    }
}
