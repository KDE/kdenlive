/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiographspectrum.h"
#include "../monitormanager.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"

#include <QAction>
#include <QFontDatabase>
#include <QGridLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageWidget>

#include <cmath>

#include <mlt++/Mlt.h>

// Code borrowed from Shotcut's audiospectum by Brian Matherly <code@brianmatherly.com> (GPL)

static const int WINDOW_SIZE = 8000; // 6 Hz FFT bins at 48kHz

struct band
{
    float low;    // Low frequency
    float center; // Center frequency
    float high;   // High frequency
    const char *label;
};

// Preferred frequencies from ISO R 266-1997 / ANSI S1.6-1984
static const band BAND_TAB[] = {
    //     Low      Preferred  High                Band
    //     Freq      Center    Freq     Label       Num
    {1.12f, 1.25f, 1.41f, "1.25"},            //  1
    {1.41f, 1.60f, 1.78f, "1.6"},             //  2
    {1.78f, 2.00f, 2.24f, "2.0"},             //  3
    {2.24f, 2.50f, 2.82f, "2.5"},             //  4
    {2.82f, 3.15f, 3.55f, "3.15"},            //  5
    {3.55f, 4.00f, 4.44f, "4.0"},             //  6
    {4.44f, 5.00f, 6.00f, "5.0"},             //  7
    {6.00f, 6.30f, 7.00f, "6.3"},             //  8
    {7.00f, 8.00f, 9.00f, "8.0"},             //  9
    {9.00f, 10.00f, 11.00f, "10"},            // 10
    {11.00f, 12.50f, 14.00f, "12.5"},         // 11
    {14.00f, 16.00f, 18.00f, "16"},           // 12
    {18.00f, 20.00f, 22.00f, "20"},           // 13 - First in audible range
    {22.00f, 25.00f, 28.00f, "25"},           // 14
    {28.00f, 31.50f, 35.00f, "31"},           // 15
    {35.00f, 40.00f, 45.00f, "40"},           // 16
    {45.00f, 50.00f, 56.00f, "50"},           // 17
    {56.00f, 63.00f, 71.00f, "63"},           // 18
    {71.00f, 80.00f, 90.00f, "80"},           // 19
    {90.00f, 100.00f, 112.00f, "100"},        // 20
    {112.00f, 125.00f, 140.00f, "125"},       // 21
    {140.00f, 160.00f, 179.00f, "160"},       // 22
    {179.00f, 200.00f, 224.00f, "200"},       // 23
    {224.00f, 250.00f, 282.00f, "250"},       // 24
    {282.00f, 315.00f, 353.00f, "315"},       // 25
    {353.00f, 400.00f, 484.00f, "400"},       // 26
    {484.00f, 500.00f, 560.00f, "500"},       // 27
    {560.00f, 630.00f, 706.00f, "630"},       // 28
    {706.00f, 800.00f, 897.00f, "800"},       // 29
    {897.00f, 1000.00f, 1121.00f, "1k"},      // 30
    {1121.00f, 1250.00f, 1401.00f, "1.3k"},   // 31
    {1401.00f, 1600.00f, 1794.00f, "1.6k"},   // 32
    {1794.00f, 2000.00f, 2242.00f, "2k"},     // 33
    {2242.00f, 2500.00f, 2803.00f, "2.5k"},   // 34
    {2803.00f, 3150.00f, 3531.00f, "3.2k"},   // 35
    {3531.00f, 4000.00f, 4484.00f, "4k"},     // 36
    {4484.00f, 5000.00f, 5605.00f, "5k"},     // 37
    {5605.00f, 6300.00f, 7062.00f, "6.3k"},   // 38
    {7062.00f, 8000.00f, 8908.00f, "8k"},     // 39
    {8908.00f, 10000.00f, 11210.00f, "10k"},  // 40
    {11210.00f, 12500.00f, 14012.00f, "13k"}, // 41
    {14012.00f, 16000.00f, 17936.00f, "16k"}, // 42
    {17936.00f, 20000.00f, 22421.00f, "20k"}, // 43 - Last in audible range
};

static const int FIRST_AUDIBLE_BAND_INDEX = 12;
static const int LAST_AUDIBLE_BAND_INDEX = 42;
static const int AUDIBLE_BAND_COUNT = LAST_AUDIBLE_BAND_INDEX - FIRST_AUDIBLE_BAND_INDEX + 1;
static const int PADDING = 2; // Space between the background and the content/bars
static const int MARGIN = 2;  // Space between the background and the labels/text or widget borders
static const int MIN_BAR_WIDTH = 2;

