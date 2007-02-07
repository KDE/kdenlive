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

#include "projecticonview.h"

#include "avlistviewitem.h"
#include "docclipavfile.h"
#include "clipdrag.h"
#include "documentbasenode.h"

#include <kdebug.h>
#include <klocale.h>

ProjectIconView::ProjectIconView(QWidget * parent, const char *name):
KIconView(parent, name)
{
    m_doc = 0;
    m_popuptext = "";

    setAcceptDrops(true);
    setItemsMovable(true);
    setSelectionMode(QIconView::Extended);
    setMode(KIconView::Select);
    QToolTip::remove(this);
    new IconViewToolTip(this);
    setFrameStyle (QFrame::NoFrame);
    connect(this, SIGNAL(dropped(QDropEvent *, const QValueList<QIconDragItem>&)), this, SLOT(dragDropped(QDropEvent *, const QValueList<QIconDragItem>&)));

}

ProjectIconView::~ProjectIconView()
{
}

QString ProjectIconView::parentName()
{
	QString parentNode;
	if (!currentItem()) parentNode = m_doc->clipHierarch()->name();
	else if (!static_cast<AVIconViewItem *>(currentItem())->clip()) {
	    //currentItem()->setOpen(true);
	    parentNode = currentItem()->text();
	}
	else parentNode = m_doc->clipHierarch()->name();
	return parentNode;
}

QString ProjectIconView::popupText()
{
	return m_popuptext;
}

void ProjectIconView::setPopupText(QString txt)
{
	m_popuptext = txt;
}

DocClipRefList ProjectIconView::selectedItemsList() const
{
    DocClipRefList selectedList;
    QIconViewItem *item;
    for ( item = firstItem(); item; item = item->nextItem() ) {
	if (item->isSelected()) {
	    AVIconViewItem *avItem = (AVIconViewItem *) item;
	    if (avItem && avItem->clip()) selectedList.append(avItem->clip());
	}
    }
    return selectedList;
}

QStringList ProjectIconView::selectedItemsIds() const
{
    QStringList result;
    DocClipRefList selectedList;
    QIconViewItem *item;
    for ( item = firstItem(); item; item = item->nextItem() ) {
	if (item->isSelected()) {
	    AVIconViewItem *avItem = (AVIconViewItem *) item;
	    if (avItem && avItem->clip()) result.append(QString::number(avItem->clip()->referencedClip()->getId()));
	}
    }
    return result;
}

void ProjectIconView::selectItemsFromIds(QStringList idList)
{
    clearSelection();
    QIconViewItem *lastItem = NULL;
    for ( QIconViewItem *item = firstItem(); item; item = item->nextItem() ) {
	AVIconViewItem *avItem = (AVIconViewItem *) item;
	if (avItem && avItem->clip() && idList.findIndex(QString::number(avItem->clip()->referencedClip()->getId())) != -1) {
	    item->setSelected(true);
	    lastItem = item;
	}
    }

/*    QIconViewItem *lastItem = NULL;
    QIconViewItem *itemItt = firstChild();
    while (itemItt) {
	AVIconViewItem *avItem = (AVIconViewItem *) itemItt;
	if (avItem && avItem->clip() && idList.findIndex(QString::number(avItem->clip()->referencedClip()->getId())) != -1) {
	    itemItt->setSelected(true);
	    lastItem = itemItt;
	}
	if (itemItt->childCount() > 0) {
	    QIconViewItem *subitemItt = itemItt->firstChild();
	    while (subitemItt) {
		AVIconViewItem *avItem = (AVIconViewItem *) subitemItt;
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
	if (lastItem->depth() == 0) {
	    ensureItemVisible(lastItem);	
	    setCurrentItem(lastItem);
	}
	else if (lastItem->parent()->isOpen()) ensureItemVisible(lastItem);
	else {
	    clearSelection();
	    lastItem->parent()->setSelected(true);
	    setCurrentItem(lastItem->parent());
	    ensureItemVisible(lastItem->parent());
	}
    }*/
}

DocClipRef *ProjectIconView::selectedItem() const
{
    QIconViewItem *item;
    for ( item = firstItem(); item; item = item->nextItem() ) {
	if (item->isSelected()) break;
    }
    AVIconViewItem *avItem = (AVIconViewItem *) item;
    if (avItem) return avItem->clip();
}

/** returns a drag object which is used for drag operations. */
QDragObject *ProjectIconView::dragObject()
{
    DocClipRefList list = selectedItemsList();

    if (m_doc == 0) {
	kdError() << "m_doc undefined" << endl;
	return 0;
    }
    return new ClipDrag(list, parentWidget(), "drag object");
}

bool ProjectIconView::acceptDrag(QDropEvent * event) const
{
    return ClipDrag::canDecode(event);
}

/** This slot function should be called whenever a drag has been dropped
	onto the class. The cliplist produced will delete the clips within it
 	unless the signal is caught and told otherwise. Note that problems
  	will arise is multiple documents attempt to catch this signal and have
	different ideas about what to do with the clip list.*/
void ProjectIconView::dragDropped(QDropEvent * e, const QValueList<QIconDragItem>& list)
{
    /*if (after) emit dragDropOccured(e, after);
    else */
//	emit dragDropOccured(e, parent());
}

/** Sets the document to the one specified */
void ProjectIconView::setDocument(KdenliveDoc * doc)
{
    m_doc = doc;
}
