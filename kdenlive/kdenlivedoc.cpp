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
#include <qvaluevector.h>
#include <qptrvector.h>

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
#include "doctrackclipiterator.h"
#include "clipdrag.h"

QPtrList<KdenliveView> *KdenliveDoc::pViewList = 0L;
KRender KdenliveDoc::temporaryRenderer;

KdenliveDoc::KdenliveDoc(QWidget *parent, const char *name) : QObject(parent, name)
{
  if(!pViewList)
  {
    pViewList = new QPtrList<KdenliveView>();
  }

  m_framesPerSecond = 25; // Standard PAL.

  m_fileList.setAutoDelete(true);
	
  pViewList->setAutoDelete(true);

//  m_render = new KRender();
	m_render = &temporaryRenderer;
  connect(m_render, SIGNAL(replyGetFileProperties(QMap<QString, QString>)),
  					 this, SLOT(AVFilePropertiesArrived(QMap<QString, QString>)));

 m_render->setSceneList(generateSceneList());
}

KdenliveDoc::~KdenliveDoc()
{
//	if(m_render) delete m_render;
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
  m_doc_url=url;
}

const KURL& KdenliveDoc::URL() const
{
  return m_doc_url;
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

  if(m_modified)
  {
    KdenliveApp *win=(KdenliveApp *) parent();
    int want_save = KMessageBox::warningYesNoCancel(win,
                                         i18n("The current file has been modified.\n"
                                              "Do you want to save it?"),
                                         i18n("Warning"));
    switch(want_save)
    {
      case KMessageBox::Yes:
           if (m_doc_url.fileName() == i18n("Untitled"))
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

  setModified(false);
  m_doc_url.setFileName(i18n("Untitled"));

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

  setModified(false);  
  return true;
}

bool KdenliveDoc::saveDocument(const KURL& url, const char *format /*=0*/)
{
  /////////////////////////////////////////////////
  // TODO: Add your document saving code here
  /////////////////////////////////////////////////

  QString save = toXML().toString();

  kdDebug() << save << endl;

  if(!url.isLocalFile()) {
  	#warning network transparency still to be written.
    KMessageBox::sorry((KdenliveApp *) parent(), i18n("The current file has been modified.\n"),
     i18n("unfinished code"));
    
   	return false;
  } else {
  	QFile file(url.path());
   	if(file.open(IO_WriteOnly)) {
			file.writeBlock(save, save.length());
			file.close();
    }
  }  

  setModified(false);  
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
		m_render->getFileProperties(file);
		emit avFileListUpdated();
		setModified(true);
	}
	
	return av;
}

const AVFileList &KdenliveDoc::avFileList()
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

  emit avFileListUpdated();
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

/** Creates an xml document that describes this kdenliveDoc. */
QDomDocument KdenliveDoc::toXML()
{
	QDomDocument document;

	QDomElement elem = document.createElement("kdenlivedoc");
	document.appendChild(elem);

	elem.appendChild(document.importNode(m_fileList.toXML().documentElement(), true));
	elem.appendChild(document.importNode(m_tracks.toXML().documentElement(), true));
	
	return document;
}

/** Sets the modified state of the document, if this has changed, emits modified(state) */
void KdenliveDoc::setModified(bool state)
{
	if(m_modified != state) {
		m_modified = state;		
		emit modified(state);
	}
}

/** Removes entries from the AVFileList which are unreferenced by any clips. */
void KdenliveDoc::cleanAVFileList()
{
	QPtrListIterator<AVFile> itt(m_fileList);

	while(itt.current()) {
		QPtrListIterator<AVFile> next = itt;
		++itt;
		if(next.current()->numReferences()==0) {
			deleteAVFile(next.current());
		}
	}
}

/** Finds and removes the specified avfile from the document. If there are any
clips on the timeline which use this clip, then they will be deleted as well.
Emits AVFileList changed if successful. */
void KdenliveDoc::deleteAVFile(AVFile *file)
{
	int index = m_fileList.findRef(file);

	if(index!=-1) {
		if(file->numReferences() > 0) {
			#warning Deleting files with references not yet implemented
			kdWarning() << "Cannot delete files with references at the moment " << endl;
			return;
		}

		/** If we delete the clip before removing the pointer to it in the relevant AVListViewItem,
		bad things happen... For some reason, the text method gets called after the deletion, even
		though the very next thing we do is to emit an update signal. which removes it.*/
		m_fileList.setAutoDelete(false);
		m_fileList.removeRef(file);
		emit avFileListUpdated();
		delete file;
		m_fileList.setAutoDelete(true);
	} else {
		kdError() << "Trying to delete AVFile that is not in document!" << endl;
	}
}

void KdenliveDoc::AVFilePropertiesArrived(QMap<QString, QString> properties)
{
	if(!properties.contains("filename")) {
		kdError() << "File properties returned with no file name attached" << endl;
		return;
	}
	
	AVFile *file = findAVFile(KURL(properties["filename"]));
	if(!file) {
		kdWarning() << "File properties returned for a non-existant AVFile" << endl;
		return;
	}

	file->calculateFileProperties(properties);
}

/** Moves the currectly selected clips by the offsets specified, or returns false if this
is not possible. */
bool KdenliveDoc::moveSelectedClips(GenTime startOffset, int trackOffset)
{
	// For each track, check and make sure that the clips can be moved to their rightful place. If
	// one cannot be moved, then none of them can be moved.
	int destTrackNum;
	DocTrackBase *srcTrack, *destTrack;
	GenTime clipStartTime;
	GenTime clipEndTime;
	DocClipBase *srcClip, *destClip;

	for(int track=0; track<numTracks(); track++) {
		srcTrack = m_tracks.at(track);
		if(!srcTrack->hasSelectedClips()) continue;

		destTrackNum = track + trackOffset;

		if((destTrackNum < 0) || (destTrackNum >= numTracks())) return false;	// This track will be moving it's clips out of the timeline, so fail automatically.

		destTrack = m_tracks.at(destTrackNum);

		QPtrListIterator<DocClipBase> srcClipItt = srcTrack->firstClip(true);
		QPtrListIterator<DocClipBase> destClipItt = destTrack->firstClip(false);

		destClip = destClipItt.current();

		while( (srcClip = srcClipItt.current()) != 0) {
			clipStartTime = srcClipItt.current()->trackStart() + startOffset;
			clipEndTime = clipStartTime + srcClipItt.current()->cropDuration();

			while((destClip) && (destClip->trackStart() + destClip->cropDuration() <= clipStartTime)) {
				++destClipItt;
				destClip = destClipItt.current();
			}
			if(destClip==0) break;

			if(destClip->trackStart() < clipEndTime) return false;

			++srcClipItt;
		}
	}

	// we can now move all clips where they need to be.

	// If the offset is negative, handle tracks from forwards, else handle tracks backwards. We
	// do this so that there are no collisions between selected clips, which would be caught by DocTrackBase
	// itself.

	int startAtTrack, endAtTrack, direction;

	if(trackOffset < 0) {
		startAtTrack = 0;
		endAtTrack = numTracks();
		direction = 1;
	} else {
		startAtTrack = numTracks() - 1;
		endAtTrack = -1;
		direction = -1;
	}

	for(int track=startAtTrack; track!=endAtTrack; track += direction) {
		srcTrack = m_tracks.at(track);
		if(!srcTrack->hasSelectedClips()) continue;
		srcTrack->moveClips(startOffset, true);

		if(trackOffset) {
			destTrackNum = track + trackOffset;
			destTrack = m_tracks.at(destTrackNum);
			destTrack->addClips(srcTrack->removeClips(true), true);
		}
	}

	m_render->setSceneList(generateSceneList());

	return true;
}

/** Returns a scene list generated from the current document. */
QDomDocument KdenliveDoc::generateSceneList()
{
	static QString str_sceneList="scenelist";
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";		
	
	int totalTracks = numTracks();	
	
	QDomDocument doc;

	QDomElement elem = doc.createElement(str_sceneList);

	// generate the header
/*	QDomElement header = doc.createElement("header");
 	QDomElement version = doc.createElement("version");
  version.setAttribute("major", "0");
  version.setAttribute("minor", "06");
  header.appendChild(version);	
	elem.appendChild(header);*/
				
	// Generate the scene list.
  GenTime curTime, nextTime, clipStart, clipEnd;
  GenTime invalidTime(-1.0);
  DocClipBase *curClip;  
  int count;

  QPtrVector<DocTrackClipIterator> itt(totalTracks);

  QValueVector<GenTime> nextScene(totalTracks);
  
  itt.setAutoDelete(true);

  for(count=0; count<totalTracks; count++) {
  	itt.insert(count, new DocTrackClipIterator(*(m_tracks.at(count))));
   	if(itt[count]->current()) {
	   	nextScene[count] = itt[count]->current()->trackStart();
    } else {
	   	nextScene[count] = invalidTime;
    }
  }

  
  do {
    curTime = nextTime;	  

    for(count=0; count<totalTracks; count++) {
    	if(nextScene[count] == invalidTime) continue;
     	if(nextScene[count] <= curTime) {
	   		curClip = itt[count]->current();          
				if(curClip->trackEnd() <= curTime) {
					++(*itt[count]);
		   		curClip = itt[count]->current();
				}
				if(!curClip) {
					nextScene[count] = invalidTime;
					continue;
				} else {
					nextScene[count] = curClip->trackStart();
					if(nextScene[count] <= curTime) nextScene[count] = curClip->trackEnd();
				}
			}	

			if((nextTime == curTime) || (nextTime > nextScene[count])) {
				nextTime = nextScene[count];
			}
    }

    if(nextTime!=curTime) {
	    // generate the next scene.
	    QDomElement scene = doc.createElement("scene");
	    scene.setAttribute("duration", QString::number((nextTime-curTime).seconds()));

    	QDomElement sceneClip;

      for(count = totalTracks-1; count>=0; count--) {
      	curClip = itt[count]->current();
       	if(!curClip) continue;
        if(curClip->trackStart() >= nextTime) continue;
        if(curClip->trackEnd() <= curTime) continue;

	     sceneClip = doc.createElement("input");
	     	sceneClip.setAttribute(str_file, curClip->fileURL().path());
	     	sceneClip.setAttribute(str_inpoint, QString::number((curTime - curClip->trackStart() + curClip->cropStartTime()).seconds()));
	     	sceneClip.setAttribute(str_outpoint, QString::number((nextTime - curClip->trackStart() + curClip->cropStartTime()).seconds()));
	    }

	    if(!sceneClip.isNull()) {
	    	scene.appendChild(sceneClip);
	    }
	    
	    elem.appendChild(scene);
	  } 
  } while(curTime != nextTime);	
						
	doc.appendChild(elem);

	return doc;
}
