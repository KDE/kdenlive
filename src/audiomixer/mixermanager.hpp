/*
 *   SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef MIXERMANGER_H
#define MIXERMANGER_H

#include "definitions.h"
#include <memory>
#include <unordered_map>

#include <QWidget>

namespace Mlt {
    class Tractor;
}

class MixerWidget;
class QHBoxLayout;
class TimelineItemModel;
class QScrollArea;

class MixerManager : public QWidget
{
    Q_OBJECT

public:
    MixerManager(QWidget *parent);
    /** @brief Shows the parameters of the given transition model */
    void registerTrack(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag, const QString &trackName);
    void deregisterTrack(int tid);
    void setModel(std::shared_ptr<TimelineItemModel> model);
    void cleanup();
    /** @brief Connect the mixer widgets to the correspondent filters */
    void connectMixer(bool doConnect);
    void collapseMixers();
    /** @brief Pause/unpause audio monitoring */
    void pauseMonitoring(bool pause);
    /** @brief Release the timeline model ownership */
    void unsetModel();

public slots:
    void recordStateChanged(int tid, bool recording);

private slots:
    void resetSizePolicy();

signals:
    void updateLevels(int);
    void recordAudio(int tid);
    void purgeCache();
    void clearMixers();
    void updateRecVolume();
    void showEffectStack(int tid);

protected:
    std::unordered_map<int, std::shared_ptr<MixerWidget>> m_mixers;
    std::shared_ptr<MixerWidget> m_masterMixer;
    QSize sizeHint() const override;

private:
    std::shared_ptr<Mlt::Tractor> m_masterService;
    std::shared_ptr<TimelineItemModel> m_model;
    QHBoxLayout *m_box;
    QHBoxLayout *m_masterBox;
    QHBoxLayout *m_channelsLayout;
    QScrollArea *m_channelsBox;
    int m_lastFrame;
    bool m_visibleMixerManager;
    int m_expandedWidth;
    QVector <int> m_soloMuted;
    int m_recommandedWidth;

};

#endif

