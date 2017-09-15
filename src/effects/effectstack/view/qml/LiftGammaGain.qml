import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import Kdenlive.Controls 1.0
import QtQuick.Layouts 1.3

Rectangle {
    id: liftgammagain
    property var gammaFactor: 2.0
    property var gainFactor: 4.0
    Layout.fillWidth: true
    color: 'yellow'
    /*height: parent.width / 2.5
    anchors.left: parent.left
    anchors.right: parent.right*/
    function loadWheels() {
        /*liftwheel.color = Qt.rgba( filter.getDouble("lift_r"),
                                   filter.getDouble("lift_g"),
                                   filter.getDouble("lift_b"),
                                   1.0 )
        gammawheel.color = Qt.rgba( filter.getDouble("gamma_r") / gammaFactor,
                                    filter.getDouble("gamma_g") / gammaFactor,
                                    filter.getDouble("gamma_b") / gammaFactor,
                                    1.0 )
        gainwheel.color = Qt.rgba( filter.getDouble("gain_r") / gainFactor,
                                   filter.getDouble("gain_g") / gainFactor,
                                   filter.getDouble("gain_b") / gainFactor,
                                   1.0 )*/
    }
    RowLayout {
        spacing: 0
        Layout.fillWidth: true
    Column {
        height: parent.height
        width: liftgammagain.width / 3
            Label {
                text: 'Lift'
            }
            ColorWheelItem {
                id: liftwheel
                width: liftgammagain.width / 3
                height: width
                onColorChanged: {
                    /*filter.set("lift_r", liftwheel.red / 255.0 );
                    filter.set("lift_g", liftwheel.green / 255.0 );
                    filter.set("lift_b", liftwheel.blue / 255.0 );*/
                }
            }
        }
    Column {
        height: parent.height
        width: liftgammagain.width / 3
            Label {
                text: 'Gamma'
            }
            ColorWheelItem {
                id: gammawheel
                width: liftgammagain.width / 3
                height: width
                onColorChanged: {
                    /*filter.set("gamma_r", (gammawheel.red / 255.0) * gammaFactor);
                    filter.set("gamma_g", (gammawheel.green / 255.0) * gammaFactor);
                    filter.set("gamma_b", (gammawheel.blue / 255.0) * gammaFactor);*/
                }
            }
        }
        Column {
            height: parent.height
            width: liftgammagain.width / 3
            Label {
                text: 'Gain'
            }
            ColorWheelItem {
                id: gainwheel
                width: liftgammagain.width / 3
                height: width
                onColorChanged: {
                    /*filter.set("gain_r", (gainwheel.red / 255.0) * gainFactor);
                    filter.set("gain_g", (gainwheel.green / 255.0) * gainFactor);
                    filter.set("gain_b", (gainwheel.blue / 255.0) * gainFactor);*/
                }
            }
        }
    }
}
