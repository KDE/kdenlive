/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "dragvalue.h"

#include "kdenlivesettings.h"

#include <cmath>

#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>

#include <QFontDatabase>
#include <QStyle>
#include <klocalizedstring.h>

DragValue::DragValue(const QString &label, double defaultValue, int decimals, double min, double max, int id, const QString &suffix, bool showSlider, bool oddOnly, 
                     QWidget *parent)
    : QWidget(parent)
    , m_maximum(max)
    , m_minimum(min)
    , m_decimals(decimals)
    , m_default(defaultValue)
    , m_id(id)
    , m_intEdit(nullptr)
    , m_doubleEdit(nullptr)
{
    if (showSlider) {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    } else {
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    }
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFocusPolicy(Qt::StrongFocus);

    auto *l = new QHBoxLayout;
    l->setSpacing(0);
    l->setContentsMargins(0, 0, 0, 0);
    m_label = new CustomLabel(label, showSlider, m_maximum - m_minimum, this);
    l->addWidget(m_label);
    setMinimumHeight(m_label->sizeHint().height());
    if (decimals == 0) {
        m_label->setMaximum(max - min);
        m_label->setStep(oddOnly ? 2 : 1);
        m_intEdit = new QSpinBox(this);
        m_intEdit->setObjectName(QStringLiteral("dragBox"));
        m_intEdit->setFocusPolicy(Qt::StrongFocus);
        if (!suffix.isEmpty()) {
            m_intEdit->setSuffix(suffix);
        }
        m_intEdit->setKeyboardTracking(false);
        m_intEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_intEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_intEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        m_intEdit->setRange((int)m_minimum, (int)m_maximum);
        m_intEdit->setValue((int)m_default);
        if (oddOnly) {
            m_intEdit->setSingleStep(2);
        }
        l->addWidget(m_intEdit);
        connect(m_intEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
                static_cast<void (DragValue::*)(int)>(&DragValue::slotSetValue));
        connect(m_intEdit, &QAbstractSpinBox::editingFinished, this, &DragValue::slotEditingFinished);
    } else {
        m_doubleEdit = new QDoubleSpinBox(this);
        m_doubleEdit->setDecimals(decimals);
        m_doubleEdit->setFocusPolicy(Qt::StrongFocus);
        m_doubleEdit->setObjectName(QStringLiteral("dragBox"));
        if (!suffix.isEmpty()) {
            m_doubleEdit->setSuffix(suffix);
        }
        m_doubleEdit->setKeyboardTracking(false);
        m_doubleEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_doubleEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_doubleEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        m_doubleEdit->setRange(m_minimum, m_maximum);
        double factor = 100;
        if (m_maximum - m_minimum > 10000) {
            factor = 1000;
        }
        m_label->setStep(1);
        m_doubleEdit->setSingleStep((m_maximum - m_minimum) / factor);
        l->addWidget(m_doubleEdit);
        m_doubleEdit->setValue(m_default);
        connect(m_doubleEdit, SIGNAL(valueChanged(double)), this, SLOT(slotSetValue(double)));
        connect(m_doubleEdit, &QAbstractSpinBox::editingFinished, this, &DragValue::slotEditingFinished);
    }
    connect(m_label, SIGNAL(valueChanged(double,bool)), this, SLOT(setValueFromProgress(double,bool)));
    connect(m_label, &CustomLabel::resetValue, this, &DragValue::slotReset);
    setLayout(l);
    if (m_intEdit) {
        m_label->setMaximumHeight(m_intEdit->sizeHint().height());
    } else {
        m_label->setMaximumHeight(m_doubleEdit->sizeHint().height());
    }

    m_menu = new QMenu(this);

    m_scale = new KSelectAction(i18n("Scaling"), this);
    m_scale->addAction(i18n("Normal scale"));
    m_scale->addAction(i18n("Pixel scale"));
    m_scale->addAction(i18n("Nonlinear scale"));
    m_scale->setCurrentItem(KdenliveSettings::dragvalue_mode());
    m_menu->addAction(m_scale);

    m_directUpdate = new QAction(i18n("Direct update"), this);
    m_directUpdate->setCheckable(true);
    m_directUpdate->setChecked(KdenliveSettings::dragvalue_directupdate());
    m_menu->addAction(m_directUpdate);

    QAction *reset = new QAction(QIcon::fromTheme(QStringLiteral("edit-undo")), i18n("Reset value"), this);
    connect(reset, &QAction::triggered, this, &DragValue::slotReset);
    m_menu->addAction(reset);

    if (m_id > -1) {
        QAction *timeline = new QAction(QIcon::fromTheme(QStringLiteral("go-jump")), i18n("Show %1 in timeline", label), this);
        connect(timeline, &QAction::triggered, this, &DragValue::slotSetInTimeline);
        connect(m_label, &CustomLabel::setInTimeline, this, &DragValue::slotSetInTimeline);
        m_menu->addAction(timeline);
    }
    connect(this, &QWidget::customContextMenuRequested, this, &DragValue::slotShowContextMenu);
    connect(m_scale, static_cast<void (KSelectAction::*)(int)>(&KSelectAction::triggered), this, &DragValue::slotSetScaleMode);
    connect(m_directUpdate, &QAction::triggered, this, &DragValue::slotSetDirectUpdate);
}

