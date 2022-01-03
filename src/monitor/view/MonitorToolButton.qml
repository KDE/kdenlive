import QtQuick 2.11
import QtQuick.Controls 2.4


ToolButton {
    property string iconName
    property string toolTipText
    contentItem: Item {
        Image {
            visible: iconName
            source: 'image://icon/' + iconName
            anchors.centerIn: parent
            width: parent.width - 4
            height: width
        }
    }
    width: parent.width
    height: width
    focusPolicy: Qt.NoFocus
    ToolTip {
        visible: parent.hovered
        delay: Qt.styleHints.mousePressAndHoldInterval
        text: toolTipText
        timeout: 3000
    }
}
