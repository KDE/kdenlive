/***************************************************************************
                          playlist  -  description
                             -------------------
    begin                : Fri Jul 27 2007
    copyright            : (C) 2007 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klocale.h>
#include <klineedit.h>
#include <kdebug.h>
#include <kurlrequesterdlg.h>
#include <kurlrequester.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <kcommand.h>

#include <qlayout.h>
#include <qlistview.h>
#include <qiconview.h>
#include <qlabel.h>

#include "playlist.h"
#include "avlistviewitem.h"
#include "kselectclipcommand.h"
#include "kaddrefclipcommand.h"
#include "aviconviewitem.h"
#include "kdenlivedoc.h"
#include "docclipref.h"
#include "docclipavfile.h"
#include "loadprojectnativefilter.h"



namespace Gui {

    PlayList::PlayList(QListView *m_listView, QIconView *m_iconView, bool iconView, QWidget * parent, const char *name): KDialogBase(  KDialogBase::Swallow, i18n("Fix Broken Playlists"), KDialogBase::User1 | KDialogBase::User2 | KDialogBase::Close , KDialogBase::Close, parent, name, true) {
	QWidget *page = new QWidget( this );
   	setMainWidget(page);
	setButtonText(KDialogBase::User1, i18n("Fix All"));
	setButtonText(KDialogBase::User2, i18n("Fix Selected"));
	QGridLayout *grid = new QGridLayout(page, 2, 2);
	grid->setSpacing(5);
	QLabel *lab = new QLabel(i18n("Following broken url were found: "), page);	
	grid->addWidget(lab, 0, 0);
	m_broken_list = new KListView(page);
	m_broken_list->addColumn(i18n("Broken Playlists"));
	m_broken_list->setResizeMode(QListView::LastColumn);
	m_broken_list->setRootIsDecorated(true);
	grid->addWidget(m_broken_list, 1, 0);
	setMinimumWidth(400);

	if (!iconView) {
        QListViewItemIterator it( m_listView );
        while ( it.current() ) {
            const AVListViewItem *avitem = static_cast < AVListViewItem * >(*it);
            if (avitem && avitem->clip() && avitem->clip()->clipType() == DocClipBase::PLAYLIST) {
		QStringList urls = avitem->clip()->referencedClip()->toDocClipAVFile()->brokenPlayList();
		if (!urls.isEmpty()) {
		    KListViewItem *item = new KListViewItem(m_broken_list, avitem->clip()->fileURL().path());
    		    for ( QStringList::Iterator it = urls.begin(); it != urls.end(); ++it )
        	        (void) new KListViewItem(item, (*it));
		}
            }
            ++it;
        }
	}
	else {
	    for ( QIconViewItem *item = m_iconView->firstItem(); item; item = item->nextItem() )
	    {
            	const AVIconViewItem *avitem = static_cast < AVIconViewItem * >(item);
                if (avitem && avitem->clip() && avitem->clip()->clipType() == DocClipBase::PLAYLIST) {
		    QStringList urls = avitem->clip()->referencedClip()->toDocClipAVFile()->brokenPlayList();
		    if (!urls.isEmpty()) {
		        KListViewItem *item = new KListViewItem(m_broken_list, avitem->clip()->fileURL().path());
    		        for ( QStringList::Iterator it = urls.begin(); it != urls.end(); ++it )
        	            (void) new KListViewItem(item, (*it));
		    }
            	}
            }
	}

	connect(this, SIGNAL(user1Clicked ()), this, SLOT(slotFixAllLists()));
	connect(this, SIGNAL(user2Clicked ()), this, SLOT(slotFixLists()));

	adjustSize();
    }

    PlayList::~PlayList() {
	delete m_broken_list;
    }

    void PlayList::slotFixAllLists() {
	if (!m_broken_list->currentItem()) return;
	replaceDialog = new FixPlayList_UI(this);
	replaceDialog->old_path->setText(KURL(m_broken_list->firstChild()->firstChild()->text(0)).directory());
	replaceDialog->new_path->fileDialog()->setMode(KFile::Directory);
	replaceDialog->buttonOk->setEnabled(false);
	replaceDialog->setMinimumWidth(400);
	replaceDialog->adjustSize();
	connect(replaceDialog->preview_button, SIGNAL(clicked ()), this, SLOT(slotPreviewFix()));
	if (replaceDialog->exec() == QDialog::Accepted) {
		if (KMessageBox::questionYesNo(this, i18n("<qt>This will definitely modify all broken playlists.\nAre you sure you want to do this ?")) == KMessageBox::Yes)
		slotDoFix( replaceDialog->old_path->text(), replaceDialog->new_path->url());
	}
	delete replaceDialog;
    }

    void PlayList::slotPreviewFix() {
	    if (!m_broken_list->currentItem()) return;
	    replaceDialog->preview_list->clear();
	    QString oldPath = replaceDialog->old_path->text();
	    QString newPath = replaceDialog->new_path->url();
	    QListViewItem *item = m_broken_list->firstChild();
	    while (item) {
		// Parse all broken playlist and simulate new path
		QDomDocument doc;
		QFile file(item->text(0));
		doc.setContent(&file, false);
		file.close();
		QDomElement documentElement = doc.documentElement();
		if (documentElement.tagName() != "westley") {
		    kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
		}

    		QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
		QDomNode n;
		if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
		else n = documentElement.firstChild();
		QDomElement e;
		while (!n.isNull()) {
	    	    e = n.toElement();
	    	    if (!e.isNull() && e.tagName() == "producer") {
		    	QString prodUrl = e.attribute("resource", QString::null);
		    	if (!prodUrl.isEmpty()) {
			    (void) new KListViewItem(replaceDialog->preview_list, prodUrl, prodUrl.replace(oldPath, newPath));
			    //e.setAttribute("resource", newPath);
		    	}
	    	    }
	    	    n = n.nextSibling();
		}
		item = item->nextSibling();
	    }
	    replaceDialog->buttonOk->setEnabled(true);
    }

    void PlayList::slotFixLists() {
	    if (!m_broken_list->currentItem()) return;
	    QString brokenPath;
	    QString fileUrl;
	    if (m_broken_list->currentItem()->childCount() == 0) {
		fileUrl = m_broken_list->currentItem()->parent()->text(0);
	        brokenPath = m_broken_list->currentItem()->text(0);
	    }
	    else {
		fileUrl = m_broken_list->currentItem()->text(0);
		brokenPath = m_broken_list->currentItem()->firstChild()->text(0);
	    }
	    KURLRequesterDlg *getUrl = new KURLRequesterDlg(KURL(brokenPath).directory(), i18n("Please give new path for\n") + brokenPath, this, "new_path");
	    getUrl->fileDialog()->setMode(KFile::Directory);
	    getUrl->exec();
	    KURL newUrl = getUrl->selectedURL ();
	    delete getUrl;
	    QString oldPath = KURL(brokenPath).directory();
	    QString newPath = newUrl.path();
	    if (!newUrl.isEmpty() && KMessageBox::questionYesNo(this, i18n("<qt>This will modify playlist:<br> <b>%1</b><br><br>Replacing %2 with<br>%3</qt>").arg(fileUrl).arg(oldPath).arg(newPath)) == KMessageBox::Yes) {
		fixPlayListClip(fileUrl, brokenPath, newPath + "/" + KURL(brokenPath).fileName());
		emit signalFixed();
	    }

    }

    void PlayList::fixPlayListClip(QString fileUrl, QString oldPath, QString newPath, bool partialReplace) {
	QDomDocument doc;
	QFile file(fileUrl);
	doc.setContent(&file, false);
	file.close();
	QDomElement documentElement = doc.documentElement();
	if (documentElement.tagName() != "westley") {
	kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
	}

    	QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
	QDomNode n;
	if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	else n = documentElement.firstChild();
	QDomElement e;
	bool fixed = false;

	while (!n.isNull()) {
	    e = n.toElement();
	    if (!e.isNull() && e.tagName() == "producer") {
		    QString prodUrl = e.attribute("resource", QString::null);
		    if (!partialReplace) {
			if (prodUrl == oldPath) {
			    e.setAttribute("resource", newPath);
			    fixed = true;
		        }
		    }
		    else {
			e.setAttribute("resource", prodUrl.replace(oldPath, newPath));
		    }
	    }
	    n = n.nextSibling();
	}
	if (fixed || partialReplace) {
    	    QCString save = doc.toString().utf8();
	    if ( file.open( IO_WriteOnly ) ) {
    	        file.writeBlock(save, save.length());
		file.close();
		if (!partialReplace) {
			QListViewItem *fixed = m_broken_list->findItem(oldPath, 0);
			if (fixed->parent()->childCount() == 1) m_broken_list->takeItem(fixed->parent());
			else m_broken_list->takeItem(fixed);
		}
	    }
	    else KMessageBox::sorry(this, i18n("Unable to write to the file:\n%1").arg(fileUrl));
	}
    }


    void PlayList::slotDoFix(QString oldPath, QString newPath) {
	QListViewItem *item = m_broken_list->firstChild();
	while (item) {
	    // Parse all broken playlist and fix paths
	    fixPlayListClip(item->text(0), oldPath, newPath, true);
	    item = item->nextSibling();
	}
	emit signalFixed();
    }

    // static
    void PlayList::saveDescription(QString path, QString description) {
	QDomDocument doc;
	QFile file(path);
	doc.setContent(&file, false);
	file.close();
	QDomElement documentElement = doc.documentElement();
	if (documentElement.tagName() != "westley") {
	    kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
	    return;
	}
	documentElement.setAttribute("title", description);
	
	QCString save = doc.toString().utf8();
	if ( file.open( IO_WriteOnly ) ) {
    	        file.writeBlock(save, save.length());
		file.close();
	}
    }

    // static
    void PlayList::explodePlaylist(DocClipRef *clip, KdenliveApp *app) {
	QDomDocument doc;
	QFile file(clip->fileURL().path());
	doc.setContent(&file, false);
	file.close();
	int track = clip->trackNum();
	GenTime startTime = clip->trackStart();
	QDomElement documentElement = doc.documentElement();
	if (documentElement.tagName() != "westley") {
	    kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
	    return;
	}

	KURL::List producersList;
	int tracksCount = 0;

	QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
	QDomNode n, node;
	QDomElement e, entry;

	QString documentCopy = doc.toString();

	if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	else n = documentElement.firstChild();

	while (!n.isNull()) {
	    e = n.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "producer") {
		    QString producerId = e.attribute("id", QString::number(-1));
		    int newId  = app->getDocument()->clipManager().requestIdNumber();
		    e.setAttribute("id", QString::number(newId));
		    documentCopy.replace(" producer=\"" + producerId + "\"", " producer=\"" + QString::number(newId) + "\"", false);
		    documentCopy.replace(" producer=" + producerId + " ", " producer=" + QString::number(newId) + " ", false);
		    LoadProjectNativeFilter::addToDocument(i18n("Clips"), e, app->getDocument());
		}
	    }
	    n = n.nextSibling();
	}
	if (!kdenlivedoc.isNull() || tracksCount == 0) {
	    // it is a kdenlive document, parse for tracks
	    kdenlivedoc = documentElement.elementsByTagName("multitrack").item(0);
	    if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	    while (!n.isNull()) {
	    	e = n.toElement();
	    	if (!e.isNull()) {
		    if (e.tagName() == "playlist") tracksCount++;
		}
		n = n.nextSibling();
	    }
	}
	app->slotDeleteSelected();

	// use the document copy with updated ids
	doc.setContent(documentCopy, false);
	documentElement = doc.documentElement();
	kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);

	if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	else n = documentElement.firstChild();

	while (!n.isNull()) {
	    e = n.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "playlist") {
		    tracksCount++;
		    node = n.firstChild();
		    while (!node.isNull()) {
			entry = node.toElement();
			if (entry.tagName() == "entry") {
	    		    QString id = entry.attribute("producer", QString::null);
	    		    DocClipBase *baseClip = app->getDocument()->clipManager().findClipById(entry.attribute("producer", QString::number(-1)).toInt());
	    		    if (baseClip) {
				GenTime insertTime = GenTime(entry.attribute("in", QString::number(0)).toInt(), KdenliveSettings::defaultfps());
				GenTime endTime = GenTime(entry.attribute("out", QString::number(0)).toInt(), KdenliveSettings::defaultfps()) - insertTime;
				DocClipRef *clip = new DocClipRef(baseClip);
				DocClipRefList selectedClip;
				selectedClip.append(clip);
				if (app->getDocument()->projectClip().canAddClipsToTracks(selectedClip, track, startTime)) {
				    kdDebug()<<"/ / /ADDING CLIP TO TRACK: "<<track<<endl;
		    		    //clip->setParentTrack(app->getDocument()->track(track), track);
		    		    clip->moveTrackStart(startTime);
		    		    app->getDocument()->track(track)->addClip(clip, false);
		    		    clip->setCropStartTime(insertTime);
		    		    clip->setCropDuration(endTime);
				    clip->setParentTrack(app->getDocument()->track(track), track);
				    baseClip->addReference();
		    		    startTime+=endTime;
				    KMacroCommand *macroCommand = new KMacroCommand(i18n("Paste"));
				    macroCommand->addCommand(Command::KSelectClipCommand::selectNone(app->getDocument()));
				    macroCommand->addCommand(new Command::KSelectClipCommand(app->getDocument(), clip, true));
	  			    app->addCommand(macroCommand, true);

				    app->addCommand(new Command::KAddRefClipCommand(app->effectList(), *(app->getDocument()), clip, true), false);

				}

			    }
			}
			node = node.nextSibling();
		    }
		}
	    }
	    n = n.nextSibling();
	}
	app->getDocument()->track(track)->checkTrackLength();
	app->getDocument()->generateProducersList();
	app->getDocument()->activateSceneListGeneration(true);

    }

}