DragValue::~DragValue()
{
    delete m_intEdit;
    delete m_doubleEdit;
    delete m_menu;
    delete m_label;
    // delete m_scale;
    // delete m_directUpdate;
}

bool DragValue::hasEditFocus() const
{
    QWidget *fWidget = QApplication::focusWidget();
    return ((fWidget != nullptr) && fWidget->parentWidget() == this);
}

int DragValue::spinSize()
{
    if (m_intEdit) {
        return m_intEdit->sizeHint().width();
    }
    return m_doubleEdit->sizeHint().width();
}

void DragValue::setSpinSize(int width)
{
    if (m_intEdit) {
        m_intEdit->setMinimumWidth(width);
    } else {
        m_doubleEdit->setMinimumWidth(width);
    }
}

void DragValue::slotSetInTimeline()
{
    emit inTimeline(m_id);
}

int DragValue::precision() const
{
    return m_decimals;
}

qreal DragValue::maximum() const
{
    return m_maximum;
}

qreal DragValue::minimum() const
{
    return m_minimum;
}

qreal DragValue::value() const
{
    if (m_intEdit) {
        return m_intEdit->value();
    }
    return m_doubleEdit->value();
}

void DragValue::setMaximum(qreal max)
{
    if (!qFuzzyCompare(m_maximum, max)) {
        m_maximum = max;
        if (m_intEdit) {
            m_intEdit->setRange(m_minimum, m_maximum);
        } else {
            m_doubleEdit->setRange(m_minimum, m_maximum);
        }
    }
}

void DragValue::setMinimum(qreal min)
{
    if (!qFuzzyCompare(m_minimum, min)) {
        m_minimum = min;
        if (m_intEdit) {
            m_intEdit->setRange(m_minimum, m_maximum);
        } else {
            m_doubleEdit->setRange(m_minimum, m_maximum);
        }
    }
}

void DragValue::setRange(qreal min, qreal max)
{
    m_maximum = max;
    m_minimum = min;
    if (m_intEdit) {
        m_intEdit->setRange(m_minimum, m_maximum);
    } else {
        m_doubleEdit->setRange(m_minimum, m_maximum);
    }
}

void DragValue::setStep(qreal step)
{
    if (m_intEdit) {
        m_intEdit->setSingleStep(step);
    } else {
        m_doubleEdit->setSingleStep(step);
    }
}

void DragValue::slotReset()
{
    if (m_intEdit) {
        m_intEdit->blockSignals(true);
        m_intEdit->setValue(m_default);
        m_intEdit->blockSignals(false);
        emit valueChanged((int)m_default, true);
    } else {
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->setValue(m_default);
        m_doubleEdit->blockSignals(false);
        emit valueChanged(m_default, true);
    }
    m_label->setProgressValue((m_default - m_minimum) / (m_maximum - m_minimum) * m_label->maximum());
}

void DragValue::slotSetValue(int value)
{
    setValue(value, true);
}

void DragValue::slotSetValue(double value)
{
    setValue(value, true);
}

