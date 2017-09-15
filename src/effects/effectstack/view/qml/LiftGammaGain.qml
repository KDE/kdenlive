import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import Kdenlive.Controls 1.0
import QtQuick.Layouts 1.3

Item {
    id: liftgammagain
    property string effectName: 'lift_gamma_gain'
    property double liftFactor : 2.0
    property double gammaFactor: 2.0
    property double gainFactor: 4.0
    Layout.fillWidth: true
    function loadWheels() {
        if (!effectstackmodel.hasFilter(liftgammagain.effectName)) {
            // Set default parameter values
            liftwheel.setColor(0. / liftFactor, 0. / liftFactor, 0. / liftFactor, 1.0);
            gammawheel.setColor(1.0 / gammaFactor, 1.0 / gammaFactor, 1.0 / gammaFactor, 1.0);
            gainwheel.setColor(1.0 / gainFactor, 1.0 / gainFactor, 1.0 / gainFactor, 1.0);
        } else {
            liftwheel.setColor(effectstackmodel.getFilter(liftgammagain.effectName, 'lift_r') / liftFactor,
                                   effectstackmodel.getFilter(liftgammagain.effectName, 'lift_g') / liftFactor,
                                   effectstackmodel.getFilter(liftgammagain.effectName, 'lift_b') / liftFactor,
                                   1.0 )
            gammawheel.setColor(effectstackmodel.getFilter(liftgammagain.effectName, 'gamma_r') / gammaFactor,
                                    effectstackmodel.getFilter(liftgammagain.effectName, 'gamma_g') / gammaFactor,
                                    effectstackmodel.getFilter(liftgammagain.effectName, 'gamma_b') / gammaFactor,
                                    1.0 )
            gainwheel.setColor(effectstackmodel.getFilter(liftgammagain.effectName, 'gain_r') / gainFactor,
                                   effectstackmodel.getFilter(liftgammagain.effectName, 'gain_g') / gainFactor,
                                   effectstackmodel.getFilter(liftgammagain.effectName, 'gain_b') / gainFactor,
                                   1.0 )
        }
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
                    effectstackmodel.adjust(liftgammagain.effectName, 'lift_r', liftwheel.red );
                    effectstackmodel.adjust(liftgammagain.effectName, 'lift_g', liftwheel.green );
                    effectstackmodel.adjust(liftgammagain.effectName, 'lift_b', liftwheel.blue );
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
                    effectstackmodel.adjust(liftgammagain.effectName, 'gamma_r', gammawheel.red);
                    effectstackmodel.adjust(liftgammagain.effectName, 'gamma_g', gammawheel.green);
                    effectstackmodel.adjust(liftgammagain.effectName, 'gamma_b', gammawheel.blue);
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
                    effectstackmodel.adjust(liftgammagain.effectName, 'gain_r', gainwheel.red);
                    effectstackmodel.adjust(liftgammagain.effectName, 'gain_g', gainwheel.green);
                    effectstackmodel.adjust(liftgammagain.effectName, 'gain_b', gainwheel.blue);
                }
            }
        }
    }
    Component.onCompleted: {
        liftwheel.setFactorDefaultZero(liftgammagain.liftFactor, 0, 0.5);
        gammawheel.setFactorDefaultZero(liftgammagain.gammaFactor, 1, 0);
        gainwheel.setFactorDefaultZero(liftgammagain.gainFactor, 1, 0);
    }
}
