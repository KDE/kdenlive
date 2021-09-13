import QtQuick.Controls 2.4
import QtQuick 2.11

MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    property bool showAutoKeyframe: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * fontMetrics.font.pixelSize
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
        width: barZone.width
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
                contentItem: Item {
                    Image {
                        source: "image://icon/view-fullscreen"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Switch Full Screen")
                ToolTip.timeout: 3000
                onClicked: controller.triggerAction('monitor_fullscreen')
            }
            ToolButton {
                objectName: "switchOverlay"
                contentItem: Item {
                    Image {
                        source: "image://icon/view-grid"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocusv
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
                contentItem: Item {
                    Image {
                        source: "image://icon/keyframe-next"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Go to Next Keyframe")
                ToolTip.timeout: 3000
                onClicked: controller.seekNextKeyframe()
            }
            ToolButton {
                objectName: "prevKeyframe"
                contentItem: Item {
                    Image {
                        source: "image://icon/keyframe-previous"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Go to Previous Keyframe")
                ToolTip.timeout: 3000
                onClicked: controller.seekPreviousKeyframe()
            }
            ToolButton {
                objectName: "addKeyframe"
                contentItem: Item {
                    Image {
                        source: "image://icon/keyframe-add"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Add/Remove Keyframe")
                ToolTip.timeout: 3000
                onClicked: controller.addRemoveKeyframe()
            }
            ToolButton {
                contentItem: Item {
                    Image {
                        source: "image://icon/keyframe-record"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                ToolTip.visible: hovered
                focusPolicy: Qt.NoFocus
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Automatic Keyframes")
                ToolTip.timeout: 3000
                onClicked: controller.switchAutoKeyframe()
                checkable: true
                checked: controller.autoKeyframe
                visible: barZone.showAutoKeyframe
            }
            ToolButton {
                contentItem: Item {
                    Image {
                        source: "image://icon/zoom-in"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom in")
                ToolTip.timeout: 3000
                onClicked: controller.triggerAction('monitor_zoomin')
            }
            ToolButton {
                contentItem: Item {
                    Image {
                        source: "image://icon/zoom-out"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom out")
                ToolTip.timeout: 3000
                onClicked: controller.triggerAction('monitor_zoomout')
            }
            ToolButton {
                objectName: "moveBar"
                contentItem: Item {
                    Image {
                        source: "image://icon/transform-move-horizontal"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
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
