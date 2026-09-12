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
    required property bool mouseInsideView
    property bool isActiveKeyframe: model.active

    readonly property bool isUserInteracting: kfMouseArea.pressed || kf1MouseArea.pressed
    readonly property bool isInsideVisibleAreaX: mouseInsideView && x > K.UiUtils.baseSizeMedium / 2 && x < parent.width - K.UiUtils.baseSizeMedium / 2

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
    property double pixelValue: keyframe.keyframeContainerHeight - keyframe.value
    property double pixelPos: x
    property int dragPos : -1
    property int clickPos : -1
    property double clickVal
    anchors.bottom: parent.bottom

    onFrameTypeChanged: { requestRepaint() }
    onValueChanged: { requestRepaint() }
    onFrameChanged: { requestRepaint() }

    onIsActiveKeyframeChanged: {
        console.log(' - - -KEYFRAME ACTIVE CHANGED: ', isActiveKeyframe)
    }

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
            keyframe.dragPos = keyframe.frame
            keyframe.clickPos = keyframe.frame
        }

        onReleased: mouse => {
            keyframe.blockAutoScroll(false)
            if (keyframe.frame != keyframe.parentInPoint && keyframe.dragPos != keyframe.clickPos) {
                // Restore original position
                keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.clickPos, false)
                if (mouse.modifiers & Qt.ShiftModifier) {
                    // offset all subsequent keyframes
                    keyframe.kfrModel.offsetKeyframes(keyframe.frame, keyframe.dragPos, true)
                } else {
                    keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, true)
                }
            }
            // Restore binding
            keyframe.x = Qt.binding(function () {
                return (keyframe.model.frame - keyframe.parentInPoint) * keyframe.timeScale
            })
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
                if (newPos != keyframe.dragPos && (newPos == 0 || !keyframe.kfrModel.hasKeyframe(keyframe.parentInPoint + newPos))) {
                    keyframe.dragPos = newPos
                    parent.x = newPos * keyframe.timeScale
                    keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, false)
                } else {
                    parent.x = (keyframe.frame - keyframe.parentInPoint) * keyframe.timeScale
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
    MouseArea {
        id: kf1MouseArea
        width: K.UiUtils.baseSizeMedium
        anchors.horizontalCenter: parent.horizontalCenter
        y: keyframe.pixelValue - kf1MouseArea.width / 2
        height: width
        hoverEnabled: keyframe.allowUserInteraction
        enabled: keyframe.allowUserInteraction && keyframe.mouseInsideView
        cursorShape: kf1MouseArea.shiftPressed ? Qt.SizeVerCursor : Qt.PointingHandCursor
        //drag.target: keyframeVal
        drag.smoothed: false
        drag.threshold: 1
        drag.axis: kf1MouseArea.shiftPressed ? Drag.YAxis : Drag.XAndYAxis
        property string movingVal: keyframe.kfrModel.realValue(keyframe.model.normalizedValue)
        property double newVal: NaN
        property bool shiftPressed: false
        onPressed: mouse => {
            kf1MouseArea.shiftPressed = mouse.modifiers & Qt.ShiftModifier
            keyframe.blockAutoScroll(true)
            keyframe.dragPos = keyframe.frame
            keyframe.clickPos = keyframe.frame
            keyframe.clickVal = keyframe.model.normalizedValue
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
            shiftPressed = false
            keyframe.blockAutoScroll(false)
            //parent.x = - K.UiUtils.baseSizeMedium / 2
            if (isNaN(newVal)) {
                keyframe.dragPos = -1
                return
            }
            if (keyframe.dragPos == keyframe.frame && newVal == keyframe.clickVal) {
                let pos = keyframe.frame - keyframe.parentInPoint
                keyframe.seek(pos)
                return
            }
            // silently revert to previous pos/value for undo
            keyframe.kfrModel.moveKeyframe(keyframe.dragPos, keyframe.clickPos, keyframe.clickVal, false)

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
                    keyframe.kfrModel.updateKeyframe(keyframe.frame, newVal, true)
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
                    keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.frame == keyframe.parentInPoint ? keyframe.frame : keyframe.dragPos, newVal, true)
                }
            }
            let pos = keyframe.frame - keyframe.parentInPoint
            keyframe.seek(pos)
            keyframe.dragPos = -1
        }

        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                if (keyframe.frame == keyframe.parentInPoint) {
                    kf1MouseArea.x = - kf1MouseArea.width / 2
                } else {
                    var newPos = Math.min(Math.round(mouse.x / keyframe.timeScale), Math.round(keyframe.keyframeContainerWidth / keyframe.timeScale) - 1) + keyframe.frame
                    if (newPos <= keyframe.parentInPoint) {
                        newPos = keyframe.parentInPoint + 1
                    }

                    if (!kf1MouseArea.shiftPressed && newPos != keyframe.dragPos && !keyframe.kfrModel.hasKeyframe(newPos)) {
                        keyframe.dragPos = newPos
                    }
                    kf1MouseArea.x = (keyframe.dragPos - keyframe.frame) * keyframe.timeScale - K.UiUtils.baseSizeMedium / 2
                }
                newVal = (keyframe.keyframeContainerHeight - (kf1MouseArea.y + mouse.y)) / keyframe.keyframeContainerHeight
                movingVal = keyframe.kfrModel.realValue(Math.min(Math.max(newVal, 0), 1))
                if (keyframe.frame == keyframe.dragPos) {
                    keyframe.kfrModel.updateKeyframe(keyframe.frame, newVal, false)
                } else {
                    keyframe.kfrModel.moveKeyframe(keyframe.frame, keyframe.dragPos, newVal, false)
                }
            }
        }
        onDoubleClicked: {
            keyframe.resetSelection()
            keyframe.kfrModel.removeKeyframe(keyframe.frame);
        }
        onEntered: {
            if (!pressed && keyframe.timeline) {
                keyframe.timeline.showKeyBinding(KI18n.i18n("<b>Shift drag</b> to change value of selected keyframes, <b>Ctrl click</b> for multiple keyframe selection."))
            }
        }
        onExited: {
            if (!pressed && keyframe.timeline) {
                keyframe.timeline.showKeyBinding()
            }
        }
        ToolTip.visible: (kf1MouseArea.containsMouse || kf1MouseArea.pressed) && movingVal != ""
        ToolTip.text: movingVal
        Rectangle {
            id: keyframeVal
            anchors.fill: parent
            property bool highlightKF: kf1MouseArea.containsMouse || kf1MouseArea.pressed
            anchors.margins: highlightKF ? K.UiUtils.baseSizeMedium * 0.1 : keyframe.mouseInsideView ? K.UiUtils.baseSizeMedium * 0.3 : K.UiUtils.baseSizeMedium * 0.4
            radius: width / 2
            color: keyframe.model.active ? 'red' : keyframe.model.selected ? activePalette.highlight : activePalette.text
            border.color: highlightKF ? activePalette.highlight : activePalette.text
            border.width: highlightKF ? 1 : 0
        }
    }
}
