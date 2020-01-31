/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "spectrogram.h"

#include <QPainter>
#include <QElapsedTimer>

#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>

// Defines the number of FFT samples to store.
// Around 4 kB for a window size of 2000. Should be at least as large as the
// highest vertical screen resolution available for complete reconstruction.
// Can be less as a pre-rendered image is kept in space.
#define SPECTROGRAM_HISTORY_SIZE 1000

// Uncomment for debugging
//#define DEBUG_SPECTROGRAM

#ifdef DEBUG_SPECTROGRAM
#include "kdenlive_debug.h"
#endif

#define MIN_DB_VALUE -120
#define MAX_FREQ_VALUE 96000
#define MIN_FREQ_VALUE 1000

Spectrogram::Spectrogram(QWidget *parent)
    : AbstractAudioScopeWidget(true, parent)
    , m_fftTools()
    , m_fftHistory()
    , m_fftHistoryImg()

{
    m_ui = new Ui::Spectrogram_UI;
    m_ui->setupUi(this);

    m_aResetHz = new QAction(i18n("Reset maximum frequency to sampling rate"), this);
    m_aGrid = new QAction(i18n("Draw grid"), this);
    m_aGrid->setCheckable(true);
    m_aTrackMouse = new QAction(i18n("Track mouse"), this);
    m_aTrackMouse->setCheckable(true);
    m_aHighlightPeaks = new QAction(i18n("Highlight peaks"), this);
    m_aHighlightPeaks->setCheckable(true);

    m_menu->addSeparator();
    m_menu->addAction(m_aResetHz);
    m_menu->addAction(m_aTrackMouse);
    m_menu->addAction(m_aGrid);
    m_menu->addAction(m_aHighlightPeaks);
    m_menu->removeAction(m_aRealtime);

    m_ui->windowSize->addItem(QStringLiteral("256"), QVariant(256));
    m_ui->windowSize->addItem(QStringLiteral("512"), QVariant(512));
    m_ui->windowSize->addItem(QStringLiteral("1024"), QVariant(1024));
    m_ui->windowSize->addItem(QStringLiteral("2048"), QVariant(2048));

    m_ui->windowFunction->addItem(i18n("Rectangular window"), FFTTools::Window_Rect);
    m_ui->windowFunction->addItem(i18n("Triangular window"), FFTTools::Window_Triangle);
    m_ui->windowFunction->addItem(i18n("Hamming window"), FFTTools::Window_Hamming);

    // Note: These strings are used in both Spectogram and AudioSpectrum. Ideally change both (if necessary) to reduce workload on translators
    m_ui->labelFFTSize->setToolTip(i18n("The maximum window size is limited by the number of samples per frame."));
    m_ui->windowSize->setToolTip(i18n("A bigger window improves the accuracy at the cost of computational power."));
    m_ui->windowFunction->setToolTip(i18n("The rectangular window function is good for signals with equal signal strength (narrow peak), but creates more "
                                          "smearing. See Window function on Wikipedia."));

    connect(m_aResetHz, &QAction::triggered, this, &Spectrogram::slotResetMaxFreq);
    connect(m_ui->windowFunction, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdate()));
    connect(this, &Spectrogram::signalMousePositionChanged, this, &Spectrogram::forceUpdateHUD);

    AbstractScopeWidget::init();

    for (int i = 0; i <= 255 / 5; ++i) {
        m_colorMap[i + 0 * 255 / 5] = qRgb(0, 0, i * 5);         // black to blue
        m_colorMap[i + 1 * 255 / 5] = qRgb(0, i * 5, 255);       // blue to cyan
        m_colorMap[i + 2 * 255 / 5] = qRgb(0, 255, 255 - i * 5); // cyan to green
        m_colorMap[i + 3 * 255 / 5] = qRgb(i * 5, 255, 0);       // green to yellow
        m_colorMap[i + 4 * 255 / 5] = qRgb(255, 255 - i * 5, 0); // yellow to red
    }
}

