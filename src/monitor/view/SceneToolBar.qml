import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick.Layouts 1.3
import QtQuick 2.0

Rectangle {
    id: scenetoolbar
    objectName: "scenetoolbar"
    width: fullscreenButton.width
    height: childrenRect.height
    SystemPalette { id: myPalette; colorGroup: SystemPalette.Active }
    color: Qt.rgba(myPalette.window.r, myPalette.window.g, myPalette.window.b, 0.7)
    radius: 4
    border.color : Qt.rgba(0, 0, 0, 0.3)
    border.width: 1

    Column {
        ToolButton {
            id: fullscreenButton
            objectName: "fullScreen"
            iconName: "view-fullscreen"
            tooltip: "Switch Full Screen"
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('monitor_fullscreen')
            }
        }
        ToolButton {
            objectName: "switchOverlay"
            iconName: "view-grid"
            tooltip: "Change Overlay"
            onClicked: {
                root.switchOverlay()
            }
        }
        ToolButton {
            iconName: "zoom-in"
            tooltip: "Zoom in"
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('monitor_zoomin')
            }
        }
        ToolButton {
            iconName: "zoom-out"
            tooltip: "Zoom out"
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('monitor_zoomout')
            }
        }
        ToolButton {
            objectName: "addMarker"
            iconName: "list-add"
            tooltip: root.isClipMonitor ? "Add Marker" : "Add Guide"
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('add_marker_guide_quickly')
            }
        }
        ToolButton {
            objectName: "removeMarker"
            iconName: "list-remove"
            tooltip: root.isClipMonitor ? "Remove Marker" : "Remove Guide"
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                root.isClipMonitor ? controller.triggerAction('delete_clip_marker') : controller.triggerAction('delete_guide')
            }
        }
    }
}
