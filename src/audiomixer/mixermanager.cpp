/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mixermanager.hpp"
#include "capture/mediacapture.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mixerseparator.h"
#include "mixerwidget.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include "mlt++/MltService.h"
#include "mlt++/MltTractor.h"

#include <KLocalizedString>
#include <QApplication>
#include <QHBoxLayout>
#include <QModelIndex>
#include <QScreen>
#include <QScrollArea>

constexpr QMargins kMarginAroundMixer = QMargins(6, 6, 6, 6);
constexpr QMargins kNoMargin = QMargins(0, 0, 0, 0);

MixerManager::MixerManager(QWidget *parent)
    : QWidget(parent)
    , m_masterMixer(nullptr)
    , m_visibleMixerManager(false)
    , m_expandedWidth(-1)
    , m_recommendedWidth(300)
    , m_monitorTrack(-1)
    , m_filterIsV2(false)
{
    m_masterBox = new QHBoxLayout;
    setContentsMargins(kNoMargin);
    m_channelsBox = new QScrollArea(this);
    m_channelsBox->setContentsMargins(kNoMargin);
    m_box = new QHBoxLayout;
    m_box->setContentsMargins(kNoMargin);
    m_box->setSpacing(0);
    auto *channelsBoxContainer = new QWidget(this);
    m_channelsBox->setWidget(channelsBoxContainer);
    m_channelsBox->setWidgetResizable(true);
    m_channelsBox->setFrameShape(QFrame::NoFrame);
    m_box->addWidget(m_channelsBox);
    m_channelsLayout = new QHBoxLayout;
    m_channelsLayout->setContentsMargins(kNoMargin);
    m_masterBox->setContentsMargins(kNoMargin);
    m_channelsLayout->setSpacing(0);
    channelsBoxContainer->setLayout(m_channelsLayout);
    m_channelsLayout->addStretch(10);
    m_masterSeparator = new MixerSeparator(this);
    m_box->addWidget(m_masterSeparator);
    m_box->addLayout(m_masterBox);
    setLayout(m_box);
}

void MixerManager::checkAudioLevelVersion()
{
    m_filterIsV2 = EffectsRepository::get()->exists(QStringLiteral("audiolevel")) && EffectsRepository::get()->getVersion(QStringLiteral("audiolevel")) > 100;
}

void MixerManager::monitorAudio(int tid, bool monitor)
{
    if (!monitor) {
        if (m_mixers.count(tid) > 0) {
            m_mixers[tid]->monitorAudio(false);
        }
        m_monitorTrack = -1;
        pCore->getAudioDevice()->switchMonitorState(false);
        pCore->monitorAudio(tid, false);
        return;
    }
    // We want to monitor audio
    if (m_monitorTrack > -1) {
        // Another track is monitoring
        if (m_mixers.count(m_monitorTrack) > 0) {
            m_mixers[m_monitorTrack]->monitorAudio(false);
            pCore->monitorAudio(m_monitorTrack, false);
        }
        m_monitorTrack = -1;
    } else {
        pCore->getAudioDevice()->switchMonitorState(true);
    }
    if (m_mixers.count(tid) > 0) {
        m_monitorTrack = tid;
        m_mixers[tid]->monitorAudio(true);
        pCore->monitorAudio(tid, true);
    } else {
        return;
    }
}

