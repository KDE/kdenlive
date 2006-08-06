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
#include <qcheckbox.h>
#include <qlabel.h>
#include <qfont.h>
#include <qtabwidget.h>
#include <qtooltip.h>
#include <qslider.h>
#include <qlayout.h>
#include <qobjectlist.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <kpushbutton.h>
#include <klocale.h>
#include <kcombobox.h>

#include <effectstacklistview.h>

#include "docclipref.h"
#include "effect.h"
#include "effectdesc.h"
#include "effectparamdesc.h"
#include "effectparameter.h"
#include "effectkeyframe.h"
#include "effectdoublekeyframe.h"
#include "effectcomplexkeyframe.h"
#include "kdenlivesettings.h"

namespace Gui {

    EffectStackDialog::EffectStackDialog(KdenliveApp * app,
	KdenliveDoc * doc, QWidget * parent, const char *name)
    :EffectStackDialog_UI(parent, name), m_container(NULL), m_frame(NULL), m_app(app) {
	// Use a smaller font for that dialog, it has many infos on it. Maybe it's not a good idea?
	QFont dialogFont = font();
	 dialogFont.setPointSize(dialogFont.pointSize() - 1);
	 setFont(dialogFont);
	KIconLoader loader;

	 m_upButton->setIconSet(QIconSet(loader.loadIcon("1uparrow",
		    KIcon::Toolbar)));
	 m_downButton->setIconSet(QIconSet(loader.loadIcon("1downarrow",
		    KIcon::Toolbar)));
	 m_resetButton->setIconSet(QIconSet(loader.loadIcon("reload",
		    KIcon::Toolbar)));
	 m_deleteButton->setIconSet(QIconSet(loader.loadIcon("editdelete",
		    KIcon::Toolbar)));

	 QToolTip::add(m_upButton, i18n("Move effect up"));
	 QToolTip::add(m_downButton, i18n("Move effect down"));
	 QToolTip::add(m_resetButton,
	    i18n("Reset all parameters to default values"));
	 QToolTip::add(m_deleteButton, i18n("Remove effect"));

	// HACK - We are setting app and doc here because we cannot pass app and doc directly via the auto-generated UI file. This
	// needs to be fixed...
	 m_effectList->setAppAndDoc(app, doc);

	 m_parameter->setOrientation(Qt::Vertical);
	 m_parameter->setColumns(5);
	 m_blockUpdate = false;
	 m_effecttype = "";
	 m_hasKeyFrames = false;
	 tabWidget2->setTabEnabled(tabWidget2->page(1), false);

	 connect(m_upButton, SIGNAL(clicked()), m_effectList,
	    SLOT(slotMoveEffectUp()));
	 connect(m_downButton, SIGNAL(clicked()), m_effectList,
	    SLOT(slotMoveEffectDown()));
	 connect(m_resetButton, SIGNAL(clicked()), this,
	    SLOT(resetParameters()));
	 connect(m_deleteButton, SIGNAL(clicked()), this,
	    SLOT(slotDeleteEffect()));
	 connect(spinIndex, SIGNAL(valueChanged(int)), this,
	    SLOT(selectKeyFrame(int)));
	 connect(m_effectList, SIGNAL(effectSelected(DocClipRef *,
		    Effect *)), this, SLOT(addParameters(DocClipRef *,
		    Effect *)));

	 connect(sliderPosition, SIGNAL(valueChanged(int)), spinPosition,
	    SLOT(setValue(int)));
	 connect(spinPosition, SIGNAL(valueChanged(int)), sliderPosition,
	    SLOT(setValue(int)));
	 connect(spinPosition, SIGNAL(valueChanged(int)), this,
	    SLOT(changeKeyFramePosition(int)));
	disableButtons();

	/*connect(spinValue, SIGNAL(valueChanged(int)), sliderValue, SLOT(setValue(int)));
	   connect(sliderValue, SIGNAL(valueChanged(int)), spinValue, SLOT(setValue(int)));
	   connect(spinValue, SIGNAL(valueChanged(int)), this, SLOT(changeKeyFrameValue(int))); */
    } EffectStackDialog::~EffectStackDialog() {
    }

