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

    ProjectList::ProjectList(KdenliveApp * app, KdenliveDoc * document, bool iconView, QWidget * parent, const char *name):ProjectList_UI(parent, name),
	m_document(document), m_app(app), m_isIconView(iconView) {
	if (!document) {
	    kdError() <<
		"ProjectList created with no document - expect a crash shortly"
		<< endl;
	}
	QBoxLayout * viewLayout = new QHBoxLayout( view_frame );
        viewLayout->setAutoAdd( TRUE );
	QBoxLayout * searchLayout = new QHBoxLayout( lv_frame );
        searchLayout->setAutoAdd( TRUE );
	if (!m_isIconView) setupListView();
	else setupIconView();

	QPopupMenu *menu = contextcreateMenu();
	button_add->setPopup(menu);

	QPopupMenu *menu2 = (QPopupMenu *) m_app->factory()->container("projectlist_type", m_app);
	button_view->setPopup(menu2);

	/* clip shortcut buttons */
	KIconLoader loader;
	button_view->setIconSet(QIconSet(loader.loadIcon("view_choose",
		    KIcon::Toolbar)));
	button_delete->setIconSet(QIconSet(loader.loadIcon("editdelete",
		    KIcon::Toolbar)));
	button_add->setIconSet(QIconSet(loader.loadIcon("filenew",
		    KIcon::Toolbar)));
	button_edit->setIconSet(QIconSet(loader.loadIcon("edit",
		    KIcon::Toolbar)));

	QToolTip::add(button_view, i18n("Select View Type"));
	QToolTip::add(button_edit, i18n("Edit Clip Properties"));
	QToolTip::add(button_add, i18n("Add Clip"));
	QToolTip::add(button_delete, i18n("Delete Clip"));

	connect(button_delete, SIGNAL(clicked()), app,
	    SLOT(slotProjectDeleteClips()));
	connect(button_edit, SIGNAL(clicked()), app,
	    SLOT(slotProjectEditClip()));
    }

    ProjectList::~ProjectList() {
	if (!m_isIconView) m_listView->clear();
	else m_iconView->clear();
    }

    void ProjectList::addClipRequest()
    {
	m_app->slotProjectAddClips();
    }

    void ProjectList::setupListView() {
	m_listView = new ProjectListView(view_frame);
	lv_search = new ListViewTagSearchWidget(m_listView, lv_frame, "search_line");
	// Add our file name and description to the list
	QValueList<int> columns;
	columns.push_back(1);
	columns.push_back(2);
	lv_search->searchLine()->setSearchColumns(columns);

	m_listView->setDocument(m_document);
	m_listView->show();
	lv_search->show();
	//add header tooltips -reh
	colToolTip = new columnToolTip(m_listView->header());
	connect(m_listView, SIGNAL(dragDropOccured(QDropEvent *, QListViewItem *)), this,
	    SIGNAL(dragDropOccured(QDropEvent *, QListViewItem * )));

	connect(m_listView, SIGNAL(rightButtonPressed(QListViewItem *,
		    const QPoint &, int)), this,
	    SLOT(rightButtonPressed(QListViewItem *, const QPoint &,
		    int)));
        connect(m_listView, SIGNAL(doubleClicked( QListViewItem *, const QPoint &, int )), this, SLOT(editRequested( QListViewItem *, const QPoint &, int )));
	connect(m_listView, SIGNAL(executed(QListViewItem *)), this, SLOT(updateListItem()));
	connect(m_listView, SIGNAL(addClipRequest()), this, SLOT(addClipRequest()));
    }

    void ProjectList::setupIconView() {
	m_iconView = new ProjectIconView(view_frame);
	iv_search = new KIconViewSearchLine(lv_frame, m_iconView, "search_line");
	m_iconView->setDocument(m_document);
	m_iconView->show();
	iv_search->show();
	connect(m_iconView, SIGNAL(executed (QIconViewItem *)), this, SLOT(updateListItem()));
	connect(m_iconView, SIGNAL(rightButtonPressed(QIconViewItem *, const QPoint &)), this, SLOT(rightButtonPressed(QIconViewItem *, const QPoint &)));
	connect(m_iconView, SIGNAL(doubleClicked( QIconViewItem *)), this, SLOT(editRequested( QIconViewItem *)));
    }

    void ProjectList::setListView() {
	if (!m_isIconView) return;
	delete m_iconView;
	delete iv_search;
	setupListView();
	m_isIconView = false;
	slot_UpdateList();
    }

    void ProjectList::setIconView() {
	if (m_isIconView) return;
	delete m_listView;
	delete lv_search;
	setupIconView();
	m_isIconView = true;
	slot_UpdateList();
    }

    void ProjectList::focusView() {
	if (!m_isIconView) m_listView->setFocus();
	else m_iconView->setFocus();
    }

    bool ProjectList::isListView() {
	return !m_isIconView;
    }

    bool ProjectList::hasChildren() {
	// TODO: implement folder functionnality in icon view
	if (m_isIconView) return true;
	return m_listView->currentItem()->childCount() > 0;
    }

    QStringList ProjectList::currentItemChildrenIds() {
	// TODO: implement folder functionnality in icon view
	QStringList list;
	if (m_isIconView) return list;
	QString folderName = currentItemName();
	QListViewItem * myChild = m_listView->currentItem()->firstChild();
        while( myChild ) {
	    list.append(QString::number((static_cast<AVListViewItem*>(myChild))->clip()->referencedClip()->getId()));
	    myChild = myChild->nextSibling();
        }
	return list;
    }

    DocClipRef* ProjectList::currentClip() {
	if (!m_isIconView) {
	    if (!m_listView->currentItem()) return NULL;
	    return (static_cast<AVListViewItem*>(m_listView->currentItem()))->clip();
	}
	else return m_iconView->selectedItem();
    }

    QString ProjectList::parentName() {
	if (!m_isIconView) return m_listView->parentName();
	else return m_iconView->parentName();
    }

    QString ProjectList::currentItemName() {
	if (!m_isIconView)  return m_listView->currentItem()->text(1);
	else return m_iconView->currentItem()->text();
    }
    
    /** An item was double clicked */
    void ProjectList::editRequested( QListViewItem *, const QPoint &, int col)
    {
	// only react if the click is not in column 2 (because col 2 is the editable description
        if (col!=2) emit editItem();
    }

    /** An item was double clicked */
    void ProjectList::editRequested( QIconViewItem *)
    {
        emit editItem();
    }

