/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "audiographspectrum.h"
#include "../monitor/monitormanager.h"
#include "kdenlivesettings.h"

#include <QAction>
#include <QFontDatabase>
#include <QGridLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "iecscale.h"
#include <mlt++/Mlt.h>

// Code borrowed from Shotcut's audiospectum by Brian Matherly <code@brianmatherly.com> (GPL)

static const int WINDOW_SIZE = 8000; // 6 Hz FFT bins at 48kHz

struct band
{
    float low;  // Low frequency
    float high; // High frequency
    const char *label;
};

// Preferred frequencies from ISO R 266-1997 / ANSI S1.6-1984
static const band BAND_TAB[] = {
    //     Low      Preferred  High                Band
    //     Freq      Center    Freq     Label       Num
    {1.12, 1.25, 1.41, "1.25"},            //  1
    {1.41, 1.60, 1.78, "1.6"},             //  2
    {1.78, 2.00, 2.24, "2.0"},             //  3
    {2.24, 2.50, 2.82, "2.5"},             //  4
    {2.82, 3.15, 3.55, "3.15"},            //  5
    {3.55, 4.00, 4.44, "4.0"},             //  6
    {4.44, 5.00, 6.00, "5.0"},             //  7
    {6.00, 6.30, 7.00, "6.3"},             //  8
    {7.00, 8.00, 9.00, "8.0"},             //  9
    {9.00, 10.00, 11.00, "10"},            // 10
    {11.00, 12.50, 14.00, "12.5"},         // 11
    {14.00, 16.00, 18.00, "16"},           // 12
    {18.00, 20.00, 22.00, "20"},           // 13 - First in audible range
    {22.00, 25.00, 28.00, "25"},           // 14
    {28.00, 31.50, 35.00, "31"},           // 15
    {35.00, 40.00, 45.00, "40"},           // 16
    {45.00, 50.00, 56.00, "50"},           // 17
    {56.00, 63.00, 71.00, "63"},           // 18
    {71.00, 80.00, 90.00, "80"},           // 19
    {90.00, 100.00, 112.00, "100"},        // 20
    {112.00, 125.00, 140.00, "125"},       // 21
    {140.00, 160.00, 179.00, "160"},       // 22
    {179.00, 200.00, 224.00, "200"},       // 23
    {224.00, 250.00, 282.00, "250"},       // 24
    {282.00, 315.00, 353.00, "315"},       // 25
    {353.00, 400.00, 484.00, "400"},       // 26
    {484.00, 500.00, 560.00, "500"},       // 27
    {560.00, 630.00, 706.00, "630"},       // 28
    {706.00, 800.00, 897.00, "800"},       // 29
    {897.00, 1000.00, 1121.00, "1k"},      // 30
    {1121.00, 1250.00, 1401.00, "1.3k"},   // 31
    {1401.00, 1600.00, 1794.00, "1.6k"},   // 32
    {1794.00, 2000.00, 2242.00, "2k"},     // 33
    {2242.00, 2500.00, 2803.00, "2.5k"},   // 34
    {2803.00, 3150.00, 3531.00, "3.2k"},   // 35
    {3531.00, 4000.00, 4484.00, "4k"},     // 36
    {4484.00, 5000.00, 5605.00, "5k"},     // 37
    {5605.00, 6300.00, 7062.00, "6.3k"},   // 38
    {7062.00, 8000.00, 8908.00, "8k"},     // 39
    {8908.00, 10000.00, 11210.00, "10k"},  // 40
    {11210.00, 12500.00, 14012.00, "13k"}, // 41
    {14012.00, 16000.00, 17936.00, "16k"}, // 42
    {17936.00, 20000.00, 22421.00, "20k"}, // 43 - Last in audible range
};

static const int FIRST_AUDIBLE_BAND_INDEX = 12;
static const int LAST_AUDIBLE_BAND_INDEX = 42;
static const int AUDIBLE_BAND_COUNT = LAST_AUDIBLE_BAND_INDEX - FIRST_AUDIBLE_BAND_INDEX + 1;

