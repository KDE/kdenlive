import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.0

Item {
    id: root
    objectName: "root"

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property point center
    property double scale
    signal editCurrentMarker()

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
                color: "#ff0000ff"
                radius: 5
            }
          }
          font.pointSize: root.height / 35
        }
}
