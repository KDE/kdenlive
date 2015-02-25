/***************************************************************************
 *   Copyright (C) 2007-2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)   *
 *   Copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)              *
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2014 by Vincent Pinon (vpinon@april.org)                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "timelinecommands.h"
#include "customtrackview.h"

#include <klocalizedstring.h>

AddEffectCommand::AddEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &effect, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_track(track),
        m_effect(effect),
        m_pos(pos),
        m_doIt(doIt)
{
    QString effectName;
    QDomElement namenode = m_effect.firstChildElement(QLatin1String("name"));
    if (!namenode.isNull())
        effectName = i18n(namenode.text().toUtf8().data());
    else
        effectName = i18n("effect");
    if (doIt)
        setText(i18n("Add %1", effectName));
    else
        setText(i18n("Delete %1", effectName));
}
// virtual
void AddEffectCommand::undo()
{
    if (m_doIt)
        m_view->deleteEffect(m_track, m_pos, m_effect);
    else
        m_view->addEffect(m_track, m_pos, m_effect);
}
// virtual
void AddEffectCommand::redo()
{
    if (m_doIt)
        m_view->addEffect(m_track, m_pos, m_effect);
    else
        m_view->deleteEffect(m_track, m_pos, m_effect);
}

AddExtraDataCommand::AddExtraDataCommand(CustomTrackView *view, const QString&id, const QString&key, const QString &oldData, const QString &newData, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_oldData(oldData),
        m_newData(newData),
        m_key(key),
        m_id(id)
{
    if (m_newData.isEmpty())
        setText(i18n("Delete data"));
    else
        setText(i18n("Add data"));
}
// virtual
void AddExtraDataCommand::undo()
{
    m_view->addData(m_id, m_key, m_oldData);
}
// virtual
void AddExtraDataCommand::redo()
{
    m_view->addData(m_id, m_key, m_newData);
}

AddMarkerCommand::AddMarkerCommand(CustomTrackView *view, const CommentedTime &oldMarker, const CommentedTime &newMarker, const QString &id, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_oldMarker(oldMarker),
        m_newMarker(newMarker),
        m_id(id)
{
    if (m_newMarker.markerType() < 0)
        setText(i18n("Delete marker"));
    else if (m_oldMarker.comment().isEmpty())
        setText(i18n("Add marker"));
    else
        setText(i18n("Edit marker"));
}
// virtual
void AddMarkerCommand::undo()
{
    m_view->addMarker(m_id, m_oldMarker);
}
// virtual
void AddMarkerCommand::redo()
{
    m_view->addMarker(m_id, m_newMarker);
}

AddTimelineClipCommand::AddTimelineClipCommand(CustomTrackView *view, const QDomElement &xml, const QString &clipId, const ItemInfo &info, const EffectsList &effects, bool overwrite, bool push, bool doIt, bool doRemove, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_clipInfo(info),
        m_effects(effects),
        m_clipId(clipId),
        m_xml(xml),
        m_doIt(doIt),
        m_remove(doRemove),
        m_overwrite(overwrite),
        m_push(push)
{
    if (!m_remove) setText(i18n("Add timeline clip"));
    else setText(i18n("Delete timeline clip"));
}
// virtual
void AddTimelineClipCommand::undo()
{
    if (!m_remove)
        m_view->deleteClip(m_clipInfo);
    else
        m_view->addClip(m_xml, m_clipId, m_clipInfo, m_effects, m_overwrite, m_push);
}
// virtual
void AddTimelineClipCommand::redo()
{
    if (m_doIt) {
        if (!m_remove)
            m_view->addClip(m_xml, m_clipId, m_clipInfo, m_effects, m_overwrite, m_push);
        else
            m_view->deleteClip(m_clipInfo);
    }
    m_doIt = true;
}

AddTrackCommand::AddTrackCommand(CustomTrackView *view, int ix, const TrackInfo &info, bool addTrack, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_ix(ix),
        m_addTrack(addTrack),
        m_info(info)
{
    if (addTrack)
        setText(i18n("Add track"));
    else
        setText(i18n("Delete track"));
}
// virtual
void AddTrackCommand::undo()
{
    if (m_addTrack)
        m_view->removeTrack(m_ix);
    else
        m_view->addTrack(m_info, m_ix);
}
// virtual
void AddTrackCommand::redo()
{
    if (m_addTrack)
        m_view->addTrack(m_info, m_ix);
    else
        m_view->removeTrack(m_ix);
}

AddTransitionCommand::AddTransitionCommand(CustomTrackView *view, const ItemInfo &info, int transitiontrack, const QDomElement &params, bool remove, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_info(info),
        m_params(params),
        m_track(transitiontrack),
        m_doIt(doIt),
        m_remove(remove)
{
    if (m_remove)
        setText(i18n("Delete transition from clip"));
    else
        setText(i18n("Add transition to clip"));
    if (parent) {
        // command has a parent, so there are several operations ongoing, do not refresh monitor
        m_refresh = false;
    } else {
        m_refresh = true;
    }
}
// virtual
void AddTransitionCommand::undo()
{
    if (m_remove)
        m_view->addTransition(m_info, m_track, m_params, m_refresh);
    else
        m_view->deleteTransition(m_info, m_track, m_params, m_refresh);
}
// virtual
void AddTransitionCommand::redo()
{
    if (m_doIt) {
        if (m_remove)
            m_view->deleteTransition(m_info, m_track, m_params, m_refresh);
        else
            m_view->addTransition(m_info, m_track, m_params, m_refresh);
    }
    m_doIt = true;
}

ChangeClipTypeCommand::ChangeClipTypeCommand(CustomTrackView *view, const int track, const GenTime &pos, bool videoOnly, bool audioOnly, bool originalVideo, bool originalAudio, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_pos(pos),
        m_track(track),
        m_videoOnly(videoOnly),
        m_audioOnly(audioOnly),
        m_originalVideoOnly(originalVideo),
        m_originalAudioOnly(originalAudio)
{
    setText(i18n("Change clip type"));
}
// virtual
void ChangeClipTypeCommand::undo()
{
    m_view->doChangeClipType(m_pos, m_track, m_originalVideoOnly, m_originalAudioOnly);
}
// virtual
void ChangeClipTypeCommand::redo()
{
    m_view->doChangeClipType(m_pos, m_track, m_videoOnly, m_audioOnly);
}

ChangeEffectStateCommand::ChangeEffectStateCommand(CustomTrackView *view, const int track, const GenTime& pos, const QList <int>& effectIndexes, bool disable, bool refreshEffectStack, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_view(view),
    m_track(track),
    m_effectIndexes(effectIndexes),
    m_pos(pos),
    m_disable(disable),
    m_doIt(doIt),
    m_refreshEffectStack(refreshEffectStack)
{
    if (disable)
        setText(i18np("Disable effect", "Disable effects", effectIndexes.count()));
    else
        setText(i18np("Enable effect", "Enable effects", effectIndexes.count()));
}
// virtual
void ChangeEffectStateCommand::undo()
{
    m_view->updateEffectState(m_track, m_pos, m_effectIndexes, !m_disable, true);
}
// virtual
void ChangeEffectStateCommand::redo()
{
    if (m_doIt)
        m_view->updateEffectState(m_track, m_pos, m_effectIndexes, m_disable, m_refreshEffectStack);
    m_doIt = true;
    m_refreshEffectStack = true;
}

ChangeSpeedCommand::ChangeSpeedCommand(CustomTrackView *view, const ItemInfo &info, const ItemInfo &speedIndependantInfo, double old_speed, double new_speed, int old_strobe, int new_strobe, const QString &clipId, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_clipInfo(info),
        m_speedIndependantInfo(speedIndependantInfo),
        m_clipId(clipId),
        m_old_speed(old_speed),
        m_new_speed(new_speed),
        m_old_strobe(old_strobe),
        m_new_strobe(new_strobe)
{
    setText(i18n("Adjust clip length"));
}
// virtual
void ChangeSpeedCommand::undo()
{
    m_view->doChangeClipSpeed(m_clipInfo, m_speedIndependantInfo, m_old_speed, m_new_speed, m_old_strobe, m_clipId);
}
// virtual
void ChangeSpeedCommand::redo()
{
    m_view->doChangeClipSpeed(m_clipInfo, m_speedIndependantInfo, m_new_speed, m_old_speed, m_new_strobe, m_clipId);
}

ConfigTracksCommand::ConfigTracksCommand(CustomTrackView* view, const QList<TrackInfo> &oldInfos, const QList<TrackInfo> &newInfos, QUndoCommand* parent) :
        QUndoCommand(parent),
        m_view(view),
        m_oldInfos(oldInfos),
        m_newInfos(newInfos)
{
    setText(i18n("Configure Tracks"));
}
// virtual
void ConfigTracksCommand::undo()
{
    m_view->configTracks(m_oldInfos);
}
// virtual
void ConfigTracksCommand::redo()
{
    m_view->configTracks(m_newInfos);
}

EditEffectCommand::EditEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &oldeffect, const QDomElement &effect, int stackPos, bool refreshEffectStack, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_view(view),
    m_track(track),
    m_oldeffect(oldeffect),
    m_effect(effect),
    m_pos(pos),
    m_stackPos(stackPos),
    m_doIt(doIt),
    m_refreshEffectStack(refreshEffectStack)
{
    QString effectName;
    QDomElement namenode = effect.firstChildElement(QLatin1String("name"));
    if (!namenode.isNull())
        effectName = i18n(namenode.text().toUtf8().data());
    else
        effectName = i18n("effect");
    setText(i18n("Edit effect %1", effectName));
}
// virtual
int EditEffectCommand::id() const
{
    return 1;
}
// virtual
bool EditEffectCommand::mergeWith(const QUndoCommand * other)
{
    if (other->id() != id())
        return false;
    if (m_track != static_cast<const EditEffectCommand*>(other)->m_track)
        return false;
    if (m_stackPos != static_cast<const EditEffectCommand*>(other)->m_stackPos)
        return false;
    if (m_pos != static_cast<const EditEffectCommand*>(other)->m_pos)
        return false;
    m_effect = static_cast<const EditEffectCommand*>(other)->m_effect.cloneNode().toElement();
    return true;
}
// virtual
void EditEffectCommand::undo()
{
    m_view->updateEffect(m_track, m_pos, m_oldeffect, true);
}
// virtual
void EditEffectCommand::redo()
{
    if (m_doIt) {
        m_view->updateEffect(m_track, m_pos, m_effect, m_refreshEffectStack);
    }
    m_doIt = true;
    m_refreshEffectStack = true;
}

EditGuideCommand::EditGuideCommand(CustomTrackView *view, const GenTime &oldPos, const QString &oldcomment, const GenTime &pos, const QString &comment, bool doIt, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_oldcomment(oldcomment),
    m_comment(comment),
    m_oldPos(oldPos),
    m_pos(pos),
    m_doIt(doIt)
{
    if (m_oldcomment.isEmpty()) {
        setText(i18n("Add guide"));
        m_oldPos = GenTime(-1);
    }
    else if (m_oldPos == m_pos)
        setText(i18n("Edit guide"));
    else if (m_pos < GenTime() && m_comment.isEmpty())
        setText(i18n("Delete guide"));
    else
        setText(i18n("Move guide"));
}
// virtual
void EditGuideCommand::undo()
{
    m_view->editGuide(m_pos, m_oldPos, m_oldcomment);
}
// virtual
void EditGuideCommand::redo()
{
    if (m_doIt) {
        m_view->editGuide(m_oldPos, m_pos, m_comment);
    }
    m_doIt = true;
}


EditTransitionCommand::EditTransitionCommand(CustomTrackView *view, const int track, const GenTime &pos, const QDomElement &oldeffect, const QDomElement &effect, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_track(track),
        m_oldeffect(oldeffect),
        m_pos(pos),
        m_doIt(doIt)
{
    m_effect = effect.cloneNode().toElement();
    QString effectName;
    QDomElement namenode = effect.firstChildElement(QLatin1String("name"));
    if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
    else effectName = i18n("effect");
    setText(i18n("Edit transition %1", effectName));
}
// virtual
int EditTransitionCommand::id() const
{
    return 2;
}
// virtual
bool EditTransitionCommand::mergeWith(const QUndoCommand * other)
{
    if (other->id() != id()) return false;
    if (m_track != static_cast<const EditTransitionCommand*>(other)->m_track) return false;
    if (m_pos != static_cast<const EditTransitionCommand*>(other)->m_pos) return false;
    m_effect = static_cast<const EditTransitionCommand*>(other)->m_effect;
    return true;
}
// virtual
void EditTransitionCommand::undo()
{
    m_view->updateTransition(m_track, m_pos, m_effect, m_oldeffect, m_doIt);
}
// virtual
void EditTransitionCommand::redo()
{
    m_view->updateTransition(m_track, m_pos, m_oldeffect, m_effect, m_doIt);
    m_doIt = true;
}

GroupClipsCommand::GroupClipsCommand(CustomTrackView *view, const QList <ItemInfo> &clipInfos, const QList <ItemInfo>& transitionInfos, bool group, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_clips(clipInfos),
    m_transitions(transitionInfos),
    m_group(group)
{
    if (m_group)
        setText(i18n("Group clips"));
    else
        setText(i18n("Ungroup clips"));
}
// virtual
void GroupClipsCommand::undo()
{
    m_view->doGroupClips(m_clips, m_transitions, !m_group);
}
// virtual
void GroupClipsCommand::redo()
{
    m_view->doGroupClips(m_clips, m_transitions, m_group);
}

InsertSpaceCommand::InsertSpaceCommand(CustomTrackView *view, const QList<ItemInfo> &clipsToMove, const QList<ItemInfo> &transToMove, int track, const GenTime &duration, bool doIt, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_clipsToMove(clipsToMove),
    m_transToMove(transToMove),
    m_duration(duration),
    m_track(track),
    m_doIt(doIt)
{
    if (duration > GenTime())
        setText(i18n("Insert space"));
    else
        setText(i18n("Remove space"));
}
// virtual
void InsertSpaceCommand::undo()
{
    m_view->insertSpace(m_clipsToMove, m_transToMove, m_track, GenTime() - m_duration, m_duration);
}
// virtual
void InsertSpaceCommand::redo()
{
    if (m_doIt) {
        m_view->insertSpace(m_clipsToMove, m_transToMove, m_track, m_duration, GenTime());
    }
    m_doIt = true;
}

LockTrackCommand::LockTrackCommand(CustomTrackView *view, int ix, bool lock, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_ix(ix),
    m_lock(lock)
{
    if (lock)
        setText(i18n("Lock track"));
    else
        setText(i18n("Unlock track"));
}
// virtual
void LockTrackCommand::undo()
{
    m_view->lockTrack(m_ix, !m_lock);
}
// virtual
void LockTrackCommand::redo()
{
    m_view->lockTrack(m_ix, m_lock);
}

MoveClipCommand::MoveClipCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, QUndoCommand * parent)
    : QUndoCommand(parent),
      m_view(view),
      m_startPos(start),
      m_endPos(end),
      m_doIt(doIt),
      m_success(true)
{
    setText(i18n("Move clip"));
    if (parent) {
        // command has a parent, so there are several operations ongoing, do not refresh monitor
        m_refresh = false;
    } else {
        m_refresh = true;
    }
}


void MoveClipCommand::undo()
{
    m_doIt = true;
    // We can only undo what was done;
    // if moveClip() failed in redo() the document does (or should) not change.
    if (m_success) {
        m_view->moveClip(m_endPos, m_startPos, m_refresh);
    }
}
void MoveClipCommand::redo()
{
    if (m_doIt) {
        //        qDebug() << "Executing move clip command. End now:" << m_endPos;
        m_success = m_view->moveClip(m_startPos, m_endPos, m_refresh, &m_endPos);
        //        qDebug() << "Move clip command executed. End now: " << m_endPos;
    }
    m_doIt = true;
}

MoveEffectCommand::MoveEffectCommand(CustomTrackView *view, const int track, const GenTime &pos, const QList<int> &oldPos, int newPos, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_track(track),
    m_oldindex(oldPos),
    m_pos(pos)
{
    for (int i = 0; i < m_oldindex.count(); ++i) {
        m_newindex << newPos + i;
    }
    setText(i18n("Move effect"));
}
// virtual
int MoveEffectCommand::id() const
{
    return 2;
}
// virtual
bool MoveEffectCommand::mergeWith(const QUndoCommand * other)
{
    Q_UNUSED(other)
    return false;
}
// virtual
void MoveEffectCommand::undo()
{
    m_view->moveEffect(m_track, m_pos, m_newindex, m_oldindex);
}
// virtual
void MoveEffectCommand::redo()
{
    m_view->moveEffect(m_track, m_pos, m_oldindex, m_newindex);
}

MoveGroupCommand::MoveGroupCommand(CustomTrackView *view, const QList <ItemInfo> &startClip, const QList <ItemInfo> &startTransition, const GenTime &offset, const int trackOffset, bool doIt, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_startClip(startClip),
    m_startTransition(startTransition),
    m_offset(offset),
    m_trackOffset(trackOffset),
    m_doIt(doIt)
{
    setText(i18n("Move group"));
}
// virtual
void MoveGroupCommand::undo()
{
    m_doIt = true;
    m_view->moveGroup(m_startClip, m_startTransition, GenTime() - m_offset, - m_trackOffset, true);
}
// virtual
void MoveGroupCommand::redo()
{
    if (m_doIt)
        m_view->moveGroup(m_startClip, m_startTransition, m_offset, m_trackOffset, false);
    m_doIt = true;
}

MoveTransitionCommand::MoveTransitionCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_startPos(start),
    m_endPos(end),
    m_doIt(doIt)
{
    setText(i18n("Move transition"));
    if (parent) {
        // command has a parent, so there are several operations ongoing, do not refresh monitor
        m_refresh = false;
    } else m_refresh = true;
}
// virtual
void MoveTransitionCommand::undo()
{
    m_doIt = true;
    m_view->moveTransition(m_endPos, m_startPos, m_refresh);
}
// virtual
void MoveTransitionCommand::redo()
{
    if (m_doIt)
        m_view->moveTransition(m_startPos, m_endPos, m_refresh);
    m_doIt = true;
}

RazorClipCommand::RazorClipCommand(CustomTrackView *view, const ItemInfo &info, EffectsList stack, const GenTime &cutTime, bool doIt, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_info(info),
    m_cutTime(cutTime),
    m_doIt(doIt)
{
    m_originalStack.clone(stack);
    setText(i18n("Razor clip"));
}
// virtual
void RazorClipCommand::undo()
{
    m_view->cutClip(m_info, m_cutTime, false, m_originalStack);
}
// virtual
void RazorClipCommand::redo()
{
    if (m_doIt) {
        m_view->cutClip(m_info, m_cutTime, true);
    }
    m_doIt = true;
}
/*
RazorGroupCommand::RazorGroupCommand(CustomTrackView *view, QList <ItemInfo> clips1, QList <ItemInfo> transitions1, QList <ItemInfo> clipsCut, QList <ItemInfo> transitionsCut, QList <ItemInfo> clips2, QList <ItemInfo> transitions2, GenTime cutPos, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_clips1(clips1),
    m_transitions1(transitions1),
    m_clipsCut(clipsCut),
    m_transitionsCut(transitionsCut),
    m_clips2(clips2),
    m_transitions2(transitions2),
    m_cutPos(cutPos)
{
    setText(i18n("Cut Group"));
}
// virtual
void RazorGroupCommand::undo()
{
    m_view->slotRazorGroup(m_clips1, m_transitions1, m_clipsCut, m_transitionsCut, m_clips2, m_transitions2, m_cutPos, false);
}
// virtual
void RazorGroupCommand::redo()
{
    m_view->slotRazorGroup(m_clips1, m_transitions1, m_clipsCut, m_transitionsCut, m_clips2, m_transitions2, m_cutPos, true);
}
*/
RebuildGroupCommand::RebuildGroupCommand(CustomTrackView* view, int childTrack, const GenTime &childPos, QUndoCommand* parent) :
    QUndoCommand(parent),
    m_view(view),
    m_childTrack(childTrack),
    m_childPos(childPos)
{
    setText(i18n("Rebuild Group"));
}
// virtual
void RebuildGroupCommand::undo()
{
    m_view->rebuildGroup(m_childTrack, m_childPos);
}
// virtual
void RebuildGroupCommand::redo()
{
    m_view->rebuildGroup(m_childTrack, m_childPos);
}

