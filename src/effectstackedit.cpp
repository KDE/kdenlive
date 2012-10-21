/***************************************************************************
                          effecstackedit.cpp  -  description
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

#include "effectstackedit.h"
#include "effectstack/effectstackview2.h"
#include "effectslist.h"
#include "monitor.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KLocale>
#include <KFileDialog>
#include <KComboBox>

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QProgressBar>
#include <QWheelEvent>

// For QDomNode debugging (output into files); leaving here as sample code.
//#define DEBUG_ESE


EffectStackEdit::EffectStackEdit(Monitor *monitor, QWidget *parent) :
    QScrollArea(parent),
    m_paramWidget(NULL)
{
    m_baseWidget = new QWidget(this);
    m_metaInfo.monitor = monitor;
    m_metaInfo.trackMode = false;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
    
    setStyleSheet(EffectStackView2::getStyleSheet());
    setWidget(m_baseWidget);   
    /*m_vbox = new QVBoxLayout(m_baseWidget);
    m_vbox->setContentsMargins(0, 0, 0, 0);
    m_vbox->setSpacing(2);    */
    setWidgetResizable(true);
}

EffectStackEdit::~EffectStackEdit()
{
    delete m_baseWidget;
}

Monitor *EffectStackEdit::monitor()
{
    return m_metaInfo.monitor;
}

void EffectStackEdit::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    m_metaInfo.profile = profile;
    m_metaInfo.timecode = t;
}

void EffectStackEdit::setFrameSize(QPoint p)
{
    m_metaInfo.frameSize = p;
}

void EffectStackEdit::updateTimecodeFormat()
{
    if (m_paramWidget) m_paramWidget->updateTimecodeFormat();
}

void EffectStackEdit::updateParameter(const QString &name, const QString &value)
{
    m_paramWidget->updateParameter(name, value);

    if (name == "disable") {
        // if effect is disabled, disable parameters widget
        bool enabled = value.toInt() == 0 || !KdenliveSettings::disable_effect_parameters();
        setEnabled(enabled);
        emit effectStateChanged(enabled);
    }
}

bool EffectStackEdit::eventFilter( QObject * o, QEvent * e ) 
{
    if (e->type() == QEvent::Wheel) {
	QWheelEvent *we = static_cast<QWheelEvent *>(e);
	bool filterWheel = verticalScrollBar() && verticalScrollBar()->isVisible();
	if (!filterWheel || we->modifiers() != Qt::NoModifier) {
	    e->accept();
	    return false;
	}
	if (qobject_cast<QAbstractSpinBox*>(o)) {
	    if(qobject_cast<QAbstractSpinBox*>(o)->focusPolicy() == Qt::WheelFocus)
	    {
		e->accept();
		return false;
	    }
	    else
	    {
		e->ignore();
		return true;
	    }
	}
	if (qobject_cast<KComboBox*>(o)) {
	    if(qobject_cast<KComboBox*>(o)->focusPolicy() == Qt::WheelFocus)
	    {
		e->accept();
		return false;
	    }
	    else
	    {
		e->ignore();
		return true;
	    }
	}
	if (qobject_cast<QProgressBar*>(o)) {
	    if(qobject_cast<QProgressBar*>(o)->focusPolicy() == Qt::WheelFocus)
	    {
		e->accept();
		return false;
	    }
	    else
	    {
		e->ignore();
		return true;
	    }
	}
    }
    return QWidget::eventFilter(o, e);
}

void EffectStackEdit::transferParamDesc(const QDomElement &d, ItemInfo info, bool /*isEffect*/)
{
    if (m_paramWidget) delete m_paramWidget;
    m_paramWidget = new ParameterContainer(d, info, &m_metaInfo, m_baseWidget);
    connect (m_paramWidget, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)), this, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)));
    
    connect(m_paramWidget, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QStringList)), this, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QStringList)));
    
    connect (this, SIGNAL(syncEffectsPos(int)), m_paramWidget, SIGNAL(syncEffectsPos(int)));
    connect (m_paramWidget, SIGNAL(checkMonitorPosition(int)), this, SIGNAL(checkMonitorPosition(int)));
    connect (m_paramWidget, SIGNAL(seekTimeline(int)), this, SIGNAL(seekTimeline(int)));
    connect (m_paramWidget, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));
    
    Q_FOREACH( QSpinBox * sp, m_baseWidget->findChildren<QSpinBox*>() ) {
        sp->installEventFilter( this );
        sp->setFocusPolicy( Qt::StrongFocus );
    }
    Q_FOREACH( KComboBox * cb, m_baseWidget->findChildren<KComboBox*>() ) {
	cb->installEventFilter( this );
        cb->setFocusPolicy( Qt::StrongFocus );
    }
    Q_FOREACH( QProgressBar * cb, m_baseWidget->findChildren<QProgressBar*>() ) {
	cb->installEventFilter( this );
        cb->setFocusPolicy( Qt::StrongFocus );
    }
}

void EffectStackEdit::slotSyncEffectsPos(int pos)
{
    emit syncEffectsPos(pos);
}

bool EffectStackEdit::needsMonitorEffectScene() const
{
    if (!m_paramWidget) return false;
    return m_paramWidget->needsMonitorEffectScene();
}

void EffectStackEdit::setKeyframes(const QString &data)
{
    if (!m_paramWidget) return;
    m_paramWidget->setKeyframes(data);
}

