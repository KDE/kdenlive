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
#include "krendermanager.h"
#include "kdenlivedoc.h"
#include "kdenlive.h"
#include "kdenliveview.h"

#include "docclipavfile.h"
#include "docclipproject.h"
#include "doctrackvideo.h"
#include "doctracksound.h"
#include "doctrackclipiterator.h"
#include "clipdrag.h"
#include "avfile.h"

QPtrList<KdenliveView> *KdenliveDoc::pViewList = 0L;

KdenliveDoc::KdenliveDoc(KdenliveApp *app, QWidget *parent, const char *name) : 
				QObject(parent, name),
				m_projectClip(new DocClipProject(this)),
				m_modified(false)
{
	if(!pViewList)
	{
		pViewList = new QPtrList<KdenliveView>();
	}

	m_fileList.setAutoDelete(true);

	pViewList->setAutoDelete(true);

	m_app = app;
	m_render = m_app->renderManager()->createRenderer(i18n("Document"));

	connect(m_render, SIGNAL(replyGetFileProperties(QMap<QString, QString>)),
  					 this, SLOT(AVFilePropertiesArrived(QMap<QString, QString>)));
	connect(m_render, SIGNAL(replyErrorGetFileProperties(const QString &, const QString &)),
  					 this, SLOT(AVFilePropertiesError(const QString &, const QString &)));

	connect(this, SIGNAL(avFileListUpdated()), this, SLOT(hasBeenModified()));
	connect(this, SIGNAL(trackListChanged()), this, SLOT(hasBeenModified()));

	m_domSceneList.appendChild(m_domSceneList.createElement("scenelist"));
	generateSceneList();
	connectProjectClip();

	setModified(false);
}

KdenliveDoc::~KdenliveDoc()
{
	if(m_projectClip) delete m_projectClip;
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
	kdDebug() << "KdenliveDoc in saveModified()" << endl;
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
	kdDebug() << "KdenliveDoc in closeDocument()" << endl;
	deleteContents();
}

bool KdenliveDoc::newDocument()
{
	kdDebug() << "Creating new document" << endl;

	deleteContents();

	addVideoTrack();
	addVideoTrack();
	addVideoTrack();
	addVideoTrack();

	m_doc_url.setFileName(i18n("Untitled"));

	emit trackListChanged();
	setModified(false);

	return true;
}

