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

/**
A list view that displays an effect stack, and allows the relevant drag/drop operations for an effect stack.

@author Jason Wood
*/
class EffectStackListView : public KListView {
	Q_OBJECT
public:
	EffectStackListView(QWidget * parent = 0, const char * name = 0);

	~EffectStackListView();

	void updateEffectStack();

	/** Setup the effect stack dialog to display the given clip */
	void setEffectStack(DocClipRef *clip);

signals:
	/** Emitted when a new effect has been selected in the effect stack. */
	void effectSelected(DocClipRef *, Effect *);

private slots:
	void selectedEffect(QListViewItem *item);

private:
	DocClipRef *m_clip;
};

#endif
