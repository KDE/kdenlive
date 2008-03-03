/***************************************************************************
                       TrackPanelClipMoveFunction.cpp  -  description
                          -------------------
 begin                : Sun May 18 2003
 copyright            : (C) 2003 by Jason Wood
 email                : jasonwood@blueyonder.co.uk
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QMouseEvent>
#include <KDebug>

#include "trackpanelclipmovefunction.h"
#include "definitions.h"
#include "documenttrack.h"
#include "trackview.h"


TrackPanelClipMoveFunction::TrackPanelClipMoveFunction(TrackView *view):
        m_view(view), m_dragging(false), m_startedClipMove(false), m_masterClip(0), m_clipOffset(0) {
    // m_moveClipsCommand = 0;
    //m_deleteClipsCommand = 0;
    m_addingClips = false;
    m_firststep = true;
}


TrackPanelClipMoveFunction::~TrackPanelClipMoveFunction() {
}

bool TrackPanelClipMoveFunction::mouseApplies(DocumentTrack *,
        QMouseEvent * event) const {
    return mouseApplies(event->pos());
}

bool TrackPanelClipMoveFunction::mouseApplies(const QPoint & pos) const {
    TrackViewClip *clipUnderMouse = 0;

    DocumentTrack * panel = m_view->panelAt(pos.y());

    if (panel) {
        kDebug() << " TESTING CURSOR AT: " << pos.x() << ", MAPPED VAL: " << m_view->mapLocalToValue(pos.x());
        GenTime mouseTime((int)(m_view->mapLocalToValue(pos.x())), m_document->fps());
        clipUnderMouse = panel->getClipAt(mouseTime);
    }
    return clipUnderMouse;
}

QCursor TrackPanelClipMoveFunction::getMouseCursor(DocumentTrack *, QMouseEvent * event) {
    return QCursor(Qt::SizeAllCursor);
}

bool TrackPanelClipMoveFunction::mousePressed(DocumentTrack * panel,
        QMouseEvent * event) {
    bool result = false;
    /*
       if (panel->hasDocumentTrackIndex()) {
     DocTrackBase *track =
         m_document->track(panel->documentTrackIndex());
     if (track) {
      GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->x())),
      m_document->framesPerSecond());
         m_clipUnderMouse = 0;
         m_clipUnderMouse = track->getClipAt(mouseTime);

         if (m_clipUnderMouse) {
      emit checkTransition(m_clipUnderMouse);
      if (event->state() & Qt::ControlButton) {
          m_app->addCommand(Command::KSelectClipCommand::toggleSelectClipAt(m_document, *track, mouseTime), true);
      }
      else {
          if (!track->clipSelected(m_clipUnderMouse)) {
                KMacroCommand *macroCommand = new KMacroCommand(i18n("Select Clip"));
                macroCommand->addCommand(Command::KSelectClipCommand::selectNone(m_document));
                macroCommand->addCommand(new Command::KSelectClipCommand(m_document, m_clipUnderMouse, true));
                m_app->addCommand(macroCommand, true);
          }
      }
      result = true;
         }
     }
        }
    */
    return result;
}

bool TrackPanelClipMoveFunction::mouseDoubleClicked(DocumentTrack * panel, QMouseEvent *event) {
    /*
     if (panel->hasDocumentTrackIndex()) {
      DocTrackBase *track =
          m_document->track(panel->documentTrackIndex());
      if (track) {
       GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->x())),
      m_document->framesPerSecond());
       m_clipUnderMouse = track->getClipAt(mouseTime);
           if (m_clipUnderMouse) {
        track->openClip(m_clipUnderMouse);
       }
      }
     }*/
    return false; ///FIXME is that right ?
}

bool TrackPanelClipMoveFunction::mouseReleased(DocumentTrack *, QMouseEvent *) {
    //m_timeline->stopScrollTimer();
    return true;
}

