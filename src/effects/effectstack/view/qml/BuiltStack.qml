import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.11

Rectangle {
    id: root
    objectName: "builtinstack"

    signal valueChanged(string text, int val)
    //signal parameterChanged(string effectId, string paramName, double val)

    SystemPalette { id: activePalette }
    color: activePalette.window

    signal clipClicked()

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }


    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        EffectSlider {
            id: slider1
            sliderIcon: 'speedometer'
            sliderLabel: i18n("Speed")
            paramName: 'speed'
            slider_max: 300
            slider_def: 100
        }
        LiftGammaGain {
            id: color_correct
        }
    }

    function resetStack() {
        color_correct.loadWheels()
    }

    function setSpeed(speed) {
        if (speed == 0) {
            speed = 100;
        }
        slider1.setValue(speed)
    }
}
