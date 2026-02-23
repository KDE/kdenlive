/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mixerwidget.hpp"

#include "audiomixer/audiolevels/audiolevelwidget.hpp"
#include "audioslider.hpp"
#include "capture/mediacapture.h"
#include "core.h"
#include "iecscale.h"
#include "kdenlivesettings.h"
#include "mixermanager.hpp"
#include "mlt++/MltEvent.h"
#include "mlt++/MltFilter.h"
#include "mlt++/MltProfile.h"
#include "mlt++/MltTractor.h"
#include "utils/styledspinbox.hpp"

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
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>

// Neutral values for spin boxes
constexpr double NEUTRAL_VOLUME = 0.0;
constexpr int NEUTRAL_BALANCE = 0;

void MixerWidget::property_changed(mlt_service, MixerWidget *widget, mlt_event_data data)
{
    if (widget && !strcmp(Mlt::EventData(data).to_string(), "_position")) {
        mlt_properties filter_props = MLT_FILTER_PROPERTIES(widget->m_monitorFilter->get_filter());
        int pos = mlt_properties_get_int(filter_props, "_position");
        if (!widget->m_levels.contains(pos)) {
            QVector<double> levels;
            for (int i = 0; i < widget->m_channels; i++) {
                // NOTE: this is an approximation. To get the real peak level, we need version 2 of audiolevel MLT filter, see property_changedV2
                levels << log10(mlt_properties_get_double(filter_props, QStringLiteral("_audio_level.%1").arg(i).toUtf8().constData()) / 1.18) * 20;
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
                levels << mlt_properties_get_double(filter_props, QStringLiteral("_audio_level.%1").arg(i).toUtf8().constData());
            }
            widget->m_levels[pos] = std::move(levels);
            if (widget->m_levels.size() > widget->m_maxLevels) {
                widget->m_levels.erase(widget->m_levels.begin());
            }
        }
    }
}

MixerWidget::MixerWidget(int tid, Mlt::Tractor *service, QString trackTag, const QString &trackName, MixerManager *parent)
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
    , m_muteButton(nullptr)
    , m_showEffects(nullptr)
    , m_lastVolume(0)
    , m_listener(nullptr)
    , m_recording(false)
    , m_trackTag(std::move(trackTag))
    , m_backgroundColorRole(QPalette::Base)
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
    buildAudioMeter();
    buildVolumeControls();
    if (m_channels == 2) {
        buildBalanceControls();
    }
    buildTrackLabel(trackName);
    buildControlButtons();
    setupFilters(service);
    setupLayouts();
    setupConnections();

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    setMinimumWidth(3 * m_muteButton->sizeHint().width());
    setMaximumWidth(qMax(minimumWidth(), m_audioMeterWidget->maximumWidth() + m_volumeSlider->width() + 1));
    updateGeometry();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, getMixerBackgroundColor());
    setAutoFillBackground(true);
    setPalette(pal);

    updateTrackLabelStyle();

    if (m_tid == -1) {
        double volume = m_levelFilter->get_double("level");
        if (volume < -999) {
            setMute(true);
        }
    }
}

void MixerWidget::buildAudioMeter()
{
    m_audioMeterWidget.reset(new AudioLevelWidget(this, Qt::Vertical, AudioLevel::TickLabelsMode::Show, true));
    for (int i = 0; i < m_channels; i++) {
        m_audioData << -100;
    }
    m_audioMeterWidget->setAudioValues(m_audioData);
}

void MixerWidget::buildVolumeControls()
{
    // Default gain values for labels
    QVector<double> gainValues({-24, -10, -4, 0, 4, 10, 24});

    // Build volume widget
    // range is from -50dB to +50dB
    double neutralPosition = fromDB(0) * 100;
    m_volumeSlider = new AudioSlider(Qt::Vertical, this, false, neutralPosition);
    m_volumeSlider->setRange(0, 10000);
    m_volumeSlider->setValue(neutralPosition);
    m_volumeSlider->setSingleStep(50);
    m_volumeSlider->setToolTip(i18n("Volume"));
    m_volumeSlider->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the output volume of the audio track (affects all audio clips equally)."));
    m_volumeSlider->setTickPositions(gainValues);
    m_volumeSlider->setTicksVisible(true);
    m_volumeSlider->setTickLabelsVisible(true);

    // Set dB label formatter
    m_volumeSlider->setLabelFormatter([](double v) {
        if (v > 0.0)
            return i18nc("Gain in dB, positive", "%1", v);
        else
            return i18nc("Gain in dB, zero or negative", "%1", v);
    });
    // Set dB value-to-slider mapping
    m_volumeSlider->setValueToSliderFunction([](double dB) { return static_cast<int>(fromDB(dB) * 100.0); });

    m_volumeSpin = new StyledDoubleSpinBox(NEUTRAL_VOLUME, this);
    m_volumeSpin->setRange(-50, 24);
    m_volumeSpin->setFrame(true);
    m_volumeSpin->setKeyboardTracking(false);
    m_volumeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_volumeSpin->setDecimals(2);
    m_volumeSpin->setAlignment(Qt::AlignCenter);

    m_dbLabel = new QLabel(i18n("dB"), this);
    m_dbLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
}

