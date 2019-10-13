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

#include "mlt++/MltService.h"
#include "mlt++/MltTractor.h"

#include <klocalizedstring.h>
#include <QHBoxLayout>
#include <QModelIndex>

const double log_factor = 1.0 / log10(1.0 / 127);

static inline double levelToDB(double level)
{
    if (level <= 0) {
        return -100;
    }
    return 100 * (1.0 - log10(level) * log_factor);
}

MixerManager::MixerManager(QWidget *parent)
    : QWidget(parent)
    , renderPosition(0)
    , m_connectedWidgets(0)
{
    m_masterBox = new QHBoxLayout;
    m_box = new QHBoxLayout;
    m_box->addStretch(10);
    m_box->addLayout(m_masterBox);
    setLayout(m_box);
    m_timer.setSingleShot(true);
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, [&] () {
        for (auto w : m_mixers) {
            w.second->updateAudioLevel(m_lastFrame);
        }
    });
}

void MixerManager::registerTrack(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag)
{
    if (m_mixers.count(tid) > 0) {
        // Track already registered
        return;
    }
    std::shared_ptr<MixerWidget> mixer(new MixerWidget(tid, service, trackTag, this));
    connect(mixer.get(), &MixerWidget::muteTrack, [&](int id, bool mute) {
        m_model->setTrackProperty(id, "hide", mute ? QStringLiteral("1") : QStringLiteral("3"));
    });
    connect(this, &MixerManager::updateLevels, mixer.get(), &MixerWidget::updateAudioLevel);
    connect(mixer.get(), &MixerWidget::toggleSolo, [&](int trid, bool solo) {
        if (!solo) {
            // unmute
            for (int id : m_soloMuted) {
                if (m_mixers.count(id) > 0) {
                    m_model->setTrackProperty(id, "hide", QStringLiteral("1"));
                }
            }
            m_soloMuted.clear();
        } else {
            if (!m_soloMuted.isEmpty()) {
                // Another track was solo, discard first
                for (int id : m_soloMuted) {
                    if (m_mixers.count(id) > 0) {
                        m_model->setTrackProperty(id, "hide", QStringLiteral("1"));
                    }
                }
                m_soloMuted.clear();
            }
            for (auto item : m_mixers) {
                if (item.first > -1 && item.first != trid && !item.second->isMute()) {
                    m_model->setTrackProperty(item.first, "hide", QStringLiteral("3"));
                    m_soloMuted << item.first;
                    item.second->unSolo();
                }
            }
        }
    });
    m_mixers[tid] = mixer;
    m_box->insertWidget(0, mixer.get());
}

void MixerManager::deregisterTrack(int tid)
{
    Q_ASSERT(m_mixers.count(tid) > 0);
    m_mixers.erase(tid);
}

void MixerManager::resetAudioValues()
{
    qDebug()<<"======\n\nRESTTING AUDIO VALUES\n\n------------------_";
    for (auto item : m_mixers) {
        item.second.get()->clear();
    }
}

void MixerManager::cleanup()
{
    for (auto item : m_mixers) {
        if (item.first > -1) {
            m_box->removeWidget(item.second.get());
        }
    }
    while (!m_masterBox->isEmpty()) {
        m_masterBox->takeAt(0);
    }
    m_mixers.clear();
    m_connectedWidgets = 0;
}

void MixerManager::setModel(std::shared_ptr<TimelineItemModel> model)
{
    // Insert master mixer
    int tid = -1;
    m_model = model;
    connect(m_model.get(), &TimelineItemModel::dataChanged, [&](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
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
    if (m_mixers.count(tid) > 0) {
        // delete previous master mixer
        m_masterBox->removeWidget(m_mixers[tid].get());
        m_mixers.erase(tid);
    }
    std::shared_ptr<MixerWidget> mixer(new MixerWidget(tid, service, i18n("Master"), this));
    connect(mixer.get(), &MixerWidget::muteTrack, [&](int /*id*/, bool mute) {
        qDebug()<<"=== SETTING MUITE";
        m_model->tractor()->set("hide", mute ? 3 : 1);
    });
    connect(this, &MixerManager::updateLevels, mixer.get(), &MixerWidget::updateAudioLevel);
    m_mixers[tid] = mixer;
    m_masterBox->addWidget(mixer.get());
}

