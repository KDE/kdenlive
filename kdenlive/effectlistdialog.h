/***************************************************************************
                         effectlistdialog.h  -  description
                            -------------------
   begin                : Sun Feb 9 2003
   copyright            : (C) 2003 by Jason Wood
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

#ifndef EFFECTLISTDIALOG_H
#define EFFECTLISTDIALOG_H

#include <qwidget.h>
#include <klistview.h>
#include <qptrlist.h>

#include "effectdesc.h"
#include "listviewtagsearch.h"

namespace Gui {

/**The effectList dialog displays a list of effects and transitions known by the renderer. Effects in this dialog can be dragged to a clip, or in the case of transitions, dragged to two or more clips (how is yet to be determined)
  *@author Jason Wood
  */

    class EffectListDialog:public QWidget {
      Q_OBJECT public:
	EffectListDialog(const QPtrList < EffectDesc > &effectList,
	    QWidget * parent = 0, const char *name = 0);
	~EffectListDialog();

	/** returns a drag object which is used for drag operations. */
	QDragObject *dragObject();

    private:
	/** Generates the layout for this widget. */
	void generateLayout();

	/** 	EffectDesc::findDescription():
		@name : the name of the description we are trying to find.

		Finds an EffectDesc. [jwood 13.01.2004]

		Returns: The EffectDesc found, or 0 if nothing is found.
	*/
	EffectDesc *findDescription(const QString & name);
	QPtrList < EffectDesc > m_effectList;

	ListViewTagSearchWidget *m_effectSearch;
	KListView *m_effectView;

    public slots:
	/** Set the effect list displayed by this dialog. */
	void setEffectList(const QPtrList < EffectDesc > &effectList);

    private slots:
	/** an effect has been selected in the dialog */
	void slotEffectSelected(QListViewItem * item);
	 signals: 
	void effectSelected(const QString &);
    };

}				// namespace Gui
#endif
