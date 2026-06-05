/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import org.kde.kdenlive as K
import "ResizeLogic.js" as ResizeLogic
import "SnappingLogic.js" as SnappingLogic

Rectangle {
    id: handle
    
    // Configuration properties
    property string handleType: "TOP_LEFT"
    property alias cursorShape: mouseArea.cursorShape
    property bool showHandle: true
    property bool otherResizeHandleInUse: false
    
    // Frame properties passed from parent
    property var parentFramesize: null  // Reference to parent's framesize
    property real scalex: 1
    property real scaley: 1
    property real lockRatio: -1
    property bool isKeyframe: false
    property real rotationAngle: 0
    
    // Margin properties from parent
    property real handlesTopMargin: 0
    property real handlesBottomMargin: 0
    property real handlesLeftMargin: 0
    property real handlesRightMargin: 0
    
    // Styling
    width: K.UiUtils.baseSizeMedium
    height: width
    color: "#99ffffff"
    border.color: "#ff0000"
    visible: showHandle
    opacity: otherResizeHandleInUse ? 0 : isKeyframe ? 1 : 0.4

    // Signals
    signal resize(rect adjustedFrame)
    signal resizeStart()
    signal resizeEnd()
    signal addRemoveKeyframe()

    // Conditional anchoring based on handle type
    states: [
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.TOP_LEFT
            AnchorChanges {
                target: handle
                anchors.top: parent.top
                anchors.verticalCenter: undefined
                anchors.bottom: undefined
                anchors.left: parent.left
                anchors.horizontalCenter: undefined
                anchors.right: undefined
            }
            PropertyChanges {
                handle.anchors.topMargin: handlesTopMargin
                handle.anchors.bottomMargin: 0
                handle.anchors.rightMargin: 0
                handle.anchors.leftMargin: handlesLeftMargin
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.TOP
            AnchorChanges {
                target: handle
                anchors.top: parent.top
                anchors.verticalCenter: undefined
                anchors.bottom: undefined
                anchors.left: undefined
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.right: undefined
            }
            PropertyChanges {
                handle.anchors.topMargin: handlesTopMargin
                handle.anchors.bottomMargin: 0
                handle.anchors.rightMargin: 0
                handle.anchors.leftMargin: 0
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.TOP_RIGHT
            AnchorChanges {
                target: handle
                anchors.top: parent.top
                anchors.verticalCenter: undefined
                anchors.bottom: undefined
                anchors.left: undefined
                anchors.horizontalCenter: undefined
                anchors.right: parent.right
            }
            PropertyChanges {
                handle.anchors.topMargin: handlesTopMargin
                handle.anchors.bottomMargin: 0
                handle.anchors.rightMargin: handlesRightMargin
                handle.anchors.leftMargin: 0
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.RIGHT
            AnchorChanges {
                target: handle
                anchors.top: undefined
                anchors.verticalCenter: parent.verticalCenter
                anchors.bottom: undefined
                anchors.left: undefined
                anchors.horizontalCenter: undefined
                anchors.right: parent.right
            }
            PropertyChanges {
                handle.anchors.topMargin: 0
                handle.anchors.bottomMargin: 0
                handle.anchors.rightMargin: handlesRightMargin
                handle.anchors.leftMargin: 0
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.BOTTOM_RIGHT
            AnchorChanges {
                target: handle
                anchors.top: undefined
                anchors.verticalCenter: undefined
                anchors.bottom: parent.bottom
                anchors.left: undefined
                anchors.horizontalCenter: undefined
                anchors.right: parent.right
            }
            PropertyChanges {
                handle.anchors.topMargin: 0
                handle.anchors.bottomMargin: handlesBottomMargin
                handle.anchors.rightMargin: handlesRightMargin
                handle.anchors.leftMargin: 0
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.BOTTOM
            AnchorChanges {
                target: handle
                anchors.top: undefined
                anchors.verticalCenter: undefined
                anchors.bottom: parent.bottom
                anchors.left: undefined
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.right: undefined
            }
            PropertyChanges {
                handle.anchors.topMargin: 0
                handle.anchors.bottomMargin: handlesBottomMargin
                handle.anchors.rightMargin: 0
                handle.anchors.leftMargin: 0
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.BOTTOM_LEFT
            AnchorChanges {
                target: handle
                anchors.top: undefined
                anchors.verticalCenter: undefined
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.horizontalCenter: undefined
                anchors.right: undefined
            }
            PropertyChanges {
                handle.anchors.topMargin: 0
                handle.anchors.bottomMargin: handlesBottomMargin
                handle.anchors.rightMargin: 0
                handle.anchors.leftMargin: handlesLeftMargin
            }
        },
        State {
            when: handle.handleType === ResizeLogic.HandleTypes.LEFT
            AnchorChanges {
                target: handle
                anchors.top: undefined
                anchors.verticalCenter: parent.verticalCenter
                anchors.bottom: undefined
                anchors.left: parent.left
                anchors.horizontalCenter: undefined
                anchors.right: undefined
            }
            PropertyChanges {
                handle.anchors.topMargin: 0
                handle.anchors.bottomMargin: 0
                handle.anchors.rightMargin: 0
                handle.anchors.leftMargin: handlesLeftMargin
            }
        }
    ]

    MouseArea {
        id: mouseArea
        property int initialMouseX
        property int initialMouseY
        property var initialFrame
        
        acceptedButtons: Qt.LeftButton
        anchors.fill: parent
        cursorShape: ResizeLogic.getCursorShape(ResizeLogic.getVisualHandleType(handle.handleType, handle.rotationAngle))

        onPressed: mouse => {
            handle.resizeStart()
            initialMouseX = mouseX
            initialMouseY = mouseY
            initialFrame = Qt.rect(handle.parentFramesize.x, handle.parentFramesize.y, handle.parentFramesize.width, handle.parentFramesize.height)
        }
        
        onPositionChanged: mouse => {
            if (!pressed) return

            // Resize Handles are aligned to the framerect parent which is the red (rotated) rectangle on the Monitor
            // So the mouse x/y coordinates of the mouse event here are aligned to this reference frame not to the screen (relevant when rotation angle != 0)

            // Calculate movement delta from initial press
            var deltaX = mouseX - initialMouseX
            var deltaY = mouseY - initialMouseY
            
            var adjustedFrame = ResizeLogic.calculateResize(
                handle.handleType,
                deltaX / handle.scalex,
                deltaY / handle.scaley,
                initialFrame,
                handle.lockRatio,
                mouse.modifiers,
                handle.rotationAngle
            )

            // Apply snapping to the resized frame (similar to how move operations works)
            var snappedFrame = K.KdenliveSettings.showMonitorGrid ? 
                SnappingLogic.getSnappedResizeRect(adjustedFrame, handle.rotationAngle, handle.handleType, K.KdenliveSettings.monitorGridH, K.KdenliveSettings.monitorGridV) :
                adjustedFrame

            handle.resize(snappedFrame)
            
            if (!handle.isKeyframe && K.KdenliveSettings.autoKeyframe) {
                handle.addRemoveKeyframe()
            }
        }
        
        onReleased: {
            handle.resizeEnd()
        }
    }
} 