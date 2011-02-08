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

#include <math.h>

#include <QHBoxLayout>
#include <QToolButton>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QPainter>
#include <QStyle>

#include <KIcon>
#include <KLocalizedString>
#include <KGlobalSettings>

DragValue::DragValue(const QString &label, int defaultValue, int id, const QString suffix, QWidget* parent) :
        QWidget(parent),
        m_maximum(100),
        m_minimum(0),
        m_precision(2),
        m_default(defaultValue),
        m_id(id)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    
    QHBoxLayout *l = new QHBoxLayout;
    l->setSpacing(0);
    l->setContentsMargins(0, 0, 0, 0);
    m_label = new CustomLabel(label, this);
    m_label->setCursor(Qt::PointingHandCursor);
    m_label->setRange(m_minimum, m_maximum);
    l->addWidget(m_label);
    m_edit = new KIntSpinBox(this);
    if (!suffix.isEmpty()) m_edit->setSuffix(suffix);

    m_edit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_edit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    m_edit->setRange(m_minimum, m_maximum);
    l->addWidget(m_edit);
    connect(m_edit, SIGNAL(valueChanged(int)), this, SLOT(slotSetValue(int)));
    connect(m_label, SIGNAL(valueChanged(int,bool)), this, SLOT(setValue(int,bool)));
    setLayout(l);
    m_label->setMaximumHeight(m_edit->sizeHint().height());

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
    
    QAction *reset = new QAction(KIcon("edit-undo"), i18n("Reset value"), this);
    connect(reset, SIGNAL(triggered()), this, SLOT(slotReset()));
    m_menu->addAction(reset);
    
    if (m_id > -1) {
        QAction *timeline = new QAction(KIcon("go-jump"), i18n("Show %1 in timeline", label), this);
        connect(timeline, SIGNAL(triggered()), this, SLOT(slotSetInTimeline()));
        connect(m_label, SIGNAL(setInTimeline()), this, SLOT(slotSetInTimeline()));
        m_menu->addAction(timeline);
    }
                        
    m_label->setRange(m_minimum, m_maximum);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(slotShowContextMenu(const QPoint&)));
    connect(m_scale, SIGNAL(triggered(int)), this, SLOT(slotSetScaleMode(int)));
    connect(m_directUpdate, SIGNAL(triggered(bool)), this, SLOT(slotSetDirectUpdate(bool)));

    connect(m_edit, SIGNAL(editingFinished()), this, SLOT(slotEditingFinished()));
}

DragValue::~DragValue()
{
    delete m_edit;
    delete m_menu;
    delete m_scale;
    delete m_directUpdate;
}

int DragValue::spinSize()
{
    return m_edit->sizeHint().width();
}

void DragValue::setSpinSize(int width)
{
    m_edit->setMinimumWidth(width);
}

void DragValue::slotSetInTimeline()
{
    emit inTimeline(m_id);
}

int DragValue::precision() const
{
    return m_precision;
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
    return m_edit->value();
}

void DragValue::setMaximum(qreal max)
{
    m_maximum = max;
    m_label->setRange(m_minimum, m_maximum);
    m_edit->setRange(m_minimum, m_maximum);
}

void DragValue::setMinimum(qreal min)
{
    m_minimum = min;
    m_label->setRange(m_minimum, m_maximum);
    m_edit->setRange(m_minimum, m_maximum);
}

void DragValue::setRange(qreal min, qreal max)
{
    m_maximum = max;
    m_minimum = min;
    m_label->setRange(m_minimum, m_maximum);
    m_edit->setRange(m_minimum, m_maximum);
}

void DragValue::setPrecision(int /*precision*/)
{
    //TODO: Not implemented, in case we need double value, we should replace the KIntSpinBox with KDoubleNumInput...
    /*m_precision = precision;
    if (precision == 0)
        m_edit->setValidator(new QIntValidator(m_minimum, m_maximum, this));
    else
        m_edit->setValidator(new QDoubleValidator(m_minimum, m_maximum, precision, this));*/
}

void DragValue::setStep(qreal /*step*/)
{
    //m_step = step;
}