void MixerManager::registerTrack(int tid, Mlt::Tractor *service, const QString &trackTag, const QString &trackName)
{
    if (m_mixers.count(tid) > 0) {
        // Track already registered
        return;
    }
    std::shared_ptr<MixerWidget> mixer(new MixerWidget(tid, service, trackTag, trackName, this));
    mixer->setContentsMargins(kMarginAroundMixer);

    // Use alternating background colors for mixers
    int mixerCount = m_mixers.size();
    QPalette::ColorRole colorRole = (mixerCount % 2 == 0) ? QPalette::Base : QPalette::AlternateBase;
    mixer->setBackgroundColor(colorRole);

    connect(mixer.get(), &MixerWidget::muteTrack, this,
            [&](int id, bool mute) { m_model->setTrackProperty(id, "hide", mute ? QStringLiteral("1") : QStringLiteral("3")); });
    if (m_visibleMixerManager) {
        mixer->connectMixer(!KdenliveSettings::mixerCollapse());
    }
    connect(pCore.get(), &Core::updateMixerLevels, mixer.get(), &MixerWidget::updateAudioLevel);
    connect(this, &MixerManager::clearMixers, mixer.get(), &MixerWidget::clear);
    connect(mixer.get(), &MixerWidget::toggleSolo, this, [&](int trid, bool solo) {
        if (!solo) {
            // unmute
            for (int id : std::as_const(m_soloMuted)) {
                if (m_mixers.count(id) > 0) {
                    m_model->setTrackProperty(id, "hide", QStringLiteral("1"));
                }
            }
            m_soloMuted.clear();
        } else {
            if (!m_soloMuted.isEmpty()) {
                // Another track was solo, discard first
                for (int id : std::as_const(m_soloMuted)) {
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

    if (mixerCount > 0) {
        QWidget *separator = new MixerSeparator(this);
        m_channelsLayout->insertWidget(0, separator);
        m_separators[tid] = separator;
    }
    m_mixers[tid] = mixer;
    m_channelsLayout->insertWidget(0, mixer.get());

    m_recommendedWidth = (mixer->minimumWidth() + 1) * (qMin(2, int(m_mixers.size()))) + 3;
    if (!KdenliveSettings::mixerCollapse()) {
        m_channelsBox->setMinimumWidth(m_recommendedWidth);
    }
}

void MixerManager::deregisterTrack(int tid)
{
    Q_ASSERT(m_mixers.count(tid) > 0);

    // Remove the mixer widget
    QWidget *mixerWidget = m_mixers[tid].get();
    m_channelsLayout->removeWidget(mixerWidget);
    mixerWidget->deleteLater();

    // Remove the separator if it exists
    if (m_separators.count(tid) > 0) {
        QWidget *separator = m_separators[tid];
        m_channelsLayout->removeWidget(separator);
        separator->deleteLater();
        m_separators.erase(tid);
    }

    m_mixers.erase(tid);

    // Update background colors of remaining tracks to maintain alternating pattern
    int index = 0;
    for (auto &pair : m_mixers) {
        QPalette::ColorRole colorRole = (index % 2 == 0) ? QPalette::Base : QPalette::AlternateBase;
        pair.second->setBackgroundColor(colorRole);
        index++;
    }
}

void MixerManager::cleanup()
{
    while (QLayoutItem *item = m_channelsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_channelsLayout->addStretch(10);
    m_mixers.clear();
    m_separators.clear();
    m_monitorTrack = -1;
    if (m_masterMixer) {
        m_masterMixer->reset();
    }
}

void MixerManager::unsetModel()
{
    m_model.reset();
}

void MixerManager::setModel(std::shared_ptr<TimelineItemModel> model)
{
    // Insert master mixer
    m_model = model;
    connect(m_model.get(), &TimelineItemModel::dataChanged, this, [&](const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &roles) {
        if (roles.contains(TimelineModel::IsDisabledRole)) {
            int id = int(topLeft.internalId());
            if (m_mixers.count(id) > 0) {
                m_mixers[id]->setMute(m_model->data(topLeft, TimelineModel::IsDisabledRole).toBool());
            } else {
                qDebug() << "=== MODEL DATA CHANGED: MUTE DONE TRACK NOT FOUND!!!";
            }
        } else if (roles.contains(TimelineModel::NameRole)) {
            int id = int(topLeft.internalId());
            if (m_mixers.count(id) > 0) {
                qDebug() << "=== MODEL DATA CHANGED: CHANGED";
                m_mixers[id]->setTrackName(m_model->data(topLeft, TimelineModel::NameRole).toString());
            } else {
                qDebug() << "=== MODEL DATA CHANGED: CHANGE NAME DONE TRACK NOT FOUND!!!";
            }
        }
    });

    Mlt::Tractor *service = model->tractor();
    if (m_masterMixer != nullptr) {
        // delete previous master mixer
        m_masterBox->removeWidget(m_masterMixer.get());
    }
    m_masterMixer.reset(new MixerWidget(-1, service, i18n("Master"), QString(), this));
    m_masterMixer->setContentsMargins(kMarginAroundMixer);
    connect(m_masterMixer.get(), &MixerWidget::muteTrack, this, [&](int /*id*/, bool mute) { m_model->tractor()->set("hide", mute ? 3 : 1); });
    if (m_visibleMixerManager) {
        m_masterMixer->connectMixer(true);
    }
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
    Q_EMIT pCore->switchTimelineRecord(recording);
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
        m_channelsBox->setVisible(false);
        if (m_masterSeparator) m_masterSeparator->hide();
        if (m_masterMixer) {
            m_masterBox->setAlignment(Qt::AlignHCenter);
        }
    } else {
        m_channelsBox->setVisible(true);
        if (m_masterSeparator) m_masterSeparator->show();
        if (m_masterMixer) {
            m_masterBox->setAlignment(Qt::Alignment()); // Remove alignment
        }
    }
    QMetaObject::invokeMethod(this, "resetSizePolicy", Qt::QueuedConnection);
}

void MixerManager::resetSizePolicy()
{
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumWidth(0);
}

QSize MixerManager::sizeHint() const
{
    return QSize(m_recommendedWidth, 0);
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

int MixerManager::recordTrack() const
{
    return m_monitorTrack;
}

bool MixerManager::audioLevelV2() const
{
    return m_filterIsV2;
}