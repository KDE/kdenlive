import QtQuick.Controls 2.4
import QtQuick 2.11

MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    property bool showAutoKeyframe: true
    acceptedButtons: Qt.NoButton
    width: fullscreenButton.width + root.baseUnit
    height: parent.height
    onEntered: {
        animator.stop()
        effecttoolbar.opacity = 1
    }
    onExited: {
        effecttoolbar.opacity = 0
    }

    Rectangle {
        id: effecttoolbar
        objectName: "effecttoolbar"
        width: fullscreenButton.width
        anchors.right: barZone.right
        anchors.verticalCenter: parent.verticalCenter
        height: childrenRect.height
        color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.7)
        opacity: 0
        radius: 4
        border.color : Qt.rgba(0, 0, 0, 0.3)
        border.width: 1

        OpacityAnimator {
            id: animator
            target: effecttoolbar;
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
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Switch Full Screen")
                ToolTip.timeout: 3000
                onClicked: controller.triggerAction('monitor_fullscreen')
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
                objectName: "nextKeyframe"
                icon.name: "keyframe-next"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Go to Next Keyframe")
                ToolTip.timeout: 3000
                onClicked: controller.seekNextKeyframe()
            }
            ToolButton {
                objectName: "prevKeyframe"
                icon.name: "keyframe-previous"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Go to Previous Keyframe")
                ToolTip.timeout: 3000
                onClicked: controller.seekPreviousKeyframe()
            }
            ToolButton {
                objectName: "addKeyframe"
                icon.name: "keyframe-add"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Add/Remove Keyframe")
                ToolTip.timeout: 3000
                onClicked: controller.addRemoveKeyframe()
            }
            ToolButton {
                icon.name: "keyframe-record"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Automatic Keyframes")
                ToolTip.timeout: 3000
                onClicked: controller.switchAutoKeyframe()
                checkable: true
                checked: controller.autoKeyframe
                visible: barZone.showAutoKeyframe
            }
            ToolButton {
                icon.name: "zoom-in"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom in")
                ToolTip.timeout: 3000
                onClicked: controller.triggerAction('monitor_zoomin')
            }
            ToolButton {
                icon.name: "zoom-out"
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom out")
                ToolTip.timeout: 3000
                onClicked: controller.triggerAction('monitor_zoomout')
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
                        effecttoolbar.anchors.right = undefined
                        effecttoolbar.anchors.left = barZone.left
                        effecttoolbar.fadeBar()
                    } else {
                        barZone.anchors.left = undefined
                        barZone.anchors.right = barZone.parent.right
                        barZone.rightSide = true
                        effecttoolbar.anchors.left = undefined
                        effecttoolbar.anchors.right = barZone.right
                        effecttoolbar.fadeBar()
                    }
                }
            }
        }
    }
}
