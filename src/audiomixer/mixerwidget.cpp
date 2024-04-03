/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mixerwidget.hpp"

#include "audiolevelwidget.hpp"
#include "capture/mediacapture.h"
#include "core.h"
#include "iecscale.h"
#include "kdenlivesettings.h"
#include "mixermanager.hpp"
#include "mlt++/MltEvent.h"
#include "mlt++/MltFilter.h"
#include "mlt++/MltProfile.h"
#include "mlt++/MltTractor.h"

#include <KDualAction>
#include <KLocalizedString>
#include <KSqueezedTextLabel>
#include <QCheckBox>
#include <QDial>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>

void MixerWidget::property_changed(mlt_service, MixerWidget *widget, mlt_event_data data)
{
    if (widget && !strcmp(Mlt::EventData(data).to_string(), "_position")) {
        mlt_properties filter_props = MLT_FILTER_PROPERTIES(widget->m_monitorFilter->get_filter());
        int pos = mlt_properties_get_int(filter_props, "_position");
        if (!widget->m_levels.contains(pos)) {
            QVector<double> levels;
            for (int i = 0; i < widget->m_channels; i++) {
                // NOTE: this is an approximation. To get the real peak level, we need version 2 of audiolevel MLT filter, see property_changedV2
                levels << log10(mlt_properties_get_double(filter_props, QString("_audio_level.%1").arg(i).toUtf8().constData()) / 1.18) * 20;
            }
            widget->m_levels[pos] = std::move(levels);
            if (widget->m_levels.size() > widget->m_maxLevels) {
                widget->m_levels.erase(widget->m_levels.begin());
            }
        }
    }
}

void MixerWidget::property_changedV2(mlt_service, MixerWidget *widget, mlt_event_data data)
{
    if (widget && !strcmp(Mlt::EventData(data).to_string(), "_position")) {
        mlt_properties filter_props = MLT_FILTER_PROPERTIES(widget->m_monitorFilter->get_filter());
        int pos = mlt_properties_get_int(filter_props, "_position");
        if (!widget->m_levels.contains(pos)) {
            QVector<double> levels;
            for (int i = 0; i < widget->m_channels; i++) {
                levels << mlt_properties_get_double(filter_props, QString("_audio_level.%1").arg(i).toUtf8().constData());
            }
            widget->m_levels[pos] = std::move(levels);
            if (widget->m_levels.size() > widget->m_maxLevels) {
                widget->m_levels.erase(widget->m_levels.begin());
            }
        }
    }
}

MixerWidget::MixerWidget(int tid, Mlt::Tractor *service, QString trackTag, const QString &trackName, int sliderHandle, MixerManager *parent)
    : QWidget(parent)
    , m_manager(parent)
    , m_tid(tid)
    , m_levelFilter(nullptr)
    , m_monitorFilter(nullptr)
    , m_balanceFilter(nullptr)
    , m_channels(pCore->audioChannels())
    , m_balanceSpin(nullptr)
    , m_balanceSlider(nullptr)
    , m_maxLevels(qMax(30, int(service->get_fps() * 1.5)))
    , m_solo(nullptr)
    , m_collapse(nullptr)
    , m_monitor(nullptr)
    , m_lastVolume(0)
    , m_listener(nullptr)
    , m_recording(false)
    , m_trackTag(std::move(trackTag))
    , m_sliderHandleSize(sliderHandle)
{
    buildUI(service, trackName);
}

MixerWidget::~MixerWidget()
{
    if (m_listener) {
        delete m_listener;
    }
}

