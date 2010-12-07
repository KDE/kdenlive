/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QPainter>

#include "spectrogram.h"

// Defines the number of FFT samples to store.
// Around 4 kB for a window size of 2000. Should be at least as large as the
// highest vertical screen resolution available for complete reconstruction.
// Can be less as a pre-rendered image is kept in space.
#define SPECTROGRAM_HISTORY_SIZE 1000

#define DEBUG_SPECTROGRAM
#ifdef DEBUG_SPECTROGRAM
#include <QDebug>
#endif

Spectrogram::Spectrogram(QWidget *parent) :
        AbstractAudioScopeWidget(false, parent),
        m_fftTools(),
        m_fftHistory(),
        m_fftHistoryImg()
{
    ui = new Ui::Spectrogram_UI;
    ui->setupUi(this);




    ui->windowSize->addItem("256", QVariant(256));
    ui->windowSize->addItem("512", QVariant(512));
    ui->windowSize->addItem("1024", QVariant(1024));
    ui->windowSize->addItem("2048", QVariant(2048));

    ui->windowFunction->addItem(i18n("Rectangular window"), FFTTools::Window_Rect);
    ui->windowFunction->addItem(i18n("Triangular window"), FFTTools::Window_Triangle);
    ui->windowFunction->addItem(i18n("Hamming window"), FFTTools::Window_Hamming);

    bool b = true;
    b &= connect(ui->windowFunction, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdate()));
    Q_ASSERT(b);

    AbstractScopeWidget::init();
}

Spectrogram::~Spectrogram()
{
    writeConfig();
}

void Spectrogram::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());

    ui->windowSize->setCurrentIndex(scopeConfig.readEntry("windowSize", 0));
    ui->windowFunction->setCurrentIndex(scopeConfig.readEntry("windowFunction", 0));
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
void Spectrogram::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());

    scopeConfig.writeEntry("windowSize", ui->windowSize->currentIndex());
    scopeConfig.writeEntry("windowFunction", ui->windowFunction->currentIndex());
    scopeConfig.writeEntry("dBmax", m_dBmax);
    scopeConfig.writeEntry("dBmin", m_dBmin);

    if (m_customFreq) {
        scopeConfig.writeEntry("freqMax", m_freqMax);
    } else {
        scopeConfig.writeEntry("freqMax", 0);
    }

    scopeConfig.sync();
}

QString Spectrogram::widgetName() const { return QString("Spectrogram"); }

QRect Spectrogram::scopeRect()
{
    m_scopeRect = QRect(
            QPoint(
                    10,                                     // Left
                    ui->verticalSpacer->geometry().top()+6  // Top
            ),
            AbstractAudioScopeWidget::rect().bottomRight()
    );
    m_innerScopeRect = QRect(
            QPoint(
                    m_scopeRect.left()+6,                   // Left
                    m_scopeRect.top()+6                     // Top
            ), QPoint(
                    ui->verticalSpacer->geometry().right()-70,
                    ui->verticalSpacer->geometry().bottom()-40
            )
    );
    return m_scopeRect;
}

