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

#include <qptrlist.h>
#include <qdragobject.h>

#include "effectdesc.h"
#include "effectlist_ui.h"

namespace Gui {

/**The effectList dialog displays a list of effects and transitions known by the renderer. Effects in this dialog can be dragged to a clip, or in the case of transitions, dragged to two or more clips (how is yet to be determined)
  *@author Jason Wood
  */

    class EffectListDialog:public QWidget {
      Q_OBJECT public:
	EffectListDialog(const QPtrList < EffectDesc > &effectList,
	    QWidget * parent = 0, const char *name = 0);
	~EffectListDialog();

	EffectList_UI *m_effectListBox;

	/** returns a drag object which is used for drag operations. */
	QDragObject *dragObject();

    private:
	/** 	EffectDesc::findDescription():
		@name : the name of the description we are trying to find.

		Finds an EffectDesc. [jwood 13.01.2004]

		Returns: The EffectDesc found, or 0 if nothing is found.
	*/
	EffectDesc *findDescription(const QString & name);
	QPtrList < EffectDesc > m_effectList;

    public slots:
	/** Set the effect list displayed by this dialog. */
	void setEffectList(const QPtrList < EffectDesc > &effectList);

    private slots:
	/** an effect has been selected in the dialog */
	void slotEffectSelected();
	/** an effect has been double clicked in the dialog */
	void slotAddEffect(QListBoxItem * item);
	/** Generates the layout for this widget. */
	void generateLayout();
	/** delete a custom effect */
        void slotDeleteEffect();

    signals: 
	void effectSelected(const QString &effectName, const QString &groupName = QString::null);
	void refreshEffects();
    };

}				// namespace Gui
#endif
