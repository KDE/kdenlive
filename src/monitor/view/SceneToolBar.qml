import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.11
import QtQuick 2.11

Rectangle {
    id: scenetoolbar
    objectName: "scenetoolbar"
    width: fullscreenButton.width
    height: childrenRect.height
    property bool rightSide: true
    property bool barContainsMouse
    SystemPalette { id: myPalette; colorGroup: SystemPalette.Active }
    color: Qt.rgba(myPalette.window.r, myPalette.window.g, myPalette.window.b, 0.7)
    radius: 4
    border.color : Qt.rgba(0, 0, 0, 0.3)
    border.width: 1
    Timer {
        id: fadeTimer
        interval: 2500; running: false;
        onTriggered: {
            scenetoolbar.visible = false
            scenetoolbar.opacity = 1
        }
    }
    OpacityAnimator {
        id: animator
        target: scenetoolbar;
        from: 1;
        to: 0;
        duration: 2500
        running: false
    }

    function fadeBar()
    {
        scenetoolbar.visible = true
        animator.start()
        fadeTimer.start()
    }

    Column {
        ToolButton {
            id: fullscreenButton
            objectName: "fullScreen"
            iconName: "view-fullscreen"
            tooltip: i18n("Switch Full Screen")
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('monitor_fullscreen')
            }
        }
        ToolButton {
            objectName: "switchOverlay"
            iconName: "view-grid"
            tooltip: i18n("Change Overlay")
            onClicked: {
                if (controller.overlayType >= 5) {
                    controller.overlayType = 0
                } else {
                    controller.overlayType = controller.overlayType + 1;
                }
                root.overlayType = controller.overlayType
            }
        }
        ToolButton {
            iconName: "zoom-in"
            tooltip: i18n("Zoom in")
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('monitor_zoomin')
            }
        }
        ToolButton {
            iconName: "zoom-out"
            tooltip: i18n("Zoom out")
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('monitor_zoomout')
            }
        }
        ToolButton {
            objectName: "addMarker"
            iconName: "list-add"
            tooltip: root.isClipMonitor ? i18n("Add Marker") : i18n("Add Guide")
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                controller.triggerAction('add_marker_guide_quickly')
            }
        }
        ToolButton {
            objectName: "removeMarker"
            iconName: "list-remove"
            tooltip: root.isClipMonitor ? i18n("Remove Marker") : i18n("Remove Guide")
            onClicked: {
                controller.activateClipMonitor(root.isClipMonitor)
                root.isClipMonitor ? controller.triggerAction('delete_clip_marker') : controller.triggerAction('delete_guide')
            }
        }
        ToolButton {
            objectName: "moveBar"
            iconName: "transform-move-horizontal"
            tooltip: i18n("Move Toolbar")
            onClicked: {
                if (scenetoolbar.rightSide) {
                    scenetoolbar.anchors.right = undefined
                    scenetoolbar.anchors.left = scenetoolbar.parent.left
                    scenetoolbar.rightSide = false
                    fadeBar()
                } else {
                    scenetoolbar.anchors.left = undefined
                    scenetoolbar.anchors.right = scenetoolbar.parent.right
                    scenetoolbar.rightSide = true
                    fadeBar()
                }
            }
        }
    }
}