void MixerWidget::buildBalanceControls()
{
    m_balanceSlider = new AudioSlider(Qt::Horizontal, this, true, 0);
    m_balanceSlider->setRange(-50, 50);
    m_balanceSlider->setValue(0);
    m_balanceSlider->setToolTip(i18n("Balance"));
    m_balanceSlider->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the output balance of the track. Negative values move the output towards the left, "
                                                            "positive values to the right. Affects all audio clips equally."));
    // Show only ticks for balance, no labels
    m_balanceSlider->setTicksVisible(true);
    m_balanceSlider->setTickLabelsVisible(false);
    QVector<double> balanceTicks = {0};
    m_balanceSlider->setTickPositions(balanceTicks);
    m_balanceSlider->setLabelFormatter([](double v) {
        if (v <= -50.0) return i18nc("Balance left", "L");
        if (v >= 50.0) return i18nc("Balance right", "R");
        if (v == 0.0) return i18nc("Balance center", "C");
        return QString::number(static_cast<int>(v));
    });
    // Set balance value-to-slider mapping (linear)
    m_balanceSlider->setValueToSliderFunction([](double v) { return static_cast<int>(v); });

    m_balanceLabelLeft = new QLabel(i18nc("Left", "L"), this);
    m_balanceLabelLeft->setAlignment(Qt::AlignHCenter);
    m_balanceLabelRight = new QLabel(i18nc("Right", "R"), this);
    m_balanceLabelRight->setAlignment(Qt::AlignHCenter);

    m_balanceSpin = new StyledSpinBox(NEUTRAL_BALANCE, this);
    m_balanceSpin->setRange(-50, 50);
    m_balanceSpin->setValue(0);
    m_balanceSpin->setFrame(true);
    m_balanceSpin->setToolTip(i18n("Balance"));
    m_balanceSpin->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the output balance of the track. Negative values move the output towards the left, "
                                                          "positive values to the right. Affects all audio clips equally."));
    m_balanceSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_balanceSpin->setAlignment(Qt::AlignCenter);
}

void MixerWidget::buildTrackLabel(const QString &trackName)
{
    m_trackLabel = new KSqueezedTextLabel(this);
    m_trackLabel->setAutoFillBackground(true);
    m_trackLabel->setAlignment(Qt::AlignHCenter);
    m_trackLabel->setTextElideMode(Qt::ElideRight);
    setTrackName(trackName);
}

void MixerWidget::buildControlButtons()
{
    m_muteAction = new KDualAction(i18n("Mute track"), i18n("Unmute track"), this);
    m_muteAction->setWhatsThis(xi18nc("@info:whatsthis", "Mutes/un-mutes the audio track."));
    m_muteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("audio-off")));
    m_muteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
    m_muteButton = new QToolButton(this);
    m_muteButton->setDefaultAction(m_muteAction);
    m_muteButton->setAutoRaise(true);

    m_showEffects = new QToolButton(this);
    m_showEffects->setIcon(QIcon::fromTheme("autocorrection"));
    m_showEffects->setToolTip(i18n("Open Effect Stack"));
    if (m_tid > -1) {
        m_showEffects->setWhatsThis(xi18nc("@info:whatsthis", "Opens the effect stack for the audio track."));
    } else {
        m_showEffects->setWhatsThis(xi18nc("@info:whatsthis", "Opens the effect stack for the audio master."));
    }
    m_showEffects->setAutoRaise(true);

    if (m_tid > -1) {
        // Solo / rec button only on tracks (not on master)
        m_solo = new QToolButton(this);
        m_solo->setCheckable(true);
        m_solo->setIcon(QIcon::fromTheme("headphones"));
        m_solo->setToolTip(i18n("Solo mode"));
        m_solo->setWhatsThis(xi18nc("@info:whatsthis", "When selected mutes all other audio tracks."));
        m_solo->setAutoRaise(true);

        m_monitor = new QToolButton(this);
        m_monitor->setIcon(QIcon::fromTheme("audio-input-microphone"));
        m_monitor->setToolTip(i18n("Monitor audio"));
        m_monitor->setWhatsThis(xi18nc("@info:whatsthis", "Puts the audio track into recording mode."));
        m_monitor->setCheckable(true);
        m_monitor->setAutoRaise(true);
    } else {
        m_collapse = new QToolButton(this);
        m_collapse->setIcon(KdenliveSettings::mixerCollapse() ? QIcon::fromTheme("arrow-left") : QIcon::fromTheme("arrow-right"));
        m_collapse->setToolTip(i18n("Show channels"));
        m_collapse->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of the audio track controls in the audio mixer view."));
        m_collapse->setCheckable(true);
        m_collapse->setAutoRaise(true);
        m_collapse->setChecked(KdenliveSettings::mixerCollapse());
    }
}

