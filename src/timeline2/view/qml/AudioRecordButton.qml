/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtMultimedia

Rectangle {
    id: recbutton
    width: root.baseUnit * 1.5
    height: root.baseUnit * 1.5
    radius: recState == MediaRecorder.PausedState ? root.baseUnit * 0.2 : root.baseUnit * .75
    color: activePalette.text
    border.width: 0
    
    // Public properties
    property int recState: MediaRecorder.StoppedState
    property int trackId: -1
    property bool isLocked: false

    // Private properties
    readonly property bool isDarkTheme: activePalette.window.hslLightness < activePalette.windowText.hslLightness
    readonly property string buttonFillColorStoppedRecording: isDarkTheme ? "#c62828" : "#b71c1c"
    readonly property string buttonFillColorPaused: isDarkTheme ? 'black' : 'white'
    
    // Visual feedback for disabled state
    opacity: isLocked ? 0.5 : 1.0
    
    // Smooth color transitions for hover effects
    Behavior on color {
        ColorAnimation {
            duration: 200
            easing.type: Easing.OutQuad
        }
    }
    
    // Smooth opacity transitions for disabled state
    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutQuad
        }
    }
    
    // Recording Animation 1: Pulsing scale
    SequentialAnimation {
        id: recordingPulseAnimation
        running: recState == MediaRecorder.RecordingState
        loops: Animation.Infinite
        
        PropertyAnimation {
            target: recbutton
            property: "scale"
            from: 1.0
            to: 1.1
            duration: 800
            easing.type: Easing.InOutQuad
        }
        PropertyAnimation {
            target: recbutton
            property: "scale"
            from: 1.1
            to: 1.0
            duration: 800
            easing.type: Easing.InOutQuad
        }
        PauseAnimation { duration: 200 }
    }
    
    // Recording Animation 2: Expanding ripple effect - creates outward wave when button reaches peak scale
    Rectangle {
        id: recordingRippleEffect
        visible: recState == MediaRecorder.RecordingState
        width: recbutton.width
        height: recbutton.height
        radius: recbutton.radius
        color: "#e53e3e"  // Same red as recording state
        opacity: 0
        anchors.centerIn: recbutton
        border.width: 0
        
        SequentialAnimation {
            id: recordingRippleExpandAnimation
            running: recState == MediaRecorder.RecordingState
            loops: Animation.Infinite
            
            // Wait for button to reach max scale, then start ripple
            PauseAnimation { duration: 800 }
            
            // Expand and fade out
            ParallelAnimation {
                PropertyAnimation {
                    target: recordingRippleEffect
                    property: "scale"
                    from: 1.0
                    to: 2.0
                    duration: 600
                    easing.type: Easing.OutQuad
                }
                PropertyAnimation {
                    target: recordingRippleEffect
                    property: "opacity"
                    from: 0.6
                    to: 0
                    duration: 600
                    easing.type: Easing.OutQuad
                }
            }
            
            // Reset and wait for next cycle
            PauseAnimation { duration: 400 }
        }
    }
    
    // Use a dedicated hover-only area so tooltips still work when locked
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
        onEntered:  {
            if (!isLocked) {
                buttonFill.color = Qt.lighter(buttonFill.color, 1.5)
                recbutton.color = Qt.lighter(activePalette.text, 1.3)
            }
        }
        onExited:  {
            if (!isLocked) {
                if (recState == MediaRecorder.RecordingState) {
                    buttonFill.color = buttonFillColorStoppedRecording
                } else if (recState == MediaRecorder.PausedState) {
                    buttonFill.color = buttonFillColorPaused
                } else {
                    buttonFill.color = buttonFillColorStoppedRecording
                }
                recbutton.color = activePalette.text
            }
        }
    }
    
    ToolTip {
        visible: hoverArea.containsMouse
        delay: 1000
        timeout: 5000
        text: isLocked ? i18n("Track is locked. Cannot start Recording.") : (i18n("Record") + timeline.getActionShortcut("audio_record"))
    }
    
    // Recording Button
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2  // Thicker outer border
        radius: recState == MediaRecorder.PausedState ? parent.radius - 2 : parent.radius - 2
        // Inner border - narrow, inverse of theme
        color: isDarkTheme ? 'black' : 'white'
        border.width: 0
        
        Rectangle {
            id: buttonFill
            anchors.fill: parent
            anchors.margins: 1  // Narrow inner border
            radius: recState == MediaRecorder.PausedState ? parent.radius - 1 : parent.radius - 1
            color: buttonFillColorStoppedRecording
            border.width: 0
            opacity: isLocked ? 0.6 : 1.0
            
            // Smooth color transitions for hover effects
            Behavior on color {
                ColorAnimation {
                    duration: 150
                    easing.type: Easing.OutQuad
                }
            }
            Behavior on opacity {
                NumberAnimation { 
                    duration: 150
                    easing.type: Easing.OutQuad 
                }
            }
            
            // Pause indicator
            Row {
                visible: recState == MediaRecorder.PausedState
                anchors.centerIn: buttonFill
                spacing: buttonFill.width * 0.1
                
                Rectangle {
                    color: isDarkTheme ? 'white' : 'black'
                    width: buttonFill.width * 0.25
                    height: buttonFill.height * 0.7
                    radius: 1
                }
                
                Rectangle {
                    color: isDarkTheme ? 'white' : 'black'
                    width: buttonFill.width * 0.25
                    height: buttonFill.height * 0.7
                    radius: 1
                }
            }
        }
    }
    
    // Click handler kept separate; we only accept clicks when not locked
    MouseArea {
        id: buttonArea
        anchors.fill: parent
        acceptedButtons: isLocked ? Qt.NoButton : Qt.LeftButton
        hoverEnabled: false
        cursorShape: isLocked ? Qt.ArrowCursor : Qt.PointingHandCursor
        
        onClicked: {
            if (!isLocked) {
                flashAnimation.start()
                timeline.switchRecording(trackId, recState == MediaRecorder.RecordingState ? MediaRecorder.StoppedState : MediaRecorder.RecordingState)
            }
        }
    }
    
    // Flash animation for immediate click feedback
    SequentialAnimation {
        id: flashAnimation
        PropertyAnimation {
            target: buttonFill
            property: "opacity"
            to: 0.7
            duration: 50
            easing.type: Easing.OutQuad
        }
        PropertyAnimation {
            target: buttonFill
            property: "opacity"
            to: 1.0
            duration: 100
            easing.type: Easing.OutQuad
        }
    }
    
    onRecStateChanged: {
        // Ensure animations are properly stopped when state changes
        if (recState != MediaRecorder.RecordingState) {
            recordingPulseAnimation.stop()
            recordingRippleExpandAnimation.stop()
            recbutton.scale = 1.0
            recordingRippleEffect.scale = 1.0
            recordingRippleEffect.opacity = 0
        }
        
        if (recState == MediaRecorder.RecordingState) {
            buttonFill.color = buttonFillColorStoppedRecording
            ToolTip.text = i18n("Stop")
        } else if (recState == MediaRecorder.PausedState) {
            buttonFill.color = buttonFillColorPaused
            buttonFill.opacity = 1.0
            ToolTip.text = i18n("Paused")
        } else {
            buttonFill.color = buttonFillColorStoppedRecording
            buttonFill.opacity = 1.0
            ToolTip.text = isLocked ? i18n("Track is locked. Cannot start Recording.") : (i18n("Record") + timeline.getActionShortcut("audio_record"))
        }
    }
}
