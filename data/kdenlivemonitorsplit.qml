import QtQuick 2.0

Item {
    id: rootsplit
    objectName: "rootsplit"

    // default size, but scalable by user
    height: 300; width: 400
    signal qmlMoveSplit(real percent)
    
    MouseArea {
        width: rootsplit.width; height: rootsplit.height
        anchors.centerIn: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        onPressed: {
            rootsplit.qmlMoveSplit(mouseX / width)
        }
        onPositionChanged: {
            if (pressed) {
                rootsplit.qmlMoveSplit(mouseX / width)
            }
        }
    }
}
