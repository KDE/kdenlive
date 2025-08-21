/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
    
    This QML implementation reimplements the C++ audio levels styling from audiomixer/audiolevels.
    We implement a single style configuration:
    - AudioLevel::LevelStyle::Solid (solid color segments instead of gradients)
    - PeakIndicatorStyle::Monochrome (peak indicator uses WindowText color)
    - Tick marks are always visible but no tick dB labels
*/

import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: levelsContainer
    width: parent.width
    height: parent.height - 1
    x: 1
    y: 1
    ToolTip.text: i18n("Mic level")
    ToolTip.visible: levelArea.containsMouse
    ToolTip.delay: 1000
    ToolTip.timeout: 5000
    // Use background color matching C++ getChannelBackgroundColor()
    color: Qt.darker(activePalette.window, 1.04)
    
    // Public properties
    property var audioLevels: []
    
    // Private properties
    readonly property int channelCount: audioLevels.length === 0 ? 2 : audioLevels.length
    
    // IEC Scale function implementation (matching C++ IEC_Scale.h)
    // Converts dB values to visual scale positions
    function iecScale(dB) {
        if (dB < -70.0) {
            return 0.0
        } else if (dB < -60.0) {
            return (dB + 70.0) * 0.0025
        } else if (dB < -50.0) {
            return (dB + 60.0) * 0.005 + 0.025
        } else if (dB < -40.0) {
            return (dB + 50.0) * 0.0075 + 0.075
        } else if (dB < -30.0) {
            return (dB + 40.0) * 0.015 + 0.15
        } else if (dB < -20.0) {
            return (dB + 30.0) * 0.02 + 0.3
        } else if (dB < -0.001 || dB > 0.001) {
            return (dB + 20.0) * 0.025 + 0.5
        } else {
            return 1.0
        }
    }
    
    // Normalizes IEC scale to 0-1 range relative to 0dB
    function iecScaleMax(dB) {
        return iecScale(dB) / iecScale(0.0)
    }
    
    // Background color for each channel
    Repeater {
        model: channelCount
        id: bgRepeater
        Rectangle {
            required property int index
            width: levelsContainer.width - 2
            height: Math.max(1, levelsContainer.height / bgRepeater.count - 2)
            x: 1
            y: (height + 1) * index
            // Solid background color matching C++ getChannelBackgroundColor()
            color: Qt.darker(activePalette.window, 1.04)
        }
    }
    
    // Solid level fill for each channel
    Repeater {
        model: channelCount
        id: levelRepeater
        property int lastPos: 0
        property int currentPos: 0
        Item {
            anchors.fill: parent
            anchors.margins: 1
            property double currentLevel: audioLevels.length <= 0 || audioLevels.length === undefined ? 0 : audioLevels[index]
            property double peak: 0
            
            // Thresholds from AudioLevelStyleProvider::LevelColors
            // These define the visual boundaries between green/yellow/red segments
            readonly property double greenThresholdPos: iecScaleMax(-18.0)
            readonly property double yellowThresholdPos: iecScaleMax(-6.0)
            readonly property double redThresholdPos: iecScaleMax(-2.0)
            
            // Channel layout calculations
            readonly property int channelHeight: parent.height / levelRepeater.count
            readonly property int channelY: channelHeight * index
            readonly property int levelWidth: parent.width * currentLevel
            
            // Green segment (0 to greenThreshold or currentLevel, whichever is smaller)
            Rectangle {
                visible: currentLevel > 0
                color: "#14be14"  // green: QColor(20, 190, 20)
                width: Math.min(parent.width * greenThresholdPos, levelWidth)
                height: channelHeight
                x: 0
                y: channelY
            }
            
            // Yellow segment (greenThreshold to yellowThreshold)
            Rectangle {
                visible: currentLevel > greenThresholdPos
                color: "#f8cc1b"  // yellow: QColor(248, 204, 27)
                width: Math.min(parent.width * (yellowThresholdPos - greenThresholdPos), 
                               Math.max(0, levelWidth - parent.width * greenThresholdPos))
                height: channelHeight
                x: parent.width * greenThresholdPos
                y: channelY
            }
            
            // Red segment (yellowThreshold to currentLevel)
            Rectangle {
                visible: currentLevel > yellowThresholdPos
                color: "#e12729"  // red: QColor(225, 39, 41)
                width: Math.max(0, levelWidth - parent.width * yellowThresholdPos)
                height: channelHeight
                x: parent.width * yellowThresholdPos
                y: channelY
            }
            
            // Peak tracking with decay
            onCurrentLevelChanged: {
                if (currentLevel > peak) {
                    peak = currentLevel
                } else if (peak > 0) {
                    peak -= 0.02
                }
            }
            
            // Peak indicator (Monochrome style)
            Rectangle {
                visible: peak > 0
                color: activePalette.text
                width: 2
                height: channelHeight
                x: parent.width * peak
                y: channelY
            }
        }
    }
    
    // Channel separator lines
    Repeater {
        model: Math.max(0, channelCount - 1)
        Rectangle {
            required property int index
            width: levelsContainer.width - 2
            height: 1
            x: 1
            y: (levelsContainer.height / bgRepeater.count) * (index + 1) - 1
            color: "#000000"  // Border color matching C++ getBorderColor
        }
    }
    
    // Main frame
    Rectangle {
        anchors.fill: parent
        color: "#00000000"
        border.color: "#000000"
        border.width: 1
    }
    
    MouseArea {
        id: levelArea
        hoverEnabled: true
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
    }
    
    // Tick marks below levels
    Repeater {
        model: [
            { db: 0, pos: iecScaleMax(0) },
            { db: -6, pos: iecScaleMax(-6) },
            { db: -12, pos: iecScaleMax(-12) },
            { db: -18, pos: iecScaleMax(-18) },
            { db: -24, pos: iecScaleMax(-24) },
            { db: -30, pos: iecScaleMax(-30) },
            { db: -36, pos: iecScaleMax(-36) },
            { db: -42, pos: iecScaleMax(-42) },
            { db: -48, pos: iecScaleMax(-48) },
            { db: -54, pos: iecScaleMax(-54) }
        ]
        Rectangle {
            required property var modelData
            width: 1
            height: 2  // TICK_MARK_LENGTH = 2px from C++ implementation
            x: (levelsContainer.width - 2) * modelData.pos  // Position relative to levels area, accounting for border
            y: levelsContainer.height  // Just below the levels container
            color: activePalette.text
            opacity: 0.8  // Matching C++ opacity
        }
    }
}
