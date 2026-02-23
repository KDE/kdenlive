/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "pointwidget.h"
#include "dragvalue.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QToolButton>

PointWidget::PointWidget(const QString &name, QPointF value, QPointF min, QPointF max, QPointF factor, QPointF defaultValue, int decimals,
                         const QString &comment, int id, QWidget *parent)
    : QWidget(parent)
    , m_labelText(name)
    , m_factor(factor)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_dragValX = new DragValue(name, defaultValue.x(), decimals, min.x(), max.x(), id, QString(), false, false, this);
    m_dragValY = new DragValue(name, defaultValue.y(), decimals, min.y(), max.y(), id, QString(), false, false, this);
    layout->addWidget(m_dragValX);
    layout->addWidget(m_dragValY);
    layout->addStretch(10);
    setFixedHeight(m_dragValX->minimumHeight());

    if (!comment.isEmpty()) {
        setToolTip(comment);
    }
    m_dragValX->setValue(value.x() * factor.x(), false);
    m_dragValY->setValue(value.y() * factor.y(), false);
    connect(m_dragValX, &DragValue::customValueChanged, this,
            [this](double value, bool final, bool createUndo) { slotSetValue(QPointF(value, m_dragValY->value()), final, createUndo); });
    connect(m_dragValY, &DragValue::customValueChanged, this,
            [this](double value, bool final, bool createUndo) { slotSetValue(QPointF(m_dragValX->value(), value), final, createUndo); });
}

QLabel *PointWidget::createLabel()
{
    return new QLabel(m_labelText, this);
}

bool PointWidget::hasEditFocus() const
{
    return m_dragValX->hasEditFocus() || m_dragValY->hasEditFocus();
}

PointWidget::~PointWidget()
{
    delete m_dragValX;
    delete m_dragValY;
}

void PointWidget::setValue(QPointF value)
{
    QSignalBlocker bk(m_dragValX);
    QSignalBlocker bk2(m_dragValY);
    m_dragValX->setValue(value.x() * m_factor.x());
    m_dragValY->setValue(value.y() * m_factor.y());
}

void PointWidget::slotSetValue(QPointF value, bool final, bool createUndoEntry)
{
    if (final) {
        Q_EMIT valueChanged(QStringLiteral("%1 %2").arg(value.x() / m_factor.x()).arg(value.y() / m_factor.y()), createUndoEntry);
    }
}

QPointF PointWidget::getValue()
{
    return QPointF(m_dragValX->value() / m_factor.x(), m_dragValY->value() / m_factor.y());
}

void PointWidget::slotReset()
{
    m_dragValX->slotReset();
    m_dragValY->slotReset();
}

void PointWidget::slotShowComment(bool show)
{
    Q_UNUSED(show)
}
