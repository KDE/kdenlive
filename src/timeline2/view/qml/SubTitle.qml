import QtQuick 2.11
import QtQuick.Controls 2.4

Item {
    id: subtitleRoot
    visible : true
    height: parent.height
    z: 20
    property real duration : (model.endframe - model.startframe) 
    property int oldStartX
    property double oldStartFrame: subtitleBase.x
    Rectangle {
        id: subtitleBase
        width: duration * timeScale // to make width change wrt timeline scale factor
        height: parent.height
        x: model.startframe * timeScale;
        property bool textEditBegin: false
        color: 'yellow'
        border.width: 1
        border.color: 'orange'
        clip: true
        /*Text {
            id: subtitleText
            anchors.fill: parent
            visible: !subtitleEdit.visible
            text: model.subtitle
            color: "black"
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }*/
        MouseArea {
            // Clip shifting
            id: subtitleClipArea
            anchors.fill: parent
            hoverEnabled: true
            enabled: true
            property int newStart: -1
            property int diff: -1
            property double delta: -1
            property double originalDuration: -1
            property double oldDelta: 0
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: (pressed ? Qt.ClosedHandCursor : ((startMouseArea.drag.active || endMouseArea.drag.active)? Qt.SizeHorCursor: Qt.PointingHandCursor));
            drag.target: subtitleBase
            drag.axis: Drag.XAxis
            onPressed: {
                console.log('IT IS PRESSED')
                if (mouse.button == Qt.RightButton) {
                    console.log('RIGHT BUTTON CLICKED')
                    timeline.deleteSubtitle(subtitleBase.x / timeline.scaleFactor)
                }
                else {
                    root.autoScrolling = false
                    oldStartX = mouseX
                    oldStartFrame = subtitleBase.x
                    originalDuration = subtitleBase.width/timeScale
                    console.log("originalDuration",originalDuration)
                }                            
            }
            onPositionChanged: {
                if (pressed) {
                    newStart = Math.round((subtitleBase.x + (mouseX-oldStartX)) / timeScale)
                    if (mouseX != oldStartX) {
                        diff = (mouseX - oldStartX) / timeScale
                        subtitleBase.x = subtitleBase.x + diff
                        delta = subtitleBase.x/timeline.scaleFactor - oldStartFrame/timeline.scaleFactor
                        var diffDelta = delta - oldDelta
                        oldDelta = delta
                        subtitleBase.width = originalDuration * timeline.scaleFactor                             
                    }
                }
            }
            onReleased: {
                console.log('IT IS RELEASED')
                root.autoScrolling = timeline.autoScroll
                if (subtitleBase.x < 0)
                    subtitleBase.x = 0
                if (mouseX != oldStartX && oldStartFrame!= subtitleBase.x) {
                    console.log("old start frame",oldStartFrame/timeline.scaleFactor, "new frame afer shifting ",oldStartFrame/timeline.scaleFactor + delta)
                    timeline.shiftSubtitle(oldStartFrame/timeline.scaleFactor , subtitleBase.x / timeline.scaleFactor, subtitleBase.x / timeline.scaleFactor + duration, subtitleEdit.text)                                
                }
                console.log("originalDuration after shifting",originalDuration)
            }
            onDoubleClicked: {
                parent.textEditBegin = true
            }
        }
        TextField {
            id: subtitleEdit
            font: miniFont
            activeFocusOnPress: true
            selectByMouse: true
            onEditingFinished: {
                subtitleEdit.focus = false
                timeline.editSubtitle(subtitleBase.x / timeline.scaleFactor, subtitleEdit.displayText, (subtitleBase.x + subtitleBase.width)/ timeline.scaleFactor)
                parent.textEditBegin = false
            }
            anchors.fill: parent
            //visible: timeScale >= 6
            enabled: parent.textEditBegin
            text: model.subtitle
            height: subtitleBase.height
            width: subtitleBase.width
            wrapMode: TextField.WordWrap
            horizontalAlignment: displayText == text ? TextInput.AlignHCenter : TextInput.AlignLeft
            background: Rectangle {
                color: 'yellow'
                border {
                    width: 1
                    color: 'orange'
                }
            }
            color: 'black'
            padding: 0
        }
    }
    Rectangle {
        id: leftstart
        width: 2
        height: subtitleBase.height
        x: model.startframe * timeScale;
        anchors.top: subtitleBase.top
        anchors.left: subtitleBase.left
        color: 'green'
        visible: true
        MouseArea {
            // Right resize handle to change end timing
            id: startMouseArea
            anchors.fill: parent
            height: parent.height
            width: 2
            hoverEnabled: true
            enabled: true
            property bool sizeChanged: false
            property int newStart: -1
            property double originalDuration: -1
            property int diff: -1
            property double delta: -1
            property double oldDelta: 0
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.SizeHorCursor
            drag.target: leftstart
            drag.axis: Drag.XAxis
//                            drag.smoothed: false
            onPressed: {
                console.log('IT IS PRESSED')
                root.autoScrolling = false
                //rightend.anchors.right = undefined
                oldStartX = mouseX
                oldStartFrame = subtitleBase.x // the original start frame of subtitle
                //console.log(oldStartFrame)
                //console.log(subtitleBase.x)
                originalDuration = subtitleBase.width/timeScale
                console.log("originalDuration",originalDuration)
            }
            onPositionChanged: {
                if (pressed) {
                    //console.log('POSITION CHANGED')
                    newStart = Math.round((subtitleBase.x + (mouseX-oldStartX)) / timeScale)
                    //diff = (mouseX - oldStartX) / timeScale
                    if (((mouseX != oldStartX && duration > 1) || (mouseX < oldStartX && duration <= 1)) && subtitleBase.x >= 0) {
                        sizeChanged = true
                        diff = (mouseX - oldStartX) / timeScale
                        subtitleBase.x = subtitleBase.x + diff
                        //console.log("oldStartFrame",oldStartFrame/timeline.scaleFactor,"subtitleBase",subtitleBase.x/timeline.scaleFactor)
                        //console.log("duration:", duration)
                        delta = subtitleBase.x/timeline.scaleFactor - oldStartFrame/timeline.scaleFactor
                        var diffDelta = delta - oldDelta //update the change in start frame difference
                        oldDelta = delta
                        //console.log("Diff:",diff,"Delta:", delta, "Delta_Diff",diffDelta)
                        //console.log("new duration =", subtitleBase.width/timeScale - delta )
                        //subtitleBase.width = (originalDuration - delta)*timeScale
                        duration = duration - diffDelta //update duration to enable resizing
                        //duration = (originalDuration - diffDelta)
                        //console.log("Delta duration =", duration )
                        //console.log("originalDuration",originalDuration- diffDelta)
                        //console.log("Delta_Duaration:",subtitleBase.width/timeScale)
                        //timeline.moveSubtitle(oldStartX/ timeScale, subtitleBase.x/timeline.scaleFactor)
                    }
                }
            }
            onReleased: {
                //console.log('its RELEASED')
                root.autoScrolling = timeline.autoScroll
                //rightend.anchors.right = subtitleBase.right
                if (mouseX != oldStartX && oldStartFrame!= subtitleBase.x) {
                    console.log("old start frame",oldStartFrame/timeline.scaleFactor, "new frame",oldStartFrame/timeline.scaleFactor + delta)
                    timeline.moveSubtitle(oldStartFrame/timeline.scaleFactor , oldStartFrame/timeline.scaleFactor + delta)
                }
            }
        }
    }
    Rectangle {
        // end position resize handle
        id: rightend
        width: 2
        height: subtitleBase.height
        x: model.endframe * timeScale;
        anchors.right: subtitleBase.right
        anchors.top: subtitleBase.top
        color: 'blue' // to distinguish the resize handle
        //Drag.active: endMouseArea.drag.active
        //Drag.proposedAction: Qt.MoveAction
        visible: true
        MouseArea {
            id: endMouseArea
            anchors.fill: parent
            height: parent.height
            width: 2
            hoverEnabled: true
            enabled: true
            property bool sizeChanged: false
            property int newEnd: -1
            property int oldMouseX
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.SizeHorCursor
            drag.target: rightend
            drag.axis: Drag.XAxis
            //drag.smoothed: false

            onPressed: {
                root.autoScrolling = false
                //rightend.anchors.right = undefined
                oldMouseX = mouseX
            }
            onPositionChanged: {
                if (pressed) {
                    newEnd = Math.round((mouseX + width) / timeScale)
                    if ((mouseX != oldMouseX && duration > 1) || (duration <= 1 && mouseX > oldMouseX)) {
                        sizeChanged = true
                        //duration = subtitleBase.width + (mouseX - oldMouseX)/ timeline.scaleFactor
                        duration = Math.round((subtitleBase.width/timeScale + (mouseX - oldMouseX)/timeScale))
                    }
                }
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
                rightend.anchors.right = subtitleBase.right
                if (mouseX != oldMouseX || sizeChanged) {
                    timeline.editSubtitle(subtitleBase.x / timeline.scaleFactor, subtitleEdit.text, subtitleBase.x / timeline.scaleFactor + duration)
                    sizeChanged = false
                }
            }
        }
    }
}
