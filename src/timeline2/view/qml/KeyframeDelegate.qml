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
    required property int parentInPoint
    required property int parentItemId
    required property bool allowUserInteraction

    readonly property bool isUserInteracting: kfMouseArea.pressed || kf1MouseArea.pressed
    readonly property bool isInsideVisibleAreaX: x > K.UiUtils.baseSizeMedium / 2 && x < parent.width - K.UiUtils.baseSizeMedium / 2

    signal requestRepaint()
    signal seek(int position)
    signal keyframeSelected(int index, bool add, bool setActive)
    signal resetSelection()

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

    onRealValueChanged: {
        kf1MouseArea.movingVal = kfrModel.realValue(model.normalizedValue)
    }
    width: Math.max(1, timeScale / 2)
    color: kfMouseArea.containsMouse ? 'darkred' : 'transparent'
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

        onReleased: mouse => {
            keyframe.dragPos = -1
            var newPos = Math.round(parent.x / keyframe.timeScale) + keyframe.parentInPoint
            if (keyframe.frame != keyframe.parentInPoint && newPos != keyframe.frame) {
                if (mouse.modifiers & Qt.ShiftModifier) {
                    // offset all subsequent keyframes
                    // TODO: rewrite using timeline to ensure all kf parameters are updated
                    // keyframe.timeline.offsetKeyframes(keyframe.parentItemId, keyframe.frame, newPos)
                } else {
                    keyframe.timeline.updateEffectKeyframe(keyframe.parentItemId, keyframe.frame, newPos)
                }
            }
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                if (keyframe.frame == keyframe.parentInPoint) {
                    parent.x = keyframe.parentInPoint * keyframe.timeScale
                    return
                }
                var newPos = Math.min(Math.round(parent.x / keyframe.timeScale), Math.round(keyframeContainer.width / keyframe.timeScale) - 1)
                if (newPos < 1) {
                    newPos = 1
                }
                if (newPos != keyframe.dragPos && (newPos == 0 || !keyframe.timeline.hasKeyframeAt(keyframe.parentItemId, keyframe.frame + newPos))) {
                    keyframe.dragPos = newPos
                    parent.x = newPos * keyframe.timeScale
                    keyframe.requestRepaint()
                } else {
                    parent.x = keyframe.dragPos * keyframe.timeScale
                }
            }
        }
        onEntered: {
            keyframe.timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to move selected keyframes position. <b>Shift drag</b> to move all keyframes after this one."))
        }
        onExited: {
            keyframe.timeline.showKeyBinding()
        }
    }
    Rectangle {
        id: keyframeVal
        x: - K.UiUtils.baseSizeMedium / 2
        y: keyframeContainer.height - keyframe.value - K.UiUtils.baseSizeMedium / 2
        width: K.UiUtils.baseSizeMedium
        height: width
        radius: width / 2
        color: keyframe.model.active ? 'red' : keyframe.model.selected ? 'orange' : (kf1MouseArea.containsMouse || kf1MouseArea.pressed) ? root.textColor : root.videoColor
        border.color: kf1MouseArea.containsMouse || kf1MouseArea.pressed ? activePalette.highlight : root.textColor

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
                drag.axis = keyframe.model.moveOnly ? Drag.XAxis : (mouse.modifiers & Qt.ShiftModifier) ? Drag.YAxis : Drag.XAndYAxis
            }
            onClicked: mouse => {
                keyframeContainer.focus = true
                if (mouse.modifiers & Qt.ControlModifier && keyframe.model.selected) {
                    keyframe.keyframeSelected(keyframe.index, true, false)
                } else {
                    keyframe.keyframeSelected(keyframe.index, mouse.modifiers & Qt.ControlModifier, true)
                }
                var ix = keyframe.kfrModel.activeKeyframe()
                if (ix > -1) {
                    keyframe.seek(keyframes.itemAt(ix).frame + keyframeContainer.modelStart - keyframe.parentInPoint)
                }
            }
            onReleased: {
                if (isNaN(newVal)) {
                    return
                }
                var newPos = keyframe.frame == keyframe.parentInPoint ? keyframe.parentInPoint : Math.round((keyframe.x + parent.x + K.UiUtils.baseSizeMedium / 2) / keyframe.timeScale) + keyframe.parentInPoint
                if (newPos === keyframe.frame && keyframe.value == keyframe.height - parent.y - K.UiUtils.baseSizeMedium / 2) {
                    var pos = keyframeContainer.modelStart + keyframe.frame - keyframe.parentInPoint
                    keyframe.seek(pos)
                    return
                }
                if (newVal > 1.5 || newVal < -0.5) {
                    if (keyframe.frame != keyframe.parentInPoint) {
                        keyframe.resetSelection()
                        keyframe.timeline.removeEffectKeyframe(keyframe.parentItemId, keyframe.frame);
                    } else {
                        if (newVal < 0) {
                            newVal = 0;
                        } else if (newVal > 1) {
                            newVal = 1;
                        }
                        keyframe.timeline.updateEffectKeyframe(keyframe.parentItemId, keyframe.frame, keyframe.frame, newVal)
                    }
                } else {
                    if (newVal < 0) {
                        newVal = 0;
                    } else if (newVal > 1) {
                        newVal = 1;
                    }
                    if (keyframe.model.moveOnly) {
                        keyframe.timeline.updateEffectKeyframe(keyframe.parentItemId, keyframe.frame, newPos)
                    } else {
                        keyframe.timeline.updateEffectKeyframe(keyframe.parentItemId, keyframe.frame, keyframe.frame == keyframe.parentInPoint ? keyframe.frame : newPos, newVal)
                    }
                }
            }
            onPositionChanged: mouse => {
                shiftPressed = (mouse.modifiers & Qt.ShiftModifier)
                if (mouse.buttons === Qt.LeftButton) {
                    if (keyframe.frame == keyframe.parentInPoint) {
                        parent.x = - K.UiUtils.baseSizeMedium / 2
                    } else {
                        var newPos = Math.min(Math.round((parent.x + K.UiUtils.baseSizeMedium / 2) / keyframe.timeScale), Math.round(keyframeContainer.width / keyframe.timeScale) - keyframe.frame + keyframeContainer.inPoint - 1)
                        if (keyframe.frame + newPos <= keyframe.parentInPoint) {
                            newPos = keyframe.parentInPoint + 1 - keyframe.frame
                        }
                        if (newPos != keyframe.dragPos && (newPos == 0 || !keyframe.timeline.hasKeyframeAt(keyframe.parentItemId, keyframe.frame + newPos))) {
                            keyframe.dragPos = newPos
                            parent.x = newPos * keyframe.timeScale - K.UiUtils.baseSizeMedium / 2
                            keyframe.requestRepaint()
                        } else {
                            parent.x = keyframe.dragPos * keyframe.timeScale - K.UiUtils.baseSizeMedium / 2
                        }
                    }
                    keyframe.requestRepaint()
                    newVal = (keyframeContainer.height - (parent.y + mouse.y)) / keyframeContainer.height
                    movingVal = keyframe.kfrModel.realValue(Math.min(Math.max(newVal, 0), 1))
                }
            }
            onDoubleClicked: {
                keyframe.resetSelection()
                keyframe.timeline.removeEffectKeyframe(keyframe.parentItemId, keyframe.frame);
            }
            onEntered: {
                keyframe.timeline.showKeyBinding(KI18n.i18n("<b>Shift drag</b> to change value of selected keyframes, <b>Ctrl click</b> for multiple keyframe selection."))
            }
            onExited: {
                keyframe.timeline.showKeyBinding()
            }
            ToolTip.visible: (containsMouse || pressed) && movingVal != ""
            ToolTip.text: movingVal
        }
    }
}
