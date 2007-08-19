/***************************************************************************
                          projectlistview.cpp  -  description
                             -------------------
    begin                : Sun Jun 30 2002
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

#include "projectlistview.h"

#include "avlistviewitem.h"
#include "docclipavfile.h"
#include "clipdrag.h"
#include "documentbasenode.h"

#include <kdebug.h>
#include <klocale.h>

ProjectListView::ProjectListView(QWidget * parent, const char *name):
KListView(parent, name)
{
    m_doc = 0;
    m_popuptext = QString::null;

    addColumn(i18n("Thumbnail"));
    addColumn(i18n("Filename"));
    //addColumn( i18n( "Type" ) );
    //addColumn( i18n( "Duration" ) );
    //addColumn( i18n( "Usage Count" ) );
    //addColumn( i18n( "Size" ) );
    addColumn(i18n("Description"));

    setDragEnabled(true);
    setAcceptDrops(true);
    setItemsMovable(true);
    setDropVisualizer(true);
    setFullWidth(true);
    setShadeSortColumn(false);
    setDefaultRenameAction(Accept);
    setAllColumnsShowFocus(true);
    setRootIsDecorated(true);
    setSelectionMode(QListView::Extended);
    setSorting(1);
    QToolTip::remove(this);
    new ListViewToolTip(this);
    setFrameStyle (QFrame::NoFrame);
    connect(this, SIGNAL(dropped(QDropEvent *, QListViewItem *,
		QListViewItem *)), this, SLOT(dragDropped(QDropEvent *,
		QListViewItem *, QListViewItem *)));
    connect(this, SIGNAL(expanded ( QListViewItem *)), this, SLOT(slotOpenItem(QListViewItem *)));

}

ProjectListView::~ProjectListView()
{
}

QString ProjectListView::parentName()
{
	QString parentNode;
	if (!currentItem()) parentNode = m_doc->clipHierarch()->name();
	else {
	    QListViewItem *item = currentItem();
	    while (item != 0) {
	    	BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) item)->getType();
	    	if (type == BaseListViewItem::FOLDER) {
	    	    //currentItem()->setOpen(true);
	    	    parentNode = item->text(1);
		    break;
	    	}
	    	else item = item->parent();
	    }
	}
	if (parentNode.isEmpty()) parentNode = m_doc->clipHierarch()->name();
	return parentNode;
}

QString ProjectListView::popupText()
{
	return m_popuptext;
}

void ProjectListView::setPopupText(QString txt)
{
	m_popuptext = txt;
}

void ProjectListView::slotOpenItem(QListViewItem *item)
{
    emit createChildren(item);
}


DocClipRefList ProjectListView::selectedItemsList() const
{
    QPtrList< QListViewItem > selectedItems;
    selectedItems = KListView::selectedItems(true);
    DocClipRefList selectedList;
    QListViewItem *item;
    for ( item = selectedItems.first(); item; item = selectedItems.next() ) {
	BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) item)->getType();
	if (type == BaseListViewItem::CLIP) {
	    AVListViewItem *avItem = (AVListViewItem *) item;
	    if (avItem && avItem->clip()) selectedList.append(avItem->clip());
	}
    }
    return selectedList;
}

QStringList ProjectListView::selectedItemsIds() const
{
    QStringList result;
    QPtrList< QListViewItem > selectedItems;
    selectedItems = KListView::selectedItems(true);
    DocClipRefList selectedList;
    QListViewItem *item;
    for ( item = selectedItems.first(); item; item = selectedItems.next() ) {
	BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) item)->getType();
	if (type == BaseListViewItem::CLIP) {
	    AVListViewItem *avItem = (AVListViewItem *) item;
	    if (avItem && avItem->clip()) result.append(QString::number(avItem->clip()->referencedClip()->getId()));
	}
    }
    return result;
}

void ProjectListView::selectItemsFromIds(QStringList idList)
{
    clearSelection();
    QListViewItem *lastItem = NULL;
    QListViewItem *itemItt = firstChild();
    if (idList.isEmpty()) {
	if (itemItt) itemItt->setSelected(true);
	emit selectionChanged();
	return;
    }
    while (itemItt) {
	BaseListViewItem::ITEMTYPE type = ((BaseListViewItem *) itemItt)->getType();
	if (type == BaseListViewItem::CLIP) {
	    AVListViewItem *avItem = (AVListViewItem *) itemItt;
	    if (avItem && avItem->clip() && idList.findIndex(QString::number(avItem->clip()->referencedClip()->getId())) != -1) {
	    	itemItt->setSelected(true);
	    	lastItem = itemItt;
	    }
	}
	else if (type == BaseListViewItem::FOLDER) {
	    QListViewItem *subitemItt = itemItt->firstChild();
	    while (subitemItt) {
		AVListViewItem *avItem = (AVListViewItem *) subitemItt;
		if (avItem && avItem->clip() && idList.findIndex(QString::number(avItem->clip()->referencedClip()->getId())) != -1) {
	    	    subitemItt->setSelected(true);
		    lastItem = subitemItt;
		}
		subitemItt = subitemItt->nextSibling();
	    }
	}
	itemItt = itemItt->nextSibling();
    }
    if (lastItem) {
	// if last item is in an opened folder , focus it
	if (lastItem->depth() == 0 || lastItem->parent()->isOpen()) {
	    ensureItemVisible(lastItem);	
	    setCurrentItem(lastItem);
	}
	else {
	    clearSelection();
	    lastItem->parent()->setSelected(true);
	    setCurrentItem(lastItem->parent());
	    ensureItemVisible(lastItem->parent());
	}
    }
    else if (firstChild()) firstChild()->setSelected(true);
    emit selectionChanged();
}

/** returns a drag object which is used for drag operations. */
QDragObject *ProjectListView::dragObject()
{
    DocClipRefList list = selectedItemsList();

    if (m_doc == 0) {
	kdError() << "m_doc undefined" << endl;
	return 0;
    }
    return new ClipDrag(list, parentWidget(), "drag object");
}

bool ProjectListView::acceptDrag(QDropEvent * event) const
{
    return ClipDrag::canDecode(event);
}

/** This slot function should be called whenever a drag has been dropped
	onto the class. The cliplist produced will delete the clips within it
 	unless the signal is caught and told otherwise. Note that problems
  	will arise is multiple documents attempt to catch this signal and have
	different ideas about what to do with the clip list.*/
void ProjectListView::dragDropped(QDropEvent * e, QListViewItem * parent,
    QListViewItem * after)
{
    if (after) emit dragDropOccured(e, after);
    else emit dragDropOccured(e, parent);
}

/** Sets the document to the one specified */
void ProjectListView::setDocument(KdenliveDoc * doc)
{
    m_doc = doc;
}

void ProjectListView::contentsMouseDoubleClickEvent( QMouseEvent * e )
{
    QListViewItem *item = itemAt(e->pos());
    if (item) {
	int col = item ? header()->mapToLogical( header()->cellAt( e->x() ) ) : -1;
	emit doubleClicked(item, e->pos(), col);
    }
    else emit addClipRequest();
    e->accept();
}