bool KdenliveDoc::openDocument(const KURL& url, const char *format)
{
  if(url.isEmpty()) return false;

  if(url.filename().right(9) == ".kdenlive") {
    QString tmpfile;
    if(KIO::NetAccess::download( url, tmpfile )) {
  	  QFile file(tmpfile);
  	 	if(file.open(IO_ReadOnly)) {
  	  	QDomDocument doc;
  	    doc.setContent(&file, false);
		loadFromXML(doc);
  	  }
  	  KIO::NetAccess::removeTempFile( tmpfile );
  	  setModified(false);
  	  return true;	  
  	}

  	emit trackListChanged();
  } else {
    insertAVFile(url);
  }

  return false;
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
	kdDebug() << "deleting contents..." << endl;

	delete m_projectClip;
	m_projectClip = new DocClipProject(this);
	connectProjectClip();

	m_fileList.clear();
	emit trackListChanged();
	emit avFileListUpdated();
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

const AVFileList &KdenliveDoc::avFileList() const
{
	return m_fileList;
}

/** Returns the number of frames per second. */
int KdenliveDoc::framesPerSecond() const
{
	if(m_projectClip) {
		return m_projectClip->framesPerSecond();
	} else {
		kdWarning() << "KdenliveDoc cannot calculate frames per second - m_projectClip is null!!! Perhaps m_projectClip is in the process of being created? Temporarily returning 25 as a placeholder - needs fixing." << endl;
		return 25;
	}
}

/** Adds an empty video track to the project */
void KdenliveDoc::addVideoTrack()
{
	m_projectClip->addTrack(new DocTrackVideo(this));
}

/** Adds a sound track to the project */
void KdenliveDoc::addSoundTrack(){
	m_projectClip->addTrack(new DocTrackSound(this));
}

/** Returns the number of tracks in this project */
uint KdenliveDoc::numTracks()
{
	return m_projectClip->numTracks();
}

/** Inserts a list of clips into the document, updating the project accordingly. */
void KdenliveDoc::slot_insertClips(QPtrList<DocClipBase> clips)
{
	if(clips.isEmpty()) return;

	clips.setAutoDelete(true);

	DocClipBase *clip;
	for(clip = clips.first(); clip; clip=clips.next()) {
		insertAVFile(clip->fileURL());
	}

	emit avFileListUpdated();
	setModified(true);
}

/** Returns a reference to the AVFile matching the  url. If no AVFile matching the given url is
found, then one will be created. This method is not in charge of incrementing the reference count
of the avfile - this must be done by the calling function. */
AVFile * KdenliveDoc::getAVFileReference(KURL url)
{
	AVFile *av = insertAVFile(url);
	return av;
}

/** Find and return the AVFile with the url specified, or return null is no file matches. */
AVFile * KdenliveDoc::findAVFile(const KURL &file)
{
	return m_fileList.find(file);
}

/** Given a drop event, inserts all contained clips into the project list, if they are not there already. */
void KdenliveDoc::slot_insertClips(QDropEvent *event)
{
	// sanity check.
	if(!ClipDrag::canDecode(event)) return;

	DocClipBaseList clips = ClipDrag::decode(this, event);

	slot_insertClips(clips);	
}

/** returns the Track which holds the given clip. If the clip does not
exist within the document, returns 0; */
DocTrackBase * KdenliveDoc::findTrack(DocClipBase *clip)
{
	return m_projectClip->findTrack(clip);
}

/** Returns the track with the given index, or returns NULL if it does
not exist. */
DocTrackBase * KdenliveDoc::track(int track)
{
	return m_projectClip->track(track);
}

/** Returns the index value for this track, or -1 on failure.*/
int KdenliveDoc::trackIndex(DocTrackBase *track) const
{
	if(m_projectClip) {
		return m_projectClip->trackIndex(track);
	} else {
		kdError() << "Cannot return track index - m_projectClip is Null!" << endl;
	}
	return -1;
}

/** Sets the modified state of the document, if this has changed, emits modified(state) */
void KdenliveDoc::setModified(bool state)
{
	if(m_modified != state) {
		m_modified = state;		
		emit modified(state);
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
			kdError() << "Cannot delete a clip that has references!" << endl;
		} else {
			/** If we delete the clip before removing the pointer to it in the relevant
			 *  AVListViewItem, bad things happen... For some reason, the text method 
			 *  gets called after the deletion, even though the very next thing we do 
			 *  is to emit an update signal. which removes it.*/
			m_fileList.setAutoDelete(false);
			m_fileList.removeRef(file);
			emit avFileListUpdated();
			delete file;
			m_fileList.setAutoDelete(true);
		}
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
  emit avFileChanged(file);  
}


void KdenliveDoc::AVFilePropertiesError(const QString &path, const QString &errmsg)
{
  AVFile *file = findAVFile(KURL(path));

  KdenliveApp *win=(KdenliveApp *) parent();
  KMessageBox::sorry(win, errmsg, path);
  
  deleteAVFile(file);
}

/** Moves the currectly selected clips by the offsets specified, or returns false if this
is not possible. */
bool KdenliveDoc::moveSelectedClips(GenTime startOffset, int trackOffset)
{
	bool result = m_projectClip->moveSelectedClips(startOffset, trackOffset);

	if(result) hasBeenModified();
	return result;
}

/** Returns a scene list generated from the current document. */
QDomDocument KdenliveDoc::generateSceneList()
{
	if(m_projectClip) {
		m_domSceneList = m_projectClip->generateSceneList();
	} else {
		kdWarning() << "Cannot generate scene list - m_projectClip is null!" << endl;
	}
	return m_domSceneList;
  
}

/** Creates an xml document that describes this kdenliveDoc. */
QDomDocument KdenliveDoc::toXML()
{
	QDomDocument document;

	QDomElement elem = document.createElement("kdenlivedoc");
	document.appendChild(elem);

	elem.appendChild(document.importNode(m_fileList.toXML().documentElement(), true));
	elem.appendChild(document.importNode(m_projectClip->toXML().documentElement(), true));

	return document;
}

/** Parses the XML Dom Document elements to populate the KdenliveDoc. */
void KdenliveDoc::loadFromXML(QDomDocument &doc)
{
	bool avListLoaded = false;
	bool trackListLoaded = false;

	deleteContents();

	QDomElement elem = doc.documentElement();

	if(elem.tagName() != "kdenlivedoc") {
		kdWarning()	<< "KdenliveDoc::loadFromXML() document element has unknown tagName : " << elem.tagName() << endl;
	}

	QDomNode n = elem.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			if(e.tagName() == "AVFileList") {
				if(!avListLoaded) {
					m_fileList.generateFromXML(m_render, e);
					avListLoaded = true;
				} else {
					kdWarning() << "Second AVFileList discovered, skipping..." << endl;
				}
			} else if(e.tagName() == "DocTrackBaseList") {
				kdWarning() << "Loading old project, when this is saved it will no " <<
				       		"longer be readable by older versions of the " <<
						"software." << endl;

				if(m_projectClip) {
					delete m_projectClip;
					m_projectClip = 0;
				}
				
				if(!trackListLoaded) {
					m_projectClip = new DocClipProject(this);
					connectProjectClip();
					m_projectClip->generateTracksFromXML(e);
					trackListLoaded = true;
				} else {
					kdWarning() << "Second DocTrackBaseList discovered, skipping... " << endl;
				}
			} else if(e.tagName() == "clip") {
				if(m_projectClip) {
					delete m_projectClip;
					m_projectClip = 0;
				}
				DocClipBase *clip = DocClipBase::createClip(this, e);

				if(clip->isProjectClip()) {
					m_projectClip = dynamic_cast<DocClipProject *>(clip);
				} else {
					delete clip;
					kdError() << "Base clip detected, not a project clip. Ignoring..." << endl;
				}
			} else {
				kdWarning() << "Unknown tag " << e.tagName() << ", skipping..." << endl;
			}
		}

		n = n.nextSibling();
	}

	emit avFileListUpdated();
	emit trackListChanged();
}