const float log_factor = 1.0f / log10f(1.0f / 127);

static inline float levelToDB(float dB)
{
    if (dB <= 0) {
        return 0;
    }
    return (1.0f - log10f(dB) * log_factor);
}

/*EqualizerWidget::EqualizerWidget(QWidget *parent) : QWidget(parent)
{
    QGridLayout *box = new QGridLayout(this);
    QStringList labels;
    labels << i18n("Master") << "50Hz" <<
"100Hz"<<"156Hz"<<"220Hz"<<"311Hz"<<"440Hz"<<"622Hz"<<"880Hz"<<"1.25kHz"<<"1.75kHz"<<"2.5kHz"<<"3.5kHz"<<"5kHz"<<"10kHz"<<"20kHz";
    for (int i = 0; i < 16; i++) {
        QSlider *sl = new QSlider(Qt::Vertical, this);
        sl->setObjectName(QString::number(i));
        box->addWidget(sl, 0, i);
        QLabel *lab = new QLabel(labels.at(i), this);
        box->addWidget(lab, 1, i);
    }
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}*/

AudioGraphWidget::AudioGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    m_dbLabels << -45 << -30 << -20 << -15 << -10 << -5 << -2 << 0;
    for (int i = FIRST_AUDIBLE_BAND_INDEX; i <= LAST_AUDIBLE_BAND_INDEX; i++) {
        m_freqLabels << BAND_TAB[i].label;
    }
    m_maxDb = 0;
    // space for drawing dB labels and all the bars with minimum width
    setMinimumWidth(MIN_BAR_WIDTH * m_freqLabels.size() + fontMetrics().horizontalAdvance(QStringLiteral("-45")) + MARGIN + 2 * PADDING);
    // setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void AudioGraphWidget::showAudio(const QVector<float> &bands)
{
    m_levels = bands;
    update();
}

void AudioGraphWidget::drawDbLabels(QPainter &p, const QRect &rect)
{
    int dbLabelCount = m_dbLabels.size();
    int textHeight = fontMetrics().ascent();

    if (dbLabelCount == 0) {
        return;
    }

    int maxWidth = fontMetrics().horizontalAdvance(QStringLiteral("-45"));
    // dB scale is vertical along the left side
    int prevY = height();
    QColor textCol = palette().text().color();
    textCol.setAlphaF(0.8);
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    QColor lineCol;
    if (isDarkTheme) {
        lineCol = QColor(150, 255, 200, 32);
    } else {
        lineCol = QColor(60, 140, 80, 80);
    }

    for (int i = 0; i < dbLabelCount; i++) {
        QString label = QString::number(m_dbLabels.at(i));
        int x = rect.left() + maxWidth - fontMetrics().horizontalAdvance(label);
        int yline = int(rect.bottom() - pow(10.0, double(m_dbLabels.at(i)) / 50.0) * rect.height() * 40.0 / 42);
        int y = yline + textHeight / 2;
        if (y - textHeight < rect.top()) {
            y = textHeight;
        }
        if (prevY - y >= 2) {
            p.setPen(textCol);
            p.drawText(x, y, label);
            p.setPen(lineCol);
            p.drawLine(rect.left() + maxWidth + MARGIN, yline, rect.width(), yline);
            prevY = y - textHeight;
        }
    }
}

void AudioGraphWidget::drawChanLabels(QPainter &p, const QRect &rect, int barWidth)
{
    int chanLabelCount = m_freqLabels.size();
    int stride = 1;

    if (chanLabelCount == 0) {
        return;
    }

    QColor textCol = palette().text().color();
    textCol.setAlphaF(0.8);
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    QColor lineCol;
    if (isDarkTheme) {
        lineCol = QColor(150, 255, 200, 32);
    } else {
        lineCol = QColor(60, 140, 80, 80);
    }

    // Channel labels are horizontal along the bottom.

    // Find the widest channel label
    int chanLabelWidth = 0;
    for (int i = 0; i < chanLabelCount; i++) {
        int width = fontMetrics().horizontalAdvance(m_freqLabels.at(i)) + 2;
        chanLabelWidth = width > chanLabelWidth ? width : chanLabelWidth;
    }
    int length = rect.width();
    while (chanLabelWidth * chanLabelCount / stride > length) {
        stride++;
    }

    int prevXText = 0;
    int y = rect.bottom();
    for (int i = 0; i < chanLabelCount; i += stride) {
        QString label = m_freqLabels.at(i);
        int xLine = rect.left() + (2 * i) + i * barWidth + barWidth / 2;
        int xText = xLine - fontMetrics().horizontalAdvance(label) / 2;
        if (xText > prevXText) {
            p.setPen(textCol);
            p.drawText(xText, y, label);

            p.setPen(lineCol);
            p.drawLine(xLine, rect.top(), xLine, rect.bottom() - fontMetrics().height());

            prevXText = xText + fontMetrics().horizontalAdvance(label);
        }
    }
}

