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
#include <kcommand.h>

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

#include "documentclipnode.h"
#include "documentgroupnode.h"
#include "documentbasenode.h"

QPtrList<KdenliveView> *KdenliveDoc::pViewList = 0L;

KdenliveDoc::KdenliveDoc(KdenliveApp *app, QWidget *parent, const char *name) : 
				QObject(parent, name),
				m_projectClip(new DocClipProject),
				m_modified(false),
				m_sceneListGeneration(true),
				m_clipHierarch(0),
				m_clipManager(*app->renderManager())
{
	if(!pViewList)
	{
		pViewList = new QPtrList<KdenliveView>();
	}

	pViewList->setAutoDelete(true);

	m_app = app;
	m_render = m_app->renderManager()->createRenderer(i18n("Document"));

	connect(m_render, SIGNAL(replyErrorGetFileProperties(const QString &, const QString &)),
  					 this, SLOT(AVFilePropertiesError(const QString &, const QString &)));

	connect(this, SIGNAL(clipListUpdated()), this, SLOT(hasBeenModified()));
	connect(this, SIGNAL(trackListChanged()), this, SLOT(hasBeenModified()));

	connect(&m_clipManager, SIGNAL(clipChanged(DocClipBase* )), this, SLOT(clipChanged(DocClipBase *)));

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

void KdenliveDoc::closeDocument()
{
	kdDebug() << "KdenliveDoc in closeDocument()" << endl;
	deleteContents();
}

bool KdenliveDoc::newDocument()
{
	kdDebug() << "Creating new document" << endl;

	m_sceneListGeneration = true;

	deleteContents();

	m_clipHierarch = new DocumentGroupNode(0, i18n("Clips"));

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
    					setURL(url);
				}
				KIO::NetAccess::removeTempFile( tmpfile );
				setModified(false);
				return true;	  
		}
		emit trackListChanged();
	} else {
		m_clipManager.insertClip(url);
	}

	return false;
}

bool KdenliveDoc::saveDocument(const KURL& url, const char *format /*=0*/)
{
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
	m_projectClip = new DocClipProject;
	connectProjectClip();

	if(m_clipHierarch) {
		delete m_clipHierarch;
		m_clipHierarch = 0;
	}
		
	emit trackListChanged();

	m_clipManager.clear();
	emit clipListUpdated();
}

void KdenliveDoc::slot_InsertClip(const KURL &file)
{
	m_clipManager.insertClip(file);
}

/** Returns the number of frames per second. */
double KdenliveDoc::framesPerSecond() const
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
	m_projectClip->addTrack(new DocTrackVideo(m_projectClip));
}

/** Adds a sound track to the project */
void KdenliveDoc::addSoundTrack(){
	m_projectClip->addTrack(new DocTrackSound(m_projectClip));
}

/** Returns the number of tracks in this project */
uint KdenliveDoc::numTracks()
{
	return m_projectClip->numTracks();
}

/** Inserts a list of clips into the document, updating the project accordingly. */
void KdenliveDoc::slot_insertClips(DocClipRefList clips)
{
	if(clips.isEmpty()) return;

	clips.setAutoDelete(true);

	DocClipRef *clip;
	for(clip = clips.first(); clip; clip=clips.next()) {
		m_clipManager.insertClip(clip->fileURL());
	}

	emit clipListUpdated();
	setModified(true);
}

/** Given a drop event, inserts all contained clips into the project list, if they are not there already. */
void KdenliveDoc::slot_insertClips(QDropEvent *event)
{
	// sanity check.
	if(!ClipDrag::canDecode(event)) return;

	DocClipRefList clips = ClipDrag::decode(m_clipManager, event);

	slot_insertClips(clips);	
}

/** returns the Track which holds the given clip. If the clip does not
exist within the document, returns 0; */
DocTrackBase * KdenliveDoc::findTrack(DocClipRef *clip)
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

	elem.appendChild(document.importNode(m_clipManager.toXML("avfilelist").documentElement(), true));

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
			if(e.tagName() == "avfilelist") {
				if(!avListLoaded) {
					m_clipManager.generateFromXML(m_render, e);
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
					m_projectClip = new DocClipProject();
					connectProjectClip();
					m_projectClip->generateTracksFromXML(m_clipManager, e);
					trackListLoaded = true;
				} else {
					kdWarning() << "Second DocTrackBaseList discovered, skipping... " << endl;
				}
			} else if(e.tagName() == "clip") {
				if(m_projectClip) {
					delete m_projectClip;
					m_projectClip = 0;
				}
				DocClipBase *clip = DocClipBase::createClip(m_clipManager, e);

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

	emit clipListUpdated();
	emit trackListChanged();
}

