/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "geometrywidget.h"
#include "doublewidget.h"
#include "dragvalue.h"
#include "core.h"
#include "utils/KoIconUtils.h"

#include <QGridLayout>
#include <KLocalizedString>

GeometryWidget::GeometryWidget(const QRect &rect, bool useRatioLock, QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_defaultSize = pCore->getCurrentFrameSize();

    /*QString paramName = i18n(paramTag.toUtf8().data());
    QString comment = m_model->data(ix, AssetParameterModel::CommentRole).toString();
    if (!comment.isEmpty()) {
        comment = i18n(comment.toUtf8().data());
    }*/

    auto *horLayout = new QHBoxLayout;
    horLayout->setSpacing(2);
    m_spinX = new DragValue(i18nc("x axis position", "X"), 0, 0, -99000, 99000, -1, QString(), false, this);
    connect(m_spinX, &DragValue::valueChanged, this, &GeometryWidget::slotAdjustRectKeyframeValue);
    horLayout->addWidget(m_spinX);

    m_spinY = new DragValue(i18nc("y axis position", "Y"), 0, 0, -99000, 99000, -1, QString(), false, this);
    connect(m_spinY, &DragValue::valueChanged, this, &GeometryWidget::slotAdjustRectKeyframeValue);
    horLayout->addWidget(m_spinY);

    m_spinWidth = new DragValue(i18nc("Frame width", "W"), m_defaultSize.width(), 0, 1, 99000, -1, QString(), false, this);
    connect(m_spinWidth, &DragValue::valueChanged, this, &GeometryWidget::slotAdjustRectWidth);
    horLayout->addWidget(m_spinWidth);

    // Lock ratio stuff
    m_lockRatio = new QAction(KoIconUtils::themedIcon(QStringLiteral("link")), i18n("Lock aspect ratio"), this);
    m_lockRatio->setCheckable(true);
    connect(m_lockRatio, &QAction::triggered, this, &GeometryWidget::slotLockRatio);
    auto *ratioButton = new QToolButton;
    ratioButton->setDefaultAction(m_lockRatio);
    horLayout->addWidget(ratioButton);

    m_spinHeight = new DragValue(i18nc("Frame height", "H"), m_defaultSize.height(), 0, 1, 99000, -1, QString(), false, this);
    connect(m_spinHeight, &DragValue::valueChanged, this, &GeometryWidget::slotAdjustRectHeight);
    horLayout->addWidget(m_spinHeight);
    horLayout->addStretch(10);

    auto *horLayout2 = new QHBoxLayout;
    m_spinSize = new DragValue(i18n("Size"), 100, 2, 1, 99000, -1, i18n("%"), false, this);
    m_spinSize->setStep(10);
    connect(m_spinSize, &DragValue::valueChanged, this, &GeometryWidget::slotResize);
    horLayout2->addWidget(m_spinSize);
    horLayout2->addStretch(10);

    // Build buttons
    m_originalSize = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-original")), i18n("Adjust to original size"), this);
    connect(m_originalSize, &QAction::triggered, this, &GeometryWidget::slotAdjustToSource);
    m_originalSize->setCheckable(true);
    QAction *adjustSize = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-best")), i18n("Adjust and center in frame"), this);
    connect(adjustSize, &QAction::triggered, this, &GeometryWidget::slotAdjustToFrameSize);
    QAction *fitToWidth = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-width")), i18n("Fit to width"), this);
    connect(fitToWidth, &QAction::triggered, this, &GeometryWidget::slotFitToWidth);
    QAction *fitToHeight = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-height")), i18n("Fit to height"), this);
    connect(fitToHeight, &QAction::triggered, this, &GeometryWidget::slotFitToHeight);

    QAction *alignleft = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-left")), i18n("Align left"), this);
    connect(alignleft, &QAction::triggered, this, &GeometryWidget::slotMoveLeft);
    QAction *alignhcenter = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-hor")), i18n("Center horizontally"), this);
    connect(alignhcenter, &QAction::triggered, this, &GeometryWidget::slotCenterH);
    QAction *alignright = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-right")), i18n("Align right"), this);
    connect(alignright, &QAction::triggered, this, &GeometryWidget::slotMoveRight);
    QAction *aligntop = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-top")), i18n("Align top"), this);
    connect(aligntop, &QAction::triggered, this, &GeometryWidget::slotMoveTop);
    QAction *alignvcenter = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-vert")), i18n("Center vertically"), this);
    connect(alignvcenter, &QAction::triggered, this, &GeometryWidget::slotCenterV);
    QAction *alignbottom = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-bottom")), i18n("Align bottom"), this);
    connect(alignbottom, &QAction::triggered, this, &GeometryWidget::slotMoveBottom);

    auto *alignLayout = new QHBoxLayout;
    alignLayout->setSpacing(0);
    auto *alignButton = new QToolButton;
    alignButton->setDefaultAction(alignleft);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignhcenter);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignright);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(aligntop);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignvcenter);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignbottom);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(m_originalSize);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(adjustSize);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(fitToWidth);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(fitToHeight);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);
    alignLayout->addStretch(10);

    layout->addLayout(horLayout);
    layout->addLayout(alignLayout);
    layout->addLayout(horLayout2);
}

void GeometryWidget::slotAdjustToSource()
{
    
}
void GeometryWidget::slotAdjustToFrameSize()
{
    
}
void GeometryWidget::slotFitToWidth()
{
    
}
void GeometryWidget::slotFitToHeight()
{
    
}
void GeometryWidget::slotResize(double value)
{
    
}
/** @brief Moves the rect to the left frame border (x position = 0). */
void GeometryWidget::slotMoveLeft()
{
    
}
/** @brief Centers the rect horizontally. */
void GeometryWidget::slotCenterH()
{
    
}
/** @brief Moves the rect to the right frame border (x position = frame width - rect width). */
void GeometryWidget::slotMoveRight()
{
    
}
/** @brief Moves the rect to the top frame border (y position = 0). */
void GeometryWidget::slotMoveTop()
{
    
}
/** @brief Centers the rect vertically. */
void GeometryWidget::slotCenterV()
{
    
}
/** @brief Moves the rect to the bottom frame border (y position = frame height - rect height). */
void GeometryWidget::slotMoveBottom()
{
    
}
/** @brief Un/Lock aspect ratio for size in effect parameter. */
void GeometryWidget::slotLockRatio()
{
    
}
void GeometryWidget::slotAdjustRectHeight()
{
    
}
void GeometryWidget::slotAdjustRectWidth()
{
    
}

void GeometryWidget::slotAdjustRectKeyframeValue()
{
}

void GeometryWidget::slotUpdateGeometryRect(const QRect r)
{
    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinX->setValue(r.x());
    m_spinY->setValue(r.y());
    m_spinWidth->setValue(r.width());
    m_spinHeight->setValue(r.height());
    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
    emit valueChanged(getValue());
    //setupMonitor();
}

const QString GeometryWidget::getValue() const
{
    return QStringLiteral("%1 %2 %3 %4").arg(m_spinX->value()).arg(m_spinY->value()).arg(m_spinWidth->value()).arg( m_spinHeight->value());
}
