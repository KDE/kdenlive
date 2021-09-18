/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include "clickablelabelwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"

#include <QPushButton>
#include <QClipboard>
#include <QLabel>
#include <QIcon>
#include <QToolButton>
#include <QApplication>
#include <QVBoxLayout>

ClickableLabelParamWidget::ClickableLabelParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    // setup the comment
    m_displayName = m_model->data(m_index, Qt::DisplayRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    auto *layout = new QHBoxLayout(this);
    m_tb = new QToolButton(this);
    m_tb->setCursor(Qt::PointingHandCursor);
    m_tb->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tb);
    layout->addWidget(m_label);
    connect(m_tb, &QToolButton::clicked, this, [&]() {
        QClipboard *clipboard = QApplication::clipboard();
        QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
        clipboard->setText(value);
    });
    connect(m_label, &QLabel::linkActivated, this, [&](const QString &result) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(result);
    });
    slotRefresh();
}

void ClickableLabelParamWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
    /*if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }*/
}

void ClickableLabelParamWidget::slotRefresh()
{
    QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    m_label->setText(QStringLiteral("<a href=\"%1\">").arg(value) + m_displayName + QStringLiteral("</a>"));
    setVisible(!value.isEmpty());
    setMinimumHeight(value.isEmpty() ? 0 : m_tb->sizeHint().height());
    emit updateHeight();
}

bool ClickableLabelParamWidget::getValue()
{
    return true;
}
