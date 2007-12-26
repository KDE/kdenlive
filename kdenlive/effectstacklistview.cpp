/***************************************************************************
                        effectstacklistview  -  description
                           -------------------
  begin                : Sun Feb 22 2004
  copyright            : (C) 2004 by Jason Wood
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
#include "effectstacklistview.h"

#include <kdebug.h>
#include <klocale.h>

#include "kaddeffectcommand.h"
#include "kdenlive.h"
#include "effectdrag.h"

namespace Gui {

    EffectStackListView::EffectStackListView(QWidget * parent, KdenliveApp *app,
	KdenliveDoc *doc, const char *name):KListView(parent, name), m_app(app),
	m_document(doc) {
	//addColumn(QString::null);
	addColumn(i18n("Effect Stack"));
	setSorting(-1);
	setColumnWidthMode(0, Maximum);
	setAcceptDrops(true);
	setAllColumnsShowFocus(true);
	setFullWidth(true);
	setRootIsDecorated(true);
	setSelectionMode(QListView::Extended);
	connect(this, SIGNAL(selectionChanged()), this,
	    SLOT(selectedEffect()));

	connect(this, SIGNAL(clicked(QListViewItem *)), this,
	    SLOT(slotCheckItem(QListViewItem *)));

	connect(this, SIGNAL(dropped(QDropEvent *, QListViewItem *,
		    QListViewItem *)), this, SLOT(dragDropped(QDropEvent *,
		    QListViewItem *, QListViewItem *)));
    } 

    void EffectStackListView::checkCurrentItem(bool isOn) {
	//QCheckListItem* (currentItem())->setOn(isOn);
	/*if (isOn) currentItem()->setText(0, QString::null);
	else currentItem()->setText(0, "x");*/
    }

    EffectStackListView::~EffectStackListView() {
    }

    void EffectStackListView::updateEffectStack() {
	clear();

	if (m_clip) {
	    uint selected = m_clip->effectStack().selectedItemIndex();
	    uint ix = 0;
	    QListViewItem *lastItem = NULL;
	    for (EffectStack::const_iterator itt = m_clip->effectStack().begin();
		itt != m_clip->effectStack().end(); ++itt) {
		QCheckListItem *item;
		QString group = (*itt)->group();
		if (!group.isEmpty()) {
			QListViewItem *grp = findItem(group, 0);
			if (!grp) grp = new KListViewItem(this, lastItem, group);
			item = new QCheckListItem (grp, lastItem, (*itt)->name(), QCheckListItem::CheckBox);
		}
		else {
		    if (lastItem && lastItem->parent()) lastItem = lastItem->parent();
		    item = new QCheckListItem (this, lastItem, (*itt)->name(), QCheckListItem::CheckBox);
		}
		item->setOn((*itt)->isEnabled());
		if (ix == selected) setSelected(item, true);
		ix++; 
		lastItem = item;
	    }
	}
	/*if (firstChild())
	    setSelected(itemAt(m_clip->selectedEffectIndex()), true);*/
	triggerUpdate();
    }

    void EffectStackListView::slotCheckItem(QListViewItem *item)
    {
	if (!item) return;
	int ix = selectedEffectIndex();
	kdWarning()<<"-------SWL EFFECT: "<<ix<<endl;
	if (ix == -1) return;
	bool isEnabled = m_clip->effectStack()[ix]->isEnabled();
	if (( (QCheckListItem*)item )->isOn() != isEnabled) {
		// effect was disabled or enabled
		m_clip->effectStack()[ix]->setEnabled(!isEnabled);
		emit effectToggled();
	}
    }

    void EffectStackListView::setEffectStack(DocClipRef * clip) {
	m_clip = clip;
	updateEffectStack();
    }

    void EffectStackListView::selectedEffect() {
	if (m_clip) {
	    QListViewItem * item = currentItem();
	    if (((QCheckListItem *)item)->type() != QCheckListItem::CheckBox) {
		emit effectSelected(m_clip, NULL);
		return;
	    }
	    QListViewItem *itemItt = firstChild();

	    EffectStack::iterator itt = m_clip->effectStack().begin();
	    while ((itemItt) && (itt != m_clip->effectStack().end())) {
		if (item == itemItt) {
		    emit effectSelected(m_clip, *itt);
		    break;
		}
		if (((QCheckListItem *)itemItt)->type() == QCheckListItem::CheckBox) {
		    ++itt;
		    itemItt = itemItt->nextSibling();
		}
		else itemItt = itemItt->firstChild();
	    }
	    if (itt == m_clip->effectStack().end()) {
		kdWarning() <<
		    "EffectStackDialog::selectedEffect(QListViewItem *item) iitem not found"
		    << endl;
	    }
	} else {
	    kdWarning() <<
		"EffectStackDialog::selectedEffect() : m_clip is NULL" <<
		endl;
	}
    }

    bool EffectStackListView::acceptDrag(QDropEvent * event) const {
	return EffectDrag::canDecode(event);
    } 
    
    void EffectStackListView::dragDropped(QDropEvent * e,
	QListViewItem * parent, QListViewItem * after) {
	kdWarning() << "dragDropped()" << endl;
	if (EffectDrag::canDecode(e)) {
	    Effect *effect = EffectDrag::decode(m_document, e);
	    if (effect) {
		uint itemIx = 0;
		QListViewItem *itemItt = firstChild();
		while (itemItt) {
		    if (after == itemItt) {
			kdWarning() << "adding addEffectCommand()" << endl;
			m_app->
			    addCommand(Command::KAddEffectCommand::
			    insertEffect(m_document, m_clip, itemIx + 1,
				effect));
			break;
		    }
		    ++itemIx;;
		    itemItt = itemItt->nextSibling();
		}
		if (!itemItt) {
		    kdWarning() << "adding new addEffectCommand()" << endl;
		    // we did not find a previous item - there may not be one! This is first item in the list...
		    m_app->
			addCommand(Command::KAddEffectCommand::
			insertEffect(m_document, m_clip, 0, effect));
		}
	    } else {
		kdWarning() <<
		    "EffectStackListView::dragDropped() could not decode QDropEvent"
		    << endl;
	    }
	} else {
	    kdWarning() <<
		"EffectStackListView::dragDropped() a non-EffectDrag compatable QDropEvent"
		<< endl;
	}
    }

    void EffectStackListView::slotMoveEffectUp() {
	int selectedIx = selectedEffectIndex();
	if (selectedIx > 0) {
	    m_app->
		addCommand(Command::KAddEffectCommand::
		moveEffect(m_document, m_clip, selectedIx,
		    selectedIx - 1));
	}
    }

    void EffectStackListView::slotMoveEffectDown() {
	int selectedIx = selectedEffectIndex();
	if ((selectedIx != -1)
		&& (selectedIx < (int)m_clip->effectStack().count() - 1)) {
	    m_app->
		addCommand(Command::KAddEffectCommand::
		moveEffect(m_document, m_clip, selectedIx,
		    selectedIx + 1));
	}
    }

    void EffectStackListView::slotDeleteEffect() {
	QListViewItemIterator it( this );
	int offset = 0;
	QStringList toDelete;
        while ( it.current() ) {
            QListViewItem *item = it.current();
            if (item->isSelected()) {
		if (((QCheckListItem *)item)->type() == QCheckListItem::CheckBox) toDelete.append(QString::number(selectedEffectIndex(item)));
		else {
		    // SELECTED item is a group, select all its children
		    QListViewItem *itemChild = item->firstChild();
		    while (itemChild) {
			itemChild->setSelected(true);
			itemChild = itemChild->nextSibling();
		    }
		}
	    }
	    ++it;
	}
	int index;
	for ( QStringList::Iterator it = toDelete.begin(); it != toDelete.end(); ++it ) {
            index = (*it).toInt();
	    m_app->addCommand(Command::KAddEffectCommand::removeEffect(m_document, m_clip, index - offset));
	    offset++;
    	}
	
	setEffectStack(m_clip);
    }

    int EffectStackListView::selectedEffectIndex(QListViewItem *selectedItem) {
	int result = -1;
	QCheckListItem *item;
	if (selectedItem) item = (QCheckListItem *) selectedItem;
	else item = (QCheckListItem *) currentItem();
	if (!item || item->type() != QCheckListItem::CheckBox) return result;

	int count = 0;

	QListViewItemIterator it( this );
        while ( it.current() ) {
            QListViewItem *itemItt = it.current();
	    if (itemItt == item) {
		result = count;
		break;
	    }
	    if (((QCheckListItem *)itemItt)->type() == QCheckListItem::CheckBox) {
		++count;
	    }
	    ++it;
	}

	//kdWarning() << "selectedEffectIndex = " << result << endl;
	return result;
    }

    DocClipRef *EffectStackListView::clip() {
	return m_clip;
    }

    bool EffectStackListView::groupSelected()
    {
	QCheckListItem *item = (QCheckListItem *) currentItem();
	if (!item || item->type() == QCheckListItem::CheckBox) return false;
	return true;
    }


}				// namespace Gui
