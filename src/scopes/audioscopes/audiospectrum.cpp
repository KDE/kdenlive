/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "audiospectrum.h"

#include "lib/audio/fftTools.h"
#include "lib/external/kiss_fft/tools/kiss_fftr.h"

#include <QPainter>
#include <QTime>

#include "klocalizedstring.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <iostream>

// (defined in the header file)
#ifdef DEBUG_AUDIOSPEC
#include "kdenlive_debug.h"
#endif

// (defined in the header file)
#ifdef DETECT_OVERMODULATION
#include <limits>
#include <cmath>
#endif

// Draw lines instead of single pixels.
// This is about 25 % faster, especially when enlarging the scope to e.g. 1680x1050 px.
#define AUDIOSPEC_LINES

#define MIN_DB_VALUE -120
#define MAX_FREQ_VALUE 96000
#define MIN_FREQ_VALUE 1000
#define ALPHA_MOVING_AVG 0.125
#define MAX_OVM_COLOR 0.7

AudioSpectrum::AudioSpectrum(QWidget *parent) :
    AbstractAudioScopeWidget(true, parent)
    , m_fftTools()
    , m_lastFFT()
    , m_lastFFTLock(1)
    , m_peaks()
#ifdef DEBUG_AUDIOSPEC
    , m_timeTotal(0)
    , m_showTotal(0)
#endif
    , m_dBmin(-70)
    , m_dBmax(0)
    , m_freqMax(0)
    , m_customFreq(false)
    , colorizeFactor(0)
{
    ui = new Ui::AudioSpectrum_UI;
    ui->setupUi(this);

    m_aResetHz = new QAction(i18n("Reset maximum frequency to sampling rate"), this);
    m_aTrackMouse = new QAction(i18n("Track mouse"), this);
    m_aTrackMouse->setCheckable(true);
    m_aShowMax = new QAction(i18n("Show maximum"), this);
    m_aShowMax->setCheckable(true);

    m_menu->addSeparator();
    m_menu->addAction(m_aResetHz);
    m_menu->addAction(m_aTrackMouse);
    m_menu->addAction(m_aShowMax);
    m_menu->removeAction(m_aRealtime);

    ui->windowSize->addItem(QStringLiteral("256"), QVariant(256));
    ui->windowSize->addItem(QStringLiteral("512"), QVariant(512));
    ui->windowSize->addItem(QStringLiteral("1024"), QVariant(1024));
    ui->windowSize->addItem(QStringLiteral("2048"), QVariant(2048));

    ui->windowFunction->addItem(i18n("Rectangular window"), FFTTools::Window_Rect);
    ui->windowFunction->addItem(i18n("Triangular window"), FFTTools::Window_Triangle);
    ui->windowFunction->addItem(i18n("Hamming window"), FFTTools::Window_Hamming);

    connect(m_aResetHz, &QAction::triggered, this, &AudioSpectrum::slotResetMaxFreq);
    connect(ui->windowFunction, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdate()));
    connect(this, &AudioSpectrum::signalMousePositionChanged, this, &AudioSpectrum::forceUpdateHUD);

    // Note: These strings are used in both Spectogram and AudioSpectrum. Ideally change both (if necessary) to reduce workload on translators
    ui->labelFFTSize->setToolTip(i18n("The maximum window size is limited by the number of samples per frame."));
    ui->windowSize->setToolTip(i18n("A bigger window improves the accuracy at the cost of computational power."));
    ui->windowFunction->setToolTip(i18n("The rectangular window function is good for signals with equal signal strength (narrow peak), but creates more smearing. See Window function on Wikipedia."));

    AbstractScopeWidget::init();
}
AudioSpectrum::~AudioSpectrum()
{
    writeConfig();

    delete m_aResetHz;
    delete m_aTrackMouse;
    delete ui;
}

