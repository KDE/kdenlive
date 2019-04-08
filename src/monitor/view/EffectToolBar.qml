import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.0


Rectangle {
    id: effecttoolbar
    objectName: "effecttoolbar"
    width: fullscreenButton.width
    height: childrenRect.height
    color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.7)
    radius: 4
    border.color : Qt.rgba(0, 0, 0, 0.3)
    border.width: 1

    Column {
        ToolButton {
            id: fullscreenButton
            objectName: "fullScreen"
            iconName: "view-fullscreen"
            tooltip: "Switch Full Screen"
            onClicked: controller.triggerAction('monitor_fullscreen')
        }
        ToolButton {
            objectName: "nextKeyframe"
            iconName: "go-next"
            tooltip: "Go to Next Keyframe"
            onClicked: controller.seekNextKeyframe()
        }
        ToolButton {
            objectName: "prevKeyframe"
            iconName: "go-previous"
            tooltip: "Go to Previous Keyframe"
            onClicked: controller.seekPreviousKeyframe()
        }
        ToolButton {
            objectName: "addKeyframe"
            iconName: "list-add"
            tooltip: "Add/Remove Keyframe"
            onClicked: controller.addRemoveKeyframe()
        }
        ToolButton {
            iconName: "zoom-in"
            tooltip: "Zoom in"
            onClicked: controller.triggerAction('monitor_zoomin')
        }
        ToolButton {
            iconName: "zoom-out"
            tooltip: "Zoom out"
            onClicked: controller.triggerAction('monitor_zoomout')
        }
    }
}
