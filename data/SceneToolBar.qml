import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.0

Rectangle {
    id: scenetoolbar
    objectName: "scenetoolbar"
    width: fullscreenButton.width
    height: fullscreenButton.height * 5 + zoomSlider.height + 2
    SystemPalette { id: myPalette; colorGroup: SystemPalette.Active }
    color: Qt.rgba(myPalette.window.r, myPalette.window.g, myPalette.window.b, 0.7)
    radius: 4
    border.color : Qt.rgba(0, 0, 0, 0.3)
    border.width: 1

    function setZoom(zoom) {
        if (zoom >= 1) {
            zoomSlider.value = zoom + 2
        } else if (zoom == 0.5) {
            zoomSlider.value = 2
        } else if (zoom == 0.25) {
            zoomSlider.value = 1
        } else if (zoom == 0.125) {
            zoomSlider.value = 0
        }
    }

    Column {
        ToolButton {
            id: fullscreenButton
            objectName: "fullScreen"
            iconName: "view-fullscreen"
            tooltip: "Switch Full Screen"
        }
        ToolButton {
            objectName: "nextSnap"
            iconName: "go-next"
            tooltip: "Go to Next Snap Point"
        }
        ToolButton {
            objectName: "prevSnap"
            iconName: "go-previous"
            tooltip: "Go to Previous Snap Point"
        }
        ToolButton {
            objectName: "addMarker"
            iconName: "list-add"
            tooltip: "Add Marker"
        }
        ToolButton {
            objectName: "removeMarker"
            iconName: "list-remove"
            tooltip: "Remove Marker"
        }
        Slider {
            id: zoomSlider
            signal zoomChanged(real value)
            anchors.horizontalCenter:parent.horizontalCenter
            objectName: "zoomSlider"
            orientation: Qt.Vertical
            height: fullscreenButton.height * 3
            maximumValue: 5.0
            stepSize: 1.0
            value: 3.0
            onValueChanged: {
                zoomSlider.zoomChanged(value);
            }
        }
    }
}
