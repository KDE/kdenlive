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

#include <kdebug.h>
#include <klineedit.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kpushbutton.h>

/* This define really should go in projectlist_ui, but qt just puts the classname there at the moment :-( */
#include <qpushbutton.h>
#include <qpaintdevicemetrics.h>
#include <qpainter.h>

#include <qcursor.h>
#include <qlabel.h>


#include "documentbasenode.h"
#include "projectlist.h"
#include "avlistviewitem.h"
#include "folderlistviewitem.h"
#include "westleylistviewitem.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "docclipavfile.h"
#include "timecode.h"
#include "playlist.h"


#include <iostream>
#include <string>
#include <map>

namespace Gui {

    ProjectList::ProjectList(KdenliveApp * app, KdenliveDoc * document, bool iconView, QWidget * parent, const char *name):ProjectList_UI(parent, name),
	m_document(document), m_app(app), m_isIconView(iconView), m_listView(NULL), m_iconView(NULL) {
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
	button_add->setIconSet(QIconSet(loader.loadIcon("kdenlive_new_clip",
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
	connect(m_listView, SIGNAL(selectionChanged()), this, SLOT(updateListItem()));
	connect(m_listView, SIGNAL(addClipRequest()), this, SLOT(addClipRequest()));
	connect(m_listView, SIGNAL(itemRenamed(QListViewItem *, int)), this, SLOT(projectModified()));
	connect(m_listView, SIGNAL(createChildren(QListViewItem *)), this, SLOT(createItemChildren(QListViewItem *)));
    }

    void ProjectList::setupIconView() {
	m_iconView = new ProjectIconView(view_frame);
	iv_search = new KIconViewSearchLine(lv_frame, m_iconView, "search_line");
	m_iconView->setDocument(m_document);
	m_iconView->show();
	iv_search->show();
	connect(m_iconView, SIGNAL(selectionChanged ()), this, SLOT(updateListItem()));
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
	if (!m_listView->currentItem()) return false;
	return m_listView->currentItem()->childCount() > 0;
    }

    QStringList ProjectList::currentItemChildrenIds() {
	// TODO: implement folder functionnality in icon view
	QStringList list;
	if (m_isIconView) return list;
	QString folderName = currentItemName();
	QListViewItem * myChild = m_listView->currentItem()->firstChild();
        while( myChild ) {
	    BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) myChild)->getType();
            if (type == BaseListViewItem::CLIP) {
	        list.append(QString::number((static_cast<AVListViewItem*>(myChild))->clip()->referencedClip()->getId()));
	    }
	    myChild = myChild->nextSibling();
        }
	return list;
    }

    DocClipRef* ProjectList::currentClip() {
	if (!m_isIconView) {
	    QListViewItem *item = m_listView->currentItem();
	    if (!item) return NULL;
	    BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) item)->getType();
            if (type != BaseListViewItem::CLIP) return NULL;
	    return (static_cast<AVListViewItem*>(item))->clip();
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

    void ProjectList::projectModified()
    {
	m_app->documentModified(true);
    }

/** No descriptions */
    void ProjectList::rightButtonPressed(QListViewItem * listViewItem,
	const QPoint & pos, int column) {
	QPopupMenu *menu = NULL;
	if (!listViewItem) menu = (QPopupMenu *) m_app->factory()->container("projectlist_context", m_app);
	else {
	    BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) listViewItem)->getType();
            if (type == BaseListViewItem::FOLDER) {
		menu = (QPopupMenu *) m_app->factory()->container("projectlist_context_folder", m_app);
	    }
            else if (type == BaseListViewItem::CLIP) {
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

    void ProjectList::createItemChildren(QListViewItem *item)
    {
	if (item->childCount() > 0) return;
	BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) item)->getType();
        if (type != BaseListViewItem::CLIP) return;
        DocClipRef *clip = static_cast<AVListViewItem *>(item)->clip();
        if (clip && clip->clipType() == DocClipBase::PLAYLIST) {
	    // clip is a playlist, generate a list of children...
	    QDomDocument doc;
	    QFile file(clip->fileURL().path());
	    doc.setContent(&file, false);
	    file.close();
	    QDomElement documentElement = doc.documentElement();
	    if (documentElement.tagName() != "westley") {
	    	kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
	    }
	    int tracksCount = 0;

	    QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
	    QDomNode n, node;
	    QMap <QString, QString> prods;
	    QDomElement e, entry;

	    if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	    else n = documentElement.firstChild();

	    while (!n.isNull()) {
	    	e = n.toElement();
	    	if (!e.isNull()) {
		    if (e.tagName() == "producer") {
		    	kdDebug()<<"// FOPUND A PRODUCER"<<endl;
		    	// found producer, adding it to the document...
		    	int cliptype = e.attribute("type", QString::number(-1)).toInt();
		    	if (cliptype == DocClipBase::COLOR) {
			    QListViewItem *child = new WestleyListViewItem( item, i18n("Color Clip"));
			    QPixmap pix = QPixmap(40, 30);
			    QString col = e.attribute("colour", QString::null);
        		    col = col.replace(0, 2, "#");
        		    pix.fill(QColor(col.left(7)));
			    child->setPixmap(0, pix);
		    	}
			else if (cliptype == DocClipBase::AUDIO) {
			    KURL resource =  KURL(e.attribute("resource", QString::null));
			    QListViewItem *child = new WestleyListViewItem(item, resource.fileName());
		    	}
			else if (cliptype == DocClipBase::AV || cliptype == DocClipBase::VIDEO) {
			    KURL resource =  KURL(e.attribute("resource", QString::null));
			    QListViewItem *child = new WestleyListViewItem(item, resource.fileName());
			    QPixmap pix = m_document->renderer()->getVideoThumbnail(resource, 1, 40, 30);
			    child->setPixmap(0, pix);
		    	}
			else if (cliptype == DocClipBase::PLAYLIST) {
			     KURL resource =  KURL(e.attribute("resource", QString::null));
			    QListViewItem *child = new WestleyListViewItem(item, resource.fileName());
			    QPixmap pix = m_document->renderer()->getVideoThumbnail(resource, 1, 40, 30);
			    child->setPixmap(0, pix);
		    	}
			else if (cliptype == DocClipBase::TEXT) {
			    QListViewItem *child = new WestleyListViewItem(item, i18n("Text Clip"));
			    QString resource =  e.attribute("resource", QString::null);
			    QImage i(resource);
			    QPixmap pix(40, 30);
			    pix.convertFromImage(i.smoothScale(40, 30));
			    child->setPixmap(0, pix);
		    	}
			else if (cliptype == DocClipBase::IMAGE) {
			     KURL resource =  KURL(e.attribute("resource", QString::null));
			    QListViewItem *child = new WestleyListViewItem(item, resource.fileName());
			    QImage i(resource.path());
			    QPixmap pix(40, 30);
			    pix.convertFromImage(i.smoothScale(40, 30));
			    child->setPixmap(0, pix);
		    	}
			else if (cliptype == DocClipBase::SLIDESHOW) {
			    QListViewItem *child = new WestleyListViewItem(item, i18n("Slideshow Clip"));
		    	}
			else {
			     KURL resource =  KURL(e.attribute("resource", QString::null));
			    QListViewItem *child = new WestleyListViewItem(item, resource.fileName());
			    QPixmap pix = m_document->renderer()->getVideoThumbnail(resource, 1, 40, 30);
			    child->setPixmap(0, pix);
			}
		    	
/*
		    	QString resource = e.attribute("resource", QString::null);
		    	if (!resource.isEmpty()) {
			    producersList.append(KURL(resource));
			    prods[e.attribute("id", QString::number(-1))] = resource;
			    kdDebug()<<"// APPENDING: "<<resource<<endl;
		    	}*/
		    }
	    	}
	    	n = n.nextSibling();
	    }
    	}
    }


/** Get a fresh copy of files from KdenliveDoc and display them. */
    void ProjectList::slot_UpdateList() {
	QStringList openFolders;
	// Check which folders are open
	if (!m_isIconView) {
            QListViewItemIterator it( m_listView );
            while ( it.current() ) {
		// BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) it.current())->getType();
                if (it.current()->isOpen()) {
	            //if (type == BaseListViewItem::FOLDER) 
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
				if (child.current()->asClipNode()) (void) new AVListViewItem( m_listView, child.current());
				else {
				    FolderListViewItem *item = new FolderListViewItem( m_listView, child.current()->name());
				        // recursively populate the rest of the node tree.
    				    QPtrListIterator < DocumentBaseNode > itemchild(child.current()->children());
    				    while (itemchild.current()) {
					new AVListViewItem(item, itemchild.current());
					++itemchild;
    				    }
				}
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
	    if (!m_listView->currentItem()) return;
	    BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) m_listView->currentItem())->getType();
	    if (type == BaseListViewItem::CLIP) {
	        const AVListViewItem * avitem = (AVListViewItem *) m_listView->currentItem();
	        if (!avitem) return;
	        if (avitem->clip()) emit clipSelected(avitem->clip());
	    }
	    else if (type == BaseListViewItem::FOLDER) {
		// do nothing for the moment
	    }
	}
	else {
	    DocClipRef *clip = m_iconView->selectedItem();
	    if (clip) emit clipSelected(clip);
	}
    }

