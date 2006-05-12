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
#include <kdebug.h>
#include <kcommand.h>

// application specific includes
#include "krendermanager.h"
#include "kdenlivedoc.h"
#include "kdenlive.h"

#include "docclipavfile.h"
#include "docclipproject.h"
#include "doctrackvideo.h"
#include "doctracksound.h"
#include "doctrackclipiterator.h"

#include "documentclipnode.h"
#include "documentgroupnode.h"
#include "documentbasenode.h"

KdenliveDoc::KdenliveDoc(double fps, int width, int height, Gui::KdenliveApp * app, QWidget * parent, const char *name):
QObject(parent, name),
m_projectClip(new DocClipProject(fps, width, height)),
m_modified(false),
m_sceneListGeneration(true),
m_clipHierarch(0), m_render(app->renderManager()->findRenderer("Document")), m_clipManager(m_render), m_app(app) //, m_clipManager(*app->renderManager())
{
    //m_render = m_app->renderManager()->createRenderer("Document");
    //m_clipManager = new ClipManager(m_render)

    connect(this, SIGNAL(trackListChanged()), this, SLOT(hasBeenModified()));

    connect(&m_clipManager, SIGNAL(clipListUpdated()), this, SLOT(generateProducersList()));
    connect(&m_clipManager, SIGNAL(clipChanged(DocClipBase *)), this, SLOT(clipChanged(DocClipBase *)));
    connect(&m_clipManager, SIGNAL(updateClipThumbnails(DocClipBase *)), this, SLOT(slotUpdateClipThumbnails(DocClipBase *)));
    connect(&m_clipManager, SIGNAL(fixClipDuration(DocClipBase *)), this, SLOT(fixClipDuration(DocClipBase *)));

    m_domSceneList.appendChild(m_domSceneList.createElement("scenelist"));
    generateSceneList();
    connectProjectClip();

    setModified(false);
}

KdenliveDoc::~KdenliveDoc()
{
    if (m_projectClip)
	delete m_projectClip;
}

void KdenliveDoc::setURL(const KURL & url)
{
    m_doc_url = url;
}

const KURL & KdenliveDoc::URL() const
{
    return m_doc_url;
}

void KdenliveDoc::closeDocument()
{
    kdDebug() << "KdenliveDoc in closeDocument()" << endl;
    deleteContents();
    m_clipHierarch = new DocumentGroupNode(0, i18n("Clips"));
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
    addSoundTrack();
    addSoundTrack();

    m_doc_url.setFileName(i18n("Untitled"));

    emit trackListChanged();
    setModified(false);

    return true;
}

void KdenliveDoc::deleteContents()
{
    kdDebug() << "deleting contents..." << endl;

    delete m_projectClip;

    m_projectClip = new DocClipProject(KdenliveSettings::defaultfps(), KdenliveSettings::defaultwidth(), KdenliveSettings::defaultheight());
    connectProjectClip();

    if (m_clipHierarch) {
	delete m_clipHierarch;
	m_clipHierarch = 0;
    }

    emit trackListChanged();

    m_clipManager.clear();
    emit clipListUpdated();
}

/** Returns the number of frames per second. */
double KdenliveDoc::framesPerSecond() const
{
    if (m_projectClip) {
	return m_projectClip->framesPerSecond();
    } else {
	kdWarning() <<
	    "KdenliveDoc cannot calculate frames per second - m_projectClip is null!!! Perhaps m_projectClip is in the process of being created? Temporarily returning 25 as a placeholder - needs fixing."
	    << endl;
	return 25;
    }
}

/** Adds an empty video track to the project */
void KdenliveDoc::addVideoTrack()
{
    m_projectClip->addTrack(new DocTrackVideo(m_projectClip));
}

/** Adds a sound track to the project */
void KdenliveDoc::addSoundTrack()
{
    m_projectClip->addTrack(new DocTrackSound(m_projectClip));
}

/** Returns the number of tracks in this project */
uint KdenliveDoc::numTracks() const
{
    return m_projectClip->numTracks();
}

/** returns the Track which holds the given clip. If the clip does not
exist within the document, returns 0; */
DocTrackBase *KdenliveDoc::findTrack(DocClipRef * clip) const
{
    return m_projectClip->findTrack(clip);
}