void AudioSpectrum::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());

    ui->windowSize->setCurrentIndex(scopeConfig.readEntry("windowSize", 0));
    ui->windowFunction->setCurrentIndex(scopeConfig.readEntry("windowFunction", 0));
    m_aTrackMouse->setChecked(scopeConfig.readEntry("trackMouse", true));
    m_aShowMax->setChecked(scopeConfig.readEntry("showMax", true));
    m_dBmax = scopeConfig.readEntry("dBmax", 0);
    m_dBmin = scopeConfig.readEntry("dBmin", -70);
    m_freqMax = scopeConfig.readEntry("freqMax", 0);

    if (m_freqMax == 0) {
        m_customFreq = false;
        m_freqMax = 10000;
    } else {
        m_customFreq = true;
    }
}
void AudioSpectrum::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());

    scopeConfig.writeEntry("windowSize", ui->windowSize->currentIndex());
    scopeConfig.writeEntry("windowFunction", ui->windowFunction->currentIndex());
    scopeConfig.writeEntry("trackMouse", m_aTrackMouse->isChecked());
    scopeConfig.writeEntry("showMax", m_aShowMax->isChecked());
    scopeConfig.writeEntry("dBmax", m_dBmax);
    scopeConfig.writeEntry("dBmin", m_dBmin);
    if (m_customFreq) {
        scopeConfig.writeEntry("freqMax", m_freqMax);
    } else {
        scopeConfig.writeEntry("freqMax", 0);
    }

    scopeConfig.sync();
}

QString AudioSpectrum::widgetName() const
{
    return QStringLiteral("AudioSpectrum");
}

QImage AudioSpectrum::renderBackground(uint)
{
    return QImage();
}

