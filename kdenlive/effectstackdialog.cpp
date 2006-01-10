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
#include "effectstackdialog.h"

#include <qspinbox.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qlabel.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <kpushbutton.h>

#include <effectstacklistview.h>

#include "docclipref.h"
#include "effect.h"
#include "effectdesc.h"
#include "effectparamdesc.h"

namespace Gui
{

EffectStackDialog::EffectStackDialog(KdenliveApp *app, KdenliveDoc *doc, QWidget *parent, const char *name )
 : EffectStackDialog_UI(parent, name)
{
	KIconLoader loader;

	m_upButton->setIconSet( QIconSet( loader.loadIcon( "1uparrow", KIcon::Toolbar ) ));
	m_downButton->setIconSet( QIconSet( loader.loadIcon( "1downarrow", KIcon::Toolbar ) ));
	m_deleteButton->setIconSet( QIconSet( loader.loadIcon( "editdelete", KIcon::Toolbar ) ));

	// HACK - We are setting app and doc here because we cannot pass app and doc directly via the auto-generated UI file. This
	// needs to be fixed...
	m_effectList->setAppAndDoc(app, doc);

	m_parameter->setOrientation(Qt::Vertical);
	m_parameter->setColumns(5);

	connect(m_upButton, SIGNAL(clicked()), m_effectList, SLOT(slotMoveEffectUp()));
	connect(m_downButton, SIGNAL(clicked()), m_effectList, SLOT(slotMoveEffectDown()));
	connect(m_deleteButton, SIGNAL(clicked()), m_effectList, SLOT(slotDeleteEffect()));

	connect(m_effectList, SIGNAL(effectSelected(DocClipRef *, Effect *)), this, SLOT(addParameters(DocClipRef *, Effect *)));
}


EffectStackDialog::~EffectStackDialog()
{
}

void EffectStackDialog::addParameters(DocClipRef *clip, Effect *effect)
{
// Rebuild the effect parameters dialog

	uint parameterNum = 0;
	clip->setEffectStackSelectedItem(m_effectList->selectedEffectIndex());

	// remove all previous params
	if (m_parameter->child("container","QVBox")) delete m_parameter->child("container","QVBox");


	QVBox *container = new QVBox(m_parameter,"container");
	container->setSpacing(5);
	while (effect->parameter(parameterNum)) {
		// for each constant parameter, build a QSpinBox with its value
		if (effect->effectDescription().parameter(parameterNum)->type() == "constant") {
			int maxValue = effect->effectDescription().parameter(parameterNum)->max();
			int minValue = effect->effectDescription().parameter(parameterNum)->min();
			QHBox *gb = new QHBox(container,"box");
			(void) new QLabel(effect->effectDescription().parameter(parameterNum)->name(),gb);
			QString widgetName = QString("param");
			widgetName.append(QString::number(parameterNum));
			QSpinBox *spinParam = new QSpinBox(gb,widgetName.ascii());
			connect(spinParam, SIGNAL( valueChanged (int) ), this, SLOT( parameterChanged(int) ));
			spinParam->setMaxValue(maxValue);
			spinParam->setMinValue(minValue);
			spinParam->setValue(effect->effectDescription().parameter(parameterNum)->value());
		}
		parameterNum++;
	}
	container->show();
	emit effectSelected(clip, effect);
	emit redrawTracks();
}


void EffectStackDialog::parameterChanged(int)
{
	// A parameter was changed, sync the clip's effect with the spin box values
	uint parameterNum = 0;
	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
	while (effect->parameter(parameterNum)) {
		QString widgetName = QString("param");
		widgetName.append(QString::number(parameterNum));
		if (effect->effectDescription().parameter(parameterNum)->type() == "constant") {
			QSpinBox *sbox = dynamic_cast<QSpinBox*> (m_parameter->child(widgetName.ascii(), "QSpinBox"));
			if (!sbox) kdWarning()<<"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "<<parameterNum<<endl;
			else effect->effectDescription().parameter(parameterNum)->setValue(sbox->value());
		}
	parameterNum++;
}

emit generateSceneList();
}

void EffectStackDialog::slotSetEffectStack(DocClipRef *clip)
{
	// remove all previous params
	if (m_parameter->child("container","QVBox")) 
		delete m_parameter->child("container","QVBox");

	m_effectList->setEffectStack(clip);
}

} // namespace Gui
