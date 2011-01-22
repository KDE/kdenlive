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

#include <KColorScheme>
#include <KIcon>

DragValue::DragValue(QWidget* parent) :
        QWidget(parent),
        m_maximum(100),
        m_minimum(0),
        m_precision(2),
        m_step(1),
        m_dragMode(false),
        m_finalValue(true)
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);

    QHBoxLayout *l = new QHBoxLayout(this);
    l->setSpacing(0);
    l->setMargin(0);

    /*m_buttonDec = new QToolButton(this);
    m_buttonDec->setIcon(KIcon("arrow-left"));
    m_buttonDec->setIconSize(QSize(12, 12));
    m_buttonDec->setObjectName("ButtonDec");
    l->addWidget(m_buttonDec);*/

    m_edit = new QLineEdit(this);
    m_edit->setValidator(new QDoubleValidator(m_minimum, m_maximum, m_precision, this));
    m_edit->setAlignment(Qt::AlignCenter);
    m_edit->setEnabled(false);
    l->addWidget(m_edit);

    /*m_buttonInc = new QToolButton(this);
    m_buttonInc->setIcon(KIcon("arrow-right"));
    m_buttonInc->setIconSize(QSize(12, 12));
    m_buttonInc->setObjectName("ButtonInc");
    l->addWidget(m_buttonInc);*/

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
/*     QString stylesheet(QString("QLineEdit { background-color: rgb(%1, %2, %3); border: 1px solid rgb(%1, %2, %3); padding: 2px; padding-bottom: 0px; border-top-left-radius: 7px; border-top-right-radius: 7px; }")
                         .arg(bg.red()).arg(bg.green()).arg(bg.blue()));
     stylesheet.append(QString("QLineEdit::focus, QLineEdit::enabled { background-color: rgb(%1, %2, %3); color: rgb(%4, %5, %6); }")
                         .arg(textbg.red()).arg(textbg.green()).arg(textbg.blue())
                         .arg(textfg.red()).arg(textfg.green()).arg(textfg.blue()));
    QString stylesheet(QString("* { background-color: rgb(%1, %2, %3); margin: 0px; }").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
    stylesheet.append(QString("QLineEdit { border: 0px; height: 100%; } QLineEdit::focus, QLineEdit::enabled { background-color: rgb(%1, %2, %3); color: rgb(%4, %5, %6); }")
                         .arg(textbg.red()).arg(textbg.green()).arg(textbg.blue())
                         .arg(textfg.red()).arg(textfg.green()).arg(textfg.blue()));
    stylesheet.append(QString("QToolButton { border: 1px solid rgb(%1, %2, %3); }").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
    stylesheet.append(QString("QToolButton#ButtonDec { border-top-left-radius: 5px; border-bottom-left-radius: 5px; }"));
    stylesheet.append(QString("QToolButton#ButtonInc { border-top-right-radius: 5px; border-bottom-right-radius: 5px; }"));*/
    setStyleSheet(stylesheet);

    updateMaxWidth();

    /*connect(m_buttonDec, SIGNAL(clicked(bool)), this, SLOT(slotValueDec()));
    connect(m_buttonInc, SIGNAL(clicked(bool)), this, SLOT(slotValueInc()));*/
    connect(m_edit, SIGNAL(editingFinished()), this, SLOT(slotEditingFinished()));
}

DragValue::~DragValue()
{
    delete m_edit;
    /*delete m_buttonInc;
    delete m_buttonDec;*/
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
    m_finalValue = final;
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
        int diffDistance = e->x() - m_dragLastPosition.x();
        int direction = diffDistance > 0 ? 1 : -1; // since pow loses this info
        setValue(value() + direction * pow(e->x() - m_dragLastPosition.x(), 2) / m_step, false);
        m_dragLastPosition = e->pos();
        e->accept();
    }
}

void DragValue::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragMode = false;
    if (m_finalValue) {
        m_edit->setEnabled(true);
        m_edit->setFocus(Qt::MouseFocusReason);
    } else {
        setValue(value(), true);
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
    m_finalValue = true;
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

#include "dragvalue.moc"