void AudioGraphWidget::fillBackground(QPainter &p, const QRect &rect)
{
    QRectF fillRect = rect.toRectF();

    int linePenWidth = 1;
    int xOffset = fontMetrics().horizontalAdvance(QStringLiteral("-45")) + MARGIN - linePenWidth / 2.0;
    int yOffset = -fontMetrics().height();

    fillRect.adjust(xOffset, 0, 0, yOffset);
    p.fillRect(fillRect, palette().base().color());
}

void AudioGraphWidget::resizeEvent(QResizeEvent *event)
{
    drawBackground();
    QWidget::resizeEvent(event);
}

void AudioGraphWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        drawBackground();
    }
    QWidget::changeEvent(event);
}

void AudioGraphWidget::drawBackground()
{
    QSize s = size();
    if (!s.isValid()) {
        return;
    }
    qreal scalingFactor = devicePixelRatioF();
    m_pixmap = QPixmap(s * scalingFactor);
    m_pixmap.setDevicePixelRatio(scalingFactor);
    if (m_pixmap.isNull()) {
        return;
    }
    m_pixmap.fill(Qt::transparent);
    QPainter p(&m_pixmap);
    QRect rect(0, 0, width(), height());
    // p.setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));

    int textHeight = fontMetrics().height();
    int textWidth = fontMetrics().horizontalAdvance(QStringLiteral("-45"));

    rect = QRect(0, 0, width(), height());
    fillBackground(p, rect);

    rect = QRect(0, PADDING, width(), height() - (2 * PADDING + textHeight));
    drawDbLabels(p, rect);

    int chanLabelsXOffset = textWidth + MARGIN + PADDING;
    rect = QRect(chanLabelsXOffset, 0, width() - (chanLabelsXOffset + PADDING), height());
    int barWidth = (rect.width() - (2 * (AUDIBLE_BAND_COUNT - 1))) / AUDIBLE_BAND_COUNT;
    drawChanLabels(p, rect, barWidth);

    m_rect = QRect(chanLabelsXOffset, PADDING, width() - (chanLabelsXOffset + PADDING), height() - (2 * PADDING + textHeight));
}

void AudioGraphWidget::paintEvent(QPaintEvent *pe)
{
    QPainter p(this);
    p.setClipRect(pe->rect());
    p.drawPixmap(0, 0, m_pixmap);
    if (m_levels.isEmpty()) {
        return;
    }
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    int chanCount = m_levels.size();
    int height = m_rect.height();
    double barWidth = (m_rect.width() - (2.0 * (AUDIBLE_BAND_COUNT - 1))) / AUDIBLE_BAND_COUNT;
    p.setOpacity(0.7);
    QRectF rect(m_rect.left(), m_rect.top(), barWidth, height);
    for (int i = 0; i < chanCount; i++) {
        float level = (0.5 + m_levels.at(i)) / 1.5 * height;
        if (level < 0) {
            continue;
        }
        rect.moveLeft(m_rect.left() + i * barWidth + (2 * i));
        rect.setHeight(level);
        rect.moveBottom(m_rect.bottom());
        p.fillRect(rect, isDarkTheme ? Qt::white : Qt::black);
    }
}

AudioGraphSpectrum::AudioGraphSpectrum(MonitorManager *manager, QWidget *parent)
    : ScopeWidget(parent)
    , m_manager(manager)
{
    auto *lay = new QVBoxLayout(this);
    m_graphWidget = new AudioGraphWidget(this);
    lay->addWidget(m_graphWidget);

    /*m_equalizer = new EqualizerWidget(this);
    lay->addWidget(m_equalizer);
    lay->setStretchFactor(m_graphWidget, 5);
    lay->setStretchFactor(m_equalizer, 3);*/

    m_filter = new Mlt::Filter(pCore->getProjectProfile(), "fft");
    if (!m_filter->is_valid()) {
        KdenliveSettings::setEnableaudiospectrum(false);
        auto *mw = new KMessageWidget(this);
        mw->setCloseButtonVisible(false);
        mw->setWordWrap(true);
        mw->setMessageType(KMessageWidget::Information);
        mw->setText(i18n("MLT must be compiled with libfftw3 to enable Audio Spectrum"));
        layout()->addWidget(mw);
        mw->show();
        setEnabled(false);
        return;
    }
    m_filter->set("window_size", WINDOW_SIZE);
    QAction *a = new QAction(i18n("Enable Audio Spectrum"), this);
    a->setCheckable(true);
    a->setChecked(KdenliveSettings::enableaudiospectrum());
    if (KdenliveSettings::enableaudiospectrum() && isVisible()) {
        connect(m_manager, &MonitorManager::frameDisplayed, this, &ScopeWidget::onNewFrame, Qt::UniqueConnection);
    }
    connect(a, &QAction::triggered, this, &AudioGraphSpectrum::activate);
    addAction(a);
    setContextMenuPolicy(Qt::ActionsContextMenu);
}

