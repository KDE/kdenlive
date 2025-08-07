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
    property real baseUnit: 10
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
    width: baseUnit
    height: width
    color: "#99ffffff"
    border.color: "#ff0000"
    visible: showHandle
    opacity: otherResizeHandleInUse ? 0 : isKeyframe ? 1 : 0.4

    // Signals
    signal resize(rect adjustedFrame)
    signal resizeStart()
    signal resizeEnd()
    signal captureRightClick(bool capture)
    signal addRemoveKeyframe()

    // Conditional anchoring based on handle type
    anchors.top: (handleType === ResizeLogic.HandleTypes.TOP_LEFT || 
                  handleType === ResizeLogic.HandleTypes.TOP || 
                  handleType === ResizeLogic.HandleTypes.TOP_RIGHT) ? parent.top : undefined
    anchors.topMargin: anchors.top ? handlesTopMargin : 0
    
    anchors.bottom: (handleType === ResizeLogic.HandleTypes.BOTTOM_LEFT || 
                     handleType === ResizeLogic.HandleTypes.BOTTOM || 
                     handleType === ResizeLogic.HandleTypes.BOTTOM_RIGHT) ? parent.bottom : undefined
    anchors.bottomMargin: anchors.bottom ? handlesBottomMargin : 0
    
    anchors.left: (handleType === ResizeLogic.HandleTypes.TOP_LEFT || 
                   handleType === ResizeLogic.HandleTypes.LEFT || 
                   handleType === ResizeLogic.HandleTypes.BOTTOM_LEFT) ? parent.left : undefined
    anchors.leftMargin: anchors.left ? handlesLeftMargin : 0
    
    anchors.right: (handleType === ResizeLogic.HandleTypes.TOP_RIGHT || 
                    handleType === ResizeLogic.HandleTypes.RIGHT || 
                    handleType === ResizeLogic.HandleTypes.BOTTOM_RIGHT) ? parent.right : undefined
    anchors.rightMargin: anchors.right ? handlesRightMargin : 0
    
    anchors.horizontalCenter: (handleType === ResizeLogic.HandleTypes.TOP || 
                               handleType === ResizeLogic.HandleTypes.BOTTOM) ? parent.horizontalCenter : undefined
    
    anchors.verticalCenter: (handleType === ResizeLogic.HandleTypes.LEFT || 
                             handleType === ResizeLogic.HandleTypes.RIGHT) ? parent.verticalCenter : undefined
    

    MouseArea {
        id: mouseArea
        property int initialMouseX
        property int initialMouseY
        property var initialFrame
        
        acceptedButtons: Qt.LeftButton
        anchors.fill: parent
        cursorShape: ResizeLogic.getCursorShape(ResizeLogic.getVisualHandleType(handle.handleType, handle.rotationAngle))

        onPressed: mouse => {
            handle.captureRightClick(true)
            handle.resizeStart()
            initialMouseX = mouseX
            initialMouseY = mouseY
            initialFrame = Qt.rect(parentFramesize.x, parentFramesize.y, parentFramesize.width, parentFramesize.height)
        }
        
        onPositionChanged: mouse => {
            if (!pressed) return

            // Resize Handles are aligned to the framerect parent which is the red (rotated) rectangle on the Monitor
            // So the mouse x/y coordinates of the mouse event here are aligned to this reference frame not to the screen (relevant when rotation angle != 0)

            // Calculate movement delta from initial press
            var deltaX = mouseX - initialMouseX
            var deltaY = mouseY - initialMouseY
            
            var adjustedFrame = ResizeLogic.calculateResize(
                handleType,
                deltaX / scalex,
                deltaY / scaley,
                initialFrame,
                lockRatio,
                mouse.modifiers,
                rotationAngle
            )

            // Apply snapping to the resized frame (similar to how move operations works)
            var snappedFrame = K.KdenliveSettings.showMonitorGrid ? 
                SnappingLogic.getSnappedResizeRect(adjustedFrame, rotationAngle, handleType, K.KdenliveSettings.monitorGridH, K.KdenliveSettings.monitorGridV) :
                adjustedFrame

            handle.resize(snappedFrame)
            
            if (!isKeyframe && K.KdenliveSettings.autoKeyframe) {
                handle.addRemoveKeyframe()
            }
        }
        
        onReleased: {
            handle.captureRightClick(false)
            handle.resizeEnd()
        }
    }
} 