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
    Item {
        id: subtitleBase
        width: duration * timeScale // to make width change wrt timeline scale factor
        height: parent.height
        x: model.startframe * timeScale;
        property bool textEditBegin: false
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
            drag.minimumX: 0
            onPressed: {
                console.log('IT IS PRESSED')
                if (mouse.button == Qt.RightButton) {
                    console.log('RIGHT BUTTON CLICKED')
                    timeline.deleteSubtitle(subtitleBase.x / timeline.scaleFactor, subtitleBase.x / timeline.scaleFactor + duration, subtitleEdit.text)
                }
                else {
                    root.autoScrolling = false
                    oldStartX = mouseX
                    oldStartFrame = subtitleBase.x
                    originalDuration = subtitleBase.width/timeScale
                    console.log("originalDuration",originalDuration)
                    controller.requestSubtitleSelection(model.startframe);
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
                parent.textEditBegin = false
                if (model.subtitle != subtitleEdit.text) {
                    timeline.editSubtitle(subtitleBase.x / timeline.scaleFactor, (subtitleBase.x + subtitleBase.width)/ timeline.scaleFactor, subtitleEdit.text, model.subtitle)
                }
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
                color: root.selectedSubtitle == model.startframe ? "#fff" : '#ccccff'
                border {
                    color: root.selectedSubtitle == model.startframe ? root.selectionColor : "#000"
                    width: 2
                }
            }
            color: 'black'
            padding: 0
        }
    }
    Item {
        id: leftstart
        width: root.baseUnit / 2
        height: subtitleBase.height
        x: model.startframe * timeScale;
        anchors.top: subtitleBase.top
        anchors.left: subtitleBase.left
        visible: true
        MouseArea {
            // Right resize handle to change end timing
            id: startMouseArea
            anchors.fill: parent
            height: parent.height
            width: root.baseUnit / 2
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
                trimIn.opacity = 0
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
                    timeline.moveSubtitle(oldStartFrame/timeline.scaleFactor , oldStartFrame/timeline.scaleFactor + delta, subtitleBase.duration)
                }
            }
            onEntered: {
                if (!pressed) {
                    trimIn.opacity = 1
                }
            }
            onExited: trimIn.opacity = 0

            Rectangle {
                id: trimIn
                anchors.left: parent.left
                width: 2
                height: parent.height
                color: 'lawngreen'
                opacity: 0
                Drag.active: startMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction
                //visible: startMouseArea.pressed
            }
        }
    }
    Item {
        // end position resize handle
        id: rightend
        width: root.baseUnit / 2
        height: subtitleBase.height
        x: model.endframe * timeScale;
        anchors.right: subtitleBase.right
        anchors.top: subtitleBase.top
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
            property int oldMouseX
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.SizeHorCursor
            property int newDuration: subtitleRoot.duration
            property int originalDuration
            drag.target: rightend
            drag.axis: Drag.XAxis
            //drag.smoothed: false

            onPressed: {
                root.autoScrolling = false
                newDuration = subtitleRoot.duration
                originalDuration = subtitleRoot.duration
                //rightend.anchors.right = undefined
                oldMouseX = mouseX
                trimOut.opacity = 0
            }
            onPositionChanged: {
                if (pressed) {
                    if ((mouseX != oldMouseX && duration > 1) || (duration <= 1 && mouseX > oldMouseX)) {
                        sizeChanged = true
                        //duration = subtitleBase.width + (mouseX - oldMouseX)/ timeline.scaleFactor
                        newDuration = Math.round((subtitleBase.width/timeScale + (mouseX - oldMouseX)/timeScale))
                        // Perform resize without changing model
                        timeline.resizeSubtitle(subtitleBase.x / timeline.scaleFactor, subtitleBase.x / timeline.scaleFactor + newDuration, subtitleBase.x / timeline.scaleFactor + subtitleRoot.duration, false)
                    }
                }
            }
            onReleased: {
                root.autoScrolling = timeline.autoScroll
                rightend.anchors.right = subtitleBase.right
                console.log(' GOT RESIZE: ', newDuration, ' > ', originalDuration)
                if (mouseX != oldMouseX || sizeChanged) {
                    // Restore original size
                    timeline.resizeSubtitle(subtitleBase.x / timeline.scaleFactor, subtitleBase.x / timeline.scaleFactor + subtitleRoot.duration, subtitleBase.x / timeline.scaleFactor + newDuration, false)
                    // Perform real resize
                    timeline.resizeSubtitle(subtitleBase.x / timeline.scaleFactor, subtitleBase.x / timeline.scaleFactor + newDuration, subtitleBase.x / timeline.scaleFactor + originalDuration, true)
                    sizeChanged = false
                }
            }
            onEntered: {
                console.log('ENTER MOUSE END AREA')
                if (!pressed) {
                    trimOut.opacity = 1
                }
            }
            onExited: trimOut.opacity = 0

            Rectangle {
                id: trimOut
                anchors.right: parent.right
                width: 2
                height: parent.height
                color: 'red'
                opacity: 0
                Drag.active: endMouseArea.drag.active
                Drag.proposedAction: Qt.MoveAction
                //visible: endMouseArea.pressed
            }
        }
    }
}
