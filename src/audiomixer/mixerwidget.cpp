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

#include "mixerwidget.hpp"

#include "mlt++/MltFilter.h"
#include "mlt++/MltTractor.h"
#include "mlt++/MltEvent.h"
#include "mlt++/MltProfile.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mixermanager.hpp"
#include "audiolevelwidget.hpp"
#include "capture/mediacapture.h"

#include <klocalizedstring.h>
#include <KDualAction>
#include <QGridLayout>
#include <QToolButton>
#include <QCheckBox>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QFontDatabase>

static inline double IEC_Scale(double dB)
{
    dB = log10(dB) * 20.0;
    double fScale = 1.0f;

    if (dB < -70.0f)
        fScale = 0.0f;
    else if (dB < -60.0f)
        fScale = (dB + 70.0f) * 0.0025f;
    else if (dB < -50.0f)
        fScale = (dB + 60.0f) * 0.005f + 0.025f;
    else if (dB < -40.0)
        fScale = (dB + 50.0f) * 0.0075f + 0.075f;
    else if (dB < -30.0f)
        fScale = (dB + 40.0f) * 0.015f + 0.15f;
    else if (dB < -20.0f)
        fScale = (dB + 30.0f) * 0.02f + 0.3f;
    else if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
        fScale = (dB + 20.0f) * 0.025f + 0.5f;

    return fScale;
}

static inline int fromDB(double level)
{
    int value = 60;
    if (level > 0.) {
        // increase volume
        value = 100 - ((pow(10, 1. - level/24) - 1) / .225);
    } else if (level < 0.) {
        value = (10 - pow(10, 1. - level/-50)) / -0.11395 + 59;
    }
    return value;
}

void MixerWidget::property_changed( mlt_service , MixerWidget *widget, char *name )
{
    if (widget && !strcmp(name, "_position")) {
        mlt_properties filter_props = MLT_FILTER_PROPERTIES( widget->m_monitorFilter->get_filter());
        int pos = mlt_properties_get_int(filter_props, "_position");
        if (!widget->m_levels.contains(pos)) {
            widget->m_levels[pos] = {IEC_Scale(mlt_properties_get_double(filter_props, "_audio_level.0")), IEC_Scale(mlt_properties_get_double(filter_props, "_audio_level.1"))};
            if (widget->m_levels.size() > widget->m_maxLevels) {
                widget->m_levels.erase(widget->m_levels.begin());
            }
        }
    }
}

MixerWidget::MixerWidget(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag, MixerManager *parent)
: QWidget(parent)
    , m_manager(parent)
    , m_tid(tid)
    , m_levelFilter(nullptr)
    , m_monitorFilter(nullptr)
    , m_balanceFilter(nullptr)
    , m_maxLevels(qMax(30, (int)(service->get_fps() * 1.5)))
    , m_solo(nullptr)
    , m_record(nullptr)
    , m_collapse(nullptr)
    , m_lastVolume(0)
    , m_listener(nullptr)
    , m_recording(false)
{
    buildUI(service.get(), trackTag);
}

MixerWidget::MixerWidget(int tid, Mlt::Tractor *service, const QString &trackTag, MixerManager *parent)
    : QWidget(parent)
    , m_manager(parent)
    , m_tid(tid)
    , m_levelFilter(nullptr)
    , m_monitorFilter(nullptr)
    , m_balanceFilter(nullptr)
    , m_maxLevels(qMax(30, (int)(service->get_fps() * 1.5)))
    , m_solo(nullptr)
    , m_record(nullptr)
    , m_collapse(nullptr)
    , m_lastVolume(0)
    , m_listener(nullptr)
    , m_recording(false)
{
    buildUI(service, trackTag);
}

MixerWidget::~MixerWidget()
{
    if (m_listener) {
        delete m_listener;
    }
}

