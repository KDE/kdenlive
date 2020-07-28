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
    void registerTrack(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag);
    void deregisterTrack(int tid);
    void setModel(std::shared_ptr<TimelineItemModel> model);
    void cleanup();
    /** @brief Connect the mixer widgets to the correspondant filters */
    void connectMixer(bool doConnect);
    void collapseMixers();
    /** @brief Pause/unpause audio monitoring */
    void pauseMonitoring(bool pause);

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

