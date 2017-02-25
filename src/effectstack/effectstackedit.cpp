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
#include "effectstackview2.h"

#include "monitor/monitor.h"
#include "effectslist/effectslist.h"
#include "kdenlivesettings.h"

#include <KComboBox>

#include <QVBoxLayout>
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
    m_paramWidget(nullptr)
{
    m_baseWidget = new QWidget(this);
    m_metaInfo.monitor = monitor;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
    updatePalette();
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

void EffectStackEdit::updatePalette()
{
    setStyleSheet(QString());
    setPalette(qApp->palette());
    setStyleSheet(EffectStackView2::getStyleSheet());
}

Monitor *EffectStackEdit::monitor()
{
    return m_metaInfo.monitor;
}

void EffectStackEdit::setFrameSize(const QPoint &p)
{
    m_metaInfo.frameSize = p;
}

void EffectStackEdit::updateTimecodeFormat()
{
    if (m_paramWidget) {
        m_paramWidget->updateTimecodeFormat();
    }
}

void EffectStackEdit::updateParameter(const QString &name, const QString &value)
{
    m_paramWidget->updateParameter(name, value);

    if (name == QLatin1String("disable")) {
        // if effect is disabled, disable parameters widget
        bool enabled = value.toInt() == 0 || !KdenliveSettings::disable_effect_parameters();
        setEnabled(enabled);
        emit effectStateChanged(enabled);
    }
}

bool EffectStackEdit::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent *>(e);
        bool filterWheel = verticalScrollBar() && verticalScrollBar()->isVisible();
        if (!filterWheel || we->modifiers() != Qt::NoModifier) {
            e->accept();
            return false;
        }
        if (qobject_cast<QAbstractSpinBox *>(o)) {
            if (qobject_cast<QAbstractSpinBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            } else {
                e->ignore();
                return true;
            }
        }
        if (qobject_cast<KComboBox *>(o)) {
            if (qobject_cast<KComboBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            } else {
                e->ignore();
                return true;
            }
        }
        if (qobject_cast<QProgressBar *>(o)) {
            if (qobject_cast<QProgressBar *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            } else {
                e->ignore();
                return true;
            }
        }
    }
    return QWidget::eventFilter(o, e);
}

void EffectStackEdit::transferParamDesc(const QDomElement &d, const ItemInfo &info, bool /*isEffect*/)
{
    delete m_paramWidget;
    m_paramWidget = new ParameterContainer(d, info, &m_metaInfo, m_baseWidget);
    connect(m_paramWidget, &ParameterContainer::parameterChanged, this, &EffectStackEdit::parameterChanged);
    connect(m_paramWidget, &ParameterContainer::importKeyframes, this, &EffectStackEdit::importKeyframes);
    connect(m_paramWidget, &ParameterContainer::startFilterJob, this, &EffectStackEdit::startFilterJob);

    connect(this, &EffectStackEdit::syncEffectsPos, m_paramWidget, &ParameterContainer::syncEffectsPos);
    connect(this, &EffectStackEdit::initScene, m_paramWidget, &ParameterContainer::initScene);
    connect(m_paramWidget, &ParameterContainer::checkMonitorPosition, this, &EffectStackEdit::checkMonitorPosition);
    connect(m_paramWidget, &ParameterContainer::seekTimeline, this, &EffectStackEdit::seekTimeline);
    connect(m_paramWidget, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));

    Q_FOREACH (QSpinBox *sp, m_baseWidget->findChildren<QSpinBox *>()) {
        sp->installEventFilter(this);
        sp->setFocusPolicy(Qt::StrongFocus);
    }
    Q_FOREACH (KComboBox *cb, m_baseWidget->findChildren<KComboBox *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    Q_FOREACH (QProgressBar *cb, m_baseWidget->findChildren<QProgressBar *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    m_paramWidget->connectMonitor(true);
}

void EffectStackEdit::slotSyncEffectsPos(int pos)
{
    emit syncEffectsPos(pos);
}

void EffectStackEdit::initEffectScene(int pos)
{
    emit initScene(pos);
}

MonitorSceneType EffectStackEdit::needsMonitorEffectScene() const
{
    if (!m_paramWidget) {
        return MonitorSceneDefault;
    }
    return m_paramWidget->needsMonitorEffectScene();
}

void EffectStackEdit::setKeyframes(const QString &tag, const QString &keyframes)
{
    if (!m_paramWidget) {
        return;
    }
    m_paramWidget->setKeyframes(tag, keyframes);
}

bool EffectStackEdit::doesAcceptDrops() const
{
    if (!m_paramWidget) {
        return false;
    }
    return m_paramWidget->doesAcceptDrops();
}

void EffectStackEdit::importKeyframes(const QString &kf)
{
    QMap<QString, QString> keyframes;
    if (kf.contains(QLatin1Char('\n'))) {
        const QStringList params = kf.split(QLatin1Char('\n'), QString::SkipEmptyParts);
        for (const QString &param : params) {
            keyframes.insert(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
        }
    } else {
        keyframes.insert(kf.section(QLatin1Char('='), 0, 0), kf.section(QLatin1Char('='), 1));
    }
    emit importClipKeyframes(TransitionWidget, keyframes);
}
