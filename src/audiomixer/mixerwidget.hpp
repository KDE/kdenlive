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

#ifndef MIXERWIDGET_H
#define MIXERWIDGET_H

#include "definitions.h"

#include "mlt++/MltService.h"

#include <memory>
#include <unordered_map>
#include <QWidget>
#include <QMutex>

class KDualAction;
class AudioLevelWidget;
class QSlider;
class QSpinBox;
class QToolButton;
class MixerManager;

namespace Mlt {
    class Tractor;
}

class MixerWidget : public QWidget
{
    Q_OBJECT

public:
    MixerWidget(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag, MixerManager *parent = nullptr);
    MixerWidget(int tid, Mlt::Tractor *service, const QString &trackTag, MixerManager *parent = nullptr);
    void buildUI(Mlt::Tractor *service, const QString &trackTag);
    /** @brief discard stored audio values and reset vu-meter to 0 if requested */
    void clear(bool reset = false);
    static void property_changed( mlt_service , MixerWidget *self, char *name );
    void setMute(bool mute);
    /** @brief Returs true if track is muted
     * */
    bool isMute() const;
    /** @brief Uncheck the solo button
     * */
    void unSolo();

public slots:
    void updateAudioLevel(int pos);
    void setAudioLevel(const QVector<int> vol);

protected:
    MixerManager *m_manager;
    int m_tid;
    std::shared_ptr<Mlt::Filter> m_levelFilter;
    std::shared_ptr<Mlt::Filter> m_monitorFilter;
    std::shared_ptr<Mlt::Filter> m_balanceFilter;
    QMap<int, QPair<int, int>> m_levels;
    KDualAction *m_muteAction;
    QSpinBox *m_balanceSpin;
    QSpinBox *m_volumeSpin;

private:
    std::shared_ptr<AudioLevelWidget> m_audioMeterWidget;
    QSlider *m_volumeSlider;
    QToolButton *m_solo;
    QMutex m_storeMutex;
    int m_lastVolume;

signals:
    void updateConnection(int, bool);
    void gotLevels(QPair <double, double>);
    void muteTrack(int tid, bool mute);
    void toggleSolo(int m_tid, bool toggled);
};

#endif

