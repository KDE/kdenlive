import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.0
import AudioThumb 1.0

Item {
    id: root
    objectName: "root"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property point center
    property double scale
    signal editCurrentMarker()

    QmlAudioThumb {
        id: audioThumb
        objectName: "audiothumb"
        property bool stateVisible: true
        anchors {
            left: parent.left
            bottom: parent.bottom
        }
        height: parent.height / 6
        //font.pixelSize * 3
        width: parent.width

        states: [
            State { when: audioThumb.stateVisible;
                    PropertyChanges {   target: audioThumb; opacity: 1.0    } },
            State { when: !audioThumb.stateVisible;
                    PropertyChanges {   target: audioThumb; opacity: 0.0    } }
        ]
        transitions: [ Transition {
            NumberAnimation { property: "opacity"; duration: 500}
        } ]

        MouseArea {
            hoverEnabled: true
            onExited: audioThumb.stateVisible = false
            onEntered: audioThumb.stateVisible = true
            acceptedButtons: Qt.NoButton
            anchors.fill: parent
        }
    }

      TextField {
          id: marker
          objectName: "markertext"
          activeFocusOnPress: true
          onEditingFinished: {
            root.comment = marker.displayText
            marker.focus = false
            root.editCurrentMarker()
          }

          anchors {
            right: parent.right
            bottom: parent.bottom
            rightMargin: 4
            bottomMargin: 4
          }

          textColor: "white"
          visible: text != ""
          style: TextFieldStyle {
            background: Rectangle {
                color: "#ffff0000"
                radius: 5
            }
          }
          font.pointSize: root.height / 35
        }
}
