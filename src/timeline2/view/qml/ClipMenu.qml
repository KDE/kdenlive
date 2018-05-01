import QtQuick 2.6
import QtQuick.Controls 1.4 as OLD
import com.enums 1.0

OLD.Menu {
        id: clipMenu
        property int clipId
        property int clipStatus
        property int trackId
        property bool grouped
        function show() {
            //mergeItem.visible = timeline.mergeClipWithNext(trackIndex, index, true)
            menu.popup()
        }
        OLD.MenuItem {
            visible: true
            text: i18n('Cut')
            onTriggered: {
                console.log('cutting clip:', clipId)
                if (!parentTrack.isLocked) {
                    timeline.requestClipCut(clipId, timeline.position)
                } else {
                    root.pulseLockButtonOnTrack(trackId)
                }
            }
        }
        OLD.MenuItem {
            visible: !grouped && timeline.selection.length > 1
            text: i18n('Group')
            onTriggered: timeline.triggerAction('group_clip')
        }
        OLD.MenuItem {
            visible: grouped
            text: i18n('Ungroup')
            onTriggered: timeline.unGroupSelection(clipId)
        }

        OLD.MenuItem {
            visible: true
            text: i18n('Copy')
            onTriggered: root.copiedClip = clipId
        }
        OLD.MenuItem {
            visible: root.copiedClip != -1 && root.copiedClip != clipId
            text: i18n('Paste Effects')
            onTriggered: timeline.pasteEffects(clipId, root.copiedClip)
        }
        OLD.MenuSeparator {
            visible: true
        }
        OLD.MenuItem {
            text: i18n('Split Audio')
            onTriggered: timeline.splitAudio(clipId)
            visible: clipStatus == ClipState.VideoOnly
        }
        OLD.MenuItem {
            text: i18n('Remove')
            onTriggered: timeline.triggerAction('delete_timeline_clip')
        }
        OLD.MenuItem {
            visible: true
            text: i18n('Extract')
            onTriggered: timeline.extract(clipId)
        }
        OLD.MenuSeparator {
            visible: true
        }
        OLD.MenuItem {
            visible: true
            text: i18n('Change Speed')
            onTriggered: timeline.changeItemSpeed(clipId, -1)
        }

        OLD.MenuItem {
            text: i18n('Clip in Project Bin')
            onTriggered: timeline.triggerAction('clip_in_project_tree')
        }
        OLD.MenuItem {
            visible: true
            text: i18n('Split At Playhead')
            onTriggered: timeline.triggerAction('cut_timeline_clip')
        }
        OLD.Menu {
            title: i18n('Clip Type...')
            OLD.ExclusiveGroup {
                id: radioInputGroup
            }
            OLD.MenuItem {
                text: i18n('Video Only')
                checkable: true
                checked: clipStatus == ClipState.VideoOnly
                exclusiveGroup: radioInputGroup
                onTriggered: timeline.setClipStatus(clipId, ClipState.VideoOnly)
            }
            OLD.MenuItem {
                text: i18n('Audio Only')
                checkable: true
                checked: clipStatus == ClipState.AudioOnly
                exclusiveGroup: radioInputGroup
                onTriggered: timeline.setClipStatus(clipId, ClipState.AudioOnly)
            }
            OLD.MenuItem {
                text: i18n('Disabled')
                checkable: true
                checked: clipStatus == ClipState.Disabled
                exclusiveGroup: radioInputGroup
                onTriggered: timeline.setClipStatus(clipId, ClipState.Disabled)
            }
        }
}
