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
    property int displayFontSize
    property int duration: 300
    property bool mouseOverRuler: false
    property int mouseRulerPos: 0
    property int consumerPosition: -1
    property double frameSize: 10
    property double timeScale: 1
    property int rulerHeight: 20
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
        font.pixelSize: root.displayFontSize
        visible: root.showTimecode
        anchors {
            right: root.right
            bottom: root.bottom
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
        font.pixelSize: root.displayFontSize
        anchors {
            right: timecode.visible ? timecode.left : root.right
            bottom: root.bottom
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
        text: root.markerText
        maximumLength: 20
        style: TextFieldStyle {
            textColor: "white"
            background: Rectangle {
                color: "#99ff0000"
                width: marker.width
            }
        }
        font.pixelSize: root.displayFontSize
    }

    // Monitor ruler
    Rectangle {
        id: ruler
        height: root.rulerHeight
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        color: activePalette.window

        // frame ticks
        Repeater {
            model: ruler.width / frameSize + 2
            Rectangle {
                property int realPos: index
                x: realPos * frameSize
                anchors.bottom: ruler.bottom
                height: (realPos % 4)? ((realPos % 2) ? 3 : 7) : 12
                width: 1
                color: activePalette.windowText
                opacity: 0.5
            }
        }

        // markers
        Repeater {
            model: markersModel
            delegate:
            Item {
                anchors.fill: parent
                Rectangle {
                    id: markerBase
                    width: 1
                    height: parent.height
                    x: (model.frame) * root.timeScale;
                    color: model.color
                }
                Rectangle {
                    visible: mlabel.visible
                    opacity: 0.7
                    x: markerBase.x
                    radius: 2
                    width: mlabel.width + 4
                    height: mlabel.height
                    anchors {
                        bottom: parent.verticalCenter
                    }
                    color: model.color
                    MouseArea {
                        z: 10
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        //onDoubleClicked: timeline.editMarker(clipRoot.binId, model.frame)
                        //onClicked: timeline.position = (clipRoot.x + markerBase.x) / timeline.scaleFactor
                    }
                }
                Text {
                    id: mlabel
                    visible: mouseOverRuler && Math.abs(root.mouseRulerPos - markerBase.x) < 4
                    text: model.comment
                    font.pixelSize: root.baseUnit
                    x: markerBase.x
                    anchors {
                        bottom: parent.verticalCenter
                        topMargin: 2
                        leftMargin: 2
                    }
                    color: 'white'
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: root.mouseOverRuler = true;
            onExited: root.mouseOverRuler = false;
            onPressed: {
                controller.seekPosition = mouseX / root.timeScale;
            }
            onPositionChanged: {
                root.mouseRulerPos = mouseX
                if (pressed) {
                    controller.seekPosition = mouseX / root.timeScale;
                }
            }
        }
        // monitor zone
        Rectangle {
            id: zone
            visible: controller.zoneOut > controller.zoneIn
            color: activePalette.highlight
            x: controller.zoneIn * root.timeScale
            width: (controller.zoneOut - controller.zoneIn) * root.timeScale
            anchors.bottom: parent.bottom
            height: ruler.height / 2
            opacity: 0.4

            Rectangle {
                id: trimIn
                anchors.left: parent.left
                anchors.leftMargin: 0
                height: parent.height
                width: 5
                color: 'lawngreen'
                opacity: 0
                Drag.active: trimInMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction

                MouseArea {
                    id: trimInMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.smoothed: false

                    onPressed: {
                        parent.anchors.left = undefined
                    }
                    onReleased: {
                        parent.anchors.left = zone.left
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            controller.zoneIn = controller.zoneIn + Math.round(trimIn.x / root.timeScale)
                        }
                    }
                    onEntered: parent.opacity = 0.5
                    onExited: parent.opacity = 0
                }
            }
            Rectangle {
                id: trimOut
                anchors.right: parent.right
                anchors.rightMargin: 0
                height: parent.height
                width: 5
                color: 'darkred'
                opacity: 0
                Drag.active: trimOutMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction

                MouseArea {
                    id: trimOutMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.smoothed: false

                    onPressed: {
                        parent.anchors.right = undefined
                    }
                    onReleased: {
                        parent.anchors.right = zone.right
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            controller.zoneOut = controller.zoneIn + Math.round((trimOut.x + trimOut.width) / root.timeScale)
                        }
                    }
                    onEntered: parent.opacity = 0.5
                    onExited: parent.opacity = 0
                }
            }
        }

        Rectangle {
            id: seekCursor
            visible: controller.seekPosition > -1
            color: activePalette.highlight
            width: 4
            height: ruler.height
            opacity: 0.5
            x: controller.seekPosition * root.timeScale
            y: 0
        }

        TimelinePlayhead {
            id: playhead
            visible: root.consumerPosition > -1
            height: ruler.height * 0.5
            width: ruler.height * 1
            anchors.top: ruler.top
            x: root.consumerPosition * root.timeScale - (width / 2)
        }
    }
}