void MixerWidget::buildUI(Mlt::Tractor *service, const QString &trackName)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    // Build audio meter widget
    m_audioMeterWidget.reset(new AudioLevelWidget(width(), m_sliderHandleSize, this));
    // initialize for stereo display
    for (int i = 0; i < m_channels; i++) {
        m_audioData << -100;
    }
    m_audioMeterWidget->setAudioValues(m_audioData);

    // Build volume widget
    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 10000);
    m_volumeSlider->setValue(6000);
    m_volumeSlider->setSingleStep(50);
    m_volumeSlider->setToolTip(i18n("Volume"));
    m_volumeSlider->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the output volume of the audio track (affects all audio clips equally)."));
    m_volumeSpin = new QDoubleSpinBox(this);
    m_volumeSpin->setRange(-50, 24);
    m_volumeSpin->setSuffix(i18n("dB"));
    m_volumeSpin->setFrame(false);

    connect(m_volumeSpin, &QDoubleSpinBox::editingFinished, this, [&]() {
        double val = m_volumeSpin->value();
        if (m_monitor && m_monitor->isChecked()) {
            m_volumeSlider->setValue(val * 100.);
        } else {
            m_volumeSlider->setValue(fromDB(val) * 100.);
        }
    });

    QLabel *labelLeft = nullptr;
    QLabel *labelRight = nullptr;
    if (m_channels == 2) {
        m_balanceSlider = new QSlider(Qt::Horizontal, this);
        m_balanceSlider->setRange(-50, 50);
        m_balanceSlider->setValue(0);
        m_balanceSlider->setTickPosition(QSlider::TicksBelow);
        m_balanceSlider->setTickInterval(50);
        m_balanceSlider->setToolTip(i18n("Balance"));
        m_balanceSlider->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the output balance of the track. Negative values move the output towards the left, "
                                                                "positive values to the right. Affects all audio clips equally."));

        labelLeft = new QLabel(i18nc("Left", "L"), this);
        labelLeft->setAlignment(Qt::AlignHCenter);
        labelRight = new QLabel(i18nc("Right", "R"), this);
        labelRight->setAlignment(Qt::AlignHCenter);

        m_balanceSpin = new QSpinBox(this);
        m_balanceSpin->setRange(-50, 50);
        m_balanceSpin->setValue(0);
        m_balanceSpin->setFrame(false);
        m_balanceSpin->setToolTip(i18n("Balance"));
        m_balanceSpin->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the output balance of the track. Negative values move the output towards the left, "
                                                              "positive values to the right. Affects all audio clips equally."));
    }

    // Check if we already have built-in filters for this tractor
    int max = service->filter_count();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<Mlt::Filter> fl(service->filter(i));
        if (!fl->is_valid()) {
            continue;
        }
        const QString filterService = fl->get("mlt_service");
        if (filterService == QLatin1String("audiolevel")) {
            m_monitorFilter = fl;
            m_monitorFilter->set("disable", 0);
        } else if (filterService == QLatin1String("volume")) {
            m_levelFilter = fl;
            double volume = m_levelFilter->get_double("level");
            m_volumeSpin->setValue(volume);
            m_volumeSlider->setValue(fromDB(volume) * 100.);
        } else if (m_channels == 2 && filterService == QLatin1String("panner")) {
            m_balanceFilter = fl;
            int val = int(m_balanceFilter->get_double("start") * 100) - 50;
            m_balanceSpin->setValue(val);
            m_balanceSlider->setValue(val);
        }
    }
    // Build default filters if not found
    if (m_levelFilter == nullptr) {
        m_levelFilter.reset(new Mlt::Filter(service->get_profile(), "volume"));
        if (m_levelFilter->is_valid()) {
            m_levelFilter->set("internal_added", 237);
            m_levelFilter->set("disable", 1);
            service->attach(*m_levelFilter.get());
        }
    }
    if (m_balanceFilter == nullptr && m_channels == 2) {
        m_balanceFilter.reset(new Mlt::Filter(service->get_profile(), "panner"));
        if (m_balanceFilter->is_valid()) {
            m_balanceFilter->set("internal_added", 237);
            m_balanceFilter->set("start", 0.5);
            m_balanceFilter->set("disable", 1);
            service->attach(*m_balanceFilter.get());
        }
    }
    // Monitoring should be appended last so that other effects are reflected in audio monitor
    if (m_monitorFilter == nullptr && m_tid != -1) {
        m_monitorFilter.reset(new Mlt::Filter(service->get_profile(), "audiolevel"));
        if (m_monitorFilter->is_valid()) {
            m_monitorFilter->set("iec_scale", 0);
            if (m_manager->audioLevelV2()) {
                m_monitorFilter->set("dbpeak", 1);
            }
            service->attach(*m_monitorFilter.get());
        }
    }

    m_trackLabel = new KSqueezedTextLabel(this);
    m_trackLabel->setAutoFillBackground(true);
    m_trackLabel->setAlignment(Qt::AlignHCenter);
    m_trackLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_trackLabel->setTextElideMode(Qt::ElideRight);
    setTrackName(trackName);
    m_muteAction = new KDualAction(i18n("Mute track"), i18n("Unmute track"), this);
    m_muteAction->setWhatsThis(xi18nc("@info:whatsthis", "Mutes/un-mutes the audio track."));
    m_muteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("audio-off")));
    m_muteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-audio")));

    if (m_balanceSlider) {
        connect(m_balanceSlider, &QSlider::valueChanged, m_balanceSpin, &QSpinBox::setValue);
    }

    connect(m_muteAction, &KDualAction::activeChangedByUser, this, [&](bool active) {
        if (m_tid == -1) {
            // Muting master, special case
            if (m_levelFilter) {
                if (active) {
                    m_lastVolume = m_levelFilter->get_double("level");
                    m_levelFilter->set("level", -1000);
                    m_levelFilter->set("disable", 0);
                } else {
                    m_levelFilter->set("level", m_lastVolume);
                    m_levelFilter->set("disable", qFuzzyIsNull(m_lastVolume) ? 1 : 0);
                }
            }
        } else {
            Q_EMIT muteTrack(m_tid, !active);
            reset();
        }
        pCore->setDocumentModified();
        updateLabel();
    });

    auto *mute = new QToolButton(this);
    mute->setDefaultAction(m_muteAction);
    mute->setAutoRaise(true);

    QToolButton *showEffects = nullptr;

    // Setup default width
    setFixedWidth(3 * mute->sizeHint().width());

    if (m_tid > -1) {
        // Solo / rec button only on tracks (not on master)
        m_solo = new QToolButton(this);
        m_solo->setCheckable(true);
        m_solo->setIcon(QIcon::fromTheme("headphones"));
        m_solo->setToolTip(i18n("Solo mode"));
        m_solo->setWhatsThis(xi18nc("@info:whatsthis", "When selected mutes all other audio tracks."));
        m_solo->setAutoRaise(true);
        connect(m_solo, &QToolButton::toggled, this, [&](bool toggled) {
            Q_EMIT toggleSolo(m_tid, toggled);
            updateLabel();
        });

        m_monitor = new QToolButton(this);
        m_monitor->setIcon(QIcon::fromTheme("audio-input-microphone"));
        m_monitor->setToolTip(i18n("Record audio"));
        m_monitor->setWhatsThis(xi18nc("@info:whatsthis", "Puts the audio track into recording mode."));
        m_monitor->setCheckable(true);
        m_monitor->setAutoRaise(true);
        connect(m_monitor, &QToolButton::toggled, this, [&](bool toggled) {
            if (!toggled && (m_recording || pCore->isMediaCapturing())) {
                // Abort recording if in progress
                Q_EMIT pCore->recordAudio(m_tid, false);
            }
            m_manager->monitorAudio(m_tid, toggled);
        });
    } else {
        m_collapse = new QToolButton(this);
        m_collapse->setIcon(KdenliveSettings::mixerCollapse() ? QIcon::fromTheme("arrow-left") : QIcon::fromTheme("arrow-right"));
        m_collapse->setToolTip(i18n("Show channels"));
        m_collapse->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of the audio track controls in the audio mixer view."));
        m_collapse->setCheckable(true);
        m_collapse->setAutoRaise(true);
        m_collapse->setChecked(KdenliveSettings::mixerCollapse());
        connect(m_collapse, &QToolButton::clicked, this, [&]() {
            KdenliveSettings::setMixerCollapse(m_collapse->isChecked());
            m_collapse->setIcon(m_collapse->isChecked() ? QIcon::fromTheme("arrow-left") : QIcon::fromTheme("arrow-right"));
            m_manager->collapseMixers();
        });
    }
    showEffects = new QToolButton(this);
    showEffects->setIcon(QIcon::fromTheme("autocorrection"));
    showEffects->setToolTip(i18n("Open Effect Stack"));
    if (m_tid > -1) {
        showEffects->setWhatsThis(xi18nc("@info:whatsthis", "Opens the effect stack for the audio track."));
    } else {
        showEffects->setWhatsThis(xi18nc("@info:whatsthis", "Opens the effect stack for the audio master."));
    }
    showEffects->setAutoRaise(true);
    connect(showEffects, &QToolButton::clicked, this, [&]() { Q_EMIT m_manager->showEffectStack(m_tid); });

    connect(m_volumeSlider, &QSlider::valueChanged, this, [&](int value) {
        QSignalBlocker bk(m_volumeSpin);
        if (m_recording || (m_monitor && m_monitor->isChecked())) {
            m_volumeSpin->setValue(value / 100);
            KdenliveSettings::setAudiocapturevolume(value / 100);
            Q_EMIT m_manager->updateRecVolume();
            // TODO update capture volume
        } else if (m_levelFilter != nullptr) {
            double dbValue = 0;
            if (value > 6000) {
                // increase volume
                dbValue = 24 * (1 - log10((100 - value / 100.) * 0.225 + 1));
            } else if (value < 6000) {
                dbValue = -50 * (1 - log10(10 - (value / 100. - 59) * (-0.11395)));
            }
            m_volumeSpin->setValue(dbValue);
            m_levelFilter->set("level", dbValue);
            m_levelFilter->set("disable", value == 60 ? 1 : 0);
            m_levels.clear();
            Q_EMIT m_manager->purgeCache();
            pCore->setDocumentModified();
        }
    });
    if (m_balanceSlider) {
        connect(m_balanceSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
            QSignalBlocker bk(m_balanceSlider);
            m_balanceSlider->setValue(value);
            if (m_balanceFilter != nullptr) {
                m_balanceFilter->set("start", (value + 50) / 100.);
                m_balanceFilter->set("disable", value == 0 ? 1 : 0);
                m_levels.clear();
                Q_EMIT m_manager->purgeCache();
                pCore->setDocumentModified();
            }
        });
    }
    auto *lay = new QVBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_trackLabel);
    auto *buttonslay = new QHBoxLayout;
    buttonslay->setSpacing(0);
    buttonslay->setContentsMargins(0, 0, 0, 0);
    if (m_collapse) {
        buttonslay->addWidget(m_collapse);
    }
    buttonslay->addWidget(mute);
    if (m_solo) {
        buttonslay->addWidget(m_solo);
    }
    if (m_monitor) {
        buttonslay->addWidget(m_monitor);
    }
    if (showEffects) {
        buttonslay->addWidget(showEffects);
    }
    lay->addLayout(buttonslay);
    if (m_balanceSlider) {
        auto *balancelay = new QGridLayout;
        balancelay->addWidget(m_balanceSlider, 0, 0, 1, 3);
        balancelay->addWidget(labelLeft, 1, 0, 1, 1);
        balancelay->addWidget(m_balanceSpin, 1, 1, 1, 1);
        balancelay->addWidget(labelRight, 1, 2, 1, 1);
        lay->addLayout(balancelay);
    }
    auto *hlay = new QHBoxLayout;
    hlay->setSpacing(0);
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->addWidget(m_audioMeterWidget.get());
    hlay->addWidget(m_volumeSlider);
    lay->addLayout(hlay);
    lay->addWidget(m_volumeSpin);
    lay->setStretch(4, 10);
    setLayout(lay);
    if (service->get_int("hide") > 1) {
        setMute(true);
    }
}

void MixerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        QWidget *child = childAt(event->pos());
        if (child == m_balanceSlider) {
            m_balanceSpin->setValue(0);
        } else if (child == m_volumeSlider) {
            m_volumeSlider->setValue(6000);
        }
    } else {
        QWidget::mousePressEvent(event);
    }
}

void MixerWidget::setTrackName(const QString &name)
{
    // Show only tag on master or if name is empty
    if (name.isEmpty() || m_tid == -1) {
        m_trackLabel->setText(m_trackTag);
    } else {
        m_trackLabel->setText(QString("%1 - %2").arg(m_trackTag, name));
    }
}

void MixerWidget::setMute(bool mute)
{
    m_muteAction->setActive(mute);
    m_volumeSlider->setEnabled(!mute);
    m_volumeSpin->setEnabled(!mute);
    m_audioMeterWidget->setEnabled(!mute);
    if (m_balanceSlider) {
        m_balanceSpin->setEnabled(!mute);
        m_balanceSlider->setEnabled(!mute);
    }
    updateLabel();
}

void MixerWidget::updateLabel()
{
    if (m_recording) {
        QPalette pal = m_trackLabel->palette();
        pal.setColor(QPalette::Window, Qt::red);
        m_trackLabel->setPalette(pal);
    } else if (m_monitor && m_monitor->isChecked()) {
        QPalette pal = m_trackLabel->palette();
        pal.setColor(QPalette::Window, Qt::darkBlue);
        m_trackLabel->setPalette(pal);
    } else if (m_muteAction->isActive()) {
        QPalette pal = m_trackLabel->palette();
        pal.setColor(QPalette::Window, QColor(0xff8c00));
        m_trackLabel->setPalette(pal);
    } else if (m_solo && m_solo->isChecked()) {
        QPalette pal = m_trackLabel->palette();
        pal.setColor(QPalette::Window, Qt::darkGreen);
        m_trackLabel->setPalette(pal);
    } else {
        QPalette pal = palette();
        m_trackLabel->setPalette(pal);
    }
}