bool TrackPanelClipMoveFunction::mouseMoved(DocumentTrack * panel,
        QMouseEvent * event) {
    bool result = false;
    /*if (panel->hasDocumentTrackIndex()) {
    DocTrackBase *track =
     m_document->track(panel->documentTrackIndex());
    if (track) {
    GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->x())),
    m_document->framesPerSecond());

     if (m_dragging) {
    m_dragging = false;
    result = true;
     } else {
    if (m_clipUnderMouse) {
      if (!m_document->projectClip().clipSelected(m_clipUnderMouse)) {
    if ((event->state() & Qt::ControlButton)
       || (event->state() & Qt::ShiftButton)) {
       m_app->
    addCommand(Command::KSelectClipCommand::
    selectClipAt(m_document, *track,
        mouseTime), true);
    } else {
             KMacroCommand *macroCommand = new KMacroCommand(i18n("Select Clip"));
             macroCommand->addCommand(Command::KSelectClipCommand::selectNone(m_document));
             macroCommand->addCommand(new Command::KSelectClipCommand(m_document, m_clipUnderMouse, true));
             m_app->addCommand(macroCommand, true);
    }
      }
      m_dragging = true;
      initiateDrag(m_clipUnderMouse, mouseTime);
      result = true;
    }
     }
    }
    }
    */
    return result;
}

// virtual
bool TrackPanelClipMoveFunction::dragEntered(DocumentTrack * panel,
        QDragEnterEvent * event) {
    /*    if (m_startedClipMove) {
     m_document->activateSceneListGeneration(false);
     event->accept(true);
        } else if (ClipDrag::canDecode(event)) {
     m_document->activateSceneListGeneration(false);
     m_selection = ClipDrag::decode(m_document, event);


     if (!m_selection.isEmpty()) {
         if (m_selection.masterClip() == 0)
      m_selection.setMasterClip(m_selection.first());
         m_masterClip = m_selection.masterClip();
         m_clipOffset = GenTime();
         if (m_selection.isEmpty()) {
      event->accept(false);
         } else {
      setupSnapToGrid();
      event->accept(true);
         }
     } else {
         kdError() <<
      "ERROR! ERROR! ERROR! ClipDrag:decode decoded a null clip!!!"
      << endl;
     }
        } else if (EffectDrag::canDecode(event)) {
     event->accept(true);
        } else {
     event->accept(false);
        }
        //m_startedClipMove = false;
    */
    return true;
}

// virtual
bool TrackPanelClipMoveFunction::dragMoved(DocumentTrack *, QDragMoveEvent * event) {
    /*    QPoint pos = event->pos();
        if (ClipDrag::canDecode(event)) {
     GenTime mouseTime = m_timeline->timeUnderMouse((double) pos.x()) - m_clipOffset;
     mouseTime = m_snapToGrid.getSnappedTime(mouseTime);
     mouseTime = mouseTime + m_clipOffset;
     int trackUnder = trackUnderPoint(pos);

     if (m_selection.isEmpty() || m_dragging) {
             moveSelectedClips(trackUnder, mouseTime - m_clipOffset);
     } else {
         if (m_document->projectClip().canAddClipsToTracks(m_selection,
          trackUnder, mouseTime)) {
      m_selection_to_add = m_selection;
      addClipsToTracks(m_selection, trackUnder, mouseTime, true);
      setupSnapToGrid();
      m_selection.clear();
      m_dragging = true;
         }
     }
        } else if (EffectDrag::canDecode(event)) {
     if (mouseApplies(pos)) {
         event->accept();
     } else {
         event->ignore();
     }
        } else {
     event->ignore();
        }
        m_timeline->checkScrolling(pos);
    */
    return true;
}

int TrackPanelClipMoveFunction::trackUnderPoint(const QPoint & pos) {
    uint y = pos.y();
    DocumentTrack * panel = m_view->panelAt(y);
    /*
        if (panel) {
     return panel->documentTrackIndex();
        }*/

    return -1;
}

