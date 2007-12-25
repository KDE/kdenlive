/***************************************************************************
                         effectstackdialog  -  description
                            -------------------
   begin                : Mon Jan 12 2004
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
#ifndef EFFECTSTACKDIALOG_H
#define EFFECTSTACKDIALOG_H


#include <qgrid.h>
#include <qframe.h>


//#include <effectstackdialog_ui.h>
#include <klistbox.h>

#include "kdenlive.h"
#include "effectstacklistview.h"

#include <effectstackdialog_ui.h>

class DocClipRef;
class Effect;
class KdenliveDoc;

namespace Gui {

/**
Implementation of the EffectStackDialog

@author Jason Wood
*/
    class EffectStackDialog:public EffectStackDialog_UI {
      Q_OBJECT public:
	EffectStackDialog(KdenliveApp * app, KdenliveDoc * doc,
	    QWidget * parent = 0, const char *name = 0);

	 virtual ~ EffectStackDialog();
	 signals:
	/** Emitted when an effect parameter has been modified. */
	void generateSceneList();
	void redrawTrack(int, GenTime, GenTime);

	public slots:
	/** Setup the effect stack dialog to display the given clip */
	void slotSetEffectStack(DocClipRef * clip);
	void addParameters(DocClipRef * clip, Effect * effect);
	void updateKeyFrames();
	void slotSwitchEffect();

      private:
	/** When performing an operation on several parameters, we don't want the scenelist and tracks to be generated for each parameter, just at the end of the operation. The m_blockUpdate is used for that purpose*/
	bool m_blockUpdate;
	QString m_effecttype;
	bool m_hasKeyFrames;
	QGrid *m_container;
	QFrame *m_frame;
	KdenliveApp * m_app;
	EffectStackListView *m_effectList;

      private slots: 
	void parameterChanged();
	void resetParameters();
        void slotDeleteEffect();
	void selectKeyFrame(int);
	void changeKeyFramePosition(int newTime);
	void changeKeyFrameValue(int newTime);
	void disableButtons();
	void enableButtons();
	void cleanWidgets();
	void slotSaveEffect();
	void slotGroupEffects();
	void slotUngroupEffects();
    };

}				// namespace Gui
#endif