void MixerWidget::buildUI(Mlt::Tractor *service, const QString &trackTag)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    // Build audio meter widget
    m_audioMeterWidget.reset(new AudioLevelWidget(width(), this));
    // intialize for stereo display
    m_audioMeterWidget->setAudioValues({-100, -100});

    // Build volume widget
    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(60);
    m_volumeSlider->setToolTip(i18n("Volume"));
    m_volumeSpin = new QDoubleSpinBox(this);
    m_volumeSpin->setRange(-50, 24);
    m_volumeSpin->setSuffix(i18n("dB"));
    m_volumeSpin->setFrame(false);

    connect(m_volumeSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&](double val) {
        m_volumeSlider->setValue(fromDB(val));
    });

    m_balanceDial = new QDial(this);
    m_balanceDial->setRange(-50, 50);
    m_balanceDial->setValue(0);

    m_balanceSpin = new QSpinBox(this);
    m_balanceSpin->setRange(-50, 50);
    m_balanceSpin->setValue(0);
    m_balanceSpin->setFrame(false);
    m_balanceSpin->setToolTip(i18n("Balance"));

    // Check if we already have build-in filters for this tractor
    int max = service->filter_count();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<Mlt::Filter> fl(service->filter(i));
        if (!fl->is_valid()) {
            continue;
        }
        const QString filterService = fl->get("mlt_service");
        if (filterService == QLatin1String("audiolevel")) {
            m_monitorFilter = fl;
        } else if (filterService == QLatin1String("volume")) {
            m_levelFilter = fl;
            int volume = m_levelFilter->get_int("level");
            m_volumeSpin->setValue(volume);
            m_volumeSlider->setValue(fromDB(volume));
        } else if (filterService == QLatin1String("panner")) {
            m_balanceFilter = fl;
            int val = m_balanceFilter->get_double("start") * 100 - 50;
            m_balanceSpin->setValue(val);
            m_balanceDial->setValue(val);
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
    if (m_balanceFilter == nullptr) {
        m_balanceFilter.reset(new Mlt::Filter(service->get_profile(), "panner"));
        if (m_balanceFilter->is_valid()) {
            m_balanceFilter->set("internal_added", 237);
            m_balanceFilter->set("start", 0.5);
            m_balanceFilter->set("disable", 1);
            service->attach(*m_balanceFilter.get());
        }
    }
    // Monitoring should be appended last so that other effects are reflected in audio monitor
    if (m_monitorFilter == nullptr) {
        m_monitorFilter.reset(new Mlt::Filter(service->get_profile(), "audiolevel"));
        if (m_monitorFilter->is_valid()) {
            m_monitorFilter->set("iec_scale", 0);
            service->attach(*m_monitorFilter.get());
        }
    }

    m_trackLabel = new QLabel(trackTag, this);
    m_trackLabel->setAutoFillBackground(true);
    m_trackLabel->setAlignment(Qt::AlignHCenter);
    m_trackLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_muteAction = new KDualAction(i18n("Mute track"), i18n("Unmute track"), this);
    m_muteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-hide-audio")));
    m_muteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-audio")));

    connect(m_balanceDial, &QDial::valueChanged, m_balanceSpin, &QSpinBox::setValue);

    connect(m_muteAction, &KDualAction::activeChangedByUser, [&](bool active) {
        if (m_tid == -1) {
            // Muting master, special case
            if (m_levelFilter) {
                if (active) {
                    m_lastVolume = m_levelFilter->get_int("level");
                    m_levelFilter->set("level", -1000);
                } else {
                    m_levelFilter->set("level", m_lastVolume);
                }
            }
        } else {
            emit muteTrack(m_tid, !active);
            reset();
        }
        updateLabel();
    });

    QToolButton *mute = new QToolButton(this);
    mute->setDefaultAction(m_muteAction);
    mute->setAutoRaise(true);

    // Setup default width
    QFontMetrics fm(font());
    setFixedWidth(3 * mute->sizeHint().width());

    if (m_tid > -1) {
        // No solo / rec button on master
        m_solo = new QToolButton(this);
        m_solo->setCheckable(true);
        m_solo->setIcon(QIcon::fromTheme("headphones"));
        m_solo->setToolTip(i18n("Solo mode"));
        m_solo->setAutoRaise(true);
        connect(m_solo, &QToolButton::toggled, [&](bool toggled) {
            emit toggleSolo(m_tid, toggled);
            updateLabel();
        });
        m_record = new QToolButton(this);
        m_record->setIcon(QIcon::fromTheme("media-record"));
        m_record->setToolTip(i18n("Record"));
        m_record->setCheckable(true);
        m_record->setAutoRaise(true);
        connect(m_record, &QToolButton::clicked, [&]() {
            m_manager->recordAudio(m_tid);
        });
    } else {
        m_collapse = new QToolButton(this);
        m_collapse->setIcon(KdenliveSettings::mixerCollapse() ? QIcon::fromTheme("arrow-left") : QIcon::fromTheme("arrow-right"));
        m_collapse->setToolTip(i18n("Show Channels"));
        m_collapse->setCheckable(true);
        m_collapse->setAutoRaise(true);
        m_collapse->setChecked(KdenliveSettings::mixerCollapse() );
        connect(m_collapse, &QToolButton::clicked, [&]() {
            KdenliveSettings::setMixerCollapse(m_collapse->isChecked());
            m_collapse->setIcon(m_collapse->isChecked() ? QIcon::fromTheme("arrow-left") : QIcon::fromTheme("arrow-right"));
            m_manager->collapseMixers();
        });
    }

    connect(m_volumeSlider, &QSlider::valueChanged, [&](int value) {
        QSignalBlocker bk(m_volumeSpin);
        if (m_recording) {
            m_volumeSpin->setValue(value);
            KdenliveSettings::setAudiocapturevolume(value);
            //TODO update capture volume
        } else if (m_levelFilter != nullptr) {
            double dbValue = 0;
            if (value > 60) {
                // increase volume
                dbValue = 24 * (1 - log10((100 - value)*0.225 + 1));
            } else if (value < 60) {
                dbValue = -50 * (1 - log10(10 - (value - 59)*(-0.11395)));
            }
            m_volumeSpin->setValue(dbValue);
            m_levelFilter->set("level", dbValue);
            m_levelFilter->set("disable", value == 60 ? 1 : 0);
            m_levels.clear();
            m_manager->purgeCache();
        }
    });
    connect(m_balanceSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int value) {
        QSignalBlocker bk(m_balanceDial);
        m_balanceDial->setValue(value);
        if (m_balanceFilter != nullptr) {
            m_balanceFilter->set("start", (value + 50) / 100.);
            m_balanceFilter->set("disable", value == 0 ? 1 : 0);
            m_levels.clear();
            m_manager->purgeCache();
        }
    });
    QVBoxLayout *lay = new QVBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    lay->setMargin(0);
    lay->addWidget(m_trackLabel);
    QHBoxLayout *buttonslay = new QHBoxLayout;
    buttonslay->setSpacing(0);
    buttonslay->setContentsMargins(0, 0, 0, 0);
    if (m_collapse) {
        buttonslay->addWidget(m_collapse);
    }
    buttonslay->addWidget(mute);
    if (m_solo) {
        buttonslay->addWidget(m_solo);
    }
    if (m_record) {
        buttonslay->addWidget(m_record);
    }
    lay->addLayout(buttonslay);
    lay->addWidget(m_balanceDial);
    lay->addWidget(m_balanceSpin);
    QHBoxLayout *hlay = new QHBoxLayout;
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
    if(event->button() == Qt::RightButton) {
        QWidget *child = childAt(event->pos());
        if (child == m_balanceDial) {
            m_balanceSpin->setValue(0);
        } else if (child == m_volumeSlider) {
            m_volumeSlider->setValue(60);
        }
    } else {
        QWidget::mousePressEvent(event);
    }
}