/** Returns the track with the given index, or returns NULL if it does
not exist. */
DocTrackBase *KdenliveDoc::track(int track) const
{
    return m_projectClip->track(track);
}

/** Returns the index value for this track, or -1 on failure.*/
int KdenliveDoc::trackIndex(DocTrackBase * track) const
{
    if (m_projectClip) {
	return m_projectClip->trackIndex(track);
    } else {
	kdError() << "Cannot return track index - m_projectClip is Null!"
	    << endl;
    }
    return -1;
}

/** Sets the modified state of the document, if this has changed, emits modified(state) */
void KdenliveDoc::setModified(bool state)
{
    if (m_modified != state) {
	m_modified = state;
	emit modified(state);
    }
}

/** Moves the currectly selected clips by the offsets specified, or returns false if this
is not possible. */
bool KdenliveDoc::moveSelectedClips(GenTime startOffset, int trackOffset)
{
    bool result =
	m_projectClip->moveSelectedClips(startOffset, trackOffset);

    if (result)
	hasBeenModified();
    return result;
}

/** Returns a scene list generated from the current document. */

QDomDocument KdenliveDoc::generateSceneList()
{
    if (m_projectClip) {
        m_domSceneList = m_projectClip->generateSceneList();
    } else {
	kdWarning() <<
	    "Cannot generate scene list - m_projectClip is null!" << endl;
    }
    return m_domSceneList;
}

/** Creates a list of producers */
void KdenliveDoc::generateProducersList()
{
    m_projectClip->producersList = m_clipManager.producersList();
}


/** Called when the document is modifed in some way. */
void KdenliveDoc::hasBeenModified()
{
    if (m_sceneListGeneration) {
        if (m_projectClip->producersList.isNull()) generateProducersList();
	emit documentChanged(m_projectClip);
    }
    setModified(true);
}


/** Renders the current document timeline to the specified url. */
void KdenliveDoc::renderDocument(const KURL & url)
{
    m_render->setSceneList(m_domSceneList);
    m_render->render(url);
}

/** Returns renderer associated with this document. */
KRender *KdenliveDoc::renderer() const
{
    return m_render;
}

void KdenliveDoc::connectProjectClip()
{
    connect(m_projectClip, SIGNAL(trackListChanged()), this,
	SIGNAL(trackListChanged()));
    connect(m_projectClip, SIGNAL(signalClipSelected(DocClipRef *)), this,
	SIGNAL(signalClipSelected(DocClipRef *)));
    connect(m_projectClip, SIGNAL(signalOpenClip(DocClipRef *)), this,
	SIGNAL(signalOpenClip(DocClipRef *)));
    connect(m_projectClip, SIGNAL(clipChanged(DocClipRef *)), this,
	SIGNAL(clipChanged(DocClipRef *)));
    connect(m_projectClip, SIGNAL(effectStackChanged(DocClipRef *)), this,
	SIGNAL(effectStackChanged(DocClipRef *)));
    connect(m_projectClip, SIGNAL(projectLengthChanged(const GenTime &)),
	this, SIGNAL(documentLengthChanged(const GenTime &)));
    connect(m_projectClip, SIGNAL(documentChanged(DocClipBase *)),
            this, SIGNAL(documentChanged(DocClipBase *)));
    
    connect(m_projectClip, SIGNAL(deletedClipTransition()),
            this, SLOT(slotDeleteClipTransition()));

// Commented out following line, causes multiple unnecessary refreshes - jbm, 26/12/05 
    connect(m_projectClip, SIGNAL(clipLayoutChanged()), this, SLOT(hasBeenModified()));
    connect(m_projectClip, SIGNAL(effectStackChanged(DocClipRef *)), this,
	SLOT(hasBeenModified()));
}

const GenTime & KdenliveDoc::projectDuration() const
{
    return m_projectClip->duration();
}

void KdenliveDoc::indirectlyModified()
{
    hasBeenModified();
}

bool KdenliveDoc::hasSelectedClips() const
{
    bool result = false;

    if (m_projectClip) {
	result = m_projectClip->hasSelectedClips();
    } else {
	kdError() <<
	    "No selection in the project because m_projectClip is null!!"
	    << endl;
    }
    return result;
}

