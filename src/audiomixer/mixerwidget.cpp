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
#include "mlt++/MltProfile.h"
#include "mixerwidget.hpp"
#include "mixermanager.hpp"
#include "audiolevelwidget.hpp"

#include <klocalizedstring.h>
#include <KDualAction>
#include <QGridLayout>
#include <QToolButton>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDial>
#include <QLabel>
#include <QFontDatabase>

const double log_factor = 1.0 / log10(1.0 / 127);

static inline int levelToDB(double level)
{
    if (level <= 0) {
        return -100;
    }
    return 100 * (1.0 - log10(level) * log_factor);
}

void MixerWidget::property_changed( mlt_service , MixerWidget *widget, char *name )
{
    //if (!widget->m_levels.contains(widget->m_manager->renderPosition)) {
    if (!strcmp(name, "_position")) {
        mlt_properties filter_props = MLT_FILTER_PROPERTIES( widget->m_monitorFilter->get_filter());
        int pos = mlt_properties_get_int(filter_props, "_position");
        if (!widget->m_levels.contains(pos)) {
        widget->m_levels[pos] = {levelToDB(mlt_properties_get_double(filter_props, "_audio_level.0")), levelToDB(mlt_properties_get_double(filter_props, "_audio_level.1"))};
        }
        //widget->m_levels[widget->m_manager->renderPosition] = {levelToDB(mlt_properties_get_double(filter_props, "_audio_level.0")), levelToDB(mlt_properties_get_double(filter_props, "_audio_level.1"))};
    //}
    }
}

MixerWidget::MixerWidget(int tid, std::shared_ptr<Mlt::Tractor> service, const QString &trackTag, MixerManager *parent)
: QWidget(parent)
    , m_manager(parent)
    , m_tid(tid)
    , m_levelFilter(nullptr)
    , m_monitorFilter(nullptr)
    , m_balanceFilter(nullptr)
    , m_solo(nullptr)
    , m_lastVolume(0)
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
    , m_solo(nullptr)
    , m_lastVolume(0)
{
    buildUI(service, trackTag);
}

void MixerWidget::buildUI(Mlt::Tractor *service, const QString &trackTag)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    //setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    // Build audio meter widget
    m_audioMeterWidget.reset(new AudioLevelWidget(width(), this));
    // intialize for stereo display
    m_audioMeterWidget->setAudioValues({-100, -100});

    // Build volume widget
    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(-100, 60);
    m_volumeSlider->setValue(0);
    m_volumeSlider->setToolTip(i18n("Volume"));
    m_volumeSpin = new QSpinBox(this);
    m_volumeSpin->setRange(-100, 60);
    m_volumeSpin->setSuffix(i18n("dB"));
    m_volumeSpin->setFrame(false);
    connect(m_volumeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int val) {
        m_volumeSlider->setValue(val);
    });

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
            m_volumeSlider->setValue(m_levelFilter->get_int("level"));
        } else if (filterService == QLatin1String("panner")) {
            m_balanceFilter = fl;
            m_balanceSpin->setValue(m_balanceFilter->get_double("start") * 100 + 50);
        }
    }
    // Build default filters if not found
    if (m_levelFilter == nullptr) {
        m_levelFilter.reset(new Mlt::Filter(service->get_profile(), "volume"));
        if (m_levelFilter->is_valid()) {
            service->attach(*m_levelFilter.get());
        }
    }
    if (m_balanceFilter == nullptr) {
        m_balanceFilter.reset(new Mlt::Filter(service->get_profile(), "panner"));
        if (m_balanceFilter->is_valid()) {
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

    QLabel *lab = new QLabel(trackTag, this);
    m_muteAction = new KDualAction(i18n("Mute track"), i18n("Unmute track"), this);
    m_muteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-hide-audio")));
    m_muteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-audio")));

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
            clear(true);
        }
    });

    QToolButton *mute = new QToolButton(this);
    mute->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    mute->setDefaultAction(m_muteAction);
    mute->setAutoRaise(true);

    if (m_tid > -1) {
        // No solo button on master
        m_solo = new QToolButton(this);
        m_solo->setCheckable(true);
        m_solo->setIcon(QIcon::fromTheme("headphones"));
        m_solo->setAutoRaise(true);
        connect(m_solo, &QToolButton::toggled, [&](bool toggled) {
            emit toggleSolo(m_tid, toggled);
        });
    }

    connect(m_volumeSlider, &QSlider::valueChanged, [&](int value) {
        m_volumeSpin->setValue(value);
        if (m_levelFilter != nullptr) {
            m_levelFilter->set("level", value);
        }
    });
    connect(m_balanceSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int value) {
        if (m_balanceFilter != nullptr) {
            m_balanceFilter->set("start", (value + 50) / 100.);
        }
    });
    QVBoxLayout *lay = new QVBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    lay->setMargin(0);
    lay->addWidget(lab);
    QHBoxLayout *buttonslay = new QHBoxLayout;
    buttonslay->setSpacing(0);
    buttonslay->addWidget(mute);
    if (m_solo) {
        buttonslay->addWidget(m_solo);
    }
    lay->addLayout(buttonslay);
    lay->addWidget(m_balanceSpin);
    QHBoxLayout *hlay = new QHBoxLayout;
    hlay->addWidget(m_audioMeterWidget.get());
    hlay->addWidget(m_volumeSlider);
    lay->addLayout(hlay);
    lay->addWidget(m_volumeSpin);
    lay->setStretch(3, 10);
    setLayout(lay);
    if (service->get_int("hide") > 1) {
        setMute(true);
    }
    m_monitorFilter->listen("property-changed", this, (mlt_listener)property_changed);
}

void MixerWidget::setMute(bool mute)
{
    m_muteAction->setActive(mute);
    m_volumeSlider->setEnabled(!mute);
    m_volumeSpin->setEnabled(!mute);
    m_audioMeterWidget->setEnabled(!mute);
    m_balanceSpin->setEnabled(!mute);
}

void MixerWidget::setAudioLevel(const QVector<int> vol)
{
    m_audioMeterWidget->setAudioValues(vol);
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

void MixerWidget::clear(bool reset)
{
    if (reset) {
        m_audioMeterWidget->setAudioValues({-100, -100});
    }
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