    bool ProjectList::isEmpty() {
	if (!m_isIconView) return m_listView->childCount() == 0;
	return m_iconView->count() == 0;
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


    void ProjectList::fixPlaylists() {
	PlayList *pl = new PlayList(m_listView, m_iconView, m_isIconView, this);
	connect(pl, SIGNAL(signalFixed()), &(m_document->clipManager()), SLOT(refreshThumbNails()));
	pl->exec();
    }

    void ProjectList::doPrinting(KPrinter *printer, QPainter *p, uint images, bool fullPath, bool filtered) {
        // We use a QPaintDeviceMetrics to know the actual page size in pixel,
        // this gives the real painting area
	//p->begin(printer);
	QFontMetrics fm = p->fontMetrics();
        QPaintDeviceMetrics metrics(p->device());
	int yPos = 0;

        const int Margin = 10;
        int pageNo = 1;
        int indent = 0;

	p->drawText( Margin, Margin, metrics.width(), fm.lineSpacing(), ExpandTabs | DontClip, m_document->URL().path() + ", " + i18n("Page %1").arg(pageNo));
	yPos = 2* fm.lineSpacing();

	if (!m_isIconView) {
        QListViewItemIterator it( m_listView );
	int maxItems = m_listView->childCount();
        while ( it.current() ) {
            const AVListViewItem *avitem = static_cast < AVListViewItem * >(*it);
	    if (!filtered || (*it)->isVisible())
            if (avitem && avitem->clip()) {
		if (indent != 0) {
		    indent = (*it)->depth() * 3 * Margin;
		    if (indent == 0) {
			p->drawLine ( Margin, Margin + yPos, metrics.width() - Margin, Margin + yPos);
		    	yPos += Margin;
		    }
		}
		else indent = (*it)->depth() * 3 * Margin;

		// Get thumbnail for current item
	    	DocClipBase *baseClip = avitem->clip()->referencedClip();
	    	QPixmap pix;
		if (images == 1) {
		    pix = avitem->clip()->thumbnail();
		    if (pix.isNull()) {
		        avitem->clip()->generateThumbnails();
		        pix = avitem->clip()->thumbnail();
		    }
		    if (pix.isNull()) pix = baseClip->thumbnail();
		}
		else if (images == 2) {
		    pix = avitem->clip()->extractThumbnail(80 * KdenliveSettings::displayratio(), 80);
		    if (pix.isNull()) pix = baseClip->thumbnail();
		}

		// check if it needs a new page
		if ( Margin + yPos +  pix.height()> metrics.height() - Margin ) {
		    printer->newPage();
		    pageNo++;
		    p->drawText( Margin, Margin, metrics.width(), fm.lineSpacing(), ExpandTabs | DontClip, m_document->URL().path() + ", " + i18n("Page %1").arg(pageNo));
		    yPos = 2* fm.lineSpacing();
		}

		QString title;
		if (!fullPath) title = baseClip->name();
		else title = baseClip->fileURL().path();
		title += " (" + Timecode::getEasyTimecode(baseClip->duration(), KdenliveSettings::defaultfps()) + ")";

		p->drawPixmap(Margin + indent, Margin + yPos, pix);
		p->drawText( Margin + indent + pix.width() + Margin, Margin + yPos,
                            metrics.width(), fm.lineSpacing(),
                            ExpandTabs | DontClip, title);
		
		p->drawText( Margin + indent + pix.width() + Margin, Margin + yPos + fm.lineSpacing(),
                            metrics.width(), fm.lineSpacing(),
                            ExpandTabs | DontClip,
                            baseClip->description() );
		yPos = yPos + fm.lineSpacing() + pix.height();
		
                }
		else {
		    if ( Margin + yPos > metrics.height() - Margin ) {
		        printer->newPage();
		        pageNo++;
		        p->drawText( Margin, Margin, metrics.width(), fm.lineSpacing(), ExpandTabs | DontClip, m_document->URL().path() + ", " + i18n("Page %1").arg(pageNo));
		        yPos = 2* fm.lineSpacing();
		    }
		    p->drawLine ( Margin, Margin + yPos, metrics.width() - Margin, Margin + yPos);
		    yPos += Margin;
		    p->drawText( Margin, Margin + yPos,
                            metrics.width(), fm.lineSpacing(),
                            ExpandTabs | DontClip, (*it)->text(1) );
		    yPos = yPos + 2*fm.lineSpacing();
	    }
            ++it;
        }
	}
	else {
	    for ( QIconViewItem *item = m_iconView->firstItem(); item; item = item->nextItem() )
	    {
            	const AVIconViewItem *avitem = static_cast < AVIconViewItem * >(item);
            	if (avitem && avitem->clip()) {
            	}
            }
	}
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