QImage AudioSpectrum::renderAudioScope(uint, const audioShortVector &audioFrame, const int freq, const int num_channels,
                                       const int num_samples, const int)
{
    if (
        audioFrame.size() > 63
        && m_innerScopeRect.width() > 0 && m_innerScopeRect.height() > 0    // <= 0 if widget is too small (resized by user)
    ) {
        if (!m_customFreq) {
            m_freqMax = freq / 2;
        }

        QTime start = QTime::currentTime();

        /*******FIXME!!!
        #ifdef DETECT_OVERMODULATION
                bool overmodulated = false;
                int overmodulateCount = 0;

                for (int i = 0; i < audioFrame.size(); ++i) {
                    if (
                            audioFrame[i] == std::numeric_limits<qint16>::max()
                            || audioFrame[i] == std::numeric_limits<qint16>::min()) {
                        overmodulateCount++;
                        if (overmodulateCount > 3) {
                            overmodulated = true;
                            break;
                        }
                    }
                }
                if (overmodulated) {
                    colorizeFactor = 1;
                } else {
                    if (colorizeFactor > 0) {
                        colorizeFactor -= .08;
                        if (colorizeFactor < 0) {
                            colorizeFactor = 0;
                        }
                    }
                }
        #endif
        *******/

        // Determine the window size to use. It should be
        // * not bigger than the number of samples actually available
        // * divisible by 2
        int fftWindow = ui->windowSize->itemData(ui->windowSize->currentIndex()).toInt();
        if (fftWindow > num_samples) {
            fftWindow = num_samples;
        }
        if ((fftWindow & 1) == 1) {
            fftWindow--;
        }

        // Show the window size used, for information
        ui->labelFFTSizeNumber->setText(QVariant(fftWindow).toString());

        // Get the spectral power distribution of the input samples,
        // using the given window size and function
        float freqSpectrum[fftWindow / 2];
        FFTTools::WindowType windowType = (FFTTools::WindowType) ui->windowFunction->itemData(ui->windowFunction->currentIndex()).toInt();
        m_fftTools.fftNormalized(audioFrame, 0, num_channels, freqSpectrum, windowType, fftWindow, 0);

        // Store the current FFT window (for the HUD) and run the interpolation
        // for easy pixel-based dB value access
        QVector<float> dbMap;
        m_lastFFTLock.acquire();
        m_lastFFT = QVector<float>(fftWindow / 2);
        memcpy(m_lastFFT.data(), &(freqSpectrum[0]), fftWindow / 2 * sizeof(float));

        uint right = ((float) m_freqMax) / (m_freq / 2) * (m_lastFFT.size() - 1);
        dbMap = FFTTools::interpolatePeakPreserving(m_lastFFT, m_innerScopeRect.width(), 0, right, -180);
        m_lastFFTLock.release();

#ifdef DEBUG_AUDIOSPEC
        QTime drawTime = QTime::currentTime();
#endif

        // Draw the spectrum
        QImage spectrum(m_scopeRect.size(), QImage::Format_ARGB32);
        spectrum.fill(qRgba(0, 0, 0, 0));
        const uint w = m_innerScopeRect.width();
        const uint h = m_innerScopeRect.height();
        const uint leftDist = m_innerScopeRect.left() - m_scopeRect.left();
        const uint topDist = m_innerScopeRect.top() - m_scopeRect.top();
        QColor spectrumColor(AbstractScopeWidget::colDarkWhite);
        int yMax;

#ifdef DETECT_OVERMODULATION
        if (colorizeFactor > 0) {
            QColor col = AbstractScopeWidget::colHighlightDark;
            QColor spec = spectrumColor;
            float f = std::sin(M_PI_2 * colorizeFactor);
            spectrumColor = QColor(
                                (int)(f * col.red() + (1 - f) * spec.red()),
                                (int)(f * col.green() + (1 - f) * spec.green()),
                                (int)(f * col.blue() + (1 - f) * spec.blue()),
                                spec.alpha()
                            );
            // Limit the maximum colorization for non-overmodulated frames to better
            // recognize consecutively overmodulated frames
            if (colorizeFactor > MAX_OVM_COLOR) {
                colorizeFactor = MAX_OVM_COLOR;
            }
        }
#endif

#ifdef AUDIOSPEC_LINES
        QPainter davinci(&spectrum);
        davinci.setPen(QPen(QBrush(spectrumColor.rgba()), 1, Qt::SolidLine));
#endif

        for (uint i = 0; i < w; ++i) {
            yMax = (dbMap[i] - m_dBmin) / (m_dBmax - m_dBmin) * (h - 1);
            if (yMax < 0) {
                yMax = 0;
            } else if (yMax >= (int)h) {
                yMax = h - 1;
            }
#ifdef AUDIOSPEC_LINES
            davinci.drawLine(leftDist + i, topDist + h - 1, leftDist + i, topDist + h - 1 - yMax);
#else
            for (int y = 0; y < yMax && y < (int)h; ++y) {
                spectrum.setPixel(leftDist + i, topDist + h - y - 1, spectrumColor.rgba());
            }
#endif
        }

        // Calculate the peak values. Use the new value if it is bigger, otherwise adapt to lower
        // values using the Moving Average formula
        if (m_aShowMax->isChecked()) {
            davinci.setPen(QPen(QBrush(AbstractScopeWidget::colHighlightLight), 2));
            if (m_peaks.size() != fftWindow / 2) {
                m_peaks = QVector<float>(m_lastFFT);
            } else {
                for (int i = 0; i < fftWindow / 2; ++i) {
                    if (m_lastFFT[i] > m_peaks[i]) {
                        m_peaks[i] = m_lastFFT[i];
                    } else {
                        m_peaks[i] = ALPHA_MOVING_AVG * m_lastFFT[i] + (1 - ALPHA_MOVING_AVG) * m_peaks[i];
                    }
                }
            }
            int prev = 0;
            m_peakMap = FFTTools::interpolatePeakPreserving(m_peaks, m_innerScopeRect.width(), 0, right, -180);
            for (uint i = 0; i < w; ++i) {
                yMax = (m_peakMap[i] - m_dBmin) / (m_dBmax - m_dBmin) * (h - 1);
                if (yMax < 0) {
                    yMax = 0;
                } else if (yMax >= (int)h) {
                    yMax = h - 1;
                }

                davinci.drawLine(leftDist + i - 1, topDist + h - prev - 1, leftDist + i, topDist + h - yMax - 1);
                spectrum.setPixel(leftDist + i, topDist + h - yMax - 1, AbstractScopeWidget::colHighlightLight.rgba());
                prev = yMax;
            }
        }

#ifdef DEBUG_AUDIOSPEC
        m_showTotal++;
        m_timeTotal += drawTime.elapsed();
        qCDebug(KDENLIVE_LOG) << widgetName() << " took " << drawTime.elapsed() << " ms for drawing. Average: " << ((float)m_timeTotal / m_showTotal);
#endif

        emit signalScopeRenderingFinished(start.elapsed(), 1);

        return spectrum;
    } else {
        emit signalScopeRenderingFinished(0, 1);
        return QImage();
    }
}
QImage AudioSpectrum::renderHUD(uint)
{
    QTime start = QTime::currentTime();

    if (m_innerScopeRect.height() > 0 && m_innerScopeRect.width() > 0) { // May be below 0 if widget is too small

        // Minimum distance between two lines
        const uint minDistY = 30;
        const uint minDistX = 40;
        const uint textDistX = 10;
        const uint textDistY = 25;
        const uint topDist = m_innerScopeRect.top() - m_scopeRect.top();
        const uint leftDist = m_innerScopeRect.left() - m_scopeRect.left();
        const uint dbDiff = ceil((float)minDistY / m_innerScopeRect.height() * (m_dBmax - m_dBmin));
        const int mouseX = m_mousePos.x() - m_innerScopeRect.left();
        const int mouseY = m_mousePos.y() - m_innerScopeRect.top();

        QImage hud(m_scopeRect.size(), QImage::Format_ARGB32);
        hud.fill(qRgba(0, 0, 0, 0));

        QPainter davinci(&hud);
        davinci.setPen(AbstractScopeWidget::penLight);

        int y;
        for (int db = -dbDiff; db > m_dBmin; db -= dbDiff) {
            y = topDist + m_innerScopeRect.height() * ((float)db) / (m_dBmin - m_dBmax);
            if (y - topDist > m_innerScopeRect.height() - minDistY + 10) {
                // Abort here, there is still a line left for min dB to paint which needs some room.
                break;
            }
            davinci.drawLine(leftDist, y, leftDist + m_innerScopeRect.width() - 1, y);
            davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, y + 6, i18n("%1 dB", m_dBmax + db));
        }
        davinci.drawLine(leftDist, topDist, leftDist + m_innerScopeRect.width() - 1, topDist);
        davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, topDist + 6, i18n("%1 dB", m_dBmax));
        davinci.drawLine(leftDist, topDist + m_innerScopeRect.height() - 1, leftDist + m_innerScopeRect.width() - 1, topDist + m_innerScopeRect.height() - 1);
        davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, topDist + m_innerScopeRect.height() + 6, i18n("%1 dB", m_dBmin));

        const uint hzDiff = ceil(((float)minDistX) / m_innerScopeRect.width() * m_freqMax / 1000) * 1000;
        int x = 0;
        const int rightBorder = leftDist + m_innerScopeRect.width() - 1;
        y = topDist + m_innerScopeRect.height() + textDistY;
        for (int hz = 0; x <= rightBorder; hz += hzDiff) {
            davinci.setPen(AbstractScopeWidget::penLighter);
            x = leftDist + m_innerScopeRect.width() * ((float)hz) / m_freqMax;

            if (x <= rightBorder) {
                davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() + 6);
            }
            if (hz < m_freqMax && x + textDistY < leftDist + m_innerScopeRect.width()) {
                davinci.drawText(x - 4, y, QVariant(hz / 1000).toString());
            } else {
                x = leftDist + m_innerScopeRect.width();
                davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() + 6);
                davinci.drawText(x - 10, y, i18n("%1 kHz", QString("%1").arg((double)m_freqMax / 1000, 0, 'f', 1)));
            }

            if (hz > 0) {
                // Draw finer lines between the main lines
                davinci.setPen(AbstractScopeWidget::penLightDots);
                for (uint dHz = 3; dHz > 0; --dHz) {
                    x = leftDist + m_innerScopeRect.width() * ((float)hz - dHz * hzDiff / 4.0f) / m_freqMax;
                    if (x > rightBorder) {
                        break;
                    }
                    davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() - 1);
                }
            }
        }

        if (m_aTrackMouse->isChecked() && m_mouseWithinWidget && mouseX < m_innerScopeRect.width() - 1) {
            davinci.setPen(AbstractScopeWidget::penThin);

            x = leftDist + mouseX;

            float db = 0;
            float freq = ((float) mouseX) / (m_innerScopeRect.width() - 1) * m_freqMax;
            bool drawDb = false;

            m_lastFFTLock.acquire();
            // We need to test whether the mouse is inside the widget
            // because the position could already have changed in the meantime (-> crash)
            if (!m_lastFFT.isEmpty() && mouseX >= 0 && mouseX < m_innerScopeRect.width()) {
                uint right = ((float) m_freqMax) / (m_freq / 2) * (m_lastFFT.size() - 1);
                QVector<float> dbMap = FFTTools::interpolatePeakPreserving(m_lastFFT, m_innerScopeRect.width(), 0, right, -120);

                db = dbMap[mouseX];
                y = topDist + m_innerScopeRect.height() - 1 - (dbMap[mouseX] - m_dBmin) / (m_dBmax - m_dBmin) * (m_innerScopeRect.height() - 1);

                if (y < (int)topDist + m_innerScopeRect.height() - 1) {
                    drawDb = true;
                    davinci.drawLine(x, y, leftDist + m_innerScopeRect.width() - 1, y);
                }
            } else {
                y = topDist + mouseY;
            }
            m_lastFFTLock.release();

            if (y > (int)topDist + mouseY) {
                y = topDist + mouseY;
            }
            davinci.drawLine(x, y, x, topDist + m_innerScopeRect.height() - 1);

            if (drawDb) {
                QPoint dist(20, -20);
                QRect rect(
                    leftDist + mouseX + dist.x(),
                    topDist + mouseY + dist.y(),
                    100,
                    40
                );
                if (rect.right() > (int)leftDist + m_innerScopeRect.width() - 1) {
                    // Mirror the rectangle at the y axis to keep it inside the widget
                    rect = QRect(
                               rect.topLeft() - QPoint(rect.width() + 2 * dist.x(), 0),
                               rect.size());
                }

                QRect textRect(
                    rect.topLeft() + QPoint(12, 4),
                    rect.size()
                );

                davinci.fillRect(rect, AbstractScopeWidget::penBackground.brush());
                davinci.setPen(AbstractScopeWidget::penLighter);
                davinci.drawRect(rect);
                davinci.drawText(textRect, QString(
                                     i18n("%1 dB", QString("%1").arg(db, 0, 'f', 2))
                                     + '\n'
                                     + i18n("%1 kHz", QString("%1").arg(freq / 1000, 0, 'f', 2))));
            }

        }

        emit signalHUDRenderingFinished(start.elapsed(), 1);
        return hud;

    } else {
#ifdef DEBUG_AUDIOSPEC
        qCDebug(KDENLIVE_LOG) << "Widget is too small for painting inside. Size of inner scope rect is "
                              << m_innerScopeRect.width() << 'x' << m_innerScopeRect.height() << ".";
#endif
        emit signalHUDRenderingFinished(0, 1);
        return QImage();
    }

}

