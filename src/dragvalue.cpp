/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
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

#include <KColorScheme>
#include <KLocalizedString>

DragValue::DragValue(QWidget* parent) :
        QWidget(parent),
        m_maximum(100),
        m_minimum(0),
        m_precision(2),
        m_step(1),
        m_dragMode(false)
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);

    QHBoxLayout *l = new QHBoxLayout(this);
    l->setSpacing(0);
    l->setMargin(0);

    m_edit = new QLineEdit(this);
    m_edit->setValidator(new QDoubleValidator(m_minimum, m_maximum, m_precision, this));
    m_edit->setAlignment(Qt::AlignCenter);
    m_edit->setEnabled(false);
    l->addWidget(m_edit);

    m_menu = new QMenu(this);

    m_nonlinearScale = new QAction(i18n("Nonlinear scale"), this);
    m_nonlinearScale->setCheckable(true);
    m_nonlinearScale->setChecked(KdenliveSettings::dragvalue_nonlinear());
    m_menu->addAction(m_nonlinearScale);

    m_directUpdate = new QAction(i18n("Direct update"), this);
    m_directUpdate->setCheckable(true);
    m_directUpdate->setChecked(KdenliveSettings::dragvalue_directupdate());
    m_menu->addAction(m_directUpdate);

    QPalette p = palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor bg = scheme.background(KColorScheme::LinkBackground).color();
    QColor fg = scheme.foreground(KColorScheme::LinkText).color();
    QColor editbg = scheme.background(KColorScheme::ActiveBackground).color();
    QColor editfg = scheme.foreground(KColorScheme::ActiveText).color();
    QString stylesheet(QString("QLineEdit { background-color: rgb(%1, %2, %3); border: 1px solid rgb(%1, %2, %3); border-radius: 5px; padding: 0px; color: rgb(%4, %5, %6); } QLineEdit::disabled { color: rgb(%4, %5, %6); }")
                        .arg(bg.red()).arg(bg.green()).arg(bg.blue())
                        .arg(fg.red()).arg(fg.green()).arg(fg.blue()));
    stylesheet.append(QString("QLineEdit::focus, QLineEdit::enabled { background-color: rgb(%1, %2, %3); color: rgb(%4, %5, %6); }")
                        .arg(editbg.red()).arg(editbg.green()).arg(editbg.blue())
                        .arg(editfg.red()).arg(editfg.green()).arg(editfg.blue()));
    setStyleSheet(stylesheet);

    updateMaxWidth();

    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(slotShowContextMenu(const QPoint&)));
    connect(m_nonlinearScale, SIGNAL(triggered(bool)), this, SLOT(slotSetNonlinearScale(bool)));
    connect(m_directUpdate, SIGNAL(triggered(bool)), this, SLOT(slotSetDirectUpdate(bool)));

    connect(m_edit, SIGNAL(editingFinished()), this, SLOT(slotEditingFinished()));
}

DragValue::~DragValue()
{
    delete m_edit;
    delete m_menu;
    delete m_nonlinearScale;
    delete m_directUpdate;
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
    return m_edit->text().toDouble();
}

void DragValue::setMaximum(qreal max)
{
    m_maximum = max;
    updateMaxWidth();
}

void DragValue::setMinimum(qreal min)
{
    m_minimum = min;
    updateMaxWidth();
}

void DragValue::setRange(qreal min, qreal max)
{
    m_maximum = max;
    m_minimum = min;
    updateMaxWidth();
}

void DragValue::setPrecision(int precision)
{
    m_precision = precision;
    if (precision == 0)
        m_edit->setValidator(new QIntValidator(m_minimum, m_maximum, this));
    else
        m_edit->setValidator(new QDoubleValidator(m_minimum, m_maximum, precision, this));
}

void DragValue::setStep(qreal step)
{
    m_step = step;
}

void DragValue::setValue(qreal value, bool final)
{
    value = qBound(m_minimum, value, m_maximum);

    m_edit->setText(QString::number(value, 'f', m_precision));

    emit valueChanged(value, final);
}

void DragValue::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStartPosition = m_dragLastPosition = e->pos();
        m_dragMode = true;
        e->accept();
    }
}

void DragValue::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragMode && (e->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
        int diff = e->x() - m_dragLastPosition.x();

        if (e->modifiers() == Qt::ControlModifier)
            diff *= 2;
        else if (e->modifiers() == Qt::ShiftModifier)
            diff /= 2;
        if (KdenliveSettings::dragvalue_nonlinear())
            diff = (diff > 0 ? 1 : -1) * pow(diff, 2);

        setValue(value() + diff / m_step, KdenliveSettings::dragvalue_directupdate());
        m_dragLastPosition = e->pos();
        e->accept();
    }
}

void DragValue::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragMode = false;
    if (m_dragLastPosition == m_dragStartPosition) {
        // value was not changed through dragging (mouse position stayed the same since mousePressEvent)
        m_edit->setEnabled(true);
        m_edit->setFocus(Qt::MouseFocusReason);
    } else {
        setValue(value(), true);
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    }
}

void DragValue::wheelEvent(QWheelEvent* e)
{
    if (e->delta() > 0)
        slotValueInc();
    else
        slotValueDec();
}

void DragValue::focusInEvent(QFocusEvent* e)
{
    if (e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason) {
        m_edit->setEnabled(true);
        m_edit->setFocus(e->reason());
    } else {
        QWidget::focusInEvent(e);
    }
}

void DragValue::slotValueInc()
{
    setValue(m_edit->text().toDouble() + m_step);
}

void DragValue::slotValueDec()
{
    setValue(m_edit->text().toDouble() - m_step);
}

void DragValue::slotEditingFinished()
{
    qreal value = m_edit->text().toDouble();
    m_edit->setEnabled(false);
    emit valueChanged(value, true);
}

void DragValue::updateMaxWidth()
{
    int val = (int)(log10(qAbs(m_maximum) > qAbs(m_minimum) ? qAbs(m_maximum) : qAbs(m_minimum)) + .5);
    val += m_precision;
    if (m_precision)
        val += 1;
    if (m_minimum < 0)
        val += 1;
    QFontMetrics fm = m_edit->fontMetrics();
    m_edit->setMaximumWidth(fm.width(QString().rightJustified(val, '8')));
}

void DragValue::slotShowContextMenu(const QPoint& pos)
{
    // values might have been changed by another object of this class
    m_nonlinearScale->setChecked(KdenliveSettings::dragvalue_nonlinear());
    m_directUpdate->setChecked(KdenliveSettings::dragvalue_directupdate());
    m_menu->exec(mapToGlobal(pos));
}

void DragValue::slotSetNonlinearScale(bool nonlinear)
{
    KdenliveSettings::setDragvalue_nonlinear(nonlinear);
}

void DragValue::slotSetDirectUpdate(bool directUpdate)
{
    KdenliveSettings::setDragvalue_directupdate(directUpdate);
}

#include "dragvalue.moc"