void MixerWidget::setupFilters(Mlt::Tractor *service)
{
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
            m_monitorFilter->set("internal_added", 237);
            if (m_manager->audioLevelV2()) {
                m_monitorFilter->set("dbpeak", 1);
            }
            service->attach(*m_monitorFilter.get());
        }
    }
}

void MixerWidget::setupLayouts()
{
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
    buttonslay->addWidget(m_muteButton);
    if (m_solo) {
        buttonslay->addWidget(m_solo);
    }
    if (m_monitor) {
        buttonslay->addWidget(m_monitor);
    }
    buttonslay->addWidget(m_showEffects);
    lay->addLayout(buttonslay);

    if (m_balanceSlider) {
        auto *balancelay = new QGridLayout;
        balancelay->addWidget(m_balanceSlider, 0, 0, 1, 3);
        balancelay->addWidget(m_balanceLabelLeft, 1, 0, 1, 1);
        balancelay->addWidget(m_balanceSpin, 1, 1, 1, 1);
        balancelay->addWidget(m_balanceLabelRight, 1, 2, 1, 1);
        lay->addLayout(balancelay);
    }

    auto *hlay = new QHBoxLayout;
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->addWidget(m_audioMeterWidget.get(), 1);
    hlay->addWidget(m_volumeSlider, 0);
    hlay->setSpacing(1);
    lay->addLayout(hlay, 1);

    // Add a horizontal layout for label + spinner, centered
    auto *volSpinLay = new QHBoxLayout;
    volSpinLay->setContentsMargins(0, 0, 0, 0);
    volSpinLay->setSpacing(4);
    volSpinLay->addStretch();
    volSpinLay->addWidget(m_volumeSpin);
    volSpinLay->addWidget(m_dbLabel);
    volSpinLay->addStretch();
    lay->addLayout(volSpinLay, 0);
    setLayout(lay);
}