/** No descriptions */
    void ProjectList::rightButtonPressed(QListViewItem * listViewItem,
	const QPoint & pos, int column) {
	QPopupMenu *menu;
	if (!listViewItem) menu = (QPopupMenu *) m_app->factory()->container("projectlist_context", m_app);
	else if (!static_cast<AVListViewItem*>(listViewItem)->clip())
	    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_folder", m_app);
	else {
	    switch (static_cast<AVListViewItem*>(listViewItem)->clip()->clipType()) {
		case DocClipBase::VIRTUAL:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_virtual", m_app);
		break;
		case DocClipBase::TEXT:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_text", m_app);
		break;
		case DocClipBase::AUDIO:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_audio", m_app);
		break;
		case DocClipBase::IMAGE:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_audio", m_app);
		break;
		default:
	            menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_clip", m_app);
		break;
	    }
	}
	if (menu) {
	    menu->popup(QCursor::pos());
	}
    }

    void ProjectList::rightButtonPressed(QIconViewItem * iconViewItem,
	const QPoint & pos) {
	QPopupMenu *menu;
	if (!iconViewItem) menu = (QPopupMenu *) m_app->factory()->container("projectlist_context", m_app);
	else if (!static_cast<AVIconViewItem*>(iconViewItem)->clip())
	    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_folder", m_app);
	else {
	    switch (static_cast<AVIconViewItem*>(iconViewItem)->clip()->clipType()) {
		case DocClipBase::VIRTUAL:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_virtual", m_app);
		break;
		case DocClipBase::TEXT:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_text", m_app);
		break;
		case DocClipBase::AUDIO:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_audio", m_app);
		break;
		case DocClipBase::IMAGE:
		    menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_audio", m_app);
		break;
		default:
	            menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_clip", m_app);
		break;
	    }
	}
	if (menu) {
	    menu->popup(QCursor::pos());
	}
    }

    void ProjectList::selectItem(int id)
    {
	QStringList ids;
	ids.append(QString::number(id));
	if (!m_isIconView) m_listView->selectItemsFromIds(ids);
	else m_iconView->selectItemsFromIds(ids);
	updateListItem();
    }

