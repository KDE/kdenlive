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

//#include <effectstackdialog_ui.h>
#include <klistbox.h>
#include "effectstacklistview.h"

#include <effectstackdialog_ui.h>

class DocClipRef;
class Effect;
class KdenliveDoc;

namespace Gui
{
	class KdenliveApp;

/**
Implementation of the EffectStackDialog

@author Jason Wood
*/
class EffectStackDialog : public EffectStackDialog_UI
{
	Q_OBJECT
public:
	EffectStackDialog( KdenliveApp *app, KdenliveDoc *doc, QWidget *parent = 0, const char *name = 0 );

	virtual ~EffectStackDialog();
signals:
	/** Emitted when a new effect has been selected in the effect stack. */
	void effectSelected(DocClipRef *, Effect *);
	/** Emitted when an effect parameter has been modified. */
	void generateSceneList();

public slots:
	/** Setup the effect stack dialog to display the given clip */
	void slotSetEffectStack(DocClipRef *clip);

	void addParameters(DocClipRef *clip, Effect *effect);
private:
	void updateEffectStack();

private slots:
	void parameterChanged(int);
};

} // namespace Gui

#endif