// virtual
bool TrackPanelClipMoveFunction::dragLeft(DocumentTrack *, QDragLeaveEvent *) {
    m_dragging = false;
    /*
        if (!m_selection.isEmpty()) {
     m_selection.setAutoDelete(true);
     m_selection.clear();
     m_selection.setAutoDelete(false);
        }

        if (m_addingClips) {
     m_addingClips = false;

     QPtrListIterator < DocTrackBase >
         trackItt(m_document->trackList());

     while (trackItt.current()) {
         (*trackItt)->deleteClips(true);
         ++trackItt;
     }

     m_document->activateSceneListGeneration(true);
        }

        if (m_moveClipsCommand) {
            m_moveClipsCommand->setEndLocation(m_masterClip);
            m_app->addCommand(m_moveClipsCommand, false);
     // In a drag Leave Event, any clips in the selection are removed from the timeline.
     //delete m_moveClipsCommand;
            m_moveClipsCommand = 0;
            m_document->activateSceneListGeneration(true);
        }

        if (m_deleteClipsCommand) {
     m_app->addCommand(m_deleteClipsCommand, false);
     m_deleteClipsCommand = 0;

     QPtrListIterator < DocTrackBase >
         trackItt(m_document->trackList());

     while (trackItt.current()) {
         trackItt.current()->deleteClips(true);
         ++trackItt;
     }
        }

        m_timeline->drawTrackViewBackBuffer();
        m_timeline->stopScrollTimer();
    */
    return true;
}

// virtual
bool TrackPanelClipMoveFunction::dragDropped(DocumentTrack * panel,
        QDropEvent * event) {
    /*
        m_dragging = false;
        m_startedClipMove = false;
        if (ClipDrag::canDecode(event)) {
     if (!m_selection.isEmpty()) {
         m_selection.setAutoDelete(true);
         m_selection.clear();
         m_selection.setAutoDelete(false);
     }

     if (m_addingClips) {

         m_app->addCommand(createAddClipsCommand(), true);
         m_addingClips = false;
                m_app->clipReferenceChanged();

         //if (m_firststep) m_document->activateSceneListGeneration(true);
         m_firststep = false;
     }

     if (m_deleteClipsCommand) {
         delete m_deleteClipsCommand;
         m_deleteClipsCommand = 0;
     }

     if (m_moveClipsCommand) {
         m_moveClipsCommand->setEndLocation(m_masterClip);
         if (!m_moveClipsCommand->doesMove())
         {
      //m_document->activateSceneListGeneration(true);
      moveSelectedClips(m_moveClipsCommand->startTrack(), m_moveClipsCommand->startTime());
             m_app->addCommand(m_moveClipsCommand, true);
             m_moveClipsCommand = 0;
      m_document->slotUpdateMonitorPlaytime();
      // KdenliveApp is now managing this command, we do not need to delete it.
         }
         else {
      m_document->activateSceneListGeneration(true, false);
      delete m_moveClipsCommand;
      m_moveClipsCommand = 0;
         }
     }
     event->accept();
        } else if (EffectDrag::canDecode(event)) {
     DocClipRef *clipUnderMouse = 0;
     DocumentTrack * panel =
         m_view->panelAt(event->pos().y());
     if (panel) {
         DocTrackBase *track =
      m_document->track(panel->documentTrackIndex());
         if (track) {
        GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->pos().
       x())), m_document->framesPerSecond());
      clipUnderMouse = track->getClipAt(mouseTime);
         }
     }

     if (clipUnderMouse) {
         Effect *effect = EffectDrag::decode(m_document, event);
         if (effect) {
      m_app->
          addCommand(Command::KAddEffectCommand::
          appendEffect(m_document, clipUnderMouse, effect),
          true);
          if (effect->name() == i18n("Freeze")) m_app->getDocument()->emitCurrentClipPosition();
         } else {
      kdWarning() <<
          "EffectDrag::decode did not return an effect, ignoring drag drop..."
          << endl;
         }
         delete effect;
     }
        }
        m_timeline->stopScrollTimer();
        m_timeline->drawTrackViewBackBuffer();
    */
    return true;
}

bool TrackPanelClipMoveFunction::moveSelectedClips(int newTrack,
        GenTime start) {
    /*
        if (!m_masterClip) return false;
        int trackOffset =
     m_document->trackIndex(m_document->findTrack(m_masterClip));
        GenTime startOffset;

        if (trackOffset == -1) {
     kdError() <<
         "Trying to move selected clips, master clip is not set." <<
         endl;
     return false;
        } else {
     startOffset = m_masterClip->trackStart();
        }

        trackOffset = newTrack - trackOffset;
        startOffset = start - startOffset;
        if (startOffset == GenTime()) return false;
        m_document->moveSelectedClips(startOffset, trackOffset);
    */
    return true;
}

