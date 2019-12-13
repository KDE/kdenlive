
import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.11

Item {
    id: sliderroot
    property string sliderIcon
    property string sliderLabel
    property string paramName
    property int slider_max: 100
    property int slider_min: 0
    property int slider_def: 100
    property bool blockSignals: false
    implicitHeight: 20
    anchors.fill: parent

    function resetSlider() {
        slider.value = sliderroot.slider_def
    }

    function setValue(val) {
        sliderroot.blockSignals = true
        slider.value = val
        sliderroot.blockSignals = false
    }

    RowLayout{
        spacing: 0
        anchors.fill: parent
        anchors.margins: 0
    ToolButton {
        id: enableButton
        implicitHeight: 20
        implicitWidth: 20
        iconName: sliderroot.sliderIcon
        visible: sliderroot.sliderIcon != ''
        checkable: true
        checked: true
        Layout.rightMargin: 2
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        Layout.minimumWidth: 50
        Layout.maximumWidth: 500
        //TODO: don't hardcode
        height: sliderroot.sliderLabel == '' ? 12 : 20
        tickmarksEnabled: false
        maximumValue: sliderroot.slider_max
        minimumValue: sliderroot.slider_min
        value: sliderroot.slider_def
        stepSize: 1
        enabled: enableButton.checked
        opacity: enabled ? 1 : 0.4
        updateValueWhileDragging: true
        onValueChanged: {
            spinbox.value = value
            if (!sliderroot.blockSignals) {
                root.valueChanged(sliderroot.paramName, value)
            }
        }
        /*Text {
            text: sliderroot.sliderLabel
            leftPadding: 5
            opacity: 0.5
            color: activePalette.text
        }*/
        style: SliderStyle {
            handle: Rectangle {
                height: slider.height
                width: 4
                radius: 2
                color: container.containsMouse ? activePalette.highlight : activePalette.text
            }
            groove: Rectangle {
                anchors.bottom: parent.bottom
                implicitHeight: slider.height / 4
                implicitWidth: 100
                radius: 4
                border.color: activePalette.dark
                color: activePalette.base
                Rectangle {
                    height: parent.height
                    width: styleData.handlePosition + 2
                    implicitHeight: slider.height
                    implicitWidth: 100
                    radius: 4
                    color: activePalette.highlight
                    opacity: container.containsMouse ? 0.6 : 0.3
                }
            }

        }
        Repeater {
            id: repeater
            model: 9
            Rectangle {
                color: activePalette.text
                width: 1 ; height: (index % 2 == 0) ? 5 : 3
                y: slider.height - height
                x: (index * slider.width) / (repeater.count-1)
                opacity: 0.5
            }
        }
        Component.onCompleted: {
        // Create some controls.
        //slider.onValueChanged.connect(appWindow.sliderChanged)
        //spinBox.onValueChanged.connect(appWindow.spinBoxChanged)
    }
}
SpinBox {
    id: spinbox
    maximumValue: sliderroot.slider_max
    minimumValue: sliderroot.slider_min
    value: sliderroot.slider_def
    enabled: enableButton.checked
    onValueChanged: {
        slider.value = value
    }
    style: SpinBoxStyle{
        textColor: activePalette.highlight
        background: Rectangle {
            implicitWidth: 60
            implicitHeight: 20
            //control.hoverEnabled: true
            border.color: control.focus ? "red" : control.hovered ? activePalette.highlight : "transparent"
            color: 'transparent'
            radius: 2
        }
    }
}
    }
    MouseArea {
            id: container
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            enabled: enableButton.checked
    }
}
