/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "mlt++/MltService.h"

#include <QAbstractSpinBox>
#include <QMutex>
#include <QWidget>
#include <memory>
#include <unordered_map>

class KDualAction;
class AudioLevelWidget;
class AudioSlider;
class QSlider;
class QDial;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QToolButton;
class MixerManager;
class KSqueezedTextLabel;

namespace Mlt {
class Tractor;
class Event;
} // namespace Mlt

class MixerWidget : public QWidget
{
    Q_OBJECT

public:
    MixerWidget(int tid, Mlt::Tractor *service, QString trackTag, const QString &trackName, MixerManager *parent = nullptr);
    ~MixerWidget() override;
    void buildUI(Mlt::Tractor *service, const QString &trackName);
    /** @brief discard stored audio values and reset vu-meter to 0 if requested */
    void reset();
    /** @brief discard stored audio values */
    void clear();
    static void property_changed(mlt_service, MixerWidget *self, mlt_event_data data);
    static void property_changedV2(mlt_service, MixerWidget *widget, mlt_event_data data);
    void setTrackName(const QString &name);
    void setMute(bool mute);
    /** @brief Returns true if track is muted
     * */
    bool isMute() const;
    /** @brief Uncheck the solo button
     * */
    void unSolo();
    /** @brief Connect the mixer widgets to the correspondent filters */
    void connectMixer(bool doConnect);
    /** @brief Disable/enable monitoring by disabling/enabling filter */
    void pauseMonitoring(bool pause);
    /** @brief Update widget to reflect state (monitor/record/none) */
    void updateMonitorState();
    /** @brief Enable/disable audio monitoring on this mixer */
    void monitorAudio(bool monitor);
    void setBackgroundColor(const QColor &color);
    void setBackgroundColor(QPalette::ColorRole role);

public Q_SLOTS:
    void updateAudioLevel(int pos);
    void setRecordState(bool recording);

private Q_SLOTS:
    void gotRecLevels(QVector<qreal> levels);

protected:
    MixerManager *m_manager;
    int m_tid;
    std::shared_ptr<Mlt::Filter> m_levelFilter;
    std::shared_ptr<Mlt::Filter> m_monitorFilter;
    std::shared_ptr<Mlt::Filter> m_balanceFilter;
    QMap<int, QVector<double>> m_levels;
    int m_channels;
    KDualAction *m_muteAction;
    QSpinBox *m_balanceSpin;
    AudioSlider *m_balanceSlider;
    QDoubleSpinBox *m_volumeSpin;
    int m_maxLevels;

private:
    std::shared_ptr<AudioLevelWidget> m_audioMeterWidget;
    AudioSlider *m_volumeSlider;
    QToolButton *m_solo;
    QToolButton *m_collapse;
    QToolButton *m_monitor;
    QToolButton *m_muteButton;
    QToolButton *m_showEffects;
    KSqueezedTextLabel *m_trackLabel;
    QMutex m_storeMutex;
    double m_lastVolume;
    QVector<double> m_audioData;
    Mlt::Event *m_listener;
    bool m_recording;
    const QString m_trackTag;
    QPalette::ColorRole m_backgroundColorRole;

    // Add label pointers for enabled/disabled style control
    QLabel *m_dbLabel;
    QLabel *m_balanceLabelLeft;
    QLabel *m_balanceLabelRight;

    // UI Building methods
    void buildAudioMeter();
    void buildVolumeControls();
    void buildBalanceControls();
    void buildTrackLabel(const QString &trackName);
    void buildControlButtons();
    void setupLayouts();
    void setupConnections();
    void setupFilters(Mlt::Tractor *service);
    void setupMasterControls();
    void setupTrackControls();

    /** @Update track label to reflect state */
    void updateTrackLabelStyle();
    QColor getMixerBackgroundColor();

    // Helper to set value and highlight color for spinboxes
    void updateSpinBoxStyle(QAbstractSpinBox *spin, double neutral);

Q_SIGNALS:
    void gotLevels(QPair<double, double>);
    void muteTrack(int tid, bool mute);
    void toggleSolo(int tid, bool toggled);
};
