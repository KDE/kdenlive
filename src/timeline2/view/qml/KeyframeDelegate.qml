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
    property int frame : model.frame
    property int frameType : model.type
    property string realValue: model.value
    x: (model.frame - keyframeContainer.inPoint) * keyframeContainer.timescale
    height: parent.height
    property int value: parent.height * model.normalizedValue
    property int tmpVal : keyframeVal.y + K.UiUtils.baseSizeMedium / 2
    property int tmpPos : x + keyframeVal.x + K.UiUtils.baseSizeMedium / 2
    property int dragPos : -1
    anchors.bottom: parent.bottom
    onFrameTypeChanged: {
        keyframecanvas.requestPaint()
    }
    onValueChanged: {
        keyframecanvas.requestPaint()
    }
    onFrameChanged: {
        keyframecanvas.requestPaint()
        console.log('Frame ', keyframe.index,' MOVED TO: ', keyframe.frame)
    }
    onRealValueChanged: {
        kf1MouseArea.movingVal = kfrModel.realValue(model.normalizedValue)
    }
    width: Math.max(1, keyframeContainer.timescale / 2)
    color: kfMouseArea.containsMouse ? 'darkred' : 'transparent'
    MouseArea {
        id: kfMouseArea
        anchors.fill: parent
        anchors.leftMargin: - K.UiUtils.baseSizeMedium / 3
        anchors.rightMargin: - K.UiUtils.baseSizeMedium / 3
        hoverEnabled: !keyframeContainer.isPanning
        cursorShape: Qt.SizeHorCursor
        enabled: !keyframeContainer.isPanning && parent.x > K.UiUtils.baseSizeMedium / 2 && parent.x < keyframeContainer.width - K.UiUtils.baseSizeMedium / 2
        drag.target: parent
        drag.smoothed: false
        drag.axis: Drag.XAxis
        onPressed: {
            setBlockAutoScroll(true)
        }

        onReleased: mouse => {
            setBlockAutoScroll(false)
            if (keyframe.frame != keyframeContainer.inPoint && keyframe.dragPos != keyframe.frame) {
                if (mouse.modifiers & Qt.ShiftModifier) {
                    // offset all subsequent keyframes
                    kfrModel.offsetKeyframes(keyframe.frame, keyframe.dragPos, true)
                } else {
                    kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, true)
                }
            }
            keyframe.dragPos = -1
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                if (keyframe.frame == keyframeContainer.inPoint) {
                    parent.x = keyframeContainer.inPoint * keyframeContainer.timescale
                    return
                }
                var newPos = Math.min(Math.round(parent.x / keyframeContainer.timescale), Math.round(keyframeContainer.width / keyframeContainer.timescale) - 1)
                if (newPos < 1) {
                    newPos = 1
                }
                if (newPos != keyframe.dragPos && (newPos == 0 || !kfrModel.hasKeyframe(frame + newPos))) {
                    keyframe.dragPos = newPos
                    parent.x = newPos * keyframeContainer.timescale
                    keyframecanvas.requestPaint()
                } else {
                    parent.x = keyframe.dragPos * keyframeContainer.timescale
                }
            }
        }
        onEntered: {
            //timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to move selected keyframes position. <b>Shift drag</b> to move all keyframes after this one."))
        }
        onExited: {
            //timeline.showKeyBinding()
        }
    }
    Rectangle {
        id: keyframeVal
        x: - K.UiUtils.baseSizeMedium / 2
        y: keyframeContainer.height - keyframe.value - K.UiUtils.baseSizeMedium / 2
        width: K.UiUtils.baseSizeMedium
        height: width
        radius: width / 2
        color: keyframe.model.active ? 'red' : keyframe.model.selected ? 'orange' : (kf1MouseArea.containsMouse || kf1MouseArea.pressed) ? keyframeContainer.textColor : keyframeContainer.kfColor
        border.color: kf1MouseArea.containsMouse || kf1MouseArea.pressed ? activePalette.highlight : keyframeContainer.textColor

        MouseArea {
            id: kf1MouseArea
            anchors.fill: parent
            hoverEnabled: !keyframeContainer.isPanning
            enabled: !keyframeContainer.isPanning
            cursorShape: shiftPressed ? Qt.SizeVerCursor : Qt.PointingHandCursor
            drag.target: parent
            drag.smoothed: false
            drag.threshold: 1
            property string movingVal: kfrModel.realValue(keyframe.model.normalizedValue)
            property double newVal: NaN
            property bool shiftPressed: false
            onPressed: mouse => {
                setBlockAutoScroll(true)
                drag.axis = keyframe.model.moveOnly ? Drag.XAxis : (mouse.modifiers & Qt.ShiftModifier) ? Drag.YAxis : Drag.XAndYAxis
            }
            onClicked: mouse => {
                keyframeContainer.focus = true
                if (mouse.modifiers & Qt.ControlModifier && keyframe.model.selected) {
                    kfrModel.setActiveKeyframe(-1)
                    keyframeContainer.activeIndex = -1
                    kfrModel.setSelectedKeyframe(keyframe.index, true)
                } else {
                    kfrModel.setActiveKeyframe(keyframe.index)
                    keyframeContainer.activeIndex = keyframe.index
                    kfrModel.setSelectedKeyframe(keyframe.index, mouse.modifiers & Qt.ControlModifier)
                }
                var ix = kfrModel.activeKeyframe()
                if (ix > -1) {
                    seek(keyframes.itemAt(ix).frame + keyframeContainer.modelStart - keyframeContainer.inPoint)
                }
            }
            onReleased: {
                setBlockAutoScroll(false)
                parent.x = - K.UiUtils.baseSizeMedium / 2
                if (isNaN(newVal)) {
                    keyframe.dragPos = -1
                    return
                }
                if (keyframe.dragPos == keyframe.frame && keyframe.value == keyframe.height - parent.y - K.UiUtils.baseSizeMedium / 2) {
                    var pos = keyframeContainer.modelStart + keyframe.frame - keyframeContainer.inPoint
                    if (proxy.position !== pos) {
                        seek(pos)
                    }
                    return
                }
                if (newVal > 1.5 || newVal < -0.5) {
                    if (keyframe.frame != keyframeContainer.inPoint) {
                        keyframeContainer.resetSelection()
                        kfrModel.removeKeyframe(keyframe.frame);
                    } else {
                        if (newVal < 0) {
                            newVal = 0;
                        } else if (newVal > 1) {
                            newVal = 1;
                        }
                        kfrModel.updateKeyframe(keyframe.frame, newVal)
                    }
                } else {
                    if (newVal < 0) {
                        newVal = 0;
                    } else if (newVal > 1) {
                        newVal = 1;
                    }
                    if (keyframe.model.moveOnly) {
                        kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, true)
                    } else {
                        console.log('MOVING KFR TO NEW POS: ', keyframe.dragPos)
                        kfrModel.moveKeyframe(keyframe.frame, keyframe.frame == keyframeContainer.inPoint ? keyframe.frame : keyframe.dragPos, newVal, true)
                    }
                }
                keyframe.dragPos = -1
            }
            onPositionChanged: mouse => {
                shiftPressed = (mouse.modifiers & Qt.ShiftModifier)
                if (mouse.buttons === Qt.LeftButton) {
                    if (keyframe.frame == keyframeContainer.inPoint) {
                        parent.x = - K.UiUtils.baseSizeMedium / 2
                    } else {
                        var newPos = Math.min(Math.round((parent.x + K.UiUtils.baseSizeMedium / 2) / keyframeContainer.timescale), Math.round(keyframeContainer.width / keyframeContainer.timescale) - keyframe.frame + keyframeContainer.inPoint - 1) + keyframe.frame
                        if (newPos <= keyframeContainer.inPoint) {
                            newPos = keyframeContainer.inPoint + 1
                        }

                        if (newPos != keyframe.dragPos && !kfrModel.hasKeyframe(newPos)) {
                            keyframe.dragPos = newPos
                            parent.x = (keyframe.dragPos - keyframe.frame) * keyframeContainer.timescale - K.UiUtils.baseSizeMedium / 2
                        } else {
                            parent.x = (keyframe.dragPos - keyframe.frame) * keyframeContainer.timescale - K.UiUtils.baseSizeMedium / 2
                        }
                    }
                    keyframecanvas.requestPaint()
                    newVal = (keyframeContainer.height - (parent.y + mouse.y)) / keyframeContainer.height
                    movingVal = kfrModel.realValue(Math.min(Math.max(newVal, 0), 1))
                }
            }
            onDoubleClicked: {
                keyframeContainer.resetSelection()
                kfrModel.removeKeyframe(keyframe.frame);
            }
            onEntered: {
                //timeline.showKeyBinding(KI18n.i18n("<b>Shift drag</b> to change value of selected keyframes, <b>Ctrl click</b> for multiple keyframe selection."))
            }
            onExited: {
                //timeline.showKeyBinding()
            }
            ToolTip.visible: (containsMouse || pressed) && movingVal != ""
            ToolTip.text: movingVal
        }
    }
}
