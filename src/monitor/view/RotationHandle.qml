/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import org.kde.kdenlive as K

Rectangle {
    id: rotationHandle
    
    // Configuration properties
    property real baseUnit: 10
    property bool rotatable: true
    property bool showHandle: true
    property int smallRectMargin: 0
    
    // State properties
    property bool iskeyframe: false
    property real rotationAngle: 0
    
    // Signals
    signal rotationStart()
    signal rotationEnd()
    // Returns rotation angle between -360 and 360 (inclusive)
    signal rotationChanged(real angle)
    signal captureRightClick(bool capture)
    signal addRemoveKeyframe()
    
    // Positioning
    anchors {
        horizontalCenter: parent.horizontalCenter
        top: parent.top
        topMargin: parent.height / 4
    }
    
    width: baseUnit
    height: width
    radius: width / 2
    color: "#99ffffff"
    border.color: "#ff0000"
    visible: rotatable && showHandle && smallRectMargin == 0
    opacity: iskeyframe ? 1 : 0.4
    
    MouseArea {
        property point center
        property int oldMouseX
        property int oldMouseY
        
        id: rotateMouseArea
        acceptedButtons: Qt.LeftButton
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor

        function calculateNewAngle(mouse, startingAngle) {
            // Calculate current angle based on Mouse position
            var mousePos = mapToItem(null, mouse.x, mouse.y)
            var currentAngle = Math.atan2(mousePos.y - center.y, mousePos.x - center.x) * (180/Math.PI)
            currentAngle = (currentAngle + 90) % 360
            
            // Calculate the angle difference from the starting position
            var angleDiff = currentAngle - (startingAngle % 360)
            
            // Handle angle wrapping for continuous rotation
            if (angleDiff > 180) {
                angleDiff -= 360
            } else if (angleDiff < -180) {
                angleDiff += 360
            }
            
            var newAngle = (startingAngle + angleDiff)
            
            // Normalize angle to equivalent value between -360 and +360 (inclusive ends)
            while (newAngle > 360) {
                newAngle -= 360
            }
            while (newAngle < -360) {
                newAngle += 360
            }

            return newAngle
        }
        
        onPressed: mouse => {
            rotationHandle.captureRightClick(true)
            rotationHandle.rotationStart()
            oldMouseX = mouseX
            oldMouseY = mouseY
            // Calculate center relative to the global coordinate system
            var globalCenter = rotationHandle.parent.mapToItem(null, 
                rotationHandle.parent.width/2, 
                rotationHandle.parent.height/2)
            center = globalCenter
        }
        
        onPositionChanged: mouse => {
            if (!pressed) return
            
            rotationHandle.rotationAngle = calculateNewAngle(mouse, rotationHandle.rotationAngle)
            rotationHandle.rotationChanged(rotationHandle.rotationAngle)
            
            if (rotationHandle.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                rotationHandle.addRemoveKeyframe()
            }
        }
        
        onReleased: {
            rotationHandle.captureRightClick(false)
            rotationHandle.rotationEnd()
        }
        
        onDoubleClicked: {
            // Reset rotation to 0 on double-click
            rotationHandle.rotationAngle = 0
            rotationHandle.rotationChanged(rotationHandle.rotationAngle)
            if (rotationHandle.iskeyframe == false && K.KdenliveSettings.autoKeyframe) {
                rotationHandle.addRemoveKeyframe()
            }
        }
    }
} 