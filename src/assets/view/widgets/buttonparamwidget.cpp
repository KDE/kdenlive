/***************************************************************************
 *   Copyright (C) 2019 by Jean-Baptiste Mardelle                          *
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

#include "buttonparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "jobs/filterclipjob.h"
#include "jobs/jobmanager.h"
#include "assets/model/assetcommand.hpp"
#include "core.h"
#include <mlt++/Mlt.h>

#include <KMessageWidget>
#include <QPushButton>
#include <QVBoxLayout>

ButtonParamWidget::ButtonParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_label(nullptr)
{
    // setup the comment
    m_buttonName = m_model->data(m_index, Qt::DisplayRole).toString();
    m_alternatebuttonName = m_model->data(m_index, AssetParameterModel::AlternateNameRole).toString();
    //QString name = m_model->data(m_index, AssetParameterModel::NameRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    //setEnabled(m_model->getOwnerId().first != ObjectType::TimelineTrack);
    auto *layout = new QVBoxLayout(this);
    QVariantList filterData = m_model->data(m_index, AssetParameterModel::FilterJobParamsRole).toList();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList filterAddedParams = m_model->data(m_index, AssetParameterModel::FilterParamsRole).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
    QStringList filterAddedParams = m_model->data(m_index, AssetParameterModel::FilterParamsRole).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList consumerParams = m_model->data(m_index, AssetParameterModel::FilterConsumerParamsRole).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
    QStringList consumerParams = m_model->data(m_index, AssetParameterModel::FilterConsumerParamsRole).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif

    QString conditionalInfo;
    for (const QVariant &jobElement : qAsConst(filterData)) {
        QStringList d = jobElement.toStringList();
        if (d.size() == 2) {
            if (d.at(0) == QLatin1String("conditionalinfo")) {
                conditionalInfo = d.at(1);
            } else if (d.at(0) == QLatin1String("key")) {
                m_keyParam = d.at(1);
            }
        }
    }
    QVector<QPair<QString, QVariant>> filterParams = m_model->getAllParameters();
    m_displayConditional = true;
    for (const auto &param : qAsConst(filterParams)) {
        if (param.first == m_keyParam) {
            if (!param.second.toString().isEmpty()) {
                m_displayConditional = false;
            }
            break;
        }
    }
    if (!conditionalInfo.isEmpty()) {
        m_label = new KMessageWidget(conditionalInfo, this);
        m_label->setWordWrap(true);
        layout->addWidget(m_label);
        m_label->setVisible(m_displayConditional);
    }
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_button = new QPushButton(m_displayConditional ? m_buttonName : m_alternatebuttonName, this);
    layout->addWidget(m_button);
    setMinimumHeight(m_button->sizeHint().height() + (m_label != nullptr ? m_label->sizeHint().height() : 0));

    // emit the signal of the base class when appropriate
    connect(this->m_button, &QPushButton::clicked, this, [&, filterData, filterAddedParams, consumerParams]() {
        // Trigger job
        if (!m_displayConditional) {
            QVector<QPair<QString, QVariant>> values;
            values << QPair<QString, QVariant>(m_keyParam,QVariant());
            auto *command = new AssetUpdateCommand(m_model, values);
            pCore->pushUndo(command);
            return;
        }
        QVector<QPair<QString, QVariant>> filterLastParams = m_model->getAllParameters();
        ObjectId owner = m_model->getOwnerId();
        const QString assetId = m_model->getAssetId();
        QString binId;
        int cid = -1;
        int in = -1;
        int out = -1;
        if (owner.first == ObjectType::BinClip) {
            binId = QString::number(owner.second);
        } else if (owner.first == ObjectType::TimelineClip) {
            cid = owner.second;
            binId = pCore->getTimelineClipBinId(cid);
            in = pCore->getItemIn(owner);
            out = in + pCore->getItemDuration(owner);
        } else if (owner.first == ObjectType::TimelineTrack || owner.first == ObjectType::Master) {
            in = 0;
            out = pCore->getItemDuration(owner);
        }
        std::unordered_map<QString, QVariant> fParams;
        std::unordered_map<QString, QString> fData;
        for (const QVariant &jobElement : filterData) {
            QStringList d = jobElement.toStringList();
            if (d.size() == 2)
            fData.insert({d.at(0), d.at(1)});
        }
        for (const auto &param : qAsConst(filterLastParams)) {
            if (param.first != m_keyParam) {
                fParams.insert({param.first, param.second});
            }
        }
        for (const QString &fparam : filterAddedParams) {
            if (fparam.contains(QLatin1Char('='))) {
                fParams.insert({fparam.section(QLatin1Char('='), 0, 0), fparam.section(QLatin1Char('='), 1)});
            }
        }
        emit pCore->jobManager()->startJob<FilterClipJob>({binId}, -1, QString(), owner, m_model, assetId, in, out, assetId, fParams, fData, consumerParams);
        if (m_label) {
            m_label->setVisible(false);
        }
        m_button->setEnabled(false);
    });
}

void ButtonParamWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
    //if (!m_labelComment->text().isEmpty()) {
    //    m_widgetComment->setVisible(show);
    //}
}

void ButtonParamWidget::slotRefresh()
{
    QVector<QPair<QString, QVariant>> filterParams = m_model->getAllParameters();
    m_displayConditional = true;
    for (const auto &param : qAsConst(filterParams)) {
        if (param.first == m_keyParam && !param.second.isNull()) {
            m_displayConditional = false;
            break;
        }
    }
    if (m_label) {
        m_label->setVisible(m_displayConditional);
    }
    m_button->setText(m_displayConditional ? m_buttonName : m_alternatebuttonName);
    m_button->setEnabled(true);
    updateGeometry();
}

bool ButtonParamWidget::getValue()
{
    return true;
}
