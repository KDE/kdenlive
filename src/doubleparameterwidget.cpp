/**************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "doubleparameterwidget.h"
#include "dragvalue.h"

#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QToolButton>

#include <KIcon>
#include <KLocalizedString>


DoubleParameterWidget::DoubleParameterWidget(const QString &name, int value, int min, int max, int defaultValue, const QString &comment, int id, const QString suffix, QWidget *parent) :
        QWidget(parent),
        m_commentLabel(NULL)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    QGridLayout *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    m_dragVal = new DragValue(name, defaultValue, 0, id, suffix, this);
    m_dragVal->setRange(min, max);
    m_dragVal->setPrecision(0);
    layout->addWidget(m_dragVal, 0, 1);

    if (!comment.isEmpty()) {
        m_commentLabel = new QLabel(comment, this);
        m_commentLabel->setWordWrap(true);
        m_commentLabel->setTextFormat(Qt::RichText);
        m_commentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_commentLabel->setFrameShape(QFrame::StyledPanel);
        m_commentLabel->setFrameShadow(QFrame::Raised);
        m_commentLabel->setHidden(true);
        layout->addWidget(m_commentLabel, 1, 0, 1, -1);
    }
    m_dragVal->setValue(value);
    connect(m_dragVal, SIGNAL(valueChanged(int, bool)), this, SLOT(slotSetValue(int, bool)));
    connect(m_dragVal, SIGNAL(inTimeline(int)), this, SIGNAL(setInTimeline(int)));
}

int DoubleParameterWidget::spinSize()
{
    return m_dragVal->spinSize();
}

void DoubleParameterWidget::setSpinSize(int width)
{
    m_dragVal->setSpinSize(width);
}

void DoubleParameterWidget::setValue(int value)
{
    m_dragVal->blockSignals(true);
    m_dragVal->setValue(value);
    m_dragVal->blockSignals(false);
}

void DoubleParameterWidget::slotSetValue(int value, bool final)
{
    if (final) {
        emit valueChanged(value);
    }
}

int DoubleParameterWidget::getValue()
{
    return m_dragVal->value();
}

void DoubleParameterWidget::slotReset()
{
    m_dragVal->slotReset();
}

void DoubleParameterWidget::setInTimelineProperty(bool intimeline)
{
    m_dragVal->setInTimelineProperty(intimeline);
}

void DoubleParameterWidget::slotShowComment( bool show)
{
    if (m_commentLabel) {
        m_commentLabel->setVisible(show);
        if (show)
            layout()->setContentsMargins(0, 0, 0, 15);
        else
            layout()->setContentsMargins(0, 0, 0, 0);
    }
}

#include "doubleparameterwidget.moc"