DocClipRef *KdenliveDoc::selectedClip() const
{
    DocClipRef *pResult = 0;

    if (m_projectClip) {
	pResult = m_projectClip->selectedClip();
    } else {
	kdError() <<
	    "No selection in the project because m_projectClip is null!!!"
	    << endl;
    }

    return pResult;
}

void KdenliveDoc::activateSceneListGeneration(bool active)
{
    m_sceneListGeneration = active;
    if (active) {
	hasBeenModified();
    }
}

DocClipRefList KdenliveDoc::referencedClips(DocClipBase * clip) const
{
    return m_projectClip->referencedClips(clip);
}

void KdenliveDoc::deleteClipNode(const QString & name)
{
    DocumentBaseNode *node = findClipNode(name);

    if (node) {
	if (!node->hasChildren()) {
	    if ((node->asClipNode() == NULL)
		|| (!m_projectClip->referencesClip(node->asClipNode()->
			clipRef()->referencedClip()))) {
		node->parent()->removeChild(node);
		emit nodeDeleted(node);
		delete node;
	    } else {
		kdError() <<
		    "Trying to delete clip that has references in the document - "
		    <<
		    "must delete references first. Silently ignoring delete request"
		    << endl;
	    }
	} else {
	    kdError() <<
		"cannot delete DocumentBaseNode if it has children" <<
		endl;
	}
    } else {
	kdError() << "Cannot delete node, cannot find clip" << endl;
    }
}

DocumentBaseNode *KdenliveDoc::findClipNode(const QString & name) const
{
    return m_clipHierarch->findClipNode(name);
}

void KdenliveDoc::AVFilePropertiesError(const QString & path,
    const QString & errmsg)
{
    DocClipBase *file = m_clipManager.findClip(KURL(path));

    Gui::KdenliveApp * win = (Gui::KdenliveApp *) parent();
    KMessageBox::sorry(win, errmsg, path);

    deleteClipNode(file->name());
}

void KdenliveDoc::addClipNode(const QString & parent,
    DocumentBaseNode * newNode)
{
    kdDebug() << "*** DOCUMENT adding clip: " << newNode->name() << endl;
    DocumentBaseNode *node = findClipNode(parent);
    if (node) {
	node->addChild(newNode);
	emit clipListUpdated();
    } else {
	kdWarning() <<
	    "Could not add document node to KdenliveDoc : parent clip does not exist!"
	    << endl;
	kdWarning() << "causes memory leak!!!" << endl;
    }
}

void KdenliveDoc::fixClipDuration(DocClipBase * file)
{
    m_projectClip->fixClipDuration(file->fileURL(), file->duration());
}


void KdenliveDoc::clipChanged(DocClipBase * file)
{
    DocumentBaseNode *node = findClipNode(file->name());
    if (node) {
	DocumentClipNode *clipNode = node->asClipNode();
	if (clipNode) {
	    clipNode->clipRef()->setTrackEnd(clipNode->clipRef()->
		duration());
	    emit clipChanged(clipNode->clipRef());
	}
    } else {
	kdWarning() <<
	    "Got a request for a changed clip that is not in the document : "
	    << file->name() << endl;
    }
}

void KdenliveDoc::setProjectClip(DocClipProject * projectClip)
{
    if (m_projectClip) {
	delete m_projectClip;
    }
    m_projectClip = projectClip;
    connectProjectClip();
    updateReferences();
    emit trackListChanged();
    emit documentLengthChanged(projectDuration());
}

void KdenliveDoc::slotUpdateClipThumbnails(DocClipBase *clip)
{
    QPtrListIterator < DocTrackBase > trackItt(trackList());
    while (trackItt.current()) {
        QPtrListIterator < DocClipRef > clipItt(trackItt.current()->firstClip(true));
        while (clipItt.current()) {
            if ((*clipItt)->referencedClip() == clip)
                (*clipItt)->generateThumbnails();
            ++clipItt;
            
        }
        QPtrListIterator < DocClipRef > clipItt2(trackItt.current()->firstClip(false));
        while (clipItt2.current()) {
            if ((*clipItt2)->referencedClip() == clip)
                (*clipItt2)->generateThumbnails();
            ++clipItt2;
        }
        ++trackItt;
    }
    emit timelineClipUpdated();
}