/** Called when the document is modifed in some way. */
void KdenliveDoc::hasBeenModified()
{
	kdDebug() << "Document has changed" << endl;
	if(m_projectClip) {
		m_projectClip->setTrackEnd(m_projectClip->duration());
	} else {
		kdWarning() << "m_projectClip is Null!" << endl;
	}
	generateSceneList();
	emit documentChanged();
	emit documentChanged(m_projectClip);
	setModified(true);
}

/** Renders the current document timeline to the specified url. */
void KdenliveDoc::renderDocument(const KURL &url)
{
	m_render->setSceneList(m_domSceneList);
	m_render->render(url);
}

/** Returns true if we should snape values to frame. */
bool KdenliveDoc::snapToFrame()
{
  return m_app->snapToFrameEnabled();
}

/** Returns renderer associated with this document. */
KRender * KdenliveDoc::renderer()
{
  return m_render;
}

void KdenliveDoc::connectProjectClip()
{
	connect(m_projectClip, SIGNAL(trackListChanged()), this, SIGNAL(trackListChanged()));
	connect(m_projectClip, SIGNAL(clipLayoutChanged()), this, SLOT(hasBeenModified()));
	connect(m_projectClip, SIGNAL(signalClipSelected(DocClipBase *)), this, SIGNAL(signalClipSelected(DocClipBase *)));
}

DocTrackBase * KdenliveDoc::nextTrack()
{
	return m_projectClip->nextTrack();
}

DocTrackBase * KdenliveDoc::firstTrack()
{
	return m_projectClip->firstTrack();
}

GenTime KdenliveDoc::projectDuration() const
{
	// for m_projectClip, trackEnd should always be the same as duration. However, 
	// trackEnd is cached, whilst duration is not...
	return m_projectClip->trackEnd();
}

void KdenliveDoc::indirectlyModified()
{
	hasBeenModified();
}

QPtrList<DocClipBase> KdenliveDoc::referencedClips(AVFile *file)
{
	QPtrList<DocClipBase> list;

	if(m_projectClip) {
		list = m_projectClip->referencedClips(file);
	} else {
		kdError() << "Cannot get referenced clips - m_projectClip is null!" << endl;
	}

	return list;
}

bool KdenliveDoc::hasSelectedClips()
{
	bool result = false;
	
	if(m_projectClip) {
		result = m_projectClip->hasSelectedClips();
	} else {
		kdError() << "No selection in the project because m_projectClip is null!!" << endl;
	}
	return result;
}

DocClipBase *KdenliveDoc::selectedClip()
{
	DocClipBase *pResult = 0;

	if(m_projectClip) {
		pResult = m_projectClip->selectedClip();
	} else {
		kdError() << "No selection in the project because m_projectClip is null!!!" << endl;
	}
	
	return pResult;
}
