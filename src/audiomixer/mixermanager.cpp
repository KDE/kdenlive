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
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>

MySlider::MySlider(QWidget *parent)
    : QSlider(parent)
{
}

int MySlider::getHandleHeight()
{
    // Get slider handle size
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    return style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle).height();
}

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
    // m_channelsLayout->setSpacing(4);
    m_channelsLayout->setSpacing(1);
    channelsBoxContainer->setLayout(m_channelsLayout);
    m_channelsLayout->addStretch(10);
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedWidth(3);
    m_box->addWidget(line);
    m_box->addLayout(m_masterBox);
    setLayout(m_box);
    MySlider slider;
    m_sliderHandle = slider.getHandleHeight();
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
        pCore->displayMessage(i18n("Monitoring audio. Press <b>Space</b> to start/pause recording, <b>Esc</b> to end."), InformationMessage, 8000);
        pCore->getAudioDevice()->switchMonitorState(true);
    }
    if (m_mixers.count(tid) > 0) {
        m_mixers[tid]->monitorAudio(true);
        pCore->monitorAudio(tid, true);
    } else {
        return;
    }
    m_monitorTrack = tid;
}

void MixerManager::registerTrack(int tid, Mlt::Tractor *service, const QString &trackTag, const QString &trackName)
{
    if (m_mixers.count(tid) > 0) {
        // Track already registered
        return;
    }
    std::shared_ptr<MixerWidget> mixer(new MixerWidget(tid, service, trackTag, trackName, m_sliderHandle, this));
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
    m_channelsLayout->insertWidget(0, mixer.get());
    m_recommendedWidth = (mixer->minimumWidth() + 1) * (qMin(2, int(m_mixers.size()))) + 3;
    if (!KdenliveSettings::mixerCollapse()) {
        m_channelsBox->setMinimumWidth(m_recommendedWidth);
    }
}

void MixerManager::deregisterTrack(int tid)
{
    Q_ASSERT(m_mixers.count(tid) > 0);
    m_mixers.erase(tid);
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
    m_masterMixer.reset(new MixerWidget(-1, service, i18n("Master"), QString(), m_sliderHandle, this));
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
        m_expandedWidth = width();
        m_channelsBox->setFixedWidth(0);
        // m_line->setMaximumWidth(0);
        if (!pCore->window()->isMixedTabbed()) {
            setFixedWidth(m_masterMixer->width() + 2 * m_box->contentsMargins().left());
        }
    } else {
        // m_line->setMaximumWidth(QWIDGETSIZE_MAX);
        m_channelsBox->setMaximumWidth(QWIDGETSIZE_MAX);
        m_channelsBox->setMinimumWidth(m_recommendedWidth);
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