void DragValue::slotReset()
{
    m_edit->blockSignals(true);
    m_edit->setValue(m_default);
    m_label->setValue(m_default);
    m_edit->blockSignals(false);
    emit valueChanged(m_default, true);
}

void DragValue::slotSetValue(int value)
{
    setValue(value, KdenliveSettings::dragvalue_directupdate());
}

void DragValue::setValue(int value, bool final)
{
    value = qBound(m_minimum, value, m_maximum);
    m_edit->blockSignals(true);
    m_edit->setValue(value);
    m_edit->blockSignals(false);
    m_label->setValue(value);
    emit valueChanged(value, final);
}

void DragValue::focusInEvent(QFocusEvent* e)
{
    if (e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason) {
        //m_edit->setEnabled(true);
        m_edit->setFocus(e->reason());
    } else {
        QWidget::focusInEvent(e);
    }
}

void DragValue::slotEditingFinished()
{
    qreal value = m_edit->value();
    m_edit->clearFocus();
    emit valueChanged(value, true);
}

void DragValue::slotShowContextMenu(const QPoint& pos)
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
    if (m_label->property("inTimeline").toBool() == intimeline) return;
    m_label->setProperty("inTimeline", intimeline);
    m_edit->setProperty("inTimeline", intimeline);
    style()->unpolish(m_label);
    style()->polish(m_label);
    m_label->update();
    style()->unpolish(m_edit);
    style()->polish(m_edit);
    m_edit->update();
}

CustomLabel::CustomLabel(const QString &label, QWidget* parent) :
    QProgressBar(parent),
    m_dragMode(false),
    m_step(1)

{
    setFormat(" " + label);
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    setRange(0, 100);
    setValue(0);
    setFont(KGlobalSettings::toolBarFont());
}

void CustomLabel::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStartPosition = m_dragLastPosition = e->pos();
        e->accept();
    }
    else QWidget::mousePressEvent(e);
}

void CustomLabel::mouseMoveEvent(QMouseEvent* e)
{
    if ((e->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragMode = true;
        if (KdenliveSettings::dragvalue_mode() > 0) {
            int diff = e->x() - m_dragLastPosition.x();

            if (e->modifiers() == Qt::ControlModifier)
                diff *= 2;
            else if (e->modifiers() == Qt::ShiftModifier)
                diff /= 2;
            if (KdenliveSettings::dragvalue_mode() == 2)
                diff = (diff > 0 ? 1 : -1) * pow(diff, 2);

            int nv = value() + diff / m_step;
            if (nv != value()) setNewValue(nv, KdenliveSettings::dragvalue_directupdate());
        }
        else {
            int nv = minimum() + ((double) maximum() - minimum()) / width() * e->pos().x();
            if (nv != value()) setNewValue(nv, KdenliveSettings::dragvalue_directupdate());
        }
        m_dragLastPosition = e->pos();
        e->accept();
    }
    else QWidget::mouseMoveEvent(e);
}

void CustomLabel::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->modifiers() == Qt::ControlModifier) {
        emit setInTimeline();
        e->accept();
        return;
    }
    if (m_dragMode) {
        setNewValue(value(), true);
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    }
    else {
        setNewValue((int) (minimum() + ((double)maximum() - minimum()) / width() * e->pos().x()), true);
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    }
    m_dragMode = false;
}

void CustomLabel::wheelEvent(QWheelEvent* e)
{
    if (e->delta() > 0) {
        if (e->modifiers() == Qt::ControlModifier) slotValueInc(10);
        else slotValueInc();
    }
    else {
        if (e->modifiers() == Qt::ControlModifier) slotValueDec(10);
        else slotValueDec();
    }
    e->accept();
}

void CustomLabel::slotValueInc(int factor)
{
    setNewValue((int) (value() + m_step * factor), true);
}

void CustomLabel::slotValueDec(int factor)
{
    setNewValue((int) (value() - m_step * factor), true);
}

void CustomLabel::setNewValue(int value, bool update)
{
    setValue(value);
    emit valueChanged(value, update);
}

#include "dragvalue.moc"