void DragValue::setValueFromProgress(double value, bool final)
{
    value = m_minimum + value * (m_maximum - m_minimum) / m_label->maximum();
    if (m_decimals == 0) {
        setValue(qRound(value), final);
    } else {
        setValue(value, final);
    }
}

void DragValue::setValue(double value, bool final)
{
    value = qBound(m_minimum, value, m_maximum);
    if (m_intEdit && m_intEdit->singleStep() > 1) {
        int div = (value - m_minimum) / m_intEdit->singleStep();
        value = m_minimum + (div * m_intEdit->singleStep());
    }
    m_label->setProgressValue((value - m_minimum) / (m_maximum - m_minimum) * m_label->maximum());
    if (m_intEdit) {
        m_intEdit->blockSignals(true);
        m_intEdit->setValue((int)value);
        m_intEdit->blockSignals(false);
        emit valueChanged((int)value, final);
    } else {
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->setValue(value);
        m_doubleEdit->blockSignals(false);
        emit valueChanged(value, final);
    }
}

void DragValue::focusOutEvent(QFocusEvent *)
{
    if (m_intEdit) {
        m_intEdit->setFocusPolicy(Qt::StrongFocus);
    } else {
        m_doubleEdit->setFocusPolicy(Qt::StrongFocus);
    }
}

void DragValue::focusInEvent(QFocusEvent *e)
{
    if (m_intEdit) {
        m_intEdit->setFocusPolicy(Qt::WheelFocus);
    } else {
        m_doubleEdit->setFocusPolicy(Qt::WheelFocus);
    }

    if (e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason) {
        if (m_intEdit) {
            m_intEdit->setFocus(e->reason());
        } else {
            m_doubleEdit->setFocus(e->reason());
        }
    } else {
        QWidget::focusInEvent(e);
    }
}

void DragValue::slotEditingFinished()
{
    if (m_intEdit) {
        int newValue = m_intEdit->value();
        m_intEdit->blockSignals(true);
        if (m_intEdit->singleStep() > 1) {
            int div = (newValue - m_minimum) / m_intEdit->singleStep();
            newValue = m_minimum + (div * m_intEdit->singleStep());
            m_intEdit->setValue(newValue);
        }
        m_intEdit->clearFocus();
        m_intEdit->blockSignals(false);
        if (!KdenliveSettings::dragvalue_directupdate()) {
            emit valueChanged((double)newValue, true);
        }
    } else {
        double value = m_doubleEdit->value();
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->clearFocus();
        m_doubleEdit->blockSignals(false);
        if (!KdenliveSettings::dragvalue_directupdate()) {
            emit valueChanged(value, true);
        }
    }
}

void DragValue::slotShowContextMenu(const QPoint &pos)
{
    // values might have been changed by another object of this class
    m_scale->setCurrentItem(KdenliveSettings::dragvalue_mode());
    m_directUpdate->setChecked(KdenliveSettings::dragvalue_directupdate());
    m_menu->exec(mapToGlobal(pos));
}

void DragValue::slotSetScaleMode(int mode)
{
    KdenliveSettings::setDragvalue_mode(mode);
}

void DragValue::slotSetDirectUpdate(bool directUpdate)
{
    KdenliveSettings::setDragvalue_directupdate(directUpdate);
}

void DragValue::setInTimelineProperty(bool intimeline)
{
    if (m_label->property("inTimeline").toBool() == intimeline) {
        return;
    }
    m_label->setProperty("inTimeline", intimeline);
    style()->unpolish(m_label);
    style()->polish(m_label);
    m_label->update();
    if (m_intEdit) {
        m_intEdit->setProperty("inTimeline", intimeline);
        style()->unpolish(m_intEdit);
        style()->polish(m_intEdit);
        m_intEdit->update();
    } else {
        m_doubleEdit->setProperty("inTimeline", intimeline);
        style()->unpolish(m_doubleEdit);
        style()->polish(m_doubleEdit);
        m_doubleEdit->update();
    }
}

CustomLabel::CustomLabel(const QString &label, bool showSlider, int range, QWidget *parent)
    : QProgressBar(parent)
    , m_dragMode(false)
    , m_showSlider(showSlider)
    , m_step(10.0)