/** Called when the document is modifed in some way. */
void KdenliveDoc::hasBeenModified()
{
	if(m_sceneListGeneration) {
		kdDebug() << "Document has changed" << endl;
		generateSceneList();
		emit documentChanged();
		emit documentChanged(m_projectClip);
	}
	setModified(true);
}

/** Renders the current document timeline to the specified url. */
void KdenliveDoc::renderDocument(const KURL &url)
{
	m_render->setSceneList(m_domSceneList);
	m_render->render(url);
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
	connect(m_projectClip, SIGNAL(signalClipSelected(DocClipRef *)), this, SIGNAL(signalClipSelected(DocClipRef *)));
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
	return m_projectClip->duration();
}

void KdenliveDoc::indirectlyModified()
{
	hasBeenModified();
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

DocClipRef *KdenliveDoc::selectedClip()
{
	DocClipRef *pResult = 0;

	if(m_projectClip) {
		pResult = m_projectClip->selectedClip();
	} else {
		kdError() << "No selection in the project because m_projectClip is null!!!" << endl;
	}
	
	return pResult;
};

void KdenliveDoc::activeSceneListGeneration(bool active)
{
	m_sceneListGeneration = active;
	if(active)
	{
		hasBeenModified();
	}
}

DocClipRefList KdenliveDoc::referencedClips(DocClipBase *clip)
{
	return m_projectClip->referencedClips(clip);
}

void KdenliveDoc::deleteClipNode(const QString &name)
{
	DocumentBaseNode *node = findClipNode(name);

	if(node) {
		if(!node->hasChildren()) {
			if( (node->asClipNode() == NULL) || (!m_projectClip->referencesClip(node->asClipNode()->clipRef()->referencedClip()))) {
				node->parent()->removeChild(node);
				delete node;
			} else {
				kdError() << "Trying to delete clip that has references in the document - "
					<< "must delete references first. Silently ignoring delete request" << endl;
			}
		} else {
			kdError() << "cannot delete DocumentBaseNode if it has children" << endl;
		}
	} else {
		kdError() << "Cannot delete node, cannot find clip" << endl;
	}
}

DocumentBaseNode *KdenliveDoc::findClipNode(const QString &name)
{
	return m_clipHierarch->findClipNode(name);
}

void KdenliveDoc::AVFilePropertiesError(const QString &path, const QString &errmsg)
{
	DocClipBase *file = m_clipManager.findClip(KURL(path));

	KdenliveApp *win=(KdenliveApp *) parent();
	KMessageBox::sorry(win, errmsg, path);

	deleteClipNode(file->name());
}

KMacroCommand *KdenliveDoc::createCleanProjectCommand()
{
	KMacroCommand *macroCommand = new KMacroCommand( i18n("Clean Project") );

	KCommand *command = m_clipHierarch->createCleanChildrenCommand(*this);
	macroCommand->addCommand(command);

	return macroCommand;
}

void KdenliveDoc::addClipNode(const QString &parent, DocumentBaseNode *newNode)
{
	DocumentBaseNode *node = findClipNode(parent);
	if(node) {
		node->addChild(newNode);
		emit clipListUpdated();
	} else {
		kdWarning() << "Could not add document node to KdenliveDoc : parent clip does not exist!" << endl;
		kdWarning() << "causes memory leak!!!" << endl;
	}
}

void KdenliveDoc::clipChanged(DocClipBase *file)
{
	DocumentBaseNode *node = findClipNode(file->name());
	if(node) {
		DocumentClipNode *clipNode = node->asClipNode();
		if(clipNode) {
			clipNode->clipRef()-> setTrackEnd(clipNode->clipRef()->duration());
			emit clipChanged(clipNode->clipRef());
		}
	} else {
		kdWarning() << "Got a request for a changed clip that is not in the document" << endl;
	}
}
