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
#include <qfont.h>
#include <qtabwidget.h>
#include <qtooltip.h>
#include <qslider.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <kpushbutton.h>
#include <klocale.h>

#include <effectstacklistview.h>

#include "docclipref.h"
#include "effect.h"
#include "effectdesc.h"
#include "effectparamdesc.h"
#include "effectparameter.h"
#include "effectkeyframe.h"
#include "effectdoublekeyframe.h"

namespace Gui
{

EffectStackDialog::EffectStackDialog(KdenliveApp *app, KdenliveDoc *doc, QWidget *parent, const char *name )
 : EffectStackDialog_UI(parent, name)
{
	// Use a smaller font for that dialog, it has many infos on it. Maybe it's not a good idea?
	QFont dialogFont = font();
	dialogFont.setPointSize(dialogFont.pointSize()-1);
	setFont(dialogFont);
	KIconLoader loader;

	m_upButton->setIconSet( QIconSet( loader.loadIcon( "1uparrow", KIcon::Toolbar ) ));
	m_downButton->setIconSet( QIconSet( loader.loadIcon( "1downarrow", KIcon::Toolbar ) ));
	m_resetButton->setIconSet( QIconSet( loader.loadIcon( "reload", KIcon::Toolbar ) ));
	m_deleteButton->setIconSet( QIconSet( loader.loadIcon( "editdelete", KIcon::Toolbar ) ));

   	QToolTip::add( m_upButton, i18n("Move effect up") );
	QToolTip::add( m_downButton, i18n("Move effect down") );
	QToolTip::add( m_resetButton, i18n("Reset all parameters to default values") );
	QToolTip::add( m_deleteButton, i18n("Remove effect") );

	// HACK - We are setting app and doc here because we cannot pass app and doc directly via the auto-generated UI file. This
	// needs to be fixed...
	m_effectList->setAppAndDoc(app, doc);

	m_parameter->setOrientation(Qt::Vertical);
	m_parameter->setColumns(5);
	m_blockUpdate = false;
	m_hasKeyFrames = false;

	connect(m_upButton, SIGNAL(clicked()), m_effectList, SLOT(slotMoveEffectUp()));
	connect(m_downButton, SIGNAL(clicked()), m_effectList, SLOT(slotMoveEffectDown()));
	connect(m_resetButton, SIGNAL(clicked()), this, SLOT(resetParameters()));
	connect(m_deleteButton, SIGNAL(clicked()), m_effectList, SLOT(slotDeleteEffect()));
	connect(spinIndex, SIGNAL(valueChanged(int)), this, SLOT(selectKeyFrame(int)));
	connect(m_effectList, SIGNAL(effectSelected(DocClipRef *, Effect *)), this, SLOT(addParameters(DocClipRef *, Effect *)));

	connect(sliderPosition, SIGNAL(valueChanged(int)), spinPosition, SLOT(setValue(int)));
	connect(sliderValue, SIGNAL(valueChanged(int)), spinValue, SLOT(setValue(int)));
	connect(spinPosition, SIGNAL(valueChanged(int)), sliderPosition, SLOT(setValue(int)));
	connect(spinValue, SIGNAL(valueChanged(int)), sliderValue, SLOT(setValue(int)));
	connect(spinPosition, SIGNAL(valueChanged(int)), this, SLOT(changeKeyFramePosition(int)));
	connect(spinValue, SIGNAL(valueChanged(int)), this, SLOT(changeKeyFrameValue(int)));
}


EffectStackDialog::~EffectStackDialog()
{
}

void EffectStackDialog::addParameters(DocClipRef *clip, Effect *effect)
{
// Rebuild the effect parameters dialog

	uint parameterNum = 0;
	m_hasKeyFrames = false;
	spinIndex->setValue(0);
	updateKeyFrames();
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
		if (effect->effectDescription().parameter(parameterNum)->type() == "double") {
			spinIndex->setMaxValue(effect->parameter(parameterNum)->numKeyFrames()-1);
			m_hasKeyFrames = true;
			}
		parameterNum++;
	}

	tabWidget2->setTabEnabled(tabWidget2->page(1), m_hasKeyFrames);
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
	if (!m_blockUpdate) emit generateSceneList();
}