void KdenliveDoc::updateReferences()
{
    QPtrListIterator < DocTrackBase > trackItt(trackList());
    while (trackItt.current()) {
        QPtrListIterator < DocClipRef > clipItt(trackItt.current()->firstClip(true));
        while (clipItt.current()) {
            (*clipItt)->referencedClip()->addReference();
            ++clipItt;
            
        }
        QPtrListIterator < DocClipRef > clipItt2(trackItt.current()->firstClip(false));
        while (clipItt2.current()) {
            (*clipItt2)->referencedClip()->addReference();
            ++clipItt2;
        }
        ++trackItt;
    }
}

QValueVector < GenTime > KdenliveDoc::getSnapTimes(bool includeClipEnds,
    bool includeSnapMarkers,
    bool includeUnselectedClips, bool includeSelectedClips)
{
    QValueVector < GenTime > list;

    for (uint count = 0; count < numTracks(); ++count) {
	if (includeUnselectedClips) {
	    QPtrListIterator < DocClipRef > clipItt =
		track(count)->firstClip(false);
	    while (clipItt.current()) {
		if (includeClipEnds) {
		    list.append(clipItt.current()->trackStart());
		    list.append(clipItt.current()->trackEnd());
		}

		if (includeSnapMarkers) {
		    QValueVector < GenTime > markers =
			clipItt.current()->snapMarkersOnTrack();
		    for (uint count = 0; count < markers.count(); ++count) {
			list.append(markers[count]);
		    }
		}

		++clipItt;
	    }
	}

	if (includeSelectedClips) {
	    QPtrListIterator < DocClipRef > clipItt =
		track(count)->firstClip(true);
	    while (clipItt.current()) {
		if (includeClipEnds) {
		    list.append(clipItt.current()->trackStart());
		    list.append(clipItt.current()->trackEnd());
		}

		if (includeSnapMarkers) {
		    QValueVector < GenTime > markers =
			clipItt.current()->snapMarkersOnTrack();
		    for (uint count = 0; count < markers.count(); ++count) {
			list.append(markers[count]);
		    }
		}

		++clipItt;
	    }
	}
    }

    return list;
}

void KdenliveDoc::updateTracksThumbnails()
{
    QPtrListIterator < DocTrackBase > trackItt(trackList());

    while (trackItt.current()) {
        QPtrListIterator < DocClipRef > clipItt(trackItt.current()->firstClip(true));
        while (clipItt.current()) {
            (*clipItt)->generateThumbnails();
            ++clipItt;
        }
        
        QPtrListIterator < DocClipRef > clipItt2(trackItt.current()->firstClip(false));
        while (clipItt2.current()) {
            (*clipItt2)->generateThumbnails();
            ++clipItt2;
        }
        ++trackItt;
    }
}

DocClipRefList KdenliveDoc::listSelected() const
{
    DocClipRefList list;

    QPtrListIterator < DocTrackBase > trackItt(trackList());

    while (trackItt.current()) {
	QPtrListIterator < DocClipRef >
	    clipItt(trackItt.current()->firstClip(true));

	while (clipItt.current()) {
	    list.inSort(clipItt.current());
	    ++clipItt;
	}

	++trackItt;
    }

    return list;
}

const EffectDescriptionList & KdenliveDoc::effectDescriptions() const
{
    return m_render->effectList();
}

Effect *KdenliveDoc::createEffect(const QDomElement & element) const
{
    Effect *effect = 0;

    if (element.tagName() != "effect") {
	kdWarning() <<
	    "KdenliveDoc::createEffect() element is not an effect, trying to parse anyway..."
	    << endl;
    }

    EffectDesc *desc = effectDescription(element.attribute("type"));
    if (desc) {
	effect = Effect::createEffect(*desc, element);
    } else {
	kdWarning() <<
	    "KdenliveDoc::createEffect() cannot find effect description "
	    << element.attribute("effect") << endl;
    }

    return effect;
}

EffectDesc *KdenliveDoc::effectDescription(const QString & type) const
{
    return effectDescriptions().effectDescription(type);
}

void KdenliveDoc::slotDeleteClipTransition()
{
    application()->transitionPanel()->setTransition(0);
    emit documentChanged(m_projectClip);
}


const DocTrackBaseList & KdenliveDoc::trackList() const
{
    return m_projectClip->trackList();
}
