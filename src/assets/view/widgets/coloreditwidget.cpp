/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "coloreditwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "widgets/colorpickerwidget.h"
#include "utils/qcolorutils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextStream>

#include <KColorButton>

ColorEditWidget::ColorEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    // setup the comment
    QString name = m_model->data(m_index, Qt::DisplayRole).toString();
    bool alphaEnabled = m_model->data(m_index, AssetParameterModel::AlphaRole).toBool();
    QString color = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(name, this);

    QWidget *rightSide = new QWidget(this);
    auto *rightSideLayout = new QHBoxLayout(rightSide);
    rightSideLayout->setContentsMargins(0, 0, 0, 0);
    rightSideLayout->setSpacing(0);

    m_button = new KColorButton(QColorUtils::stringToColor(color), rightSide);
    if (alphaEnabled) {
        m_button->setAlphaChannelEnabled(alphaEnabled);
    }
    //     m_button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *picker = new ColorPickerWidget(rightSide);

    layout->addWidget(label, 1);
    layout->addWidget(rightSide, 1);
    rightSideLayout->addStretch();
    rightSideLayout->addWidget(m_button, 2);
    rightSideLayout->addWidget(picker);

    connect(picker, &ColorPickerWidget::colorPicked, this, &ColorEditWidget::setColor);
    connect(picker, &ColorPickerWidget::disableCurrentFilter, this, &ColorEditWidget::disableCurrentFilter);
    connect(m_button, &KColorButton::changed, this, &ColorEditWidget::modified);

    // emit the signal of the base class when appropriate
    connect(this, &ColorEditWidget::modified, [this](const QColor &) { emit valueChanged(m_index, getColor(), true); });

    // setup comment
    setToolTip(comment);
    setMinimumHeight(m_button->sizeHint().height());
}

void ColorEditWidget::slotShowComment(bool) {}

void ColorEditWidget::slotRefresh()
{
    QSignalBlocker bk(this);
    QString color = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    m_button->setColor(QColorUtils::stringToColor(color));
}

QString ColorEditWidget::getColor() const
{
    return QColorUtils::colorToString(m_button->color(), m_button->isAlphaChannelEnabled());
}

void ColorEditWidget::setColor(const QColor &color)
{
    m_button->setColor(color);
}

void ColorEditWidget::slotColorModified(const QColor &color)
{
    blockSignals(true);
    m_button->setColor(color);
    blockSignals(false);
}