void MixerWidget::setupConnections()
{
    connect(m_muteAction, &KDualAction::activeChangedByUser, this, [&](bool active) {
        if (m_tid == -1) {
            // Muting master, special case
            if (m_levelFilter) {
                if (active) {
                    m_lastVolume = m_levelFilter->get_double("level");
                    m_levelFilter->set("kdenlive:muted_level", m_lastVolume);
                    m_levelFilter->set("level", -1000);
                    m_levelFilter->set("disable", 0);
                } else {
                    m_lastVolume = m_levelFilter->get_double("kdenlive:muted_level");
                    m_levelFilter->set("level", m_lastVolume);
                    m_levelFilter->set("disable", qFuzzyIsNull(m_lastVolume) ? 1 : 0);
                    m_volumeSpin->setValue(m_lastVolume);
                    m_volumeSlider->setValue(fromDB(m_lastVolume) * 100.);
                }
            }
            setMute(active);
        } else {
            Q_EMIT muteTrack(m_tid, !active);
            reset();
        }
        pCore->setDocumentModified();
    });

    if (m_solo) {
        connect(m_solo, &QToolButton::toggled, this, [&](bool toggled) {
            Q_EMIT toggleSolo(m_tid, toggled);
            updateTrackLabelStyle();
        });
    }

    if (m_monitor) {
        connect(m_monitor, &QToolButton::toggled, this, [&](bool toggled) {
            if (!toggled && (m_recording || pCore->isMediaCapturing())) {
                // Abort recording if in progress
                Q_EMIT pCore->recordAudio(m_tid, false);
            }
            m_manager->monitorAudio(m_tid, toggled);
        });
    }

    if (m_collapse) {
        connect(m_collapse, &QToolButton::clicked, this, [&]() {
            KdenliveSettings::setMixerCollapse(m_collapse->isChecked());
            m_collapse->setIcon(m_collapse->isChecked() ? QIcon::fromTheme("arrow-left") : QIcon::fromTheme("arrow-right"));
            m_manager->collapseMixers();
        });
    }
    connect(m_showEffects, &QToolButton::clicked, this, [&]() { Q_EMIT m_manager->showEffectStack(m_tid); });

    connect(m_volumeSlider, &QAbstractSlider::valueChanged, this, [&](int value) {
        QSignalBlocker bk(m_volumeSlider);
        if (m_recording || (m_monitor && m_monitor->isChecked())) {
            m_volumeSpin->setValue(value / 100);
            KdenliveSettings::setAudiocapturevolume(value / 100);
            Q_EMIT m_manager->updateRecVolume();
        } else if (m_levelFilter != nullptr) {
            double dbValue = toDB(value / 100.0);
            m_volumeSpin->setValue(dbValue);
            m_levelFilter->set("level", dbValue);
            m_levelFilter->set("disable", value == 60 ? 1 : 0);
            m_levels.clear();
            Q_EMIT m_manager->purgeCache();
            pCore->setDocumentModified();
        }
    });
    connect(m_volumeSpin, &QDoubleSpinBox::valueChanged, this, [&](double val) {
        if (m_monitor && m_monitor->isChecked()) {
            m_volumeSlider->setValue(val * 100.);
        } else {
            m_volumeSlider->setValue(fromDB(val) * 100.);
        }
    });

    if (m_balanceSlider) {
        connect(m_balanceSlider, &QAbstractSlider::valueChanged, m_balanceSpin, [this]() {
            QSignalBlocker bk(m_balanceSlider);
            m_balanceSpin->setValue(m_balanceSlider->value());
        });
        connect(m_balanceSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
            QSignalBlocker bk(m_balanceSpin);
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
    connect(pCore.get(), &Core::updatePalette, this, [this]() {
        // Update all widgets were we customizing the palette or stylesheet
        QPalette mixerPalette = palette();
        mixerPalette.setColor(QPalette::Window, getMixerBackgroundColor());
        setPalette(mixerPalette);

        qDebug() << ":::: UPDATE PALETTE: " << getMixerBackgroundColor();

        QPalette pal = qApp->palette();
        if (m_dbLabel) {
            m_dbLabel->setPalette(pal);
        }
        if (m_balanceLabelLeft) {
            m_balanceLabelLeft->setPalette(pal);
        }
        if (m_balanceLabelRight) {
            m_balanceLabelRight->setPalette(pal);
        }
        if (m_volumeSpin) {
            m_volumeSpin->setPalette(pal);
        }
        if (m_balanceSpin) {
            m_balanceSpin->setPalette(pal);
        }
        updateTrackLabelStyle();
        update();
    });
}

void MixerWidget::setTrackName(const QString &name)
{
    // Show only tag on master or if name is empty
    if (name.isEmpty() || m_tid == -1) {
        m_trackLabel->setText(m_trackTag);
    } else {
        m_trackLabel->setText(QStringLiteral("%1 - %2").arg(m_trackTag, name));
    }
}

void MixerWidget::setMute(bool mute)
{
    QSignalBlocker bk(m_muteAction);
    m_muteAction->setActive(mute);
    m_volumeSlider->setEnabled(!mute);
    m_volumeSpin->setEnabled(!mute);
    m_audioMeterWidget->setEnabled(!mute);
    m_dbLabel->setEnabled(!mute);
    if (m_balanceSpin) {
        m_balanceSpin->setEnabled(!mute);
    }
    if (m_balanceSlider) {
        m_balanceSlider->setEnabled(!mute);
    }
    if (m_balanceLabelLeft) {
        m_balanceLabelLeft->setEnabled(!mute);
    }
    if (m_balanceLabelRight) {
        m_balanceLabelRight->setEnabled(!mute);
    }
    updateTrackLabelStyle();
    if (mute) {
        reset();
    }
}

void MixerWidget::updateTrackLabelStyle()
{
    QString style;
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    QColor borderColor = isDarkTheme ? palette().color(QPalette::Light).lighter(120) : palette().color(QPalette::Dark).darker(120);

    // Default: neutral background, normal text, underline in text color
    QString bg = QString("background-color: %1;").arg(palette().color(QPalette::Window).name(QColor::HexArgb));
    QString text = QString("color: %1;").arg(palette().color(QPalette::WindowText).name(QColor::HexArgb));
    QString underline = QString("border-bottom: 3px solid %1;").arg(palette().color(QPalette::WindowText).name(QColor::HexArgb));

    QString colorCode;
    if (m_recording) {
        colorCode = isDarkTheme ? "#c62828" : "#b71c1c"; // Darker red for light theme
    } else if (m_monitor && m_monitor->isChecked()) {
        colorCode = isDarkTheme ? "#1976d2" : "#0d47a1"; // Darker blue for light theme
    } else if (m_muteAction->isActive()) {
        colorCode = isDarkTheme ? "#ef6c00" : "#e65100"; // Darker orange for light theme
    } else if (m_solo && m_solo->isChecked()) {
        colorCode = isDarkTheme ? "#388e3c" : "#1b5e20"; // Darker green for light theme
    }

    if (!colorCode.isEmpty()) {
        text = QString("color: %1;").arg(colorCode);
        underline = QString("border-bottom: 3px solid %1;").arg(colorCode);
    }

    style = QString("%1 %2 padding: 2px; margin: 0; border: 1px solid %3; %4").arg(bg).arg(text).arg(borderColor.name(QColor::HexArgb)).arg(underline);
    m_trackLabel->setStyleSheet(style);
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
    m_audioMeterWidget->reset();
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
    if (m_monitor && m_monitor->isChecked()) {
        connect(pCore->getAudioDevice().get(), &MediaCapture::audioLevels, this, &MixerWidget::gotRecLevels);
        if (m_balanceSlider) {
            m_balanceSlider->setEnabled(false);
            m_balanceSpin->setEnabled(false);
        }
        m_volumeSpin->setRange(0, 100);
        m_dbLabel->setText(QStringLiteral("%"));
        m_volumeSlider->setValueToSliderFunction([](double v) { return static_cast<int>(v * 100.0); });
        QVector<double> tickValues({10, 20, 40, 60, 80, 90});
        m_volumeSlider->setTickPositions(tickValues);
        m_volumeSlider->setTickLabelsVisible(true);
        m_volumeSlider->setNeutralPosition(KdenliveSettings::audiocapturevolume() * 100);
        m_volumeSpin->setNeutralPosition(KdenliveSettings::audiocapturevolume());
        m_volumeSpin->setValue(KdenliveSettings::audiocapturevolume());
    } else {
        disconnect(pCore->getAudioDevice().get(), &MediaCapture::audioLevels, this, &MixerWidget::gotRecLevels);
        if (m_balanceSlider) {
            m_balanceSlider->setEnabled(true);
            m_balanceSpin->setEnabled(true);
        }
        int level = m_levelFilter->get_int("level");
        m_volumeSpin->setRange(-100, 60);
        m_dbLabel->setText(i18n("dB"));
        m_volumeSlider->setValueToSliderFunction([](double dB) { return static_cast<int>(fromDB(dB) * 100.0); });
        QVector<double> gainValues({-24, -10, -4, 0, 4, 10, 24});
        m_volumeSlider->setTickPositions(gainValues);
        m_volumeSlider->setTickLabelsVisible(true);
        m_volumeSlider->setNeutralPosition(fromDB(NEUTRAL_VOLUME) * 100);
        m_volumeSpin->setNeutralPosition(NEUTRAL_VOLUME);
        m_volumeSpin->setValue(level);
    }
    updateTrackLabelStyle();
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
    if (m_balanceLabelLeft) {
        m_balanceLabelLeft->setEnabled(!monitor);
    }
    if (m_balanceLabelRight) {
        m_balanceLabelRight->setEnabled(!monitor);
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

// Helper to update the style of spinboxes based on their value
void MixerWidget::updateSpinBoxStyle(QAbstractSpinBox *spin, double neutral)
{
    // Get current value
    double currentValue;
    if (auto *dspin = qobject_cast<QDoubleSpinBox *>(spin)) {
        currentValue = dspin->value();
    } else if (auto *ispin = qobject_cast<QSpinBox *>(spin)) {
        currentValue = ispin->value();
    } else {
        return;
    }
    // if value is not neutral / has been modified, change the text color to highlight
    bool isNeutral = qFuzzyCompare(currentValue, neutral);
    QColor textColor = isNeutral ? palette().color(QPalette::Text) : palette().highlight().color();

    QPalette pal = spin->palette();
    if (pal.color(QPalette::Text) != textColor) {
        pal.setColor(QPalette::Text, textColor);
        spin->setPalette(pal);
    }
}

QColor MixerWidget::getMixerBackgroundColor()
{
    QPalette palette = qApp->palette();
    if (m_tid == -1) return palette.color(QPalette::AlternateBase);
    return palette.color(m_backgroundColorRole);
}

void MixerWidget::setBackgroundColor(QPalette::ColorRole role)
{
    m_backgroundColorRole = role;
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getMixerBackgroundColor());
    setPalette(pal);
}