QRect AudioSpectrum::scopeRect()
{
    m_scopeRect = QRect(
                      QPoint(
                          10,                                     // Left
                          ui->verticalSpacer->geometry().top() + 6 // Top
                      ),
                      AbstractAudioScopeWidget::rect().bottomRight()
                  );
    m_innerScopeRect = QRect(
                           QPoint(
                               m_scopeRect.left() + 6,                 // Left
                               m_scopeRect.top() + 6                   // Top
                           ), QPoint(
                               ui->verticalSpacer->geometry().right() - 70,
                               ui->verticalSpacer->geometry().bottom() - 40
                           )
                       );
    return m_scopeRect;
}

void AudioSpectrum::slotResetMaxFreq()
{
    m_customFreq = false;
    forceUpdateHUD();
    forceUpdateScope();
}

///// EVENTS /////

void AudioSpectrum::handleMouseDrag(const QPoint &movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers)
{
    if (rescaleDirection == North) {
        // Nort-South direction: Adjust the dB scale

        if ((rescaleModifiers & Qt::ShiftModifier) == 0) {

            // By default adjust the min dB value
            m_dBmin += movement.y();

        } else {

            // Adjust max dB value if Shift is pressed.
            m_dBmax += movement.y();

        }

        // Ensure the dB values lie in [-100, 0] (or rather [MIN_DB_VALUE, 0])
        // 0 is the upper bound, everything below -70 dB is most likely noise
        if (m_dBmax > 0) {
            m_dBmax = 0;
        }
        if (m_dBmin < MIN_DB_VALUE) {
            m_dBmin = MIN_DB_VALUE;
        }
        // Ensure there is at least 6 dB between the minimum and the maximum value;
        // lower values hardly make sense
        if (m_dBmax - m_dBmin < 6) {
            if ((rescaleModifiers & Qt::ShiftModifier) == 0) {
                // min was adjusted; Try to adjust the max value to maintain the
                // minimum dB difference of 6 dB
                m_dBmax = m_dBmin + 6;
                if (m_dBmax > 0) {
                    m_dBmax = 0;
                    m_dBmin = -6;
                }
            } else {
                // max was adjusted, adjust min
                m_dBmin = m_dBmax - 6;
                if (m_dBmin < MIN_DB_VALUE) {
                    m_dBmin = MIN_DB_VALUE;
                    m_dBmax = MIN_DB_VALUE + 6;
                }
            }
        }

        forceUpdateHUD();
        forceUpdateScope();

    } else if (rescaleDirection == East) {
        // East-West direction: Adjust the maximum frequency
        m_freqMax -= 100 * movement.x();
        if (m_freqMax < MIN_FREQ_VALUE) {
            m_freqMax = MIN_FREQ_VALUE;
        }
        if (m_freqMax > MAX_FREQ_VALUE) {
            m_freqMax = MAX_FREQ_VALUE;
        }
        m_customFreq = true;

        forceUpdateHUD();
        forceUpdateScope();
    }
}