Spectrogram::~Spectrogram()
{
    writeConfig();

    delete m_aResetHz;
    delete m_aTrackMouse;
    delete m_aGrid;
    delete m_ui;
}

void Spectrogram::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());

    m_ui->windowSize->setCurrentIndex(scopeConfig.readEntry("windowSize", 0));
    m_ui->windowFunction->setCurrentIndex(scopeConfig.readEntry("windowFunction", 0));
    m_aTrackMouse->setChecked(scopeConfig.readEntry("trackMouse", true));
    m_aGrid->setChecked(scopeConfig.readEntry("drawGrid", true));
    m_aHighlightPeaks->setChecked(scopeConfig.readEntry("highlightPeaks", true));
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
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());

    scopeConfig.writeEntry("windowSize", m_ui->windowSize->currentIndex());
    scopeConfig.writeEntry("windowFunction", m_ui->windowFunction->currentIndex());
    scopeConfig.writeEntry("trackMouse", m_aTrackMouse->isChecked());
    scopeConfig.writeEntry("drawGrid", m_aGrid->isChecked());
    scopeConfig.writeEntry("highlightPeaks", m_aHighlightPeaks->isChecked());
    scopeConfig.writeEntry("dBmax", m_dBmax);
    scopeConfig.writeEntry("dBmin", m_dBmin);

    if (m_customFreq) {
        scopeConfig.writeEntry("freqMax", m_freqMax);
    } else {
        scopeConfig.writeEntry("freqMax", 0);
    }

    scopeConfig.sync();
}

QString Spectrogram::widgetName() const
{
    return QStringLiteral("Spectrogram");
}

QRect Spectrogram::scopeRect()
{
    m_scopeRect = QRect(QPoint(10,                                        // Left
                               m_ui->verticalSpacer->geometry().top() + 6 // Top
                               ),
                        AbstractAudioScopeWidget::rect().bottomRight());
    m_innerScopeRect = QRect(QPoint(m_scopeRect.left() + 66, // Left
                                    m_scopeRect.top() + 6    // Top
                                    ),
                             QPoint(m_ui->verticalSpacer->geometry().right() - 70, m_ui->verticalSpacer->geometry().bottom() - 40));
    return m_scopeRect;
}