/** Get a fresh copy of files from KdenliveDoc and display them. */
    void ProjectList::slot_UpdateList() {
	QStringList openFolders;
	// Check which folders are open
	if (!m_isIconView) {
            QListViewItemIterator it( m_listView );
            while ( it.current() ) {
                if (it.current()->isOpen()) {
	        openFolders.append(it.current()->text(1));
	        }
                ++it;
            }
	    QStringList selectedItems = m_listView->selectedItemsIds();

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

	if (!openFolders.isEmpty()) {
		QListViewItemIterator it( m_listView );
        	while ( it.current() ) {
            		if (openFolders.find(it.current()->text(1)) != openFolders.end())
				it.current()->setOpen(true);
            		++it;
        	}
	}
	m_listView->selectItemsFromIds(selectedItems);
	}
	else {
	    QStringList selectedItems = m_iconView->selectedItemsIds();
	    m_iconView->clear();
	    DocumentBaseNode *node = m_document->clipHierarch();
	    if (node) {
		QPtrListIterator < DocumentBaseNode > child(node->children());
    		while (child.current()) {
			if (child.current())
				new AVIconViewItem(m_document, m_iconView, child.current());
			++child;
    		}
	    }
	m_iconView->selectItemsFromIds(selectedItems);
	}
    }

/** The clip specified has changed - update the display.
 */
    void ProjectList::slot_clipChanged(DocClipRef * ) {
        slot_clipChanged();
    }

    void ProjectList::refresh() {
	if (!m_isIconView) m_listView->triggerUpdate();
	else m_iconView->update();
    }
        
    void ProjectList::slot_clipChanged() {
	slot_UpdateList();
	if (!m_isIconView) m_listView->triggerUpdate();
	else m_iconView->update();
    }

    void ProjectList::slot_nodeDeleted(DocumentBaseNode * node) {
	slot_UpdateList();
	if (!m_isIconView) m_listView->triggerUpdate();
	else m_iconView->update();
    }

/** Called when the project list changes. */
    void ProjectList::updateListItem() {
	if (!m_isIconView) {
	    const AVListViewItem * avitem = (AVListViewItem *) m_listView->currentItem();
	    if (!avitem) return;
	    if (avitem->clip()) emit clipSelected(avitem->clip());
	}
	else {
	    DocClipRef *clip = m_iconView->selectedItem();
	    if (clip) emit clipSelected(clip);
	}
    }
    

    DocClipRefList ProjectList::currentSelection() {
	if (!m_isIconView) return m_listView->selectedItemsList();
	return m_iconView->selectedItemsList();
    }
    
    void ProjectList::selectClip(DocClipBase *clip) {
	if (!m_isIconView) {
        m_listView->clearSelection();
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
	else {
	    m_iconView->clearSelection();
	    for ( QIconViewItem *item = m_iconView->firstItem(); item; item = item->nextItem() )
	    {
            	const AVIconViewItem *avitem = static_cast < AVIconViewItem * >(item);
            	if (avitem && avitem->clip() && avitem->clip()->referencedClip() == clip) {
                    m_iconView->setSelected(item, true);
                    m_iconView->ensureItemVisible(item);
                    break;
            	}
            }
	}
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