EqualizerWidget::EqualizerWidget(QWidget *parent)
    : QWidget(parent)
{
    QGridLayout *box = new QGridLayout(this);
    QStringList labels;
    labels << i18n("Master") << "50Hz"
           << "100Hz"
           << "156Hz"
           << "220Hz"
           << "311Hz"
           << "440Hz"
           << "622Hz"
           << "880Hz"
           << "1.25kHz"
           << "1.75kHz"
           << "2.5kHz"
           << "3.5kHz"
           << "5kHz"
           << "10kHz"
           << "20kHz";
    for (int i = 0; i < 16; i++) {
        QSlider *sl = new QSlider(Qt::Vertical, this);
        sl->setObjectName(QString::number(i));
        box->addWidget(sl, 0, i);
        QLabel *lab = new QLabel(labels.at(i), this);
        box->addWidget(lab, 1, i);
    }
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

AudioGraphWidget::AudioGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    m_dbLabels << -50 << -40 << -35 << -30 << -25 << -20 << -15 << -10 << -5 << 0;
    for (int i = FIRST_AUDIBLE_BAND_INDEX; i <= LAST_AUDIBLE_BAND_INDEX; i++) {
        m_freqLabels << BAND_TAB[i].label;
    }
    m_maxDb = 0;
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void AudioGraphWidget::showAudio(const QVector<double> &bands)
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

    int maxWidth = fontMetrics().width(QStringLiteral("-50"));
    // dB scale is vertical along the left side
    int prevY = height();
    QColor textCol = palette().text().color();
    QColor lineCol = textCol;
    p.setOpacity(0.8);
    lineCol.setAlpha(100);
    for (int i = 0; i < dbLabelCount; i++) {
        int value = m_dbLabels[i];
        QString label = QString::number(value);
        int x = rect.left() + maxWidth - fontMetrics().width(label);
        int yline = rect.bottom() - IEC_ScaleMax(value, m_maxDb) * rect.height();
        int y = yline + textHeight / 2;
        if (y - textHeight < 0) {
            y = textHeight;
        }
        if (prevY - y >= 2) {
            p.setPen(textCol);
            p.drawText(x, y, label);
            p.setPen(lineCol);
            p.drawLine(rect.left() + maxWidth + 2, yline, rect.width(), yline);
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

    p.setPen(palette().text().color().rgb());

    // Channel labels are horizontal along the bottom.

    // Find the widest channel label
    int chanLabelWidth = 0;
    for (int i = 0; i < chanLabelCount; i++) {
        int width = fontMetrics().width(m_freqLabels.at(i)) + 2;
        chanLabelWidth = width > chanLabelWidth ? width : chanLabelWidth;
    }
    int length = rect.width();
    while (chanLabelWidth * chanLabelCount / stride > length) {
        stride++;
    }

    int prevX = 0;
    int y = rect.bottom();
    for (int i = 0; i < chanLabelCount; i += stride) {
        QString label = m_freqLabels.at(i);
        int x = rect.left() + (2 * i) + i * barWidth + barWidth / 2 - fontMetrics().width(label) / 2;
        if (x > prevX) {
            p.drawText(x, y, label);
            prevX = x + fontMetrics().width(label);
        }
    }
}

void AudioGraphWidget::resizeEvent(QResizeEvent *event)
{
    drawBackground();
    QWidget::resizeEvent(event);
}

void AudioGraphWidget::drawBackground()
{
    QSize size(width(), height());
    if (!size.isValid()) {
        return;
    }
    m_pixmap = QPixmap(size);
    if (m_pixmap.isNull()) {
        return;
    }
    m_pixmap.fill(palette().base().color());
    QPainter p(&m_pixmap);
    QRect rect(0, 0, width() - 3, height());
    p.setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    drawDbLabels(p, rect);
    int offset = fontMetrics().width(QStringLiteral("888")) + 2;
    rect.adjust(offset, 0, 0, 0);
    int barWidth = (rect.width() - (2 * (AUDIBLE_BAND_COUNT - 1))) / AUDIBLE_BAND_COUNT;
    if (barWidth > 0) {
        drawChanLabels(p, rect, barWidth);
    }
    rect.adjust(0, 0, 0, -fontMetrics().height());
    m_rect = rect;
}

void AudioGraphWidget::paintEvent(QPaintEvent *pe)
{
    QPainter p(this);
    p.setClipRect(pe->rect());
    p.drawPixmap(0, 0, m_pixmap);
    if (m_levels.isEmpty()) {
        return;
    }
    int chanCount = m_levels.size();
    int height = m_rect.height();
    int barWidth = (m_rect.width() - (2 * (AUDIBLE_BAND_COUNT - 1))) / AUDIBLE_BAND_COUNT;
    p.setOpacity(0.6);
    for (int i = 0; i < chanCount; i++) {
        double level = IEC_ScaleMax(m_levels.at(i), m_maxDb) * height;
        p.fillRect(m_rect.left() + i * barWidth + (2 * i), height - level, barWidth, level, Qt::darkGreen);
    }
}

AudioGraphSpectrum::AudioGraphSpectrum(MonitorManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_filter(0)
    , m_graphWidget(0)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    m_graphWidget = new AudioGraphWidget(this);
    lay->addWidget(m_graphWidget);
    Mlt::Profile profile;
    m_filter = new Mlt::Filter(profile, "fft");
    if (!m_filter->is_valid()) {
        KdenliveSettings::setEnableaudiospectrum(false);
        KMessageWidget *mw = new KMessageWidget(this);
        mw->setCloseButtonVisible(false);
        mw->setWordWrap(true);
        mw->setMessageType(KMessageWidget::Information);
        mw->setText(i18n("MLT must be compiled with libfftw3 to enable Audio Spectrum"));
        lay->addWidget(mw);
        mw->show();
        setEnabled(false);
        return;
    }
    /*m_equalizer = new EqualizerWidget(this);
    lay->addWidget(m_equalizer);
    lay->setStretchFactor(m_graphWidget, 5);
    lay->setStretchFactor(m_equalizer, 3);*/

    m_filter->set("window_size", WINDOW_SIZE);
    connect(m_manager, &MonitorManager::updateAudioSpectrum, this, &AudioGraphSpectrum::processSpectrum);
    QAction *a = new QAction(i18n("Enable Audio Spectrum"), this);
    a->setCheckable(true);
    a->setChecked(KdenliveSettings::enableaudiospectrum());
    connect(a, &QAction::triggered, m_manager, &MonitorManager::activateAudioSpectrum);
    addAction(a);
    setContextMenuPolicy(Qt::ActionsContextMenu);
}

AudioGraphSpectrum::~AudioGraphSpectrum()
{
    delete m_graphWidget;
    // delete m_filter;
}

void AudioGraphSpectrum::refreshPixmap()
{
    if (m_graphWidget) {
        m_graphWidget->drawBackground();
    }
}

void AudioGraphSpectrum::processSpectrum(const SharedFrame &frame)
{
    if (!isVisible()) {
        return;
    }
    mlt_audio_format format = mlt_audio_s16;
    int channels = frame.get_audio_channels();
    int frequency = frame.get_audio_frequency();
    int samples = frame.get_audio_samples();
    Mlt::Frame mFrame = frame.clone(true, false, false);
    m_filter->process(mFrame);
    mFrame.get_audio(format, frequency, channels, samples);
    QVector<double> bands(AUDIBLE_BAND_COUNT);
    float *bins = (float *)m_filter->get_data("bins");
    int bin_count = m_filter->get_int("bin_count");
    double bin_width = m_filter->get_double("bin_width");

    int band = 0;
    bool firstBandFound = false;
    for (int bin = 0; bin < bin_count; bin++) {
        // Loop through all the FFT bins and align bin frequencies with
        // band frequencies.
        double F = bin_width * (double)bin;

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
        double mag = bands[band];
        double dB = mag > 0.0 ? 20 * log10(mag) : -1000.0;
        bands[band] = dB;
    }

    // Update the audio signal widget
    QMetaObject::invokeMethod(m_graphWidget, "showAudio", Qt::QueuedConnection, Q_ARG(const QVector<double> &, bands));
}
