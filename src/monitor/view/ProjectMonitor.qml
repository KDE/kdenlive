/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls
import QtQuick.Window
import QtQuick

import org.kde.ki18n

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    required property K.MonitorProxy controller
    property int viewType: K.SceneType.MonitorSceneDefault
    property string markerText
    property point profile: root.controller.profile
    property double zoom
    property point center
    property double scalex
    property double scaley
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showAudiothumb: false
    property double offsetx : 0
    property double offsety : 0
    property int duration: 300
    property bool isClipMonitor: false
    Component.onCompleted: {
        root.controller.rulerHeight = 0
    }

    FontMetrics {
        id: fontMetrics
        font: K.UiUtils.fixedFont
    }

    signal editCurrentMarker()
    signal startRecording(bool showCountDown)

    function updatePalette() {
        clipMonitorRuler.forceRepaint()
    }

    function startCountdown() {
        countDownLoader.source = "Countdown.qml"
    }
    function stopCountdown() {
        root.startRecording(false)
        countDownLoader.source = ""
    }

    MouseArea {
        id: barOverArea
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        onWheel: wheel => {
            root.controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onEntered: {
            root.controller.setWidgetKeyBinding(KI18n.i18n("<b>Click</b> to play, <b>Double click</b> for fullscreen, <b>Hover right</b> for toolbar, <b>Wheel</b> or <b>arrows</b> to seek, <b>Ctrl wheel</b> to zoom"));
        }
        onExited: {
            root.controller.setWidgetKeyBinding();
        }
    }
    DropArea { //Drop area for effects
        id: effectArea
        anchors.fill: parent
        keys: ['kdenlive/effect']
        property string droppedData
        property string droppedDataSource
        onEntered: drag => {
            drag.acceptProposedAction()
            droppedData = drag.getDataAsString('kdenlive/effect')
            droppedDataSource = drag.getDataAsString('kdenlive/effectsource')
        }
        onDropped: {
            root.controller.addEffect(droppedData, droppedDataSource)
            droppedData = ""
            droppedDataSource = ""
        }
    }
    SceneToolBar {
        id: sceneToolBar
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
        monitorController: root.controller
        isClipMonitor: root.isClipMonitor
    }

    Item {
        height: root.height - root.controller.rulerHeight
        width: root.width
        id: monitorArea
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            x: root.center.x - width / 2 - root.offsetx;
            y: root.center.y - height / 2 - root.offsety;

            K.MonitorOverlay {
                anchors.fill: frame
                color: K.KdenliveSettings.overlayColor
                overlayType: root.controller.overlayType
            }

            K.MonitorSafeZone {
                anchors.fill: frame
                showSafeZone: root.controller.showSafezone
                profile: root.controller.profile
            }

            Loader {
                id: countDownLoader
                anchors.fill: frame
            }

            Connections {
                target: countDownLoader.item

                function onStopCountdown() {
                    root.stopCountdown()
                }
            }
        }
        Item {
            id: monitorOverlay
            anchors.fill: monitorArea

            Label {
                id: timecode
                font.family: fontMetrics.font.family
                font.pointSize: 1.5 * fontMetrics.font.pointSize
                objectName: "timecode"
                color: "#ffffff"
                padding: 2
                background: Rectangle {
                    color: root.controller.monitorIsActive ? "#DD006600": "#66000000"
                }
                text: root.controller.timecode
                visible: root.showTimecode
                anchors {
                    right: monitorOverlay.right
                    bottom: monitorOverlay.bottom
                }
            }
            Label {
                id: fpsdropped
                font.family: fontMetrics.font.family
                font.pointSize: 1.5 * fontMetrics.font.pointSize
                objectName: "fpsdropped"
                color: "#ffffff"
                padding: 2
                background: Rectangle {
                    color: root.dropped ? "#99ff0000" : "#66004400"
                }
                text: KI18n.i18n("%1fps", root.fps)
                visible: root.showFps
                anchors {
                    right: timecode.visible ? timecode.left : parent.right
                    bottom: parent.bottom
                }
            }
            Label {
                id: labelSpeed
                font: K.UiUtils.fixedFont
                anchors {
                    left: parent.left
                    top: parent.top
                }
                visible: Math.abs(root.controller.speed) > 1
                text: "x" + root.controller.speed
                color: "white"
                background: Rectangle {
                    color: "darkgreen"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: inPoint
                font: K.UiUtils.fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && root.controller.position == root.controller.zoneIn
                text: KI18n.i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: outPoint
                font: K.UiUtils.fixedFont
                anchors {
                    left: inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && root.controller.position == root.controller.zoneOut
                text: KI18n.i18n("Out Point")
                color: "white"
                background: Rectangle {
                    color: "#770000"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            TextField {
                id: marker
                font: K.UiUtils.fixedFont
                objectName: "markertext"
                activeFocusOnPress: true
                onEditingFinished: {
                    root.markerText = marker.displayText
                    marker.focus = false
                    root.editCurrentMarker()
                }
                anchors {
                    left: outPoint.visible ? outPoint.right : inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && text != ""
                text: root.controller.markerComment
                height: inPoint.height
                width: fontMetrics.boundingRect(displayText).width + 10
                horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
                background: Rectangle {
                    color: root.controller.markerColor
                }
                color: "#000000"
                padding: 0
                maximumLength: 25
            }
        }
        Rectangle {
            id: transformcontainer
            width: 2 * fontMetrics.font.pixelSize
            height: width
            anchors.top: monitorArea.top
            anchors.left: monitorArea.left
            anchors.topMargin: 10
            anchors.leftMargin: 10
            color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.5)
            visible: K.KdenliveSettings.enableBuiltInEffects && root.controller.speed == 0 && (barOverArea.containsMouse || transformbutton.hovered)
            radius: 4
            border.color : Qt.rgba(0, 0, 0, 0.3)
            border.width: 1
            K.MonitorToolButton {
                id: transformbutton
                anchors.fill: transformcontainer
                hoverEnabled: true
                iconName: "transform-crop"
                toolTipText: KI18n.i18nc("@tooltip Transform, a tool to resize", "Enable Transform")
                checkable: false
                onClicked: {
                    root.controller.enableTransform()
                }
            }
        }
    }
    MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        height: root.controller.rulerHeight
        monitorController: root.controller
        duration: root.duration
    }
}