    void EffectStackDialog::disableButtons()
    {
	m_upButton->setEnabled(false);
	m_downButton->setEnabled(false);
	m_resetButton->setEnabled(false);
	m_deleteButton->setEnabled(false);
    }

    void EffectStackDialog::enableButtons()
    {
	m_upButton->setEnabled(true);
	m_downButton->setEnabled(true);
	m_resetButton->setEnabled(true);
	m_deleteButton->setEnabled(true);
    }

    void EffectStackDialog::cleanWidgets()
    {/*
	if (m_parameter->child("container", "QGrid"))
	    delete m_parameter->child("container", "QGrid");
	
	if (k_container->child("container2", "QWidget")) 
	    delete k_container->child("container2", "QWidget");*/
	if (m_container) delete m_container;
	if (m_frame) delete m_frame;
	m_container = 0;
	m_frame = 0;
    }



    void EffectStackDialog::addParameters(DocClipRef * clip,
	Effect * effect) {
	// Rebuild the effect parameters dialog
	kdDebug()<<"++++++++++++  REBUILD PARAMETER DIALOG FOR CLIP: "<<clip->name()<<endl;
	uint parameterNum = 0;
	m_hasKeyFrames = false;
        if (!effect->parameter(parameterNum)) {
		disableButtons();
		return;
	}
	enableButtons();

	m_effecttype = effect->effectDescription().parameter(parameterNum)->type();
	spinIndex->setValue(0);
	updateKeyFrames();
	clip->setEffectStackSelectedItem(m_effectList->
	    selectedEffectIndex());

	// remove all previous params
	cleanWidgets();

	//QVBox *container = new QVBox(m_parameter, "container");

	m_container = new QGrid(2, Qt::Horizontal, m_parameter, "container");
	m_container->setSpacing(5);
	m_container->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum));

	while (effect->parameter(parameterNum)) {
	    m_effecttype = effect->effectDescription().parameter(parameterNum)->type();
	    // for each constant parameter, build a QSpinBox with its value
            if (m_effecttype == "constant") {
		int maxValue =
		    effect->effectDescription().parameter(parameterNum)->
		    max();
		int minValue =
		    effect->effectDescription().parameter(parameterNum)->
		    min();
                if (maxValue != minValue) {
		  (void) new QLabel(effect->effectDescription().parameter(parameterNum)->description(), m_container);
		  QString widgetName = QString("param");
		  widgetName.append(QString::number(parameterNum));
		  QSpinBox *spinParam = new QSpinBox(m_container, widgetName.ascii());
		  spinParam->setMaxValue(maxValue);
		  spinParam->setMinValue(minValue);
		  spinParam->setValue(effect->effectDescription().parameter(parameterNum)->value().toInt());
		  connect(spinParam, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
                }
	    }
            else if (m_effecttype == "bool") {
		  (void) new QLabel(effect->effectDescription().parameter(parameterNum)->description(), m_container);
		  QString widgetName = QString("param");
		  widgetName.append(QString::number(parameterNum));
		  QCheckBox *checkParam = new QCheckBox(m_container, widgetName.ascii());
		  int value = effect->effectDescription().parameter(parameterNum)->value().toInt();
		  kdDebug()<<" ++++  VALUE FOR BOOL PARAM: "<<effect->effectDescription().parameter(parameterNum)->description()<<" IS: "<<value<<endl;
		  if (value == 1) checkParam->setChecked(true);
		  else checkParam->setChecked(false);
		  connect(checkParam, SIGNAL(toggled(bool)), this, SLOT(parameterChanged()));
	    }
	    else if (m_effecttype == "list") {
		QStringList list = QStringList::split(",", 
		    effect->effectDescription().parameter(parameterNum)->
		    list());

		  // build combobox

		  (void) new QLabel(effect->effectDescription().parameter(parameterNum)->description(), m_container);
		  QString widgetName = QString("param");
		  widgetName.append(QString::number(parameterNum));
		  KComboBox *comboParam = new KComboBox(m_container, widgetName.ascii());
		  comboParam->insertStringList(list);
		  comboParam->setCurrentItem(effect->effectDescription().parameter(parameterNum)->value());
		  connect(comboParam, SIGNAL(activated(int)), this, SLOT(parameterChanged()));
                
	    }
	    else if (m_effecttype == "double") {
		m_frame = new QFrame(k_container, "container2");
		m_frame->setSizePolicy(QSizePolicy::MinimumExpanding,
		    QSizePolicy::MinimumExpanding);
		QGridLayout *grid = new QGridLayout(m_frame, 3, 0, 0, 5);
		spinIndex->setMaxValue(effect->parameter(parameterNum)->
		    numKeyFrames() - 1);
		int ix =
		    effect->parameter(parameterNum)->selectedKeyFrame();
		uint maxVal = 100;
		uint minVal = 0;
		uint currVal =
		    effect->parameter(parameterNum)->keyframe(ix)->
		    toDoubleKeyFrame()->value();
		QLabel *label =
		    new QLabel(effect->effectDescription().
		    parameter(parameterNum)->name(), m_frame);
		QSlider *sliderParam = new QSlider(Qt::Horizontal, m_frame);
		sliderParam->setRange(minVal, maxVal);
		QSpinBox *spinParam = new QSpinBox(m_frame, "value");

		grid->addWidget(label, 0, 0);
		grid->addWidget(sliderParam, 0, 1);
		grid->addWidget(spinParam, 0, 2);

		connect(sliderParam, SIGNAL(valueChanged(int)), spinParam,
		    SLOT(setValue(int)));
		connect(spinParam, SIGNAL(valueChanged(int)), sliderParam,
		    SLOT(setValue(int)));
		spinParam->setMaxValue(maxVal);
		spinParam->setMinValue(minVal);
		spinParam->setValue(currVal);

		connect(spinParam, SIGNAL(valueChanged(int)), this,
		    SLOT(changeKeyFrameValue(int)));
		m_frame->adjustSize();
		m_frame->show();
		m_hasKeyFrames = true;
	    }

	    else if (m_effecttype == "complex") {
		m_frame = new QFrame(k_container, "container2");
		m_frame->setSizePolicy(QSizePolicy::MinimumExpanding,
		    QSizePolicy::MinimumExpanding);
		QGridLayout *grid = new QGridLayout(m_frame, 3, 0, 0, 5);
		spinIndex->setMaxValue(effect->parameter(parameterNum)->
		    numKeyFrames() - 1);

		uint paramNum =
		    effect->effectDescription().parameter(parameterNum)->
		    complexParamNum();

		for (uint i = 0; i < paramNum; i++) {
		    int ix =
			effect->parameter(parameterNum)->
			selectedKeyFrame();
		    uint maxVal =
			effect->effectDescription().
			parameter(parameterNum)->max(i);
		    uint minVal =
			effect->effectDescription().
			parameter(parameterNum)->min(i);
		    uint currVal =
			effect->parameter(parameterNum)->keyframe(ix)->
			toComplexKeyFrame()->value(i);

		    QLabel *label =
			new QLabel(effect->effectDescription().
			parameter(parameterNum)->complexParamName(i),
			m_frame);
		    QString widgetName = QString("param");
		    widgetName.append(QString::number(i));
		    QSlider *sliderParam =
			new QSlider(Qt::Horizontal, m_frame);
		    sliderParam->setRange(minVal, maxVal);
		    QSpinBox *spinParam =
			new QSpinBox(m_frame, widgetName.ascii());

		    grid->addWidget(label, i, 0);
		    grid->addWidget(sliderParam, i, 1);
		    grid->addWidget(spinParam, i, 2);

		    connect(sliderParam, SIGNAL(valueChanged(int)),
			spinParam, SLOT(setValue(int)));
		    connect(spinParam, SIGNAL(valueChanged(int)),
			sliderParam, SLOT(setValue(int)));

		    spinParam->setMaxValue(maxVal);
		    spinParam->setMinValue(minVal);
		    spinParam->setValue(currVal);

		    connect(spinParam, SIGNAL(valueChanged(int)), this,
			SLOT(changeKeyFrameValue(int)));
		}
		m_frame->adjustSize();
		m_frame->show();
		m_hasKeyFrames = true;
	    }

	    parameterNum++;
	}

	tabWidget2->setTabEnabled(tabWidget2->page(1), m_hasKeyFrames);
	m_container->adjustSize();
	m_container->show();
	emit effectSelected(clip, effect);
	emit redrawTracks();
    }

    void EffectStackDialog::parameterChanged() {
	// A parameter was changed, sync the clip's effect with the spin box values
	uint parameterNum = 0;
	Effect *effect =
	    m_effectList->clip()->effectAt(m_effectList->
	    selectedEffectIndex());
	while (effect->parameter(parameterNum)) {
	    m_effecttype = effect->effectDescription().parameter(parameterNum)->type();
	    QString widgetName = QString("param");
	    widgetName.append(QString::number(parameterNum));
	    if (m_effecttype == "constant") {
		QSpinBox *sbox =
		    dynamic_cast <
		    QSpinBox * >(m_parameter->child(widgetName.ascii(),
			"QSpinBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "
			<< parameterNum << endl;
		else
		    effect->effectDescription().parameter(parameterNum)->
			setValue(QString::number(sbox->value()));
	    }
	    else if (m_effecttype == "bool") {
		QCheckBox *sbox =
		    dynamic_cast <
		    QCheckBox * >(m_parameter->child(widgetName.ascii(),
			"QCheckBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "
			<< parameterNum <<", "<<widgetName.ascii()<< endl;
		else {
		    int value = sbox->isChecked() ? 1 : 0;
		    effect->effectDescription().parameter(parameterNum)->
			setValue(QString::number(value));
		}
	    }
	    else if (m_effecttype == "list") {
		KComboBox *sbox =
		    dynamic_cast <
		    KComboBox * >(m_parameter->child(widgetName.ascii(),
			"KComboBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "
			<< parameterNum << endl;
		else
		    effect->effectDescription().parameter(parameterNum)->
			setValue(sbox->currentText());
	    }		
	    parameterNum++;
	}
	if (!m_blockUpdate) {
	    emit generateSceneList();
 	    m_app->focusTimelineWidget();
	}
    }


    void EffectStackDialog::slotDeleteEffect() {
	// remove all previous params
	cleanWidgets();
	m_effectList->slotDeleteEffect();
    }

    void EffectStackDialog::resetParameters() {
	m_blockUpdate = true;
	uint parameterNum = 0;
	spinIndex->setValue(0);
	Effect *effect =
	    m_effectList->clip()->effectAt(m_effectList->
	    selectedEffectIndex());
	while (effect->parameter(parameterNum)) {
	    m_effecttype = effect->effectDescription().parameter(parameterNum)->type();
	    QString widgetName = QString("param");
	    widgetName.append(QString::number(parameterNum));

	    if (m_effecttype == "double" || m_effecttype == "complex") {
		uint ix = effect->parameter(parameterNum)->numKeyFrames();
		while (ix > 0) {
		    effect->parameter(parameterNum)->deleteKeyFrame(ix -
			1);
		    ix--;
		}
		effect->addKeyFrame(0, 0.0);
		effect->addKeyFrame(0, 1.0);
	    } else if (effect->effectDescription().
		parameter(parameterNum)->type() == "constant") {
		QSpinBox *sbox =
		    dynamic_cast <
		    QSpinBox * >(m_parameter->child(widgetName.ascii(),
			"QSpinBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "
			<< parameterNum << endl;
		else
		    sbox->setValue(effect->effectDescription().
			parameter(parameterNum)->defaultValue().toInt());
	    } else if (effect->effectDescription().
		parameter(parameterNum)->type() == "bool") {
		QCheckBox *sbox =
		    dynamic_cast <
		    QCheckBox * >(m_parameter->child(widgetName.ascii(),
			"QCheckBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "
			<< parameterNum << endl;
		else
		    sbox->setChecked(effect->effectDescription().
			parameter(parameterNum)->defaultValue().toInt());
	    }
	    else if (effect->effectDescription().parameter(parameterNum)->type() == "list") {
		KComboBox *sbox =
		    dynamic_cast <
		    KComboBox * >(m_parameter->child(widgetName.ascii(),
			"KComboBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX FOR PARAMETER "
			<< parameterNum << endl;
		else
		    sbox->setCurrentText(effect->effectDescription().
			parameter(parameterNum)->defaultValue());
	    }
	    parameterNum++;
	}
	m_blockUpdate = false;
	emit redrawTracks();
	parameterChanged();
    }


    void EffectStackDialog::updateKeyFrames() {
	Effect *effect =
	    m_effectList->clip()->effectAt(m_effectList->
	    selectedEffectIndex());
	uint parameterNum = 0;
	uint numKeyFrames = effect->parameter(parameterNum)->numKeyFrames();
	if (numKeyFrames == 0 || !m_hasKeyFrames)
	    return;

	int ix = effect->parameter(parameterNum)->selectedKeyFrame();
        //if (ix > numKeyFrames -1) ix = numKeyFrames -1;
	spinIndex->setValue(ix);
	spinIndex->setMaxValue(numKeyFrames - 1);
	selectKeyFrame(ix);
    }


    void EffectStackDialog::selectKeyFrame(int ix) {
	// User selected a keyframe
	if (ix == -1 || !m_hasKeyFrames)
	    return;
	m_blockUpdate = true;
	uint parameterNum = 0;
	uint previousTime, nextTime, currentTime;
	uint currentValue;

	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
        if (!effect->parameter(parameterNum)) return;
	effect->parameter(parameterNum)->setSelectedKeyFrame(ix);

	m_effecttype = effect->effectDescription().parameter(parameterNum)->type();
	// Find the keyframe value & position
	if (m_effecttype == "double") {
	    currentValue =
		effect->parameter(parameterNum)->keyframe(ix)->
		toDoubleKeyFrame()->value();
	    QSpinBox *sbox =
		dynamic_cast < QSpinBox * >(k_container->child("value",
		    "QSpinBox"));
	    sbox->setValue(currentValue);
	} else if (m_effecttype == "complex") {
	    uint paramNum =
		effect->effectDescription().parameter(parameterNum)->
		complexParamNum();
	    for (uint i = 0; i < paramNum; i++) {
		QString widgetName = QString("param");
		widgetName.append(QString::number(i));
		QSpinBox *sbox =
		    dynamic_cast <
		    QSpinBox * >(k_container->child(widgetName.ascii(),
			"QSpinBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX: " << i
			<< endl;
		else {
		    disconnect(sbox, SIGNAL(valueChanged(int)), this,
			SLOT(changeKeyFrameValue(int)));
		    sbox->setMaxValue(effect->effectDescription().
			parameter(parameterNum)->max(i));
		    sbox->setMinValue(effect->effectDescription().
			parameter(parameterNum)->min(i));
		    sbox->setValue(effect->parameter(parameterNum)->
			keyframe(ix)->toComplexKeyFrame()->value(i));
		    connect(sbox, SIGNAL(valueChanged(int)), this,
			SLOT(changeKeyFrameValue(int)));
		}
	    }
	}

	currentTime =
	    effect->parameter(parameterNum)->keyframe(ix)->time() *
	    m_effectList->clip()->cropDuration().frames(KdenliveSettings::defaultfps());

	// Find the previous keyframe position to make sure the current keyframe cannot be moved before the previous one
	if (ix == 0)
	    previousTime = 0;
	else
	    previousTime =
		effect->parameter(parameterNum)->keyframe(ix -
		1)->time() *
		m_effectList->clip()->cropDuration().frames(KdenliveSettings::defaultfps()) + 1;

	// Find the next keyframe position to make sure the current keyframe cannot be moved after the next one
	if (ix == effect->parameter(parameterNum)->numKeyFrames() - 1)
	    nextTime = m_effectList->clip()->cropDuration().frames(KdenliveSettings::defaultfps());
	else
	    nextTime =
		effect->parameter(parameterNum)->keyframe(ix +
		1)->time() *
		m_effectList->clip()->cropDuration().frames(KdenliveSettings::defaultfps()) - 1;

	spinPosition->setMinValue(previousTime);
	spinPosition->setMaxValue(nextTime);
	sliderPosition->setMinValue(previousTime);
	sliderPosition->setMaxValue(nextTime);
	spinPosition->setValue(currentTime);
	m_blockUpdate = false;
	emit redrawTracks();
    }


    void EffectStackDialog::changeKeyFramePosition(int newTime) {
	uint parameterNum = 0;
	double currentTime;

	currentTime =
	    newTime / m_effectList->clip()->cropDuration().frames(KdenliveSettings::defaultfps());
	Effect *effect =
	    m_effectList->clip()->effectAt(m_effectList->
	    selectedEffectIndex());

	int ix = effect->parameter(parameterNum)->selectedKeyFrame();
	effect->parameter(parameterNum)->keyframe(ix)->
	    setTime(currentTime);

	if (!m_blockUpdate) {
	    emit redrawTracks();
	    emit generateSceneList();
	}
    }


    void EffectStackDialog::changeKeyFrameValue(int newValue) {
	uint parameterNum = 0;
	double currentTime;

	Effect *effect = m_effectList->clip()->effectAt(m_effectList->selectedEffectIndex());
        if (!effect->parameter(parameterNum)) return;
	int ix = effect->parameter(parameterNum)->selectedKeyFrame();
	m_effecttype = effect->effectDescription().parameter(parameterNum)->type();
	if (m_effecttype == "double")
	    effect->parameter(parameterNum)->keyframe(ix)->
		toDoubleKeyFrame()->setValue(newValue);
	else if (m_effecttype == "complex") {
	    uint paramNum = effect->effectDescription().parameter(parameterNum)->complexParamNum();
	    for (uint i = 0; i < paramNum; i++) {
		QString widgetName = QString("param");
		widgetName.append(QString::number(i));
		QSpinBox *sbox =
		    dynamic_cast <
		    QSpinBox * >(k_container->child(widgetName.ascii(),
			"QSpinBox"));
		if (!sbox)
		    kdWarning() <<
			"EFFECTSTACKDIALOG ERROR, CANNOT FIND BOX: " << i
			<< endl;
		else {
		    effect->parameter(parameterNum)->keyframe(ix)->
			toComplexKeyFrame()->setValue(i,
			QString::number(sbox->value()));
		}

	    }
	}

	if (!m_blockUpdate) {
	    emit redrawTracks();
	    emit generateSceneList();
	    m_app->focusTimelineWidget();
	}
    }


    void EffectStackDialog::slotSetEffectStack(DocClipRef * clip) {
	// remove all previous params
	cleanWidgets();
	disableButtons();
	tabWidget2->setTabEnabled(tabWidget2->page(1), false);
	m_effectList->setEffectStack(clip);
    }

}				// namespace Gui
