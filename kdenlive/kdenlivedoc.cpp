/***************************************************************************
                          kdenlivedoc.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 01:46:16 GMT 2002
    copyright            : (C) 2002 by Jason Wood
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

// include files for Qt
#include <qdir.h>
#include <qwidget.h>

// include files for KDE
#include <klocale.h>
#include <kmessagebox.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kdebug.h>

// application specific includes
#include "kdenlivedoc.h"
#include "kdenlive.h"
#include "kdenliveview.h"

#include "docclipavfile.h"
#include "doctrackvideo.h"
#include "doctracksound.h"
#include "clipdrag.h"

QPtrList<KdenliveView> *KdenliveDoc::pViewList = 0L;

KdenliveDoc::KdenliveDoc(QWidget *parent, const char *name) : QObject(parent, name)
{
  if(!pViewList)
  {
    pViewList = new QPtrList<KdenliveView>();
  }

  m_framesPerSecond = 25; // Standard PAL.

  m_fileList.setAutoDelete(true);
	
  pViewList->setAutoDelete(true);
}

KdenliveDoc::~KdenliveDoc()
{
}

void KdenliveDoc::addView(KdenliveView *view)
{
  pViewList->append(view);
}

void KdenliveDoc::removeView(KdenliveView *view)
{
  pViewList->remove(view);
}

void KdenliveDoc::setURL(const KURL &url)
{
  doc_url=url;
}

const KURL& KdenliveDoc::URL() const
{
  return doc_url;
}

void KdenliveDoc::slotUpdateAllViews(KdenliveView *sender)
{
  KdenliveView *w;
  if(pViewList)
  {
    for(w=pViewList->first(); w!=0; w=pViewList->next())
    {
      if(w!=sender)
        w->repaint();
    }
  }
}

bool KdenliveDoc::saveModified()
{
  bool completed=true;

  if(modified)
  {
    KdenliveApp *win=(KdenliveApp *) parent();
    int want_save = KMessageBox::warningYesNoCancel(win,
                                         i18n("The current file has been modified.\n"
                                              "Do you want to save it?"),
                                         i18n("Warning"));
    switch(want_save)
    {
      case KMessageBox::Yes:
           if (doc_url.fileName() == i18n("Untitled"))
           {
             win->slotFileSaveAs();
           }
           else
           {
             saveDocument(URL());
       	   };

       	   deleteContents();
           completed=true;
           break;

      case KMessageBox::No:
           setModified(false);
           deleteContents();
           completed=true;
           break;

      case KMessageBox::Cancel:
           completed=false;
           break;

      default:
           completed=false;
           break;
    }
  }

  return completed;
}

void KdenliveDoc::closeDocument()
{
  deleteContents();
}

bool KdenliveDoc::newDocument()
{
  /////////////////////////////////////////////////
  // TODO: Add your document initialization code here
  /////////////////////////////////////////////////

  m_fileList.setAutoDelete(true);

  addVideoTrack();
  addVideoTrack();
  addVideoTrack();
  addVideoTrack();  

  modified=false;
  doc_url.setFileName(i18n("Untitled"));

  return true;
}

bool KdenliveDoc::openDocument(const KURL& url, const char *format /*=0*/)
{
  QString tmpfile;
  KIO::NetAccess::download( url, tmpfile );
  /////////////////////////////////////////////////
  // TODO: Add your document opening code here
  /////////////////////////////////////////////////

  KIO::NetAccess::removeTempFile( tmpfile );

  modified=false;
  return true;
}

bool KdenliveDoc::saveDocument(const KURL& url, const char *format /*=0*/)
{
  /////////////////////////////////////////////////
  // TODO: Add your document saving code here
  /////////////////////////////////////////////////

  modified=false;
  return true;
}

void KdenliveDoc::deleteContents()
{
  /////////////////////////////////////////////////
  // TODO: Add implementation to delete the document contents
  /////////////////////////////////////////////////

  m_fileList.clear();
}