void MixerWidget::updateAudioLevel(int pos)
{
    QMutexLocker lk(&m_storeMutex);
    if (m_levels.contains(pos)) {
        m_audioMeterWidget->setAudioValues(m_levels.value(pos));
        // m_levels.remove(pos);
    } else {
        m_audioMeterWidget->setAudioValues(m_audioData);
    }
}

void MixerWidget::reset()
{
    QMutexLocker lk(&m_storeMutex);
    m_levels.clear();
    m_audioMeterWidget->setAudioValues(m_audioData);
}

void MixerWidget::clear()
{
    QMutexLocker lk(&m_storeMutex);
    m_levels.clear();
}

bool MixerWidget::isMute() const
{
    return m_muteAction->isActive();
}

void MixerWidget::unSolo()
{
    if (m_solo) {
        QSignalBlocker bl(m_solo);
        m_solo->setChecked(false);
    }
}

void MixerWidget::gotRecLevels(QVector<qreal> levels)
{
    switch (levels.size()) {
    case 0:
        m_audioMeterWidget->setAudioValues({-100, -100});
        break;
    case 1:
        m_audioMeterWidget->setAudioValues({levels[0]});
        break;
    default:
        m_audioMeterWidget->setAudioValues({levels[0], levels[1]});
        break;
    }
}