AudioGraphSpectrum::~AudioGraphSpectrum()
{
    delete m_graphWidget;
    delete m_filter;
}

void AudioGraphSpectrum::dockVisible(bool visible)
{
    if (KdenliveSettings::enableaudiospectrum()) {
        if (!visible) {
            disconnect(m_manager, &MonitorManager::frameDisplayed, this, &ScopeWidget::onNewFrame);
        } else {
            connect(m_manager, &MonitorManager::frameDisplayed, this, &ScopeWidget::onNewFrame);
        }
    }
}

void AudioGraphSpectrum::activate(bool enable)
{
    if (enable) {
        connect(m_manager, &MonitorManager::frameDisplayed, this, &ScopeWidget::onNewFrame, Qt::UniqueConnection);
    } else {
        disconnect(m_manager, &MonitorManager::frameDisplayed, this, &ScopeWidget::onNewFrame);
    }
    KdenliveSettings::setEnableaudiospectrum(enable);
}

void AudioGraphSpectrum::refreshPixmap()
{
    if (m_graphWidget) {
        m_graphWidget->drawBackground();
    }
}

void AudioGraphSpectrum::refreshScope(const QSize & /*size*/, bool /*full*/)
{
    SharedFrame sFrame;
    while (m_queue.count() > 0) {
        sFrame = m_queue.pop();
        if (sFrame.is_valid() && sFrame.get_audio_samples() > 0) {
            mlt_audio_format format = mlt_audio_s16;
            int channels = sFrame.get_audio_channels();
            int frequency = sFrame.get_audio_frequency();
            int samples = sFrame.get_audio_samples();
            Mlt::Frame mFrame = sFrame.clone(true, false, false);
            m_filter->process(mFrame);
            mFrame.get_audio(format, frequency, channels, samples);
            if (samples == 0 || format == 0) {
                // There was an error processing audio from frame
                continue;
            }
            processSpectrum();
        }
    }
}

void AudioGraphSpectrum::processSpectrum()
{
    QVector<float> bands(AUDIBLE_BAND_COUNT);
    auto *bins = static_cast<float *>(m_filter->get_data("bins"));
    int bin_count = m_filter->get_int("bin_count");
    float bin_width = float(m_filter->get_double("bin_width"));

    int band = 0;
    bool firstBandFound = false;
    for (int bin = 0; bin < bin_count; bin++) {
        // Loop through all the FFT bins and align bin frequencies with
        // band frequencies.
        float F = bin_width * bin;

        if (!firstBandFound) {
            // Skip bins that come before the first band.
            if (BAND_TAB[band + FIRST_AUDIBLE_BAND_INDEX].low > F) {
                continue;
            } else {
                firstBandFound = true;
                bands[band] = bins[bin];
            }
        } else if (BAND_TAB[band + FIRST_AUDIBLE_BAND_INDEX].high < F) {
            // This bin is outside of this band - move to the next band.
            band++;
            if ((band + FIRST_AUDIBLE_BAND_INDEX) > LAST_AUDIBLE_BAND_INDEX) {
                // Skip bins that come after the last band.
                break;
            }
            bands[band] = bins[bin];
        } else if (bands[band] < bins[bin]) {
            // Pick the highest bin level within this band to represent the
            // whole band.
            bands[band] = bins[bin];
        }
    }

    // At this point, bands contains the magnitude of the signal for each
    // band. Convert to dB.
    for (band = 0; band < bands.size(); band++) {
        float mag = bands[band];
        float dB = mag > 0.0f ? levelToDB(mag) : -100.0;
        bands[band] = dB;
    }

    // Update the audio signal widget
    QMetaObject::invokeMethod(m_graphWidget, "showAudio", Qt::QueuedConnection, Q_ARG(QVector<float>, bands));
}