RefreshMonitorCommand::RefreshMonitorCommand(CustomTrackView *view, bool execute, bool refreshOnUndo, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_exec(execute),
    m_execOnUndo(refreshOnUndo)
{
}
// virtual
void RefreshMonitorCommand::undo()
{
    if (m_execOnUndo)
        m_view->monitorRefresh();
}
// virtual
void RefreshMonitorCommand::redo()
{
    if (m_exec && !m_execOnUndo)
        m_view->monitorRefresh();
    m_exec = true;
}

ResizeClipCommand::ResizeClipCommand(CustomTrackView *view, const ItemInfo &start, const ItemInfo &end, bool doIt, bool dontWorry, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_startPos(start),
    m_endPos(end),
    m_doIt(doIt),
    m_dontWorry(dontWorry)
{
    setText(i18n("Resize clip"));
}
// virtual
void ResizeClipCommand::undo()
{
    m_view->resizeClip(m_endPos, m_startPos, m_dontWorry);
}
// virtual
void ResizeClipCommand::redo()
{
    if (m_doIt) {
        m_view->resizeClip(m_startPos, m_endPos, m_dontWorry);
    }
    m_doIt = true;
}

SplitAudioCommand::SplitAudioCommand(CustomTrackView *view, const int track, const GenTime &pos, const EffectsList &effects, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_pos(pos),
    m_track(track),
    m_effects(effects)
{
    setText(i18n("Split audio"));
}
// virtual
void SplitAudioCommand::undo()
{
    m_view->doSplitAudio(m_pos, m_track, m_effects, false);
}
// virtual
void SplitAudioCommand::redo()
{
    m_view->doSplitAudio(m_pos, m_track, m_effects, true);
}

