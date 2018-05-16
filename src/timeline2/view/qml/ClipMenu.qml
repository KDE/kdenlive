import QtQuick 2.6
import QtQuick.Controls 1.4 as OLD
import com.enums 1.0

OLD.Menu {
        id: clipMenu
        property int clipId
        property int clipStatus
        property int trackId
        property bool grouped
        property bool canBeAudio
        property bool canBeVideo
        function show() {
            //mergeItem.visible = timeline.mergeClipWithNext(trackIndex, index, true)
            menu.popup()
        }
        OLD.MenuItem {
            visible: true
            text: i18n('Copy')
            onTriggered: root.copiedClip = clipId
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
            visible: canBeAudio && clipStatus == ClipState.VideoOnly
        }
        OLD.MenuItem {
            text: i18n('Split Video')
            onTriggered: timeline.splitVideo(clipId)
            visible: canBeVideo && clipStatus == ClipState.AudioOnly
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
        OLD.MenuItem {
            visible: true
            text: clipStatus != ClipState.Disabled ? i18n('Disable clip') : i18n('Enable clip')
            onTriggered: timeline.switchEnableState(clipId)
        }
}
