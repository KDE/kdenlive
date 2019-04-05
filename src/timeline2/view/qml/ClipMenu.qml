import QtQuick 2.6
import QtQuick.Controls 1.4
import com.enums 1.0

Menu {
        id: clipMenu
        property int clipId
        property int clipFrame
        property int clipStatus
        property int trackId
        property bool grouped
        property bool canBeAudio
        property bool canBeVideo
        property bool canBeGrouped: timeline.isInSelection(clipId)

        onAboutToHide: {
            timeline.ungrabHack()
        }
        onAboutToShow: {
            canBeGrouped = timeline.isInSelection(clipId)
        }

        MenuItem {
            text: i18n('Copy')
            iconName: 'edit-copy'
            onTriggered: {
                root.copiedClip = clipId
                timeline.copyItem()
            }
        }
        MenuItem {
            visible: canBeGrouped
            text: i18n('Group')
            iconName: 'object-group'
            onTriggered: timeline.triggerAction('group_clip')
        }
        MenuItem {
            visible: grouped
            text: i18n('Ungroup')
            iconName: 'object-ungroup'
            onTriggered: timeline.unGroupSelection(clipId)
        }
        MenuItem {
            text: i18n('Edit Duration')
            iconName: 'measure'
            onTriggered: {
                timeline.editItemDuration(clipId)
            }
        }
        MenuItem {
            visible: root.copiedClip != -1 && root.copiedClip != clipId
            text: i18n('Paste Effects')
            iconName: 'edit-paste'
            onTriggered: timeline.pasteEffects(clipId)
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
        Menu {
            title: i18n('Markers')
            MenuItem {
                text: i18n('Add Marker')
                onTriggered: timeline.addMarker(clipId, timeline.position)
            }
            MenuItem {
                text: i18n('Add Marker/Guide quickly')
                onTriggered: timeline.addQuickMarker(clipId, timeline.position)
            }
            MenuItem {
                text: i18n('Edit Marker')
                onTriggered: timeline.editMarker(clipId, timeline.position)
            }
            MenuItem {
                text: i18n('Delete Marker')
                onTriggered: timeline.deleteMarker(clipId, timeline.position)
            }
            MenuItem {
                text: i18n('Delete All Markers')
                onTriggered: timeline.deleteAllMarkers(clipId)
            }
        }
        MenuItem {
            text: i18n('Set Audio Reference')
            onTriggered: timeline.setAudioRef(clipId)
            visible: canBeAudio
        }
        MenuItem {
            text: i18n('Align Audio')
            onTriggered: timeline.alignAudio(clipId)
            visible: canBeAudio
        }
        MenuItem {
            text: i18n('Remove')
            iconName: 'edit-delete'
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
                timeline.addCompositionToClip(assetId, clipId, clipFrame)
            }
        }
}
