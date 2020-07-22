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

#include "mixermanager.hpp"
#include "mixerwidget.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "kdenlivesettings.h"

#include "mlt++/MltService.h"
#include "mlt++/MltTractor.h"

#include <klocalizedstring.h>
#include <QHBoxLayout>
#include <QModelIndex>
#include <QScrollArea>
#include <QApplication>
#include <QScreen>
#include <QTimer>

const double log_factor = 1.0 / log10(1.0 / 127);

MixerManager::MixerManager(QWidget *parent)
    : QWidget(parent)
    , m_masterMixer(nullptr)
    , m_visibleMixerManager(false)
    , m_expandedWidth(-1)
    , m_recommandedWidth(300)
{
    m_masterBox = new QHBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    m_channelsBox = new QScrollArea(this);
    m_channelsBox->setContentsMargins(0, 0, 0, 0);
    m_box = new QHBoxLayout;
    m_box->setSpacing(0);
    auto *channelsBoxContainer = new QWidget(this);
    m_channelsBox->setWidget(channelsBoxContainer);
    m_channelsBox->setWidgetResizable(true);
    m_channelsBox->setFrameShape(QFrame::NoFrame);
    m_box->addWidget(m_channelsBox);
    m_channelsLayout = new QHBoxLayout;
    m_channelsLayout->setContentsMargins(0, 0, 0, 0);
    m_masterBox->setContentsMargins(0, 0, 0, 0);
    m_channelsLayout->setSpacing(4);
    channelsBoxContainer->setLayout(m_channelsLayout);
    m_channelsLayout->addStretch(10);
    m_box->addLayout(m_masterBox);
    setLayout(m_box);
}

void MixerManager::registerTrack(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag)
{
    if (m_mixers.count(tid) > 0) {
        // Track already registered
        return;
    }
    std::shared_ptr<MixerWidget> mixer(new MixerWidget(tid, service, trackTag, this));
    connect(mixer.get(), &MixerWidget::muteTrack, this, [&](int id, bool mute) {
        m_model->setTrackProperty(id, "hide", mute ? QStringLiteral("1") : QStringLiteral("3"));
    });
    if (m_visibleMixerManager) {
        mixer->connectMixer(!KdenliveSettings::mixerCollapse());
    }
    connect(this, &MixerManager::updateLevels, mixer.get(), &MixerWidget::updateAudioLevel);
    connect(this, &MixerManager::clearMixers, mixer.get(), &MixerWidget::clear);
    connect(mixer.get(), &MixerWidget::toggleSolo, this, [&](int trid, bool solo) {
        if (!solo) {
            // unmute
            for (int id : qAsConst(m_soloMuted)) {
                if (m_mixers.count(id) > 0) {
                    m_model->setTrackProperty(id, "hide", QStringLiteral("1"));
                }
            }
            m_soloMuted.clear();
        } else {
            if (!m_soloMuted.isEmpty()) {
                // Another track was solo, discard first
                for (int id : qAsConst(m_soloMuted)) {
                    if (m_mixers.count(id) > 0) {
                        m_model->setTrackProperty(id, "hide", QStringLiteral("1"));
                    }
                }
                m_soloMuted.clear();
            }
            for (const auto &item : m_mixers) {
                if (item.first != trid && !item.second->isMute()) {
                    m_model->setTrackProperty(item.first, "hide", QStringLiteral("3"));
                    m_soloMuted << item.first;
                    item.second->unSolo();
                }
            }
        }
    });
    m_mixers[tid] = mixer;
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    m_channelsLayout->insertWidget(0, line);
    m_channelsLayout->insertWidget(0, mixer.get());
    m_recommandedWidth = (mixer->minimumWidth() + 12 + line->minimumWidth()) * (qMin(2, int(m_mixers.size())));
    m_channelsBox->setMinimumWidth(m_recommandedWidth);
}

void MixerManager::deregisterTrack(int tid)
{
    Q_ASSERT(m_mixers.count(tid) > 0);
    m_mixers.erase(tid);
}

void MixerManager::cleanup()
{
    while (QLayoutItem* item = m_channelsLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_channelsLayout->addStretch(10);
    m_mixers.clear();
    if (m_masterMixer) {
        m_masterMixer->reset();
    }
}

void MixerManager::setModel(std::shared_ptr<TimelineItemModel> model)
{
    // Insert master mixer
    m_model = model;
    connect(m_model.get(), &TimelineItemModel::dataChanged, this, [&](const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &roles) {
        if (roles.contains(TimelineModel::IsDisabledRole)) {
            int id = (int) topLeft.internalId();
            if (m_mixers.count(id) > 0) {
                m_mixers[id]->setMute(m_model->data(topLeft, TimelineModel::IsDisabledRole).toBool());
            } else {
                qDebug()<<"=== MODEL DATA CHANGED: MUTE DONE TRACK NOT FOUND!!!";
            }
        }
    });

    Mlt::Tractor *service = model->tractor();
    if (m_masterMixer != nullptr) {
        // delete previous master mixer
        m_masterBox->removeWidget(m_masterMixer.get());
    }
    m_masterMixer.reset(new MixerWidget(-1, service, i18n("Master"), this));
    connect(m_masterMixer.get(), &MixerWidget::muteTrack, this, [&](int /*id*/, bool mute) {
        m_model->tractor()->set("hide", mute ? 3 : 1);
    });
    if (m_visibleMixerManager) {
        m_masterMixer->connectMixer(true);
    }
    connect(this, &MixerManager::updateLevels, m_masterMixer.get(), &MixerWidget::updateAudioLevel);
    connect(this, &MixerManager::clearMixers, m_masterMixer.get(), &MixerWidget::clear);
    m_masterBox->addWidget(m_masterMixer.get());
    if (KdenliveSettings::mixerCollapse()) {
        collapseMixers();
    }
}

void MixerManager::recordStateChanged(int tid, bool recording)
{
    if (m_mixers.count(tid) > 0) {
        m_mixers[tid]->setRecordState(recording);
    }
}

void MixerManager::connectMixer(bool doConnect)
{
    m_visibleMixerManager = doConnect;
    for (const auto &item : m_mixers) {
        item.second->connectMixer(m_visibleMixerManager && !KdenliveSettings::mixerCollapse());
    }
    if (m_masterMixer != nullptr) {
        m_masterMixer->connectMixer(m_visibleMixerManager);
    }
}

void MixerManager::collapseMixers()
{
    connectMixer(m_visibleMixerManager);
    if (KdenliveSettings::mixerCollapse()) {
        m_expandedWidth = width();
        m_channelsBox->setFixedWidth(0);
        //m_line->setMaximumWidth(0);
        setFixedWidth(m_masterMixer->width() + 2 * m_box->contentsMargins().left());
    } else {
        //m_line->setMaximumWidth(QWIDGETSIZE_MAX);
        m_channelsBox->setMaximumWidth(QWIDGETSIZE_MAX);
        m_channelsBox->setMinimumWidth(m_recommandedWidth);
        setFixedWidth(m_expandedWidth);
        QMetaObject::invokeMethod(this, "resetSizePolicy", Qt::QueuedConnection);
    }
}

void MixerManager::resetSizePolicy()
{
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumWidth(0);
}

QSize MixerManager::sizeHint() const
{
    return QSize(m_recommandedWidth, 0);
}

void MixerManager::pauseMonitoring(bool pause)
{
    for (const auto &item : m_mixers) {
        item.second->pauseMonitoring(pause);
    }
    if (m_masterMixer != nullptr) {
        m_masterMixer->pauseMonitoring(pause);
    }
}
