import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.11
import com.enums 1.0

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property int itemType: 0
    property point profile: controller.profile
    property double zoom
    property double scalex
    property double scaley
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showSafezone: false
    property bool showAudiothumb: false
    property bool showToolbar: false
    property string clipName: controller.clipName
    property real baseUnit: fontMetrics.font.pixelSize
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property color overlayColor: 'cyan'
    property bool isClipMonitor: true
    property int dragType: 0

    FontMetrics {
        id: fontMetrics
        font: fixedFont
    }

    signal editCurrentMarker()

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }
    onClipNameChanged: {
        // Animate clip name
        clipNameLabel.opacity = 1
        showAnimate.restart()
    }

    function updatePalette() {
        clipMonitorRuler.forceRepaint()
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
    }
    SceneToolBar {
        id: sceneToolBar
        barContainsMouse: sceneToolBar.rightSide ? barOverArea.mouseX >= x - 10 : barOverArea.mouseX < x + width + 10
        onBarContainsMouseChanged: {
            sceneToolBar.opacity = 1
            sceneToolBar.visible = sceneToolBar.barContainsMouse
        }
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
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            anchors.centerIn: parent

            Loader {
                anchors.fill: parent
                source: {
                    switch(root.overlayType)
                    {
                        case 0:
                            return '';
                        case 1:
                            return "OverlayStandard.qml";
                        case 2:
                            return "OverlayMinimal.qml";
                        case 3:
                            return "OverlayCenter.qml";
                        case 4:
                            return "OverlayCenterDiagonal.qml";
                        case 5:
                            return "OverlayThirds.qml";
                    }
                }
            }
        }
        Item {
            id: monitorOverlay
            anchors.fill: parent

            Item {
                id: audioThumb
                property bool stateVisible: (clipMonitorRuler.containsMouse || thumbMouseArea.containsMouse)
                property bool isAudioClip: controller.clipType == ProducerType.Audio
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                Label {
                    id: clipStreamLabel
                    font: fixedFont
                    anchors {
                        bottom: audioThumb.isAudioClip ? parent.bottom : parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                    color: "white"
                    text: controller.clipStream
                    background: Rectangle {
                        color: "#222277"
                    }
                    visible: text != ""
                    padding :4
                }
                height: isAudioClip ? parent.height : parent.height / 6
                //font.pixelSize * 3
                width: parent.width
                visible: root.showAudiothumb && (isAudioClip || controller.clipType == ProducerType.AV)

                states: [
                    State { when: audioThumb.stateVisible || audioThumb.isAudioClip;
                        PropertyChanges {   target: audioThumb; opacity: 1.0    } },
                    State { when: !audioThumb.stateVisible && !audioThumb.isAudioClip;
                        PropertyChanges {   target: audioThumb; opacity: 0.0    } }
                ]
                transitions: [ Transition {
                    NumberAnimation { property: "opacity"; duration: audioThumb.isAudioClip ? 0 : 500}
                } ]
                Rectangle {
                    color: activePalette.base
                    opacity: audioThumb.isAudioClip ? 1 : 0.6
                    anchors.fill: parent
                }
                Rectangle {
                    color: "yellow"
                    opacity: 0.3
                    height: parent.height
                    x: controller.zoneIn * timeScale
                    width: (controller.zoneOut - controller.zoneIn) * timeScale
                    visible: controller.zoneIn > 0 || controller.zoneOut < duration - 1
                }
                Repeater {
                    id: streamThumb
                    model: controller.audioThumb.length
                    property double streamHeight: parent.height / streamThumb.count
                    Item {
                        anchors.fill: parent
                        Image {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: streamThumb.streamHeight
                            y: model.index * height
                            source: controller.audioThumb[model.index]
                            asynchronous: true
                        }
                        Rectangle {
                            width: parent.width
                            y: (model.index + 1) * streamThumb.streamHeight
                            height: 1
                            visible: streamThumb.count > 1 && model.index < streamThumb.count - 1
                            color: 'black'
                        }
                    }
                }
                Rectangle {
                    color: "red"
                    width: 1
                    height: parent.height
                    x: controller.position * timeScale
                }
                MouseArea {
                    id: thumbMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onPositionChanged: {
                        if (mouse.modifiers & Qt.ShiftModifier) {
                            var pos = Math.max(mouseX, 0)
                            controller.setPosition(Math.min(pos / root.timeScale, root.duration));
                        }
                    }
                }
            }
            Label {
                id: clipNameLabel
                font: fixedFont
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                }
                color: "white"
                text: clipName
                background: Rectangle {
                    color: "#222277"
                }
                visible: clipName != ""
                padding :4
                SequentialAnimation {
                    id: showAnimate
                    running: false
                    NumberAnimation { target: clipNameLabel; duration: 3000 }
                    NumberAnimation { target: clipNameLabel; property: "opacity"; to: 0; duration: 1000 }
                }
            }

            Label {
                id: timecode
                font.family: fontMetrics.font.family
                font.pointSize: 1.5 * fontMetrics.font.pointSize
                objectName: "timecode"
                color: "#ffffff"
                padding: 2
                background: Rectangle {
                    color: "#66000000"
                }
                text: controller.toTimecode(controller.position)
                visible: root.showTimecode
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    bottomMargin: (audioThumb.stateVisible && !audioThumb.isAudioClip && audioThumb.visible) ? audioThumb.height : 0
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
                    bottomMargin: (audioThumb.stateVisible && !audioThumb.isAudioClip && audioThumb.visible) ? audioThumb.height : 0
                }
            }
            Label {
                id: inPoint
                font: fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && controller.position == controller.zoneIn
                text: i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                padding:4
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label {
                id: outPoint
                font: fixedFont
                anchors {
                    left: inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && controller.position + 1 == controller.zoneOut
                text: i18n("Out Point")
                color: "white"
                background: Rectangle {
                    color: "#770000"
                }
                padding: 4
                horizontalAlignment: TextInput.AlignHCenter
            }
            TextField {
                id: marker
                font: fixedFont
                objectName: "markertext"
                activeFocusOnPress: true
                text: controller.markerComment
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
                height: inPoint.height
                width: fontMetrics.boundingRect(displayText).width + 10
                horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
                background: Rectangle {
                        color: "#990000ff"
                }
                color: "#ffffff"
                padding: 0
                maximumLength: 20
            }
        }

        Rectangle {
            // Audio or video only drag zone
            x: 2
            y: inPoint.visible || outPoint.visible || marker.visible ? parent.height - inPoint.height - height - 2 : parent.height - height - 2
            width: childrenRect.width
            height: childrenRect.height
            color: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.7)
            radius: 4
            opacity: (dragAudioArea.containsMouse || dragVideoArea.containsMouse  || thumbMouseArea.containsMouse || (barOverArea.containsMouse && barOverArea.mouseY >= y)) ? 1 : 0
            visible: controller.clipHasAV
            onOpacityChanged: {
                if (opacity == 1) {
                    videoDragButton.x = 0
                    videoDragButton.y = 0
                    audioDragButton.x = videoDragButton.x + videoDragButton.width
                    audioDragButton.y = 0
                }
            }
            Row {
                id: dragRow
                ToolButton {
                    id: videoDragButton
                    icon.name: "kdenlive-show-video"
                    Drag.active: dragVideoArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "kdenlive/producerslist" : "V" + controller.clipId + "/" + controller.zoneIn + "/" + (controller.zoneOut - 1)
                    }
                    MouseArea {
                        id: dragVideoArea
                        hoverEnabled: true
                        anchors.fill: parent
                        propagateComposedEvents: true
                        cursorShape: Qt.PointingHand
                        drag.target: parent
                        onExited: {
                            parent.x = 0
                            parent.y = 0
                        }
                    }
                }
                ToolButton {
                    id: audioDragButton
                    icon.name: "audio-volume-medium"
                    Drag.active: dragAudioArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "kdenlive/producerslist" : "A" + controller.clipId + "/" + controller.zoneIn + "/" + (controller.zoneOut - 1)
                    }
                    MouseArea {
                        id: dragAudioArea
                        hoverEnabled: true
                        anchors.fill: parent
                        propagateComposedEvents: true
                        cursorShape: Qt.PointingHand
                        drag.target: parent
                        onExited: {
                            parent.x = videoDragButton.x + videoDragButton.width
                            parent.y = 0
                        }
                    }
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
