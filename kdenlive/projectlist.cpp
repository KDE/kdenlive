/***************************************************************************
                          projectlist.cpp  -  description
                             -------------------
    begin                : Sat Feb 16 2002
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

#include "projectlist.h"
#include "avlistviewitem.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "docclipavfile.h"
#include "timecode.h"

/* This define really should go in projectlist_ui, but qt just puts the classname there at the moment :-( */
#include <qpushbutton.h>

#include <qcursor.h>
#include <qlabel.h>

#include <kdebug.h>
#include <klineedit.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <documentbasenode.h>


#include <iostream>
#include <string>
#include <map>

namespace Gui {

    ProjectList::ProjectList(KdenliveApp * app, KdenliveDoc * document,
	QWidget * parent, const char *name):ProjectList_UI(parent, name),
	m_document(document), m_app(app) {
	if (!document) {
	    kdError() <<
		"ProjectList created with no document - expect a crash shortly"
		<< endl;
	}
	QBoxLayout * l = new QHBoxLayout( lv_frame );
        l->setAutoAdd( TRUE );
	lv_search = new KListViewSearchLineWidget(m_listView, lv_frame, "search_line");
	m_listView->setDocument(document);

	QPopupMenu *menu = contextcreateMenu();
	button_add->setPopup(menu);

	/* clip shortcut buttons */
	KIconLoader loader;
	button_delete->setIconSet(QIconSet(loader.loadIcon("editdelete",
		    KIcon::Toolbar)));
	button_add->setIconSet(QIconSet(loader.loadIcon("filenew",
		    KIcon::Toolbar)));
	button_edit->setIconSet(QIconSet(loader.loadIcon("edit",
		    KIcon::Toolbar)));

	QToolTip::add(button_edit, i18n("Edit Clip"));
	QToolTip::add(button_add, i18n("Add Clip"));
	QToolTip::add(button_delete, i18n("Delete Clip"));

	connect(button_delete, SIGNAL(clicked()), app,
	    SLOT(slotProjectDeleteClips()));
	connect(button_edit, SIGNAL(clicked()), app,
	    SLOT(slotProjectEditClip()));

	//add header tooltips -reh
	colToolTip = new columnToolTip(m_listView->header());

	connect(m_listView, SIGNAL(dragDropOccured(QDropEvent *, QListViewItem *)), this,
	    SIGNAL(dragDropOccured(QDropEvent *, QListViewItem * )));

	connect(m_listView, SIGNAL(rightButtonPressed(QListViewItem *,
		    const QPoint &, int)), this,
	    SLOT(rightButtonPressed(QListViewItem *, const QPoint &,
		    int)));
        
        connect(m_listView, SIGNAL(doubleClicked( QListViewItem *, const QPoint &, int )), this, SLOT(editRequested( QListViewItem *, const QPoint &, int )));

	//connect(m_listView, SIGNAL(executed(QListViewItem *)), this, SLOT(projectListSelectionChanged(QListViewItem *)));

	connect(m_listView, SIGNAL(selectionChanged()), this,
	    SLOT(updateListItem()));

	//connect(m_listView, SIGNAL(dragStarted(QListViewItem *)), this, SLOT(projectListSelectionChanged(QListViewItem *)));
    }

    ProjectList::~ProjectList() {
    }
    
    /** An item was double clicked */
    void ProjectList::editRequested( QListViewItem *, const QPoint &, int col)
    {
        // only react if the click is not in column 2 (because col 2 is the editable description
        if (col!=2) emit editItem();
    }

/** No descriptions */
    void ProjectList::rightButtonPressed(QListViewItem * listViewItem,
	const QPoint & pos, int column) {
	QPopupMenu *menu = contextMenu();
	if (menu) {
	    menu->popup(QCursor::pos());
	}
    }

/** Get a fresh copy of files from KdenliveDoc and display them. */
    void ProjectList::slot_UpdateList() {
	m_listView->clear();
	DocumentBaseNode *node = m_document->clipHierarch();
	if (node) {
		QPtrListIterator < DocumentBaseNode > child(node->children());
    		while (child.current()) {
			if (child.current())
				new AVListViewItem(m_document, m_listView, child.current());
			++child;
    		}
	}

	/*if (node) {
	    AVListViewItem *item =
		new AVListViewItem(m_document, m_listView, node);
	    item->setOpen(true);
	}*/
    }

/** The clip specified has changed - update the display.
 */
    void ProjectList::slot_clipChanged(DocClipRef * ) {
        slot_clipChanged();
    }
        
    void ProjectList::slot_clipChanged() {
	slot_UpdateList();
	m_listView->triggerUpdate();
    }

