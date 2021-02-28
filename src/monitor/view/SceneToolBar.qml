import QtQuick.Controls 2.4
import QtQuick 2.11


MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    acceptedButtons: Qt.NoButton
    width: fullscreenButton.width + root.baseUnit
    height: parent.height
    onEntered: {
        animator.stop()
        scenetoolbar.opacity = 1
    }
    onExited: {
        scenetoolbar.opacity = 0
    }

    Rectangle {
        id: scenetoolbar
        objectName: "scenetoolbar"
        width: fullscreenButton.width
        height: childrenRect.height
        anchors.right: barZone.right
        SystemPalette { id: myPalette; colorGroup: SystemPalette.Active }
        color: Qt.rgba(myPalette.window.r, myPalette.window.g, myPalette.window.b, 0.7)
        opacity: 0
        radius: 4
        border.color : Qt.rgba(0, 0, 0, 0.3)
        border.width: 1
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
            animator.start()
        }

    
        
        Column {
            ToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                icon.name: "view-fullscreen"
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Switch Full Screen")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_fullscreen')
                }
            }
            ToolButton {
                objectName: "switchOverlay"
                icon.name: "view-grid"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Change Overlay")
                ToolTip.timeout: 3000
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
                icon.name: "zoom-in"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom in")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_zoomin')
                }
            }
            ToolButton {
                icon.name: "zoom-out"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom out")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_zoomout')
                }
            }
            ToolButton {
                objectName: "addMarker"
                icon.name: "list-add"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: root.isClipMonitor ? i18n("Add Marker") : i18n("Add Guide")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('add_marker_guide_quickly')
                }
            }
            ToolButton {
                objectName: "removeMarker"
                icon.name: "list-remove"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: root.isClipMonitor ? i18n("Remove Marker") : i18n("Remove Guide")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    root.isClipMonitor ? controller.triggerAction('delete_clip_marker') : controller.triggerAction('delete_guide')
                }
            }
            ToolButton {
                objectName: "moveBar"
                icon.name: "transform-move-horizontal"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Move Toolbar")
                ToolTip.timeout: 3000
                onClicked: {
                    if (barZone.rightSide) {
                        barZone.anchors.right = undefined
                        barZone.anchors.left = barZone.parent.left
                        barZone.rightSide = false
                        scenetoolbar.anchors.right = undefined
                        scenetoolbar.anchors.left = barZone.left
                        scenetoolbar.fadeBar()
                    } else {
                        barZone.anchors.left = undefined
                        barZone.anchors.right = barZone.parent.right
                        barZone.rightSide = true
                        scenetoolbar.anchors.left = undefined
                        scenetoolbar.anchors.right = barZone.right
                        scenetoolbar.fadeBar()
                    }
                }
            }
        }
    }
}
