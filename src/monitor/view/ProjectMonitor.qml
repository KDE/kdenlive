/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls
import QtQuick.Window
import QtQuick

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property point profile: controller.profile
    property double zoom
    property point center
    property double scalex
    property double scaley
    property bool captureRightClick: false
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showSafezone: false
    property bool showAudiothumb: false
    // Zoombar properties
    property double zoomStart: 0
    property double zoomFactor: 1
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double offsetx : 0
    property double offsety : 0
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property bool isClipMonitor: false
    Component.onCompleted: {
        // adjust monitor image size if audio thumb is displayed
        controller.rulerHeight = root.zoomOffset
    }

    function updateClickCapture() {
        root.captureRightClick = false
    }

    FontMetrics {
        id: fontMetrics
        font.family: fixedFont
    }

    signal editCurrentMarker()
    signal startRecording()

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }

    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }
    function updatePalette() {
        clipMonitorRuler.forceRepaint()
    }

    function startCountdown() {
        countDownLoader.source = "Countdown.qml"
    }
    function stopCountdown() {
        root.startRecording()
        countDownLoader.source = ""
    }

    function switchOverlay() {
        if (controller.overlayType >= 5) {
            controller.overlayType = 0
        } else {
            controller.overlayType = controller.overlayType + 1;
        }
        root.overlayType = controller.overlayType
    }
    MouseArea {
        id: barOverArea
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        onWheel: wheel => {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onEntered: {
            controller.setWidgetKeyBinding(i18n("<b>Click</b> to play, <b>Double click</b> for fullscreen, <b>Hover right</b> for toolbar, <b>Wheel</b> or <b>arrows</b> to seek, <b>Ctrl wheel</b> to zoom"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
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
            controller.addEffect(droppedData, droppedDataSource)
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
    }

    Item {
        height: root.height - controller.rulerHeight
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
                overlayType: root.overlayType
            }

            Loader {
                id: countDownLoader
                anchors.fill: frame
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
                    color: controller.monitorIsActive ? "#DD006600": "#66000000"
                }
                text: controller.timecode
                visible: root.showTimecode
                anchors {
                    right: monitorOverlay.right
                    bottom: monitorOverlay.bottom
                    bottomMargin: root.zoomOffset
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
                text: i18n("%1fps", root.fps)
                visible: root.showFps
                anchors {
                    right: timecode.visible ? timecode.left : parent.right
                    bottom: parent.bottom
                    bottomMargin: root.zoomOffset
                }
            }
            Label {
                id: labelSpeed
                font.family: fixedFont
                anchors {
                    left: parent.left
                    top: parent.top
                }
                visible: Math.abs(controller.speed) > 1
                text: "x" + controller.speed
                color: "white"
                background: Rectangle {
                    color: "darkgreen"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: inPoint
                font.family: fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    bottomMargin: root.zoomOffset
                }
                visible: root.showMarkers && controller.position == controller.zoneIn
                text: i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: outPoint
                font.family: fixedFont
                anchors {
                    left: inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                    bottomMargin: root.zoomOffset
                }
                visible: root.showMarkers && controller.position == controller.zoneOut
                text: i18n("Out Point")
                color: "white"
                background: Rectangle {
                    color: "#770000"
                }
                padding: 5
                horizontalAlignment: TextInput.AlignHCenter
            }
            TextField {
                id: marker
                font.family: fixedFont
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
                    bottomMargin: root.zoomOffset
                }
                visible: root.showMarkers && text != ""
                text: controller.markerComment
                height: inPoint.height
                width: fontMetrics.boundingRect(displayText).width + 10
                horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
                background: Rectangle {
                    color: controller.markerColor
                }
                color: "#000"
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
            visible: K.KdenliveSettings.enableBuiltInEffects && controller.speed == 0 && (barOverArea.containsMouse || transformbutton.hovered)
            radius: 4
            border.color : Qt.rgba(0, 0, 0, 0.3)
            border.width: 1
            K.MonitorToolButton {
                id: transformbutton
                anchors.fill: transformcontainer
                hoverEnabled: true
                iconName: "transform-crop"
                toolTipText: i18nc("@tooltip Transform, a tool to resize", "Enable Transform")
                checkable: false
                onClicked: {
                    controller.enableTransform()
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
        height: controller.rulerHeight
    }
}
