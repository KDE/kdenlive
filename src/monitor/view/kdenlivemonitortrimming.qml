import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.11

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
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
    // Zoombar properties
    property double zoomStart: 0
    property double zoomFactor: 1
    property int zoomOffset: 0
    property bool showZoomBar: false
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property color overlayColor: 'cyan'
    property bool isClipMonitor: false

    FontMetrics {
        id: fontMetrics
        font: fixedFont
    }

    signal editCurrentMarker()

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
        onWheel: {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
        onEntered: {
            controller.setWidgetKeyBinding(i18n("TODO: This is the wonderful new trimming monitor overlay!"));
        }
        onExited: {
            controller.setWidgetKeyBinding();
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
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            anchors.centerIn: parent
            Rectangle {
                width: parent.width / 2
                height: parent.height / 2
                x: 0
                y: parent.height / 4
                color: "transparent"
                //border.color: "#ff0000"
                //border.width: 2
                Label {
                    text: i18n("In")
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "#990000"
                    }
                }
                Label {
                    id: trimmingTC1
                    font.family: fontMetrics.font.family
                    objectName: "trimmingTC1"
                    text: controller.trimmingTC1
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "black"
                        opacity: 0.5
                    }
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 8
                    }
                }
            }
            Rectangle {
                width: parent.width / 2
                height: parent.height / 2
                x: parent.width / 2
                y: parent.height / 4
                color: "transparent"
                //border.color: "#ff0000"
                //border.width: 2
                Label {
                    text: i18n("Out")
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "#990000"
                    }
                }
                Label {
                    id: trimmingTC2
                    font.family: fontMetrics.font.family
                    objectName: "trimmingTC2"
                    text: controller.trimmingTC2
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "black"
                        opacity: 0.5
                    }
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 8
                    }
                }
            }
            /*Rectangle {
                width: parent.width / 4
                height: parent.height / 4
                x: 0
                y: 0
                color: "transparent"
                border.color: "#ff0000"
                border.width: 2
                Label {
                    text: "Previous"
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "#009900"
                    }
                }
            }
            Rectangle {
                width: parent.width / 4
                height: parent.height / 4
                x: (parent.width / 4) * 3
                y: (parent.height / 4) * 3
                color: "transparent"
                border.color: "#ff0000"
                border.width: 2
                Label {
                    text: "Next"
                    color: "#ffffff"
                    padding: 4
                    background: Rectangle {
                        color: "#009900"
                    }
                }
            }*/
            /*Repeater {
                id: trackSeparators
                model: tracks
                property int rows: trackSeparators.count < 2 ? 1 : trackSeparators.count < 5 ? 2 : 3
                property int columns: trackSeparators.count < 2 ? 1 : trackSeparators.count < 3 ? 1 : trackSeparators.count < 7 ? 2 : 3
                Rectangle {
                    width: parent.width / trackSeparators.rows
                    height: parent.height / trackSeparators.columns
                    x: width * (index % trackSeparators.rows)
                    y: height * (Math.floor(index / trackSeparators.rows))
                    color: "transparent"
                    border.color: index == root.activeTrack ? "#ff0000" : "#00000000"
                    border.width: 2
                    Label {
                        text: modelData
                        color: "#ffffff"
                        padding :4
                        background: Rectangle {
                            color: index == root.activeTrack ? "#990000" : "#000066"
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activateTrack(index);
                    }
                }
            }*/
        }
    }
}
