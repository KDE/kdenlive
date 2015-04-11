import QtQuick 2.0

Item {
    id: rootsplit
    objectName: "rootsplit"

    // default size, but scalable by user
    height: 300; width: 400
    signal qmlMoveSplit(real percent)
    property int splitterPos
    splitterPos: this.width / 2

    MouseArea {
        width: rootsplit.width; height: rootsplit.height
        anchors.centerIn: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        onPressed: {
            rootsplit.qmlMoveSplit(mouseX / width)
            rootsplit.splitterPos = mouseX
        }
        onPositionChanged: {
            if (pressed) {
                rootsplit.qmlMoveSplit(mouseX / width)
                rootsplit.splitterPos = mouseX
            }
            timer.restart()
            splitter.visible = true
        }
        //onEntered: { splitter.visible = true }
        onExited: { splitter.visible = false }
    }
    
    Rectangle {
        id: splitter
        x: rootsplit.splitterPos
        y: 0
        width: 1
        height: rootsplit.height
        color: "red"
        visible: false
    }
    
    Timer {
        id: timer

        interval: 1000; running: false; repeat: false
        onTriggered:  {
            splitter.visible = false
        }
    }
}