QImage Spectrogram::renderHUD(uint)
{
    if (m_innerScopeRect.width() > 0 && m_innerScopeRect.height() > 0) {
        QElapsedTimer timer;
        timer.start();

        int x, y;
        const int minDistY = 30; // Minimum distance between two lines
        const int minDistX = 40;
        const int textDistX = 10;
        const int textDistY = 25;
        const int topDist = m_innerScopeRect.top() - m_scopeRect.top();
        const int leftDist = m_innerScopeRect.left() - m_scopeRect.left();
        const int mouseX = m_mousePos.x() - m_innerScopeRect.left();
        const int mouseY = m_mousePos.y() - m_innerScopeRect.top();
        bool hideText;

        QImage hud(m_scopeRect.size(), QImage::Format_ARGB32);
        hud.fill(qRgba(0, 0, 0, 0));

        QPainter davinci(&hud);
        davinci.setPen(AbstractScopeWidget::penLight);

        // Frame display
        if (m_aGrid->isChecked()) {
            for (int frameNumber = 0; frameNumber < m_innerScopeRect.height(); frameNumber += minDistY) {
                y = topDist + m_innerScopeRect.height() - 1 - frameNumber;
                hideText = m_aTrackMouse->isChecked() && m_mouseWithinWidget && abs(y - mouseY) < (int)textDistY && mouseY < m_innerScopeRect.height() &&
                           mouseX < m_innerScopeRect.width() && mouseX >= 0;

                davinci.drawLine(leftDist, y, leftDist + m_innerScopeRect.width() - 1, y);
                if (!hideText) {
                    davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, y + 6, QVariant(frameNumber).toString());
                }
            }
        }
        // Draw a line through the mouse position with the correct Frame number
        if (m_aTrackMouse->isChecked() && m_mouseWithinWidget && mouseY < m_innerScopeRect.height() && mouseX < m_innerScopeRect.width() && mouseX >= 0) {
            davinci.setPen(AbstractScopeWidget::penLighter);

            x = leftDist + mouseX;
            y = topDist + mouseY - 20;
            if (y < 0) {
                y = 0;
            }
            if (y > (int)topDist + m_innerScopeRect.height() - 1 - 30) {
                y = topDist + m_innerScopeRect.height() - 1 - 30;
            }
            davinci.drawLine(x, topDist + mouseY, leftDist + m_innerScopeRect.width() - 1, topDist + mouseY);
            davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, y, m_scopeRect.right() - m_innerScopeRect.right() - textDistX, 40, Qt::AlignLeft,
                             i18n("Frame\n%1", m_innerScopeRect.height() - 1 - mouseY));
        }

        // Frequency grid
        const uint hzDiff = (uint)ceil(((float)minDistX) / (float)m_innerScopeRect.width() * (float)m_freqMax / 1000.) * 1000;
        const int rightBorder = leftDist + m_innerScopeRect.width() - 1;
        x = 0;
        y = topDist + m_innerScopeRect.height() + textDistY;
        if (m_aGrid->isChecked()) {
            for (uint hz = 0; x <= rightBorder; hz += hzDiff) {
                davinci.setPen(AbstractScopeWidget::penLight);
                x = int((float)leftDist + (float)(m_innerScopeRect.width() - 1) * ((float)hz) / (float)m_freqMax);

                // Hide text if it would overlap with the text drawn at the mouse position
                hideText = m_aTrackMouse->isChecked() && m_mouseWithinWidget && abs(x - (int)(leftDist + mouseX + 20)) < (int)minDistX + 16 &&
                           mouseX < m_innerScopeRect.width() && mouseX >= 0;

                if (x <= rightBorder) {
                    davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() + 6);
                }
                if (x + textDistY < leftDist + m_innerScopeRect.width()) {
                    // Only draw the text label if there is still enough room for the final one at the right.
                    if (!hideText) {
                        davinci.drawText(x - 4, y, QVariant(hz / 1000).toString());
                    }
                }

                if (hz > 0) {
                    // Draw finer lines between the main lines
                    davinci.setPen(AbstractScopeWidget::penLightDots);
                    for (uint dHz = 3; dHz > 0; --dHz) {
                        x = int((float)leftDist + (float)m_innerScopeRect.width() * ((float)hz - (float)dHz * (float)hzDiff / 4.0f) / (float)m_freqMax);
                        if (x > rightBorder) {
                            break;
                        }
                        davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() - 1);
                    }
                }
            }
            // Draw the line at the very right (maximum frequency)
            x = leftDist + m_innerScopeRect.width() - 1;
            hideText = m_aTrackMouse->isChecked() && m_mouseWithinWidget && qAbs(x - (int)(leftDist + mouseX + 30)) < (int)minDistX &&
                       mouseX < m_innerScopeRect.width() && mouseX >= 0;
            davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() + 6);
            if (!hideText) {
                davinci.drawText(x - 10, y, i18n("%1 kHz", QString("%1").arg((double)m_freqMax / 1000, 0, 'f', 1)));
            }
        }

        // Draw a line through the mouse position with the correct frequency label
        if (m_aTrackMouse->isChecked() && m_mouseWithinWidget && mouseX < m_innerScopeRect.width() && mouseX >= 0) {
            davinci.setPen(AbstractScopeWidget::penThin);
            x = leftDist + mouseX;
            davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height() + 6);
            davinci.drawText(
                x - 10, y,
                i18n("%1 kHz", QString("%1").arg((double)(m_mousePos.x() - m_innerScopeRect.left()) / m_innerScopeRect.width() * m_freqMax / 1000, 0, 'f', 2)));
        }

        // Draw the dB brightness scale
        davinci.setPen(AbstractScopeWidget::penLighter);
        for (y = topDist; y < (int)topDist + m_innerScopeRect.height(); ++y) {
            float val = 1. - ((float)y - (float)topDist) / ((float)m_innerScopeRect.height() - 1.);
            uint col = m_colorMap[(int)(val * 255)];
            for (x = leftDist - 6; x >= (int)leftDist - 13; --x) {
                hud.setPixel(x, y, col);
            }
        }
        const int rectWidth = leftDist - m_scopeRect.left() - 22;
        const int rectHeight = 50;
        davinci.setFont(QFont(QFont().defaultFamily(), 10));
        davinci.drawText(m_scopeRect.left(), topDist, rectWidth, rectHeight, Qt::AlignRight, i18n("%1\ndB", m_dBmax));
        davinci.drawText(m_scopeRect.left(), topDist + m_innerScopeRect.height() - 20, rectWidth, rectHeight, Qt::AlignRight, i18n("%1\ndB", m_dBmin));

        emit signalHUDRenderingFinished((uint)timer.elapsed(), 1);
        return hud;
    }
    emit signalHUDRenderingFinished(0, 1);
    return QImage();
}
QImage Spectrogram::renderAudioScope(uint, const audioShortVector &audioFrame, const int freq, const int num_channels, const int num_samples, const int newData)
{
    if (audioFrame.size() > 63 && m_innerScopeRect.width() > 0 && m_innerScopeRect.height() > 0) {
        if (!m_customFreq) {
            m_freqMax = freq / 2;
        }
        bool newDataAvailable = newData > 0;

#ifdef DEBUG_SPECTROGRAM
        qCDebug(KDENLIVE_LOG) << "New data for " << widgetName() << ": " << newDataAvailable << QStringLiteral(" (") << newData << " units)";
#endif

        QElapsedTimer timer;
        timer.start();

        int fftWindow = m_ui->windowSize->itemData(m_ui->windowSize->currentIndex()).toInt();
        if (fftWindow > num_samples) {
            fftWindow = num_samples;
        }
        if ((fftWindow & 1) == 1) {
            fftWindow--;
        }

        // Show the window size used, for information
        m_ui->labelFFTSizeNumber->setText(QVariant(fftWindow).toString());

        if (newDataAvailable) {

            auto *freqSpectrum = new float[(uint)fftWindow / 2];

            // Get the spectral power distribution of the input samples,
            // using the given window size and function
            FFTTools::WindowType windowType = (FFTTools::WindowType)m_ui->windowFunction->itemData(m_ui->windowFunction->currentIndex()).toInt();
            m_fftTools.fftNormalized(audioFrame, 0, (uint)num_channels, freqSpectrum, windowType, (uint)fftWindow, 0);

            // This method might be called also when a simple refresh is required.
            // In this case there is no data to append to the history. Only append new data.
            QVector<float> spectrumVector(fftWindow / 2);
            memcpy(spectrumVector.data(), &freqSpectrum[0], (uint)fftWindow / 2 * sizeof(float));
            m_fftHistory.prepend(spectrumVector);
            delete[] freqSpectrum;
        }
#ifdef DEBUG_SPECTROGRAM
        else {
            qCDebug(KDENLIVE_LOG) << widgetName() << ": Has no new data to Fourier-transform";
        }
#endif

        // Limit the maximum history size to avoid wasting space
        while (m_fftHistory.size() > SPECTROGRAM_HISTORY_SIZE) {
            m_fftHistory.removeLast();
        }

        // Draw the spectrum
        QImage spectrum(m_scopeRect.size(), QImage::Format_ARGB32);
        spectrum.fill(qRgba(0, 0, 0, 0));
        QPainter davinci(&spectrum);
        const int h = m_innerScopeRect.height();
        const int leftDist = m_innerScopeRect.left() - m_scopeRect.left();
        const int topDist = m_innerScopeRect.top() - m_scopeRect.top();
        int windowSize;
        int y;
        bool completeRedraw = true;

        if (m_fftHistoryImg.size() == m_scopeRect.size() && !m_parameterChanged) {
            // The size of the widget and the parameters (like min/max dB) have not changed since last time,
            // so we can re-use it, shift it by one pixel, and render the single remaining line. Usually about
            // 10 times faster for a widget height of around 400 px.
            if (newDataAvailable) {
                davinci.drawImage(0, -1, m_fftHistoryImg);
            } else {
                // spectrum = m_fftHistoryImg does NOT work, leads to segfaults (anyone knows why, please tell me)
                davinci.drawImage(0, 0, m_fftHistoryImg);
            }
            completeRedraw = false;
        }

        y = 0;
        if ((newData != 0) || m_parameterChanged) {
            m_parameterChanged = false;
            bool peak = false;

            QVector<float> dbMap;
            uint right;
            ////////////////FIXME
            for (auto &it : m_fftHistory) {

                windowSize = it.size();

                // Interpolate the frequency data to match the pixel coordinates
                right = uint(((float)m_freqMax) / ((float)m_freq / 2.) * float(windowSize - 1));
                dbMap = FFTTools::interpolatePeakPreserving(it, (uint)m_innerScopeRect.width(), 0, right, -180);

                for (int i = 0; i < dbMap.size(); ++i) {
                    float val;
                    val = dbMap[i];
                    peak = val > (float)m_dBmax;

                    // Normalize dB value to [0 1], 1 corresponding to dbMax dB and 0 to dbMin dB
                    val = (val - (float)m_dBmax) / (float)(m_dBmax - m_dBmin) + 1.;
                    if (val < 0) {
                        val = 0;
                    } else if (val > 1) {
                        val = 1;
                    }
                    if (!peak || !m_aHighlightPeaks->isChecked()) {
                        spectrum.setPixel(leftDist + i, topDist + h - 1 - y, m_colorMap[(int)(val * 255)]);
                    } else {
                        spectrum.setPixel(leftDist + i, topDist + h - 1 - y, AbstractScopeWidget::colHighlightDark.rgba());
                    }
                }

                y++;
                if (y >= topDist + m_innerScopeRect.height()) {
                    break;
                }
                if (!completeRedraw) {
                    break;
                }
            }
        }

#ifdef DEBUG_SPECTROGRAM
        qCDebug(KDENLIVE_LOG) << "Rendered " << y - topDist << "lines from " << m_fftHistory.size() << " available samples in " << start.elapsed() << " ms"
                              << (completeRedraw ? "" : " (re-used old image)");
        uint storedBytes = 0;
        for (QList<QVector<float>>::iterator it = m_fftHistory.begin(); it != m_fftHistory.end(); ++it) {
            storedBytes += (*it).size() * sizeof((*it)[0]);
        }
        qCDebug(KDENLIVE_LOG) << QString("Total storage used: %1 kB").arg((double)storedBytes / 1000, 0, 'f', 2);
#endif

        m_fftHistoryImg = spectrum;

        emit signalScopeRenderingFinished((uint)timer.elapsed(), 1);
        return spectrum;
    }
    emit signalScopeRenderingFinished(0, 1);
    return QImage();
}
QImage Spectrogram::renderBackground(uint)
{
    return QImage();
}

bool Spectrogram::isHUDDependingOnInput() const
{
    return false;
}
bool Spectrogram::isScopeDependingOnInput() const
{
    return true;
}
bool Spectrogram::isBackgroundDependingOnInput() const
{
    return false;
}

void Spectrogram::handleMouseDrag(const QPoint &movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers)
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

        m_parameterChanged = true;
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

        m_parameterChanged = true;
        forceUpdateHUD();
        forceUpdateScope();
    }
}

void Spectrogram::slotResetMaxFreq()
{
    m_customFreq = false;
    m_parameterChanged = true;
    forceUpdateHUD();
    forceUpdateScope();
}

void Spectrogram::resizeEvent(QResizeEvent *event)
{
    m_parameterChanged = true;
    AbstractAudioScopeWidget::resizeEvent(event);
}

#undef SPECTROGRAM_HISTORY_SIZE
#ifdef DEBUG_SPECTROGRAM
#undef DEBUG_SPECTROGRAM
#endif