void MixerWidget::updateMonitorState()
{
    QSignalBlocker bk(m_volumeSpin);
    QSignalBlocker bk2(m_volumeSlider);
    if (m_monitor && m_monitor->isChecked()) {
        connect(pCore->getAudioDevice(), &MediaCapture::audioLevels, this, &MixerWidget::gotRecLevels);
        if (m_balanceSlider) {
            m_balanceSlider->setEnabled(false);
            m_balanceSpin->setEnabled(false);
        }
        m_volumeSpin->setRange(0, 100);
        m_volumeSpin->setSuffix(QStringLiteral("%"));
        m_volumeSpin->setValue(KdenliveSettings::audiocapturevolume());
        m_volumeSlider->setValue(KdenliveSettings::audiocapturevolume() * 100);
    } else {
        disconnect(pCore->getAudioDevice(), &MediaCapture::audioLevels, this, &MixerWidget::gotRecLevels);
        if (m_balanceSlider) {
            m_balanceSlider->setEnabled(true);
            m_balanceSpin->setEnabled(true);
        }
        int level = m_levelFilter->get_int("level");
        m_volumeSpin->setRange(-100, 60);
        m_volumeSpin->setSuffix(i18n("dB"));
        m_volumeSpin->setValue(level);
        m_volumeSlider->setValue(fromDB(level) * 100.);
    }
    updateLabel();
}

void MixerWidget::monitorAudio(bool monitor)
{
    QSignalBlocker bk(m_monitor);
    qDebug() << ":::: MONIOTORING AUDIO: " << monitor;
    if (monitor) {
        m_monitor->setChecked(true);
        updateMonitorState();
    } else {
        m_monitor->setChecked(false);
        updateMonitorState();
        reset();
    }
}

void MixerWidget::setRecordState(bool recording)
{
    m_recording = recording;
    updateMonitorState();
}

void MixerWidget::connectMixer(bool doConnect)
{
    if (doConnect) {
        if (m_tid == -1) {
            // Master level
            connect(pCore.get(), &Core::audioLevelsAvailable, m_audioMeterWidget.get(), &AudioLevelWidget::setAudioValues);
        } else if (m_listener == nullptr) {
            m_listener = m_monitorFilter->listen("property-changed", this,
                                                 m_manager->audioLevelV2() ? reinterpret_cast<mlt_listener>(property_changedV2)
                                                                           : reinterpret_cast<mlt_listener>(property_changed));
        }
    } else {
        if (m_tid == -1) {
            disconnect(pCore.get(), &Core::audioLevelsAvailable, m_audioMeterWidget.get(), &AudioLevelWidget::setAudioValues);
        } else {
            delete m_listener;
            m_listener = nullptr;
        }
    }
    pauseMonitoring(!doConnect);
}

void MixerWidget::pauseMonitoring(bool pause)
{
    if (m_monitorFilter) {
        m_monitorFilter->set("disable", pause ? 1 : 0);
    }
}