// m_precision(pow(10, precision)),
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setFormat(QLatin1Char(' ') + label);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    if (showSlider) {
        setRange(0, 1000);
    } else {
        setRange(0, range);
        QSize sh;
        const QFontMetrics &fm = fontMetrics();
        sh.setWidth(fm.horizontalAdvance(QLatin1Char(' ') + label + QLatin1Char(' ')));
        setMaximumWidth(sh.width());
        setObjectName(QStringLiteral("dragOnly"));
    }
    setValue(0);
}

void CustomLabel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStartPosition = m_dragLastPosition = e->pos();
        e->accept();
    } else if (e->button() == Qt::MidButton) {
        emit resetValue();
        m_dragStartPosition = QPoint(-1, -1);
    } else {
        QWidget::mousePressEvent(e);
    }
}

void CustomLabel::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragStartPosition != QPoint(-1, -1)) {
        if (!m_dragMode && (e->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragMode = true;
            m_dragLastPosition = e->pos();
            e->accept();
            return;
        }
        if (m_dragMode) {
            if (KdenliveSettings::dragvalue_mode() > 0 || !m_showSlider) {
                int diff = e->x() - m_dragLastPosition.x();

                if (e->modifiers() == Qt::ControlModifier) {
                    diff *= 2;
                } else if (e->modifiers() == Qt::ShiftModifier) {
                    diff /= 2;
                }
                if (KdenliveSettings::dragvalue_mode() == 2) {
                    diff = (diff > 0 ? 1 : -1) * pow(diff, 2);
                }

                double nv = value() + diff * m_step;
                if (!qFuzzyCompare(nv, value())) {
                    setNewValue(nv, KdenliveSettings::dragvalue_directupdate());
                }
            } else {
                double nv = minimum() + ((double)maximum() - minimum()) / width() * e->pos().x();
                if (!qFuzzyCompare(nv, value())) {
                    if (m_step > 1) {
                        int current = (int) value();
                        int diff = (nv - current) / m_step;
                        setNewValue(current + diff * m_step, true);
                    } else {
                        setNewValue(nv, KdenliveSettings::dragvalue_directupdate());
                    }
                }
            }
            m_dragLastPosition = e->pos();
            e->accept();
        }
    } else {
        QWidget::mouseMoveEvent(e);
    }
}

void CustomLabel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MidButton) {
        e->accept();
        return;
    }
    if (e->modifiers() == Qt::ControlModifier) {
        emit setInTimeline();
        e->accept();
        return;
    }
    if (m_dragMode) {
        setNewValue(value(), true);
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    } else if (m_showSlider) {
        if (m_step > 1) {
            int current = (int) value();
            int newVal = (double)maximum() * e->pos().x() / width();
            int diff = (newVal - current) / m_step;
            setNewValue(current + diff * m_step, true);
        } else {
            setNewValue((double)maximum() * e->pos().x() / width(), true);
        }
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    }
    m_dragMode = false;
}

void CustomLabel::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0) {
        if (e->modifiers() == Qt::ControlModifier) {
            slotValueInc(10);
        } else if (e->modifiers() == Qt::AltModifier) {
            slotValueInc(0.1);
        } else {
            slotValueInc();
        }
    } else {
        if (e->modifiers() == Qt::ControlModifier) {
            slotValueDec(10);
        } else if (e->modifiers() == Qt::AltModifier) {
            slotValueDec(0.1);
        } else {
            slotValueDec();
        }
    }
    e->accept();
}

void CustomLabel::slotValueInc(double factor)
{
    setNewValue(value() + m_step * factor, true);
}

void CustomLabel::slotValueDec(double factor)
{
    setNewValue(value() - m_step * factor, true);
}

void CustomLabel::setProgressValue(double value)
{
    setValue(qRound(value));
}

void CustomLabel::setNewValue(double value, bool update)
{
    setValue(qRound(value));
    emit valueChanged(qRound(value), update);
}

void CustomLabel::setStep(double step)
{
    m_step = step;
}

void CustomLabel::focusInEvent(QFocusEvent *)
{
    setFocusPolicy(Qt::WheelFocus);
}

void CustomLabel::focusOutEvent(QFocusEvent *)
{
    setFocusPolicy(Qt::StrongFocus);
}
