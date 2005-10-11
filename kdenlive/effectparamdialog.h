/***************************************************************************
                          effectparamdialog.h  -  description
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

#ifndef EFFECTPARAMDIALOG_H
#define EFFECTPARAMDIALOG_H

#include <qwidget.h>
#include <qhbox.h>
#include <qvbox.h>

#include "effectdesc.h"

class QVBox;
class QComboBox;
class KPushButton;

class DocClipRef;
class KdenliveDoc;

namespace Gui
{

class KFixedRuler;
class KTimeLine;
class KdenliveApp;

/**The effect param dialog displays the parameter settings for a particular effect. This may be in relation to a clip, or it may be in relation to the effect dialog.
  *@author Jason Wood
  */

class EffectParamDialog : public QVBox  {
   Q_OBJECT
public:
	EffectParamDialog(KdenliveApp *app, KdenliveDoc *document, QWidget *parent = 0, const char *name = 0);
	~EffectParamDialog();
public slots:
	void slotSetEffectDescription(const EffectDesc &desc);
	void slotSetEffect(DocClipRef *clip, Effect *effect);
private:
	/** Clears the effect that is seen in the parameter dialog. */
	void clearEffect();

	/** Generate the layout for the current description and effect. */
	void generateLayout();

	QHBox *m_presetLayout;
	QComboBox *m_presets;
	KPushButton *m_presetAdd;
	KPushButton *m_presetDelete;

	QVBox *m_editLayout;
	KTimeLine *m_timeline;

	const EffectDesc *m_desc;
	Effect *m_effect;
	DocClipRef *m_clip;

	KdenliveApp *m_app;
	KdenliveDoc *m_document;
};

} // namespace Gui

#endif
