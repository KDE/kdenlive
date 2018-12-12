import QtQuick 2.6
import QtQuick.Controls 1.4

Menu {
        id: compositionMenu
        property int clipId
        property int clipStatus
        property int trackId
        property bool grouped

        onAboutToHide: {
            timeline.ungrabHack()
        }

        function show() {
            //mergeItem.visible = timeline.mergeClipWithNext(trackIndex, index, true)
            popup()
        }
        MenuItem {
            visible: !grouped && timeline.selection.length > 1
            text: i18n('Group')
            onTriggered: timeline.groupSelection()
        }
        MenuItem {
            visible: grouped
            text: i18n('Ungroup')
            onTriggered: timeline.unGroupSelection(clipId)
        }
        MenuItem {
            text: i18n('Edit Duration')
            onTriggered: {
                timeline.editItemDuration(clipId)
            }
        }
        MenuItem {
            visible: true
            text: i18n('Copy')
            onTriggered: {
                root.copiedClip = clipId
                timeline.copyItem()
            }
        }
        MenuItem {
            text: i18n('Remove')
            onTriggered: timeline.triggerAction('delete_timeline_clip')
        }
        MenuSeparator {
            visible: true
        }
    }
