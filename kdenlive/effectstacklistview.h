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
#ifndef EFFECTSTACKLISTVIEW_H
#define EFFECTSTACKLISTVIEW_H

#include <klistview.h>

#include "docclipref.h"

class KdenliveDoc;

namespace Gui {
    class KdenliveApp;

/**
A list view that displays an effect stack, and allows the relevant drag/drop operations for an effect stack.

@author Jason Wood
*/
    class EffectStackListView:public KListView {
      Q_OBJECT public:
	EffectStackListView(QWidget * parent, KdenliveApp *app,
	KdenliveDoc *doc, const char *name = 0);

	~EffectStackListView();

	void updateEffectStack();
	void checkCurrentItem(bool isOn);

	/** Setup the effect stack dialog to display the given clip */
	void setEffectStack(DocClipRef * clip);

	public slots:
	/** Moves the currently selected effect one place up the effect stack. If no effect is selected, or if the selected effect is already first in the
	 * list, then nothing will happen. */
	void slotMoveEffectUp();
	/** Moves the currently selected effect one place down the effect stack. If no effect is selected, or if the selected effect is already last in the
	 * stack, then nothing will happen. */
	void slotMoveEffectDown();
	/** Delete the currently selected effect. If no effect is selected nothing will happen. */
	void slotDeleteEffect();

	/** @returns the index of the currently selected effect, or -1 if no effect is currently selected. */
	int selectedEffectIndex() const;

	DocClipRef *clip();
	 signals:
	/** Emitted when a new effect has been selected in the effect stack. */
	void effectSelected(DocClipRef *, Effect *);
	void effectToggled();

      protected:
	/** Returns true if we can accept the drag. An effectstacklistview can accept EffectDrag events. When an event is
	dropped onto the effectstack it should issue a command to insert the effect at the correct point in the list. */
	 bool acceptDrag(QDropEvent * event) const;


	private slots: void selectedEffect(QListViewItem * item);

	/** Called when a drag operation has dropped onto the effect stack list. */
	void dragDropped(QDropEvent * e, QListViewItem * parent,
	    QListViewItem * after);
	void slotCheckItem(QListViewItem *item);

      private:
	KdenliveApp *m_app;
	KdenliveDoc *m_document;
	DocClipRef * m_clip;

    };

}				// namespace Gui
#endif