QImage Spectrogram::renderHUD(uint) { return QImage(); }
QImage Spectrogram::renderAudioScope(uint, const QVector<int16_t> audioFrame, const int freq,
                                     const int num_channels, const int num_samples) {
    if (audioFrame.size() > 63) {
        if (!m_customFreq) {
            m_freqMax = freq / 2;
        }

        QTime start = QTime::currentTime();

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
        float freqSpectrum[fftWindow/2];
        FFTTools::WindowType windowType = (FFTTools::WindowType) ui->windowFunction->itemData(ui->windowFunction->currentIndex()).toInt();
        m_fftTools.fftNormalized(audioFrame, 0, num_channels, freqSpectrum, windowType, fftWindow, 0);

        QVector<float> spectrumVector(fftWindow/2);
        memcpy(spectrumVector.data(), &freqSpectrum[0], fftWindow/2 * sizeof(float));
        m_fftHistory.prepend(spectrumVector);

        // Limit the maximum history size to avoid wasting space
        while (m_fftHistory.size() > SPECTROGRAM_HISTORY_SIZE) {
            m_fftHistory.removeLast();
        }

        // Draw the spectrum
        QImage spectrum(m_scopeRect.size(), QImage::Format_ARGB32);
        spectrum.fill(qRgba(0,0,0,0));
        QPainter davinci(&spectrum);
        const uint w = m_innerScopeRect.width();
        const uint h = m_innerScopeRect.height();
        const uint leftDist = m_innerScopeRect.left() - m_scopeRect.left();
        const uint topDist = m_innerScopeRect.top() - m_scopeRect.top();
        float f;
        float x;
        float x_prev = 0;
        float val;
        uint windowSize;
        uint xi;
        uint y = topDist;
        bool completeRedraw = true;

        if (m_fftHistoryImg.size() == m_scopeRect.size()) {
            // The size of the widget has not changed since last time, so we can re-use it,
            // shift it by one pixel, and render the single remaining line. Usually about
            // 10 times faster for a widget height of around 400 px.
            davinci.drawImage(0, -1, m_fftHistoryImg);
            completeRedraw = false;
        }

        for (QList<QVector<float> >::iterator it = m_fftHistory.begin(); it != m_fftHistory.end(); it++) {

            windowSize = (*it).size();

            for (uint i = 0; i < w; i++) {

                // i:   Pixel coordinate
                // f:   Target frequency
                // x:   Frequency array index (float!) corresponding to the pixel
                // xi:  floor(x)
                // val: dB value at position x (Range: [-inf,0])

                f = i/((float) w-1.0) * m_freqMax;
                x = 2*f/freq * (windowSize - 1);
                xi = (int) floor(x);

                if (x >= windowSize) {
                    break;
                }

                // Use linear interpolation in order to get smoother display
                if (i == 0 || xi == windowSize-1) {
                    // ... except if we are at the left or right border of the display or the spectrum
                    val = (*it)[xi];
                } else {

                    if ((*it)[xi] > (*it)[xi+1]
                        && x_prev < xi) {
                        // This is a hack to preserve peaks.
                        // Consider f = {0, 100, 0}
                        //          x = {0.5,  1.5}
                        // Then x is 50 both times, and the 100 peak is lost.
                        // Get it back here for the first x after the peak.
                        val = (*it)[xi];
                    } else {
                        val =   (xi+1 - x) * (*it)[xi]
                              + (x - xi)   * (*it)[xi+1];
                    }
                }

                // Normalize to [0 1], 1 corresponding to 0 dB and 0 to dbMin dB
                val = -val/m_dBmin + 1;
                if (val < 0) {
                    val = 0;
                }

                spectrum.setPixel(leftDist + i, topDist + h-y-1, qRgba(255, 255, 255, val * 255));

                x_prev = x;
            }

            y++;
            if (y >= topDist + m_innerScopeRect.height()) {
                break;
            }
            if (!completeRedraw) {
                break;
            }
        }

#ifdef DEBUG_SPECTROGRAM
        qDebug() << "Rendered " << y-topDist << "lines from " << m_fftHistory.size() << " available samples in " << start.elapsed() << " ms"
                << (completeRedraw ? " (re-used old image)" : "");
        uint storedBytes = 0;
        for (QList< QVector<float> >::iterator it = m_fftHistory.begin(); it != m_fftHistory.end(); it++) {
            storedBytes += (*it).size() * sizeof((*it)[0]);
        }
        qDebug() << QString("Total storage used: %1 kB").arg((double)storedBytes/1000, 0, 'f', 2);
#endif

        m_fftHistoryImg = spectrum;

        emit signalScopeRenderingFinished(start.elapsed(), 1);
        return spectrum;
    } else {
        emit signalScopeRenderingFinished(0, 1);
        return QImage();
    }
}
QImage Spectrogram::renderBackground(uint) { return QImage(); }

bool Spectrogram::isHUDDependingOnInput() const { return false; }
bool Spectrogram::isScopeDependingOnInput() const { return false; }
bool Spectrogram::isBackgroundDependingOnInput() const { return false; }

#undef SPECTROGRAM_HISTORY_SIZE
#ifdef DEBUG_SPECTROGRAM
#undef DEBUG_SPECTROGRAM
#endif
