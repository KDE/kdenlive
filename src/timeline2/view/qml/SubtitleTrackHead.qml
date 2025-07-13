/*
    SPDX-FileCopyrightText: 2017-2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: trackHeader
    border.color: frameColor
    border.width: 1
    required property int collapsedHeight
    required property bool collapsed
    required property bool isDisabled
    required property bool isLocked

    function animateLock() {
        flashLock.restart();
    }

    visible: height > 0
    color: (controller && controller.isSubtitleTrack(timeline.activeTrack)) ? Qt.tint(getTrackColor(false, false), selectedTrackColor) : getTrackColor(false, false)
    MouseArea {
        anchors.fill: parent
        onClicked: {
            timeline.activeTrack = -2
        }
    }
    ToolButton {
        id: expandSubButton
        focusPolicy: Qt.NoFocus
        anchors.left: parent.left
        anchors.leftMargin: 1.5 * root.baseUnit
        width: trackHeader.collapsedHeight
        height: trackHeader.collapsedHeight
        icon.name: trackHeader.collapsed ? "go-next" : "go-down"
        onClicked: {
            if (subtitleTrack.height > trackHeader.collapsedHeight) {
                subtitleTrack.height = trackHeader.collapsedHeight
            } else {
                subtitleTrack.height = root.baseUnit * 2.5 * ((maxSubLayer == 0)? 2: (maxSubLayer + 1))
            }
        }
    }
    ComboBox {
        id: subLabel
        model: timeline.subtitlesList
        property int subIndex: timeline.activeSubPosition
        onSubIndexChanged: {
            subLabel.currentIndex = subIndex
        }
        anchors.right: parent.right
        anchors.top: expandSubButton.bottom
        anchors.left: subtitleLayerIndicator.right
        visible: (subtitleTrack.visible && subtitleTrack.height !== trackHeader.collapsedHeight)
        flat: true
        onActivated: index => {
            timeline.subtitlesMenuActivatedAsync(index)
        }
    }

    Row {
        id: subButtonsRow
        width: childrenRect.width
        x: Math.max(2 * trackHeader.collapsedHeight + 2, parent.width - width - 4)
        spacing: 0
        ToolButton {
            id: warningButton
            visible: subtitlesWarning
            focusPolicy: Qt.NoFocus
            icon.name: "data-warning"
            width: trackHeader.collapsedHeight
            height: trackHeader.collapsedHeight
            onClicked: timeline.subtitlesWarningDetails()
            ToolTip {
                visible: warningButton.hovered
                font: miniFont
                delay: 1500
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    text: i18n("Click to see details")
                }
            }
        }
        ToolButton {
            id: analyseButton
            focusPolicy: Qt.NoFocus
            icon.name: "text-speak"
            width: trackHeader.collapsedHeight
            height: trackHeader.collapsedHeight
            onClicked: timeline.triggerAction('audio_recognition')
            ToolTip.visible: hovered
            ToolTip.delay: 1500
            ToolTip.timeout: 5000
            ToolTip.text: i18n("Speech recognition")
        }
        ToolButton {
            id: muteButton
            focusPolicy: Qt.NoFocus
            icon.name: trackHeader.isDisabled ? "view-hidden" : "view-visible"
            width: trackHeader.collapsedHeight
            height: trackHeader.collapsedHeight
            onClicked: timeline.triggerAction('disable_subtitle')
            ToolTip.visible: hovered
            ToolTip.delay: 1500
            ToolTip.timeout: 5000
            ToolTip.text: trackHeader.isDisabled ? i18n("Show") : i18n("Hide")
        }

        ToolButton {
            id: lockButton
            width: trackHeader.collapsedHeight
            height: trackHeader.collapsedHeight
            focusPolicy: Qt.NoFocus
            icon.name: trackHeader.isLocked ? "lock" : "unlock"
            onClicked: timeline.triggerAction('lock_subtitle')
            ToolTip.visible: hovered
            ToolTip.delay: 1500
            ToolTip.timeout: 5000
            ToolTip.text: trackHeader.isLocked ? i18n("Unlock track") : i18n("Lock track")
            SequentialAnimation {
                id: flashLock
                loops: 3
                ParallelAnimation {
                    ScaleAnimator {target: lockButton; from: 1; to: 1.2; duration: 120}
                }
                ParallelAnimation {
                    ScaleAnimator {target: lockButton; from: 1.6; to: 1; duration: 120}
                }
            }
        }
    }
    Column {
        id: subtitleLayerIndicator
        width: root.trackTagWidth
        height: trackHeader.height
        anchors.left: expandSubButton.right
        anchors.top: trackHeader.top
        anchors.bottom: trackHeader.bottom
        property color bgColor: Qt.darker(getTrackColor(false, true), 0.55)
        visible: maxSubLayer > 0

        Repeater {
            model: maxSubLayer + 1
            id: subLayerRepeater
            delegate: Rectangle {
                height: trackHeader.height / subLayerRepeater.count
                width: subtitleLayerIndicator.width
                color: subtitleLayerIndicator.bgColor
                visible: (subtitleTrack.visible && subtitleTrack.height !== trackHeader.collapsedHeight)
                border.color: root.frameColor
                Text {
                    id: name
                    font: miniFont
                    text: "S" + index
                    color: activePalette.text
                    anchors.centerIn: trackHeader
                }
            }
        }
    }
}