void KdenliveDoc::slot_InsertAVFile(const KURL &file)
{
	insertAVFile(file);
}

AVFile *KdenliveDoc::insertAVFile(const KURL &file)
{
	AVFile *av = findAVFile(file);

	if(!av) {	
		av = new AVFile(file.fileName(), file);
		m_fileList.append(av);
		emit avFileListUpdated(m_fileList);
		setModified(true);
	}
	
	return av;
}

QPtrList<AVFile> KdenliveDoc::avFileList()
{
	return m_fileList;	
}

/** Returns the number of frames per second. */
int KdenliveDoc::framesPerSecond()
{
	return m_framesPerSecond;
}

/** Adds an empty video track to the project */
void KdenliveDoc::addVideoTrack()
{
	addTrack(new DocTrackVideo(this));
}

/** Adds a sound track to the project */
void KdenliveDoc::addSoundTrack(){
	addTrack(new DocTrackSound(this));
}

/** Adds a track to the project */
void KdenliveDoc::addTrack(DocTrackBase *track){
	m_tracks.append(track);
	emit trackListChanged();
}

/** Returns the number of tracks in this project */
int KdenliveDoc::numTracks()
{
	return m_tracks.count();
}

/** Returns the first track in the project, and resets the itterator to the first track.
	* This effectively is the same as QPtrList::first(), but the underyling implementation
	* may change. */
DocTrackBase * KdenliveDoc::firstTrack()
{
	return m_tracks.first();
}

/** Itterates through the tracks in the project. This works in the same way
	* as QPtrList::next(), although the underlying structures may be different. */
DocTrackBase * KdenliveDoc::nextTrack()
{
	return m_tracks.next();
}

/** Inserts a list of clips into the document, updating the project accordingly. */
void KdenliveDoc::slot_insertClips(QPtrList<DocClipBase> clips)
{
	clips.setAutoDelete(true);
	
	DocClipBase *clip;		
	for(clip = clips.first(); clip; clip=clips.next()) {
		insertAVFile(clip->fileURL());
	}

  emit avFileListUpdated(m_fileList);
	setModified(true);	
}

/** Returns a reference to the AVFile matching the  url. If no AVFile matching the given url is
found, then one will be created. Either way, the reference count for the AVFile will be incremented
 by one, and the file will be returned. */
AVFile * KdenliveDoc::getAVFileReference(KURL url)
{
	AVFile *av = insertAVFile(url);
	av->addReference();
	return av;
}

/** Find and return the AVFile with the url specified, or return null is no file matches. */
AVFile * KdenliveDoc::findAVFile(const KURL &file)
{
	QPtrListIterator<AVFile> itt(m_fileList);

	AVFile *av;

	while( (av = itt.current()) != 0) {
		if(av->fileURL().path() == file.path()) return av;
		++itt;
	}

	return 0;
}

/** Given a drop event, inserts all contained clips into the project list, if they are not there already. */
void KdenliveDoc::slot_insertClips(QDropEvent *event)
{
	// sanity check.
	if(!ClipDrag::canDecode(event)) return;

	DocClipBaseList clips = ClipDrag::decode(*this, event);

	slot_insertClips(clips);	
}

/** returns the Track which holds the given clip. If the clip does not
exist within the document, returns 0; */
DocTrackBase * KdenliveDoc::findTrack(DocClipBase *clip)
{
	QPtrListIterator<DocTrackBase> itt(m_tracks);

	for(DocTrackBase *track;(track = itt.current()); ++itt) {
		if(track->clipExists(clip)) {
			return track;
		}
	}
	
	return 0;
}

/** Returns the track with the given index, or returns NULL if it does
not exist. */
DocTrackBase * KdenliveDoc::track(int track)
{
	return m_tracks.at(track);
}

/** Returns the index value for this track, or -1 on failure.*/
int KdenliveDoc::trackIndex(DocTrackBase *track)
{
	return m_tracks.find(track);
}
