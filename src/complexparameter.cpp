/***************************************************************************
                          complexparameter.cpp  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "complexparameter.h"

#include <KDebug>
#include <KLocale>


#include <QHeaderView>
#include <QMenu>

ComplexParameter::ComplexParameter(QWidget *parent) :
        QWidget(parent)
{
    m_ui.setupUi(this);
    //m_ui.effectlist->horizontalHeader()->setVisible(false);
    //m_ui.effectlist->verticalHeader()->setVisible(false);


    m_ui.buttonLeftRight->setIcon(KIcon("go-next"));//better icons needed
    m_ui.buttonLeftRight->setToolTip(i18n("Allow horizontal moves"));
    m_ui.buttonUpDown->setIcon(KIcon("go-up"));
    m_ui.buttonUpDown->setToolTip(i18n("Allow vertical moves"));
    m_ui.buttonShowInTimeline->setIcon(KIcon("kmplayer"));
    m_ui.buttonShowInTimeline->setToolTip(i18n("Show keyframes in timeline"));
    m_ui.buttonHelp->setIcon(KIcon("help-about"));
    m_ui.buttonHelp->setToolTip(i18n("Parameter info"));
    m_ui.buttonNewPoints->setIcon(KIcon("document-new"));
    m_ui.buttonNewPoints->setToolTip(i18n("Add keyframe"));

    connect(m_ui.buttonLeftRight, SIGNAL(clicked()), this , SLOT(slotSetMoveX()));
    connect(m_ui.buttonUpDown, SIGNAL(clicked()), this , SLOT(slotSetMoveY()));
    connect(m_ui.buttonShowInTimeline, SIGNAL(clicked()), this , SLOT(slotShowInTimeline()));
    connect(m_ui.buttonNewPoints, SIGNAL(clicked()), this , SLOT(slotSetNew()));
    connect(m_ui.buttonHelp, SIGNAL(clicked()), this , SLOT(slotSetHelp()));
    connect(m_ui.parameterList, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(slotParameterChanged(const QString&)));
    connect(m_ui.kplotwidget, SIGNAL(parameterChanged(QDomElement)), this , SLOT(slotUpdateEffectParams(QDomElement)));
    connect(m_ui.kplotwidget, SIGNAL(parameterList(QStringList)), this , SLOT(slotUpdateParameterList(QStringList)));
    /*ÜeffectLists["audio"]=audioEffectList;
    effectLists["video"]=videoEffectList;
    effectLists["custom"]=customEffectList;*/
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    m_ui.infoBox->hide();
    updateButtonStatus();

}



void ComplexParameter::slotSetMoveX()
{
    m_ui.kplotwidget->setMoveX(!m_ui.kplotwidget->isMoveX());
    updateButtonStatus();
}

void ComplexParameter::slotSetMoveY()
{
    m_ui.kplotwidget->setMoveY(!m_ui.kplotwidget->isMoveY());
    updateButtonStatus();
}

void ComplexParameter::slotSetNew()
{
    m_ui.kplotwidget->setNewPoints(!m_ui.kplotwidget->isNewPoints());
    updateButtonStatus();
}

void ComplexParameter::slotSetHelp()
{
    m_ui.infoBox->setVisible(!m_ui.infoBox->isVisible());
    m_ui.buttonHelp->setDown(m_ui.infoBox->isVisible());
}

void ComplexParameter::slotShowInTimeline()
{

    m_ui.kplotwidget->setMoveTimeLine(!m_ui.kplotwidget->isMoveTimeline());
    updateButtonStatus();

}

void ComplexParameter::updateButtonStatus()
{
    m_ui.buttonLeftRight->setDown(m_ui.kplotwidget->isMoveX());
    m_ui.buttonUpDown->setDown(m_ui.kplotwidget->isMoveY());

    m_ui.buttonShowInTimeline->setEnabled(m_ui.kplotwidget->isMoveX() || m_ui.kplotwidget->isMoveY());
    m_ui.buttonShowInTimeline->setDown(m_ui.kplotwidget->isMoveTimeline());

    m_ui.buttonNewPoints->setEnabled(m_ui.parameterList->currentText() != "all");
    m_ui.buttonNewPoints->setDown(m_ui.kplotwidget->isNewPoints());
}

void ComplexParameter::slotParameterChanged(const QString& text)
{

    //m_ui.buttonNewPoints->setEnabled(text!="all");
    m_ui.kplotwidget->replot(text);
    updateButtonStatus();
}

void ComplexParameter::setupParam(const QDomElement d, const QString& paramName, int from, int to)
{
    m_param = d;
    m_ui.kplotwidget->setPointLists(d, paramName, from, to);
}

void ComplexParameter::itemSelectionChanged()
{
    //kDebug() << "drop";
}

void ComplexParameter::slotUpdateEffectParams(QDomElement e)
{
    m_param = e;
    emit parameterChanged();
}

QDomElement ComplexParameter::getParamDesc()
{
    return m_param;
}

void ComplexParameter::slotUpdateParameterList(QStringList l)
{
    kDebug() << l ;
    m_ui.parameterList->clear();
    m_ui.parameterList->addItem("all");
    m_ui.parameterList->addItems(l);
}

#include "complexparameter.moc"
