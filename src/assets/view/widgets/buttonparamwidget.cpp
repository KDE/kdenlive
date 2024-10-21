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

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ButtonParamWidget::ButtonParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_animated(false)
{
    // setup the comment
    m_buttonName = m_model->data(m_index, Qt::DisplayRole).toString();
    m_alternatebuttonName = m_model->data(m_index, AssetParameterModel::AlternateNameRole).toString();
    // QString name = m_model->data(m_index, AssetParameterModel::NameRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    // setEnabled(m_model->getOwnerId().first != KdenliveObjectType::TimelineTrack);
    auto *layout = new QVBoxLayout(this);
    QVariantList filterData = m_model->data(m_index, AssetParameterModel::FilterJobParamsRole).toList();
    QStringList filterAddedParams = m_model->data(m_index, AssetParameterModel::FilterParamsRole).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList consumerParams = m_model->data(m_index, AssetParameterModel::FilterConsumerParamsRole).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString defaultValue;
    for (const QVariant &jobElement : std::as_const(filterData)) {
        QStringList d = jobElement.toStringList();
        if (d.size() == 2) {
            if (d.at(0) == QLatin1String("conditionalinfo")) {
                m_conditionalText = d.at(1);
            } else if (d.at(0) == QLatin1String("key")) {
                m_keyParam = d.at(1);
            } else if (d.at(0) == QLatin1String("keydefault")) {
                defaultValue = d.at(1);
            } else if (d.at(0) == QLatin1String("animated")) {
                m_animated = true;
            }
        }
    }
    const QString paramValue = m_model->getParamFromName(m_keyParam).toString();
    bool valueIsNotSet = paramValue.isEmpty() || (m_animated && !paramValue.contains(QLatin1Char(';')));
    m_displayConditional = valueIsNotSet;

    if (m_displayConditional && !m_conditionalText.isEmpty()) {
        setToolTip(m_conditionalText);
    }
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_button = new QPushButton(m_displayConditional ? m_buttonName : m_alternatebuttonName, this);
    layout->addWidget(m_button);
    m_progress = new QProgressBar(this);
    m_progress->setMaximumHeight(m_button->height() / 5);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(QStringLiteral("QProgressBar::chunk {background-color: %1;}").arg(m_progress->palette().highlight().color().name()));
    layout->addWidget(m_progress);
    m_progress->setVisible(false);
    setMinimumHeight(m_button->sizeHint().height());

    // Q_EMIT the signal of the base class when appropriate
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
                                current.append(QStringLiteral("%1 ").arg(qRound(n)));
                            } else {
                                current.append(QStringLiteral("%1 ").arg(val));
                            }
                        }
                    } else {
                        current = defaultValue;
                    }
                }
                // values << QPair<QString, QVariant>(QStringLiteral("rect"),current);
                // values << QPair<QString, QVariant>(QStringLiteral("_reset"),1);
                values << QPair<QString, QVariant>(m_keyParam, current);
            } else {

                values << QPair<QString, QVariant>(m_keyParam, defaultValue);
            }
            m_model->setParametersFromTask(values);
            return;
        }
        QVector<QPair<QString, QVariant>> filterLastParams = m_model->getAllParameters();
        ObjectId owner = m_model->getOwnerId();
        const QString assetId = m_model->getAssetId();
        QString binId;
        int in = -1;
        int out = -1;
        if (owner.type == KdenliveObjectType::BinClip) {
            binId = QString::number(owner.itemId);
        } else if (owner.type == KdenliveObjectType::TimelineClip) {
            binId = pCore->getTimelineClipBinId(owner);
            in = pCore->getItemIn(owner);
            out = in + pCore->getItemDuration(owner);
        } else if (owner.type == KdenliveObjectType::TimelineTrack || owner.type == KdenliveObjectType::Master) {
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
        for (const auto &param : std::as_const(filterLastParams)) {
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
            setToolTip(m_conditionalText);
        } else {
            FilterTask::start(owner, binId, m_model, assetId, in, out, assetId, fParams, fData, consumerParams, this);
            setToolTip(QString());
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

QLabel *ButtonParamWidget::createLabel()
{
    return new QLabel();
}

void ButtonParamWidget::slotRefresh()
{
    const QString paramValue = m_model->getParamFromName(m_keyParam).toString();
    bool valueIsNotSet = paramValue.isEmpty() || (m_animated && !paramValue.contains(QLatin1Char(';')));
    m_displayConditional = valueIsNotSet;
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
        if (m_displayConditional) {
            setToolTip(m_conditionalText);
        }
    }
}

bool ButtonParamWidget::getValue()
{
    return true;
}
