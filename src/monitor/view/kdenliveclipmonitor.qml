import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.4
import AudioThumb 1.0

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property string timecode
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
    property real baseUnit: fontMetrics.font.pointSize
    property int duration: 300
    property bool mouseOverRuler: false
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }

    onZoomChanged: {
        sceneToolBar.setZoom(root.zoom)
    }
    signal editCurrentMarker()
    signal toolBarChanged(bool doAccept)

    onDurationChanged: {
        timeScale = width / duration
        if (duration < 200) {
            frameSize = 5 * timeScale
        } else if (duration < 2500) {
            frameSize = 25 * timeScale
        } else if (duration < 10000) {
            frameSize = 50 * timeScale
        } else {
            frameSize = 100 * timeScale
        }
    }

    Item {
        id: monitorOverlay
        height: root.height - controller.rulerHeight
        width: root.width
        SceneToolBar {
            id: sceneToolBar
            anchors {
                left: parent.left
                top: parent.top
                topMargin: 10
                leftMargin: 10
            }
            visible: root.showToolbar
        }

        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            anchors.centerIn: parent
            visible: root.showSafezone

            Rectangle {
            id: safezone
                objectName: "safezone"
                color: "transparent"
                border.color: "cyan"
                width: parent.width * 0.9
                height: parent.height * 0.9
                anchors.centerIn: parent
                Rectangle {
                    id: safetext
                    objectName: "safetext"
                    color: "transparent"
                    border.color: "cyan"
                    width: frame.width * 0.8
                    height: frame.height * 0.8
                    anchors.centerIn: parent
                }
                Rectangle {
                    color: "cyan"
                    width: root.width / 5
                    height: 1
                    anchors.centerIn: parent
                }
                Rectangle {
                    color: "cyan"
                    height: root.width / 5
                    width: 1
                    anchors.centerIn: parent
                }
            }
        }

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

            states: [
                State { when: audioThumb.stateVisible;
                        PropertyChanges {   target: audioThumb; opacity: 1.0    } },
                State { when: !audioThumb.stateVisible;
                        PropertyChanges {   target: audioThumb; opacity: 0.0    } }
            ]
            transitions: [ Transition {
                NumberAnimation { property: "opacity"; duration: 500}
            } ]

            MouseArea {
                hoverEnabled: true
                onExited: audioThumb.stateVisible = false
                onEntered: audioThumb.stateVisible = true
                acceptedButtons: Qt.NoButton
                anchors.fill: parent
            }
        }

        Text {
            id: timecode
            objectName: "timecode"
            color: "white"
            style: Text.Outline; 
            styleColor: "black"
            text: root.timecode
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
