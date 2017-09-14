import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: root
    objectName: "builtinstack"
    signal valueChanged(string text, int val)

    SystemPalette { id: activePalette }
    color: activePalette.window

    signal clipClicked()

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }
    EffectSlider {
        id: slider1
        sliderIcon: 'speedometer'
        sliderLabel: 'Speed'
        paramName: 'speed'
        slider_max: 300
        slider_def: 100
    }

    function resetStack() {
        //slider1.resetSlider()
    }

    function setSpeed(speed) {
        if (speed == 0) {
            speed = 100;
        }
        slider1.setValue(speed)
    }
}
