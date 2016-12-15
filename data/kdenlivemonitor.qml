import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.0

Item {
    id: root
    objectName: "root"

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
    onZoomChanged: {
        sceneToolBar.setZoom(root.zoom)
    }
    signal editCurrentMarker()
    signal toolBarChanged(bool doAccept)

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

    Text {
        id: timecode
        objectName: "timecode"
        color: "white"
        style: Text.Outline; 
        styleColor: "black"
        text: root.timecode
        visible: root.showTimecode
        font.pixelSize: root.displayFontSize
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
        maximumLength: 20
        text: root.markerText
        style: TextFieldStyle {
            textColor: "white"
            background: Rectangle {
                color: "#990000ff"
                width: marker.width
            }
        }
        font.pixelSize: root.displayFontSize
    }
}
