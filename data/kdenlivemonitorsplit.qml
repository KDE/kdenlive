import QtQuick 2.0

Item {
    id: root
    objectName: "rootsplit"

    // default size, but scalable by user
    height: 300; width: 400
    signal qmlMoveSplit()
    property int splitterPos
    property point center
    // percentage holds splitter pos relative to the scene percentage
    property double percentage
    // realpercent holds splitter pos relative to the frame width percentage
    property double realpercent

    percentage: 0.5
    realpercent: 0.5
    splitterPos: this.width / 2

    MouseArea {
        width: root.width; height: root.height
        anchors.centerIn: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        onPressed: {
            root.percentage = mouseX / width
            root.splitterPos = mouseX
            root.qmlMoveSplit()
        }
        onPositionChanged: {
            if (pressed) {
                root.percentage = mouseX / width
                root.splitterPos = mouseX
                root.qmlMoveSplit()
            }
            timer.restart()
            splitter.visible = true
        }
        //onEntered: { splitter.visible = true }
        onExited: { splitter.visible = false }
    }
    
    Rectangle {
        id: splitter
        x: root.splitterPos
        y: 0
        width: 1
        height: root.height
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
