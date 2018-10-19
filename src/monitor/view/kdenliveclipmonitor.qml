import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.6
import AudioThumb 1.0

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property point profile
    property double zoom
    property double scalex
    property double scaley
    property bool dropped
    property string fps
    property bool showMarkers
    property bool showTimecode
    property bool showFps
    property bool showSafezone
    property bool showAudiothumb
    property bool showToolbar: false
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
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
        font.family: "Arial"
    }

    signal editCurrentMarker()
    signal toolBarChanged(bool doAccept)

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
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
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
        }
        visible: barOverArea.mouseX >= x - 10
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
                        case 0: {
                            return '';
                        }
                        case 1: {
                            return "OverlayStandard.qml";
                        }
                        case 2:{
                            return "OverlayMinimal.qml";
                        }
                        case 3:{
                            return "OverlayCenter.qml";
                        }
                        case 4:{
                            return "OverlayCenterDiagonal.qml";
                        }
                        case 5:{
                            return "OverlayThirds.qml";
                        }
                    }
                }
            }
        }
        Item {
            id: monitorOverlay
            anchors.fill: parent

            QmlAudioThumb {
                id: audioThumb
                objectName: "audiothumb"
                property bool stateVisible: true
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                height: parent.height / 6
                //font.pixelSize * 3
                width: parent.width
                visible: root.showAudiothumb

                MouseArea {
                    id: mouseOverArea
                    hoverEnabled: true
                    onExited: audioThumb.stateVisible = false
                    onEntered: audioThumb.stateVisible = true
                    acceptedButtons: Qt.NoButton
                    anchors.fill: parent
                }

                states: [
                    State { when: audioThumb.stateVisible;
                        PropertyChanges {   target: audioThumb; opacity: 1.0    } },
                    State { when: !audioThumb.stateVisible;
                        PropertyChanges {   target: audioThumb; opacity: 0.0    } }
                ]
                transitions: [ Transition {
                    NumberAnimation { property: "opacity"; duration: 500}
                } ]
            }

            Text {
                id: timecode
                objectName: "timecode"
                color: "white"
                style: Text.Outline; 
                styleColor: "black"
                text: controller.toTimecode(controller.position)
                font.pixelSize: root.baseUnit
                visible: root.showTimecode
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    rightMargin: 4
                }
            }
            Text {
                id: fpsdropped
                objectName: "fpsdropped"
                color: root.dropped ? "red" : "white"
                style: Text.Outline;
                styleColor: "black"
                text: root.fps + "fps"
                visible: root.showFps
                font.pixelSize: root.baseUnit
                anchors {
                    right: timecode.visible ? timecode.left : parent.right
                    bottom: parent.bottom
                    rightMargin: 10
                }
            }
            TextField {
                id: marker
                objectName: "markertext"
                activeFocusOnPress: true
                onEditingFinished: {
                    root.markerText = marker.displayText
                    marker.focus = false
                    root.editCurrentMarker()
                }
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && text != ""
                text: controller.markerComment
                maximumLength: 20
                style: TextFieldStyle {
                    textColor: "white"
                    background: Rectangle {
                        color: controller.position == controller.zoneIn ? "#9900ff00" : controller.position == controller.zoneOut ? "#99ff0000" : "#990000ff"
                        width: marker.width
                    }
                }
                font.pixelSize: root.baseUnit
            }
        }
        MouseArea {
            id: dragOverArea
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            x: 0
            width: 2 * audioDragButton.width
            height: 2.5 * audioDragButton.height
            y: parent.height - height
            propagateComposedEvents: true
            onPressed: {
                // First found child is the Column
                var clickedChild = childAt(mouseX,mouseY).childAt(mouseX,mouseY)
                if (clickedChild == audioDragButton) {
                    dragType = 1
                } else if (clickedChild == videoDragButton) {
                    dragType = 2
                } else {
                    dragType = 0
                }
                mouse.accepted = false
            }
            Column {
            ToolButton {
                id: audioDragButton
                iconName: "audio-volume-medium"
                tooltip: "Audio only drag"
                x: 10
                enabled: false
                visible: dragOverArea.containsMouse
            }
            ToolButton {
                id: videoDragButton
                iconName: "kdenlive-show-video"
                tooltip: "Video only drag"
                x: 10
                enabled: false
                visible: dragOverArea.containsMouse
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
