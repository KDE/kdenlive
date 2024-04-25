/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "doublewidget.h"
#include "dragvalue.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QToolButton>

DoubleWidget::DoubleWidget(const QString &name, double value, double min, double max, double factor, double defaultValue, const QString &comment, int id,
                           const QString &suffix, int decimals, bool oddOnly, QWidget *parent)
    : QWidget(parent)
    , m_factor(factor)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_dragVal = new DragValue(name, defaultValue * m_factor, decimals, min, max, id, suffix, true, oddOnly, this);
    if (suffix == QStringLiteral("°")) {
        QToolButton *rotationTb = new QToolButton(this);
        rotationTb->setIcon(QIcon::fromTheme("object-rotate-right"));
        rotationTb->setToolTip(i18n("Rotate by 90 ° steps"));
        rotationTb->setAutoRaise(true);
        layout->addWidget(rotationTb);
        connect(rotationTb, &QToolButton::clicked, this, [this, min, max]() {
            int val = m_dragVal->value() / 90;
            val = (val + 1) * 90;
            if (val > max) {
                val = min;
            }
            m_dragVal->setValue(val);
        });
    }
    layout->addWidget(m_dragVal);
    setMinimumHeight(m_dragVal->height());

    if (!comment.isEmpty()) {
        setToolTip(comment);
    }
    m_dragVal->setValue(value * factor, false);
    connect(m_dragVal, &DragValue::valueChanged, this, &DoubleWidget::slotSetValue);
}

void DoubleWidget::setDragObjectName(const QString &name)
{
    m_dragVal->setObjectName(name);
}

bool DoubleWidget::hasEditFocus() const
{
    return m_dragVal->hasEditFocus();
}

DoubleWidget::~DoubleWidget()
{
    delete m_dragVal;
}

int DoubleWidget::spinSize()
{
    return m_dragVal->spinSize();
}

void DoubleWidget::setSpinSize(int width)
{
    m_dragVal->setSpinSize(width);
}

void DoubleWidget::setValue(double value)
{
    QSignalBlocker bk(m_dragVal);
    m_dragVal->setValue(value * m_factor);
}

void DoubleWidget::enableEdit(bool enable)
{
    m_dragVal->setEnabled(enable);
}

void DoubleWidget::slotSetValue(double value, bool final, bool createUndoEntry)
{
    if (final) {
        Q_EMIT valueChanged(value / m_factor, createUndoEntry);
    }
}

double DoubleWidget::getValue()
{
    return m_dragVal->value();
}

void DoubleWidget::slotReset()
{
    m_dragVal->slotReset();
}

void DoubleWidget::slotShowComment(bool show)
{
    Q_UNUSED(show)
}
