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

    EffectStackListView::EffectStackListView(QWidget * parent,
	const char *name):KListView(parent, name), m_app(NULL),
	m_document(NULL) {
	addColumn(QString::null);
	addColumn(i18n("Effect Stack"));
	setSorting(-1);
	setColumnWidthMode(1, Maximum);
	setAcceptDrops(true);
	setAllColumnsShowFocus(true);
	setFullWidth(true);
	connect(this, SIGNAL(selectionChanged(QListViewItem *)), this,
	    SLOT(selectedEffect(QListViewItem *)));

	connect(this, SIGNAL(dropped(QDropEvent *, QListViewItem *,
		    QListViewItem *)), this, SLOT(dragDropped(QDropEvent *,
		    QListViewItem *, QListViewItem *)));
    } 
    
    void EffectStackListView::setAppAndDoc(KdenliveApp * app, KdenliveDoc * document) {
	m_app = app;
	m_document = document;
    }

    void EffectStackListView::checkCurrentItem(bool isOn) {
	if (isOn) currentItem()->setText(0, QString::null);
	else currentItem()->setText(0, "x");
    }

    EffectStackListView::~EffectStackListView() {
    }

    void EffectStackListView::updateEffectStack() {
	clear();

	if (m_clip) {
	    uint selected = m_clip->effectStack().selectedItemIndex();
	    uint ix = 0;
	    QListViewItem *lastItem = NULL;
	    for (EffectStack::const_iterator itt =
		m_clip->effectStack().begin();
		itt != m_clip->effectStack().end(); ++itt) {
		KListViewItem *item;
		if ((*itt)->isEnabled())
		item = new KListViewItem(this, lastItem, QString::null, (*itt)->name());
		else item = new KListViewItem(this, lastItem, "x", (*itt)->name());
		if (ix == selected) setSelected(item, true);
		ix++; 
		lastItem = item;
	    }
	}
	/*if (firstChild())
	    setSelected(itemAt(m_clip->selectedEffectIndex()), true);*/
	triggerUpdate();
    }

    void EffectStackListView::setEffectStack(DocClipRef * clip) {
	m_clip = clip;
	updateEffectStack();
    }

    void EffectStackListView::selectedEffect(QListViewItem * item) {
	if (m_clip) {
	    QListViewItem *itemItt = firstChild();

	    EffectStack::iterator itt = m_clip->effectStack().begin();
	    while ((itemItt) && (itt != m_clip->effectStack().end())) {
		if (item == itemItt) {
		    emit effectSelected(m_clip, *itt);
		    break;
		}
		++itt;
		itemItt = itemItt->nextSibling();
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
	int selectedIx = selectedEffectIndex();
	if (selectedIx != -1) {
	    m_app->
		addCommand(Command::KAddEffectCommand::
		removeEffect(m_document, m_clip, selectedIx));
	}
    }

    int EffectStackListView::selectedEffectIndex() const {
	int result = -1;
	QListViewItem *item = currentItem();
	if (item) {
	    int count = 0;
	    QListViewItem *itemItt = firstChild();
	    while (itemItt) {
		if (itemItt == item) {
		    result = count;
		    break;
		}
		++count;
		itemItt = itemItt->nextSibling();
	    }
	}
	kdWarning() << "selectedEffectIndex = " << result << endl;
	return result;
    }

    DocClipRef *EffectStackListView::clip() {
	return m_clip;
    }


}				// namespace Gui