void EffectStackDialog::resetParameters()
{
	m_blockUpdate = true;
	uint parameterNum = 0;
	spinIndex->setValue(0);
	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
	while (effect->parameter(parameterNum)) {
		QString widgetName = QString("param");
		widgetName.append(QString::number(parameterNum));

		if (effect->effectDescription().parameter(parameterNum)->type() == "double") {
		uint ix = effect->parameter(parameterNum)->numKeyFrames();
		while (ix>0) {
		effect->parameter(parameterNum)->deleteKeyFrame(ix-1);
		ix--;
		}
		effect->addKeyFrame(0, 0.0);
		effect->addKeyFrame(0, 1.0);
		}
		else if (effect->effectDescription().parameter(parameterNum)->type() == "constant") {
			QSpinBox *sbox = dynamic_cast<QSpinBox*> (m_parameter->child(widgetName.ascii(), "QSpinBox"));
			if (!sbox) kdWarning()<<"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "<<parameterNum<<endl;
			else
			sbox->setValue(effect->effectDescription().parameter(parameterNum)->defaultValue());
		}
		parameterNum++;
	}
	m_blockUpdate = false;
	emit redrawTracks();
	emit generateSceneList();
}


void EffectStackDialog::updateKeyFrames()
{
	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
	uint parameterNum = 0;
	uint numKeyFrames = effect->parameter(parameterNum)->numKeyFrames();
	if (numKeyFrames == 0 || !m_hasKeyFrames) return;
	int ix = effect->parameter(parameterNum)->selectedKeyFrame();
	spinIndex->setValue(ix);
	spinIndex->setMaxValue(numKeyFrames-1);
	if (ix!=-1) {
		spinPosition->setValue(effect->parameter(parameterNum)->keyframe(ix)->time()*m_effectList->clip()->cropDuration().frames(25));
		spinValue->setValue(effect->parameter(parameterNum)->keyframe(ix)->toDoubleKeyFrame()->value());
	}
}


void EffectStackDialog::selectKeyFrame(int ix)
{
	// User selected a keyframe
	if (ix == -1 || !m_hasKeyFrames) return;
	uint parameterNum = 0;
	uint previousTime, nextTime, currentTime;
	uint currentValue;

	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
	effect->parameter(parameterNum)->setSelectedKeyFrame(ix);

	// Find the keyframe value & position
	currentValue = effect->parameter(parameterNum)->keyframe(ix)->toDoubleKeyFrame()->value();
	currentTime = effect->parameter(parameterNum)->keyframe(ix)->time()*m_effectList->clip()->cropDuration().frames(25);

	// Find the previous keyframe position to make sure the current keyframe cannot be moved before the previous one
	if (ix == 0) previousTime = 0; 
	else previousTime = effect->parameter(parameterNum)->keyframe(ix-1)->time()*m_effectList->clip()->cropDuration().frames(25)+1;

	// Find the next keyframe position to make sure the current keyframe cannot be moved after the next one
	if (ix == effect->parameter(parameterNum)->numKeyFrames()-1)
		nextTime = m_effectList->clip()->cropDuration().frames(25);
	else nextTime = effect->parameter(parameterNum)->keyframe(ix+1)->time()*m_effectList->clip()->cropDuration().frames(25)-1;

	spinPosition->setMinValue(previousTime);
	spinPosition->setMaxValue(nextTime);
	sliderPosition->setMinValue(previousTime);
	sliderPosition->setMaxValue(nextTime);
	spinPosition->setValue(currentTime);
	
	spinValue->setMinValue(0);
	spinValue->setMaxValue(100);
	sliderValue->setMinValue(0);
	sliderValue->setMaxValue(100);
	spinValue->setValue(currentValue);

	emit redrawTracks();
}


void EffectStackDialog::changeKeyFramePosition(int newTime)
{
	uint parameterNum = 0;
	double currentTime;

	currentTime = newTime / m_effectList->clip()->cropDuration().frames(25);
	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());

	int ix = effect->parameter(parameterNum)->selectedKeyFrame();
	effect->parameter(parameterNum)->keyframe(ix)->setTime(currentTime);
	emit redrawTracks();
	emit generateSceneList();
}


void EffectStackDialog::changeKeyFrameValue(int newValue)
{
	uint parameterNum = 0;
	double currentTime;

	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
	int ix = effect->parameter(parameterNum)->selectedKeyFrame();
	effect->parameter(parameterNum)->keyframe(ix)->toDoubleKeyFrame()->setValue(newValue);
	emit redrawTracks();
	emit generateSceneList();
}


void EffectStackDialog::slotSetEffectStack(DocClipRef *clip)
{
	// remove all previous params
	if (m_parameter->child("container","QVBox")) 
		delete m_parameter->child("container","QVBox");
	if (m_keyframes->child("container","QVBox")) 
		delete m_keyframes->child("container","QVBox");

	m_effectList->setEffectStack(clip);
}

} // namespace Gui
