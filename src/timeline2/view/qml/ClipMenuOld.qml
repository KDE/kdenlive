import QtQuick 2.6
import QtQuick.Controls 1.4
import com.enums 1.0

Menu {
        id: clipMenu
        property int clipId
        property int clipStatus
        property int trackId
        property bool grouped
        property bool canBeAudio
        property bool canBeVideo
        MenuItem {
            text: i18n('Copy')
            onTriggered: root.copiedClip = clipId
        }
        MenuItem {
            visible: !grouped && timeline.selection.length > 1
            text: i18n('Group')
            onTriggered: timeline.triggerAction('group_clip')
        }
        MenuItem {
            visible: grouped
            text: i18n('Ungroup')
            onTriggered: timeline.unGroupSelection(clipId)
        }
        MenuItem {
            text: i18n('Edit Duration')
            onTriggered: {
                clipMenu.close()
                timeline.editItemDuration(clipId)
            }
        }
        MenuItem {
            visible: root.copiedClip != -1 && root.copiedClip != clipId
            text: i18n('Paste Effects')
            onTriggered: timeline.pasteEffects(clipId, root.copiedClip)
        }
        MenuSeparator {
            visible: true
        }
        MenuItem {
            text: i18n('Split Audio')
            onTriggered: timeline.splitAudio(clipId)
            visible: !grouped && canBeAudio && clipStatus == ClipState.VideoOnly
        }
        MenuItem {
            text: i18n('Split Video')
            onTriggered: timeline.splitVideo(clipId)
            visible: !grouped && canBeVideo && clipStatus == ClipState.AudioOnly
        }
        MenuItem {
            text: i18n('Remove')
            onTriggered: timeline.triggerAction('delete_timeline_clip')
        }
        MenuItem {
            visible: true
            text: i18n('Extract')
            onTriggered: timeline.extract(clipId)
        }
        MenuSeparator {
            visible: true
        }
        MenuItem {
            visible: true
            text: i18n('Change Speed')
            onTriggered: {
                clipMenu.close()
                timeline.changeItemSpeed(clipId, -1)
            }
        }

        MenuItem {
            text: i18n('Clip in Project Bin')
            onTriggered: timeline.triggerAction('clip_in_project_tree')
        }
        MenuItem {
            visible: true
            text: i18n('Split At Playhead')
            onTriggered: timeline.triggerAction('cut_timeline_clip')
        }
        MenuItem {
            visible: true
            text: clipStatus != ClipState.Disabled ? i18n('Disable clip') : i18n('Enable clip')
            onTriggered: timeline.switchEnableState(clipId)
        }
        AssetMenu {
            title: i18n('Insert an effect...')
            menuModel: effectModel
            onAssetSelected: {
                timeline.addEffectToClip(assetId, clipId)
            }
        }
        AssetMenu {
            title: i18n('Insert a composition...')
            menuModel: transitionModel
            isTransition: true
            onAssetSelected: {
                timeline.addCompositionToClip(assetId, clipId)
            }
        }
}
