/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "buttonparamwidget.hpp"
#include "assets/model/assetcommand.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "jobs/filtertask.h"
#include <mlt++/Mlt.h>

#include <KMessageWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ButtonParamWidget::ButtonParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_label(nullptr)
{
    // setup the comment
    m_buttonName = m_model->data(m_index, Qt::DisplayRole).toString();
    m_alternatebuttonName = m_model->data(m_index, AssetParameterModel::AlternateNameRole).toString();
    // QString name = m_model->data(m_index, AssetParameterModel::NameRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    // setEnabled(m_model->getOwnerId().first != ObjectType::TimelineTrack);
    auto *layout = new QVBoxLayout(this);
    QVariantList filterData = m_model->data(m_index, AssetParameterModel::FilterJobParamsRole).toList();
    QStringList filterAddedParams = m_model->data(m_index, AssetParameterModel::FilterParamsRole).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList consumerParams = m_model->data(m_index, AssetParameterModel::FilterConsumerParamsRole).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString conditionalInfo;
    QString defaultValue;
    for (const QVariant &jobElement : qAsConst(filterData)) {
        QStringList d = jobElement.toStringList();
        if (d.size() == 2) {
            if (d.at(0) == QLatin1String("conditionalinfo")) {
                conditionalInfo = d.at(1);
            } else if (d.at(0) == QLatin1String("key")) {
                m_keyParam = d.at(1);
            } else if (d.at(0) == QLatin1String("keydefault")) {
                defaultValue = d.at(1);
            }
        }
    }
    QVector<QPair<QString, QVariant>> filterParams = m_model->getAllParameters();
    auto has_analyse_data = [&](const QPair<QString, QVariant> &param) {
        return param.first == m_keyParam && !param.second.isNull() && param.second.toString().contains(QLatin1Char(';'));
    };
    m_displayConditional = std::none_of(filterParams.begin(), filterParams.end(), has_analyse_data);

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
    m_progress = new QProgressBar(this);
    m_progress->setMaximumHeight(m_button->height() / 5);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(QString("QProgressBar::chunk {background-color: %1;}").arg(m_progress->palette().highlight().color().name()));
    layout->addWidget(m_progress);
    m_progress->setVisible(false);
    setMinimumHeight(m_button->sizeHint().height() + (m_label != nullptr ? m_label->sizeHint().height() : 0));

    // emit the signal of the base class when appropriate
    connect(this->m_button, &QPushButton::clicked, this, [&, filterData, filterAddedParams, consumerParams, defaultValue]() {
        // Trigger job
        bool isTracker = m_model->getAssetId() == QLatin1String("opencv.tracker");
        if (!m_displayConditional) {
            QVector<QPair<QString, QVariant>> values;
            if (isTracker) {
                // Tracker needs some special config on reset
                QString current = m_model->getAsset()->get(m_keyParam.toUtf8().constData());
                if (!current.isEmpty()) {
                    // Only keep first keyframe
                    current = current.section(QLatin1Char(';'), 0, 0);
                }
                if (current.isEmpty()) {
                    if (defaultValue.contains(QLatin1Char('%'))) {
                        QSize pSize = pCore->getCurrentFrameDisplaySize();
                        QStringList numbers = defaultValue.split(QLatin1Char(' '));
                        int ix = 0;
                        for (QString &val : numbers) {
                            if (val.endsWith(QLatin1Char('%'))) {
                                val.chop(1);
                                double n = val.toDouble() / 100.;
                                if (ix % 2 == 0) {
                                    n *= pSize.width();
                                } else {
                                    n *= pSize.height();
                                }
                                ix++;
                                current.append(QString("%1 ").arg(qRound(n)));
                            } else {
                                current.append(QString("%1 ").arg(val));
                            }
                        }
                    } else {
                        current = defaultValue;
                    }
                }
                // values << QPair<QString, QVariant>(QString("rect"),current);
                // values << QPair<QString, QVariant>(QString("_reset"),1);
                values << QPair<QString, QVariant>(m_keyParam, current);
            } else {
                values << QPair<QString, QVariant>(m_keyParam, defaultValue);
            }
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
            binId = QStringLiteral("-1");
        }
        std::unordered_map<QString, QVariant> fParams;
        std::unordered_map<QString, QString> fData;
        for (const QVariant &jobElement : filterData) {
            QStringList d = jobElement.toStringList();
            if (d.size() == 2) fData.insert({d.at(0), d.at(1)});
        }
        for (const auto &param : qAsConst(filterLastParams)) {
            if (param.first != m_keyParam) {
                if (!isTracker || param.first != QLatin1String("rect")) {
                    fParams.insert({param.first, param.second});
                }
            } else if (isTracker) {
                QString initialRect = param.second.toString();
                if (initialRect.contains(QLatin1Char('='))) {
                    initialRect = initialRect.section(QLatin1Char('='), 1);
                }
                if (initialRect.contains(QLatin1Char(';'))) {
                    initialRect = initialRect.section(QLatin1Char(';'), 0, 0);
                }
                fParams.insert({QStringLiteral("rect"), initialRect});
            }
        }
        for (const QString &fparam : filterAddedParams) {
            if (fparam.contains(QLatin1Char('='))) {
                fParams.insert({fparam.section(QLatin1Char('='), 0, 0), fparam.section(QLatin1Char('='), 1)});
            }
        }
        if (m_progress->value() > 0 && m_progress->value() < 100) {
            // The task is in progress, abort it
            pCore->taskManager.discardJobs(owner, AbstractTask::FILTERCLIPJOB);
        } else {
            FilterTask::start(owner, binId, m_model, assetId, in, out, assetId, fParams, fData, consumerParams, this);
            if (m_label) {
                m_label->setVisible(false);
            }
            m_button->setEnabled(false);
        }
    });
}

void ButtonParamWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
    // if (!m_labelComment->text().isEmpty()) {
    //    m_widgetComment->setVisible(show);
    //}
}

void ButtonParamWidget::slotRefresh()
{
    QVector<QPair<QString, QVariant>> filterParams = m_model->getAllParameters();
    auto has_analyse_data = [&](const QPair<QString, QVariant> &param) {
        return param.first == m_keyParam && !param.second.isNull() && param.second.toString().contains(QLatin1Char(';'));
    };
    m_displayConditional = std::none_of(filterParams.begin(), filterParams.end(), has_analyse_data);

    if (m_label) {
        m_label->setVisible(m_displayConditional);
    }
    m_button->setEnabled(true);
    // Check running job percentage
    int progress = m_model->data(m_index, AssetParameterModel::FilterProgressRole).toInt();
    if (progress > 0 && progress < 100) {
        m_progress->setValue(progress);
        if (!m_progress->isVisible()) {
            m_button->setText(i18n("Abort processing"));
            m_progress->setVisible(true);
        }

    } else {
        m_button->setText(m_displayConditional ? m_buttonName : m_alternatebuttonName);
        m_progress->setValue(0);
        m_progress->setVisible(false);
    }
    updateGeometry();
}

bool ButtonParamWidget::getValue()
{
    return true;
}