/*
void TrackPanelClipMoveFunction::addClipsToTracks(DocClipRefList & clips,
    int track, GenTime value, bool selected)
{

    if (clips.isEmpty())
 return;

    if (selected) {
 m_app->
     addCommand(Command::KSelectClipCommand::selectNone(m_document),
     true);
    }

    DocClipRef *masterClip = clips.masterClip();
    if (!masterClip)
 masterClip = clips.first();

    GenTime startOffset = value - masterClip->trackStart();

    int trackOffset = masterClip->trackNum();

    if (trackOffset == -1)
 trackOffset = 0;
    trackOffset = track - trackOffset;

    QPtrListIterator < DocClipRef > itt(clips);
    int moveToTrack;

    while (itt.current() != 0) {
 moveToTrack = itt.current()->trackNum();

 if (moveToTrack == -1) {
     moveToTrack = track;
     itt.current()->moveTrackStart(itt.current()->trackStart() + startOffset);
     startOffset += itt.current()->cropDuration();
 } else {
     moveToTrack += trackOffset;
     itt.current()->moveTrackStart(itt.current()->trackStart() + startOffset);
 }



 if ((moveToTrack >= 0) && (moveToTrack < (int)m_document->numTracks())) {
     //if (itt.current()->referencedClip()->numReferences() == 0)
     m_document->track(moveToTrack)->addClip(itt.current(), selected);
 }

 ++itt;
    }

    m_addingClips = true;
}
*/

void TrackPanelClipMoveFunction::setupSnapToGrid() {
    /*
        m_snapToGrid.clearSnapList();
        if (m_timeline->snapToSeekTime())
     m_snapToGrid.addToSnapList(m_timeline->seekPosition());
        m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());

        m_snapToGrid.addToSnapList(m_document->getSnapTimes(m_timeline->
         snapToBorders(), m_timeline->snapToMarkers(), true, false));

        QValueVector < GenTime > cursor =
     m_document->getSnapTimes(m_timeline->snapToBorders(),
     m_timeline->snapToMarkers(), false, true, false);
        m_snapToGrid.setCursorTimes(cursor);

      m_snapToGrid.setSnapTolerance(GenTime((int)(m_timeline->
         mapLocalToValue(Gui::KTimeLine::snapTolerance) -
        m_timeline->mapLocalToValue(0)),
         m_document->framesPerSecond()));
    */
}

/*void TrackPanelClipMoveFunction::initiateDrag(DocClipRef * clipUnderMouse,
    GenTime mouseTime)
{
    m_masterClip = clipUnderMouse;
    m_clipOffset = mouseTime - clipUnderMouse->trackStart();

    m_moveClipsCommand =
 new Command::KMoveClipsCommand(m_document, m_masterClip);

    m_moveClipsCommand->setClipList(m_document->listSelected());

    m_deleteClipsCommand =
 Command::KAddRefClipCommand::deleteSelectedClips(m_document);
    setupSnapToGrid();

    m_startedClipMove = true;

    DocClipRefList selection = m_document->listSelected();

    selection.setMasterClip(m_masterClip);
    ClipDrag *clip = new ClipDrag(selection, m_timeline, "Timeline Drag");

    clip->dragCopy();
}

KMacroCommand *TrackPanelClipMoveFunction::createAddClipsCommand()
{
    KMacroCommand *macroCommand = new KMacroCommand(i18n("Add Clips"));

  for (int count = 0; count < (int)m_document->numTracks(); ++count) {
 DocTrackBase *track = m_document->track(count);

 QPtrListIterator < DocClipRef > itt = track->firstClip(true);

 while (itt.current()) {
     Command::KAddRefClipCommand * command =
  new Command::KAddRefClipCommand(*m_document, itt.current(), true);
     macroCommand->addCommand(command);
     ++itt;
 }
 m_document->generateProducersList();
    }
    return macroCommand;
}
*/

#include "trackpanelclipmovefunction.moc"