void MixerWidget::setMute(bool mute)
{
    m_muteAction->setActive(mute);
    m_volumeSlider->setEnabled(!mute);
    m_volumeSpin->setEnabled(!mute);
    m_audioMeterWidget->setEnabled(!mute);
    m_balanceSpin->setEnabled(!mute);
    m_balanceDial->setEnabled(!mute);
    updateLabel();
}

void MixerWidget::updateLabel()
{
    if (m_recording) {
        QPalette pal = m_trackLabel->palette();
        pal.setColor(QPalette::Window, Qt::red);
        m_trackLabel->setPalette(pal);
    } else if (m_muteAction->isActive()) {
        QPalette pal = m_trackLabel->palette();
        pal.setColor(QPalette::Window, QColor("#ff8c00"));
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
        m_audioMeterWidget->setAudioValues({m_levels.value(pos).first, m_levels.value(pos).second});
        //m_levels.remove(pos);
    } else {
        m_audioMeterWidget->setAudioValues({-100, -100});
    }
}


void MixerWidget::reset()
{
    QMutexLocker lk(&m_storeMutex);
    m_levels.clear();
    m_audioMeterWidget->setAudioValues({-100, -100});
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

void MixerWidget::gotRecLevels(QVector<qreal>levels)
{
    switch (levels.size()) {
        case 0:
            m_audioMeterWidget->setAudioValues({-100, -100});
            break;
        case 1:
            m_audioMeterWidget->setAudioValues({IEC_Scale(levels[0]), -100});
            break;
        default:
            m_audioMeterWidget->setAudioValues({IEC_Scale(levels[0]), IEC_Scale(levels[1])});
            break;
    }
}

void MixerWidget::setRecordState(bool recording)
{
    m_recording = recording;
    m_record->setChecked(m_recording);
    QSignalBlocker bk(m_volumeSpin);
    QSignalBlocker bk2(m_volumeSlider);
    if (m_recording) {
        connect(pCore->getAudioDevice(), &MediaCapture::audioLevels, this, &MixerWidget::gotRecLevels);
        m_balanceDial->setEnabled(false);
        m_balanceSpin->setEnabled(false);
        m_volumeSpin->setRange(0, 100);
        m_volumeSpin->setSuffix(QStringLiteral("%"));
        m_volumeSpin->setValue(KdenliveSettings::audiocapturevolume());
        m_volumeSlider->setValue(KdenliveSettings::audiocapturevolume());
    } else {
        m_balanceDial->setEnabled(true);
        m_balanceSpin->setEnabled(true);
        int level = m_levelFilter->get_int("level");
        disconnect(pCore->getAudioDevice(), &MediaCapture::audioLevels, this, &MixerWidget::gotRecLevels);
        m_volumeSpin->setRange(-100, 60);
        m_volumeSpin->setSuffix(i18n("dB"));
        m_volumeSpin->setValue(level);
        m_volumeSlider->setValue(fromDB(level));
    }
    updateLabel();
}

void MixerWidget::connectMixer(bool doConnect)
{
    if (doConnect) {
        if (m_listener == nullptr) {
            m_listener = m_monitorFilter->listen("property-changed", this, (mlt_listener)property_changed);
        }
    } else {
        delete m_listener;
        m_listener = nullptr;
    }
}