    void ProjectList::slot_nodeDeleted(DocumentBaseNode * node) {
	slot_UpdateList();
	m_listView->triggerUpdate();
    }

/** Called when the project list changes. */
//void ProjectList::projectListSelectionChanged(QListViewItem *item)
    void ProjectList::updateListItem() {
	const AVListViewItem *avitem = (AVListViewItem *) m_listView->currentItem();
	if (!avitem) return;
	if (avitem->clip()) emit clipSelected(avitem->clip());
/*	    // display duration
	    Timecode timecode;

	    text_duration->setText(timecode.getTimecode(avitem->clip()->
                    duration(), KdenliveSettings::defaultfps()));


	    // display file size
	    QString text;
	    long fileSize = avitem->clip()->fileSize();
	    long tenth;
	    if (fileSize < 1024) {
		text = QString::number(fileSize) + i18n(" byte(s)");
	    } else {
		fileSize = (int) floor((fileSize / 1024.0) + 0.5);

		if (fileSize < 1024) {
		    text = QString::number(fileSize) + i18n(" Kb");
		} 
                else {
		    fileSize = (int) floor((fileSize / 102.4) + 0.5);
                    if (fileSize < 10000) {
		      tenth = fileSize % 10;
		      fileSize /= 10;
		      text = QString::number(fileSize) + "." + QString::number(tenth) + i18n(" Mb");
                    }
                    else {
                        fileSize = (int) floor((fileSize / 1000) + 0.5);
                        tenth = fileSize % 10;
                        fileSize /= 10;
                        text = QString::number(fileSize) + "." + QString::number(tenth) + i18n(" Gb");
                        
                    }
		}
	    }
	    text_size->setText(text);
	    
	    // display usage
	    text_usage->setText(QString::number(avitem->clip()->
		    numReferences()));

	    // display clip type
	    if (avitem->clip()->clipType() == DocClipBase::AV)
		text = i18n("video");
	    else if (avitem->clip()->clipType() == DocClipBase::VIDEO)
		text = i18n("mute video");
	    else if (avitem->clip()->clipType() == DocClipBase::AUDIO)
		text = i18n("audio");
	    else if (avitem->clip()->clipType() == DocClipBase::COLOR)
		text = i18n("color clip");
	    else if (avitem->clip()->clipType() == DocClipBase::IMAGE)
		text = i18n("image clip");
            else if (avitem->clip()->clipType() == DocClipBase::TEXT)
                text = i18n("text clip");
	    text_type->setText(text);

	} else {
	    // no clip selected, diplay blank infos
	    text_type->setText(QString::null);
	    text_duration->setText(QString::null);
	    text_size->setText(QString::null);
	    text_usage->setText(QString::null);
	}*/
    }
    
    void ProjectList::updateReference() {
        const AVListViewItem *avitem =
                (AVListViewItem *) m_listView->currentItem();
        if (!avitem)
            return;
        if (avitem->clip()) {
//            text_usage->setText(QString::number(avitem->clip()->numReferences()));
        }
    }

    DocClipRef *ProjectList::currentSelection() {
	const AVListViewItem *avitem =
	    static_cast < AVListViewItem * >(m_listView->selectedItem());
	if (avitem) {
	    return avitem->clip();
	}
	return 0;
    }
    
    void ProjectList::selectClip(DocClipBase *clip) {
        
        QListViewItemIterator it( m_listView );
        while ( it.current() ) {
            const AVListViewItem *avitem = static_cast < AVListViewItem * >(*it);
            if (avitem && avitem->clip() && avitem->clip()->referencedClip() == clip) {
                m_listView->setSelected(*it, true);
                m_listView->ensureItemVisible(*it);
                break;
            }
            ++it;
        }
    }

    QPopupMenu *ProjectList::contextMenu() {
	QPopupMenu *menu =
	    (QPopupMenu *) m_app->factory()->
	    container("projectlist_context", m_app);

	return menu;
    }

    QPopupMenu *ProjectList::contextcreateMenu() {
	QPopupMenu *menu =
	    (QPopupMenu *) m_app->factory()->
	    container("projectlist_create", m_app);
	return menu;
    }

    columnToolTip::columnToolTip(QHeader * header, QToolTipGroup * group)
  :	QToolTip(header, group) {
    }

    columnToolTip::~columnToolTip() {
    }

    void columnToolTip::maybeTip(const QPoint & p) {
	QHeader *header = (QHeader *) parentWidget();
	int section = 0;
	if (header->orientation() == Horizontal)
	    section = header->sectionAt(p.x());
	else
	    section = header->sectionAt(p.y());

	QString tipString = header->label(section);
	tip(header->sectionRect(section), tipString, "");
    }

}				// namespace Gui
