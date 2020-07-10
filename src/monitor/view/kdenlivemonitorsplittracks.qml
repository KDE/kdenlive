import QtQuick 2.11
import QtQuick.Controls 2.4

Item {
    id: root
    objectName: "root"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string comment
    property string framenum
    property rect framesize
    property point profile: controller.profile
    property int overlayType: 0
    property color overlayColor: 'cyan'
    property point center
    property double scalex
    property double scaley
    // Zoombar properties
    property double zoomStart: 0
    property double zoomFactor: 1
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double stretch : 1
    property double sourcedar : 1
    property double offsetx : 0
    property double offsety : 0
    property int activeTrack: 0
    onSourcedarChanged: refreshdar()
    property bool iskeyframe
    property int requestedKeyFrame
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property var tracks: []

    signal activateTrack(int position)

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }
    MouseArea {
        id: barOverArea
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        onWheel: {
            controller.seek(wheel.angleDelta.x + wheel.angleDelta.y, wheel.modifiers)
        }
    }

    Item {
        id: frame
        objectName: "referenceframe"
        width: root.profile.x * root.scalex
        height: root.profile.y * root.scaley
        x: root.center.x - width / 2 - root.offsetx
        y: root.center.y - height / 2 - root.offsety
        Repeater {
            id: trackSeparators
            model: tracks
            property int rows: trackSeparators.count < 2 ? 1 : trackSeparators.count < 5 ? 2 : 3
            property int columns: trackSeparators.count < 2 ? 1 : trackSeparators.count < 3 ? 1 : trackSeparators.count < 7 ? 2 : 3
            Rectangle {
                width: parent.width / trackSeparators.rows
                height: parent.height / trackSeparators.columns
                x: width * (index % trackSeparators.rows)
                y: height * (Math.floor(index / trackSeparators.rows))
                color: "transparent"
                border.color: index == root.activeTrack ? "#ff0000" : "#00000000"
                border.width: 2
                Label {
                    text: modelData
                    color: "#ffffff"
                    padding :4
                    background: Rectangle {
                        color: index == root.activeTrack ? "#990000" : "#000066"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activateTrack(index);
                }
            }
        }
    }
    SceneToolBar {
        id: sceneToolBar
        barContainsMouse: sceneToolBar.rightSide ? barOverArea.mouseX >= x - 10 : barOverArea.mouseX < x + width + 10
        onBarContainsMouseChanged: {
            sceneToolBar.opacity = 1
            sceneToolBar.visible = sceneToolBar.barContainsMouse
        }
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
    }
    MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        height: controller.rulerHeight
    }
}
