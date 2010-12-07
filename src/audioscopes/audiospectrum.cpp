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
#include "ffttools.h"
#include "tools/kiss_fftr.h"

#include <QMenu>
#include <QPainter>
#include <QMouseEvent>

#include <iostream>

// Enables debugging, like writing a GNU Octave .m file to /tmp
//#define DEBUG_AUDIOSPEC
#ifdef DEBUG_AUDIOSPEC
#include <fstream>
#include <QDebug>
bool fileWritten = false;
#endif

#define MIN_DB_VALUE -120
#define MAX_FREQ_VALUE 96000
#define MIN_FREQ_VALUE 1000

const QString AudioSpectrum::directions[] =  {"North", "Northeast", "East", "Southeast"};

AudioSpectrum::AudioSpectrum(QWidget *parent) :
        AbstractAudioScopeWidget(false, parent),
        m_fftCfgs(),
        m_windowFunctions(),
        m_freqMax(10000),
        m_customFreq(false),
        m_rescaleMinDist(8),
        m_rescaleVerticalThreshold(2.0f),
        m_rescaleActive(false),
        m_rescalePropertiesLocked(false),
        m_rescaleScale(1)
{
    ui = new Ui::AudioSpectrum_UI;
    ui->setupUi(this);


    m_aResetHz = new QAction(i18n("Reset maximum frequency to sampling rate"), this);


    m_menu->addSeparator();
    m_menu->addAction(m_aResetHz);
    m_menu->removeAction(m_aRealtime);


    ui->windowSize->addItem("256", QVariant(256));
    ui->windowSize->addItem("512", QVariant(512));
    ui->windowSize->addItem("1024", QVariant(1024));
    ui->windowSize->addItem("2048", QVariant(2048));

    ui->windowFunction->addItem(i18n("Rectangular window"), FFTTools::Window_Rect);
    ui->windowFunction->addItem(i18n("Triangular window"), FFTTools::Window_Triangle);
    ui->windowFunction->addItem(i18n("Hamming window"), FFTTools::Window_Hamming);


    bool b = true;
    b &= connect(m_aResetHz, SIGNAL(triggered()), this, SLOT(slotResetMaxFreq()));
    Q_ASSERT(b);


    ui->labelFFTSize->setToolTip(i18n("The maximum window size is limited by the number of samples per frame."));
    ui->windowSize->setToolTip(i18n("A bigger window improves the accuracy at the cost of computational power."));
    ui->windowFunction->setToolTip(i18n("The rectangular window function is good for signals with equal signal strength (narrow peak), but creates more smearing. See Window function on Wikipedia."));

    AbstractScopeWidget::init();
}
AudioSpectrum::~AudioSpectrum()
{
    writeConfig();

    QHash<QString, kiss_fftr_cfg>::iterator i;
    for (i = m_fftCfgs.begin(); i != m_fftCfgs.end(); i++) {
        free(*i);
    }
    delete m_aResetHz;
}

void AudioSpectrum::readConfig()
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
void AudioSpectrum::writeConfig()
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

QString AudioSpectrum::widgetName() const { return QString("AudioSpectrum"); }
bool AudioSpectrum::isBackgroundDependingOnInput() const { return false; }
bool AudioSpectrum::isScopeDependingOnInput() const { return true; }
bool AudioSpectrum::isHUDDependingOnInput() const { return false; }

QImage AudioSpectrum::renderBackground(uint) { return QImage(); }

QImage AudioSpectrum::renderAudioScope(uint, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples)
{
    if (audioFrame.size() > 63) {
        if (!m_customFreq) {
            m_freqMax = freq / 2;
        }

        QTime start = QTime::currentTime();


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

        // Get the kiss_fft configuration from the config cache
        // or build a new configuration if the requested one is not available.
        kiss_fftr_cfg myCfg;
        const QString signature = cfgSignature(fftWindow);
        if (m_fftCfgs.contains(signature)) {
#ifdef DEBUG_AUDIOSPEC
            qDebug() << "Re-using FFT configuration with size " << fftWindow;
#endif
            myCfg = m_fftCfgs.value(signature);
        } else {
#ifdef DEBUG_AUDIOSPEC
            qDebug() << "Creating FFT configuration with size " << fftWindow;
#endif
            myCfg = kiss_fftr_alloc(fftWindow, 0,0,0);
            m_fftCfgs.insert(signature, myCfg);
        }

        float data[fftWindow];
        float freqSpectrum[fftWindow/2];

        // Prepare frequency space vector. The resulting FFT vector is only half as long.
        kiss_fft_cpx freqData[fftWindow/2];



        // Copy the first channel's audio into a vector for the FFT display
        // (only one channel handled at the moment)
        if (num_samples < fftWindow) {
            std::fill(&data[num_samples], &data[fftWindow-1], 0);
        }

        FFTTools::WindowType windowType = (FFTTools::WindowType) ui->windowFunction->itemData(ui->windowFunction->currentIndex()).toInt();
        QVector<float> window;
        float windowScaleFactor = 1;
        if (windowType != FFTTools::Window_Rect) {
            const QString signature = FFTTools::windowSignature(windowType, fftWindow, 0);
            if (m_windowFunctions.contains(signature)) {
#ifdef DEBUG_AUDIOSPEC
                qDebug() << "Re-using window function with signature " << signature;
#endif
                window = m_windowFunctions.value(signature);
            } else {
#ifdef DEBUG_AUDIOSPEC
                qDebug() << "Building new window function with signature " << signature;
#endif
                window = FFTTools::window(windowType, fftWindow, 0);
                m_windowFunctions.insert(signature, window);
            }
            windowScaleFactor = 1.0/window[fftWindow];
        }

        // Normalize signals to [0,1] to get correct dB values later on
        for (int i = 0; i < num_samples && i < fftWindow; i++) {
            if (windowType != FFTTools::Window_Rect) {
                data[i] = (float) audioFrame.data()[i*num_channels] / 32767.0f * window[i];
            } else {
                data[i] = (float) audioFrame.data()[i*num_channels] / 32767.0f;
            }
        }

        // Calculate the Fast Fourier Transform for the input data
        kiss_fftr(myCfg, data, freqData);


        // Logarithmic scale: 20 * log ( 2 * magnitude / N ) with magnitude = sqrt(r² + i²)
        // with N = FFT size (after FFT, 1/2 window size)
        for (int i = 0; i < fftWindow/2; i++) {
            // Logarithmic scale: 20 * log ( 2 * magnitude / N ) with magnitude = sqrt(r² + i²)
            // with N = FFT size (after FFT, 1/2 window size)
            freqSpectrum[i] = 20*log(pow(pow(fabs(freqData[i].r * windowScaleFactor),2) + pow(fabs(freqData[i].i * windowScaleFactor),2), .5)/((float)fftWindow/2.0f))/log(10);;
        }



        // Draw the spectrum
        QImage spectrum(m_scopeRect.size(), QImage::Format_ARGB32);
        spectrum.fill(qRgba(0,0,0,0));
        const uint w = m_innerScopeRect.width();
        const uint h = m_innerScopeRect.height();
        const uint leftDist = m_innerScopeRect.left() - m_scopeRect.left();
        const uint topDist = m_innerScopeRect.top() - m_scopeRect.top();
        float f;
        float x;
        float x_prev = 0;
        float val;
        int xi;
        for (uint i = 0; i < w; i++) {

            // i:  Pixel coordinate
            // f: Target frequency
            // x:  Frequency array index (float!) corresponding to the pixel
            // xi: floor(x)

            f = i/((float) w-1.0) * m_freqMax;
            x = 2*f/freq * (fftWindow/2 - 1);
            xi = (int) floor(x);

            if (x >= fftWindow/2) {
                break;
            }

            // Use linear interpolation in order to get smoother display
            if (i == 0 || xi == fftWindow/2-1) {
                // ... except if we are at the left or right border of the display or the spectrum
                val = freqSpectrum[xi];
            } else {

                if (freqSpectrum[xi] > freqSpectrum[xi+1]
                    && x_prev < xi) {
                    // This is a hack to preserve peaks.
                    // Consider f = {0, 100, 0}
                    //          x = {0.5,  1.5}
                    // Then x is 50 both times, and the 100 peak is lost.
                    // Get it back here for the first x after the peak.
                    val = freqSpectrum[xi];
                } else {
                    val =   (xi+1 - x) * freqSpectrum[xi]
                          + (x - xi)   * freqSpectrum[xi+1];
                }
            }

            // freqSpectrum values range from 0 to -inf as they are relative dB values.
            for (uint y = 0; y < h*(1 - (val - m_dBmax)/(m_dBmin-m_dBmax)) && y < h; y++) {
                spectrum.setPixel(leftDist + i, topDist + h-y-1, qRgba(225, 182, 255, 255));
            }

            x_prev = x;
        }

        emit signalScopeRenderingFinished(start.elapsed(), 1);

#ifdef DEBUG_AUDIOSPEC
        if (!fileWritten || true) {
            std::ofstream mFile;
            mFile.open("/tmp/freq.m");
            if (!mFile) {
                qDebug() << "Opening file failed.";
            } else {
                mFile << "val = [ ";

                for (int sample = 0; sample < 256; sample++) {
                    mFile << data[sample] << " ";
                }
                mFile << " ];\n";

                mFile << "freq = [ ";
                for (int sample = 0; sample < 256; sample++) {
                    mFile << freqData[sample].r << "+" << freqData[sample].i << "*i ";
                }
                mFile << " ];\n";

                mFile.close();
                fileWritten = true;
                qDebug() << "File written.";
            }
        } else {
            qDebug() << "File already written.";
        }
#endif

        return spectrum;
    } else {
        emit signalScopeRenderingFinished(0, 1);
        return QImage();
    }
}
QImage AudioSpectrum::renderHUD(uint)
{
    QTime start = QTime::currentTime();

    // Minimum distance between two lines
    const uint minDistY = 30;
    const uint minDistX = 40;
    const uint textDistX = 10;
    const uint textDistY = 25;
    const uint topDist = m_innerScopeRect.top() - m_scopeRect.top();
    const uint leftDist = m_innerScopeRect.left() - m_scopeRect.left();
    const uint dbDiff = ceil((float)minDistY/m_innerScopeRect.height() * (m_dBmax-m_dBmin));

    QImage hud(m_scopeRect.size(), QImage::Format_ARGB32);
    hud.fill(qRgba(0,0,0,0));

    QPainter davinci(&hud);
    davinci.setPen(AbstractScopeWidget::penLight);

    int y;
    for (int db = -dbDiff; db > m_dBmin; db -= dbDiff) {
        y = topDist + m_innerScopeRect.height() * ((float)db)/(m_dBmin - m_dBmax);
        if (y-topDist > m_innerScopeRect.height()-minDistY+10) {
            // Abort here, there is still a line left for min dB to paint which needs some room.
            break;
        }
        davinci.drawLine(leftDist, y, leftDist + m_innerScopeRect.width()-1, y);
        davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, y + 6, i18n("%1 dB", m_dBmax + db));
    }
    davinci.drawLine(leftDist, topDist, leftDist + m_innerScopeRect.width()-1, topDist);
    davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, topDist+6, i18n("%1 dB", m_dBmax));
    davinci.drawLine(leftDist, topDist+m_innerScopeRect.height()-1, leftDist + m_innerScopeRect.width()-1, topDist+m_innerScopeRect.height()-1);
    davinci.drawText(leftDist + m_innerScopeRect.width() + textDistX, topDist+m_innerScopeRect.height()+6, i18n("%1 dB", m_dBmin));

    const uint hzDiff = ceil( ((float)minDistX)/m_innerScopeRect.width() * m_freqMax / 1000 ) * 1000;
    int x = 0;
    const int rightBorder = leftDist + m_innerScopeRect.width()-1;
    y = topDist + m_innerScopeRect.height() + textDistY;
    for (uint hz = 0; x <= rightBorder; hz += hzDiff) {
        davinci.setPen(AbstractScopeWidget::penLight);
        x = leftDist + m_innerScopeRect.width() * ((float)hz)/m_freqMax;

        if (x <= rightBorder) {
            davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height()+6);
        }
        if (hz < m_freqMax && x+textDistY < leftDist + m_innerScopeRect.width()) {
            davinci.drawText(x-4, y, QVariant(hz/1000).toString());
        } else {
            x = leftDist + m_innerScopeRect.width();
            davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height()+6);
            davinci.drawText(x-10, y, i18n("%1 kHz").arg((double)m_freqMax/1000, 0, 'f', 1));
        }

        if (hz > 0) {
            // Draw finer lines between the main lines
            davinci.setPen(AbstractScopeWidget::penLightDots);
            for (uint dHz = 3; dHz > 0; dHz--) {
                x = leftDist + m_innerScopeRect.width() * ((float)hz - dHz * hzDiff/4.0f)/m_freqMax;
                if (x > rightBorder) {
                    break;
                }
                davinci.drawLine(x, topDist, x, topDist + m_innerScopeRect.height()-1);
            }
        }
    }


    emit signalHUDRenderingFinished(start.elapsed(), 1);
    return hud;
}

QRect AudioSpectrum::scopeRect() {
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

void AudioSpectrum::slotResetMaxFreq()
{
    m_customFreq = false;
    forceUpdateHUD();
    forceUpdateScope();
}


///// EVENTS /////

void AudioSpectrum::mouseMoveEvent(QMouseEvent *event)
{
    QPoint movement = event->pos()-m_rescaleStartPoint;

    if (m_rescaleActive) {
        if (m_rescalePropertiesLocked) {
            // Direction is known, now adjust parameters

            // Reset the starting point to make the next moveEvent relative to the current one
            m_rescaleStartPoint = event->pos();


            if (!m_rescaleFirstRescaleDone) {
                // We have just learned the desired direction; Normalize the movement to one pixel
                // to avoid a jump by m_rescaleMinDist

                if (movement.x() != 0) {
                    movement.setX(movement.x() / abs(movement.x()));
                }
                if (movement.y() != 0) {
                    movement.setY(movement.y() / abs(movement.y()));
                }

                m_rescaleFirstRescaleDone = true;
            }

            if (m_rescaleClockDirection == AudioSpectrum::North) {
                // Nort-South direction: Adjust the dB scale

                if ((m_rescaleModifiers & Qt::ShiftModifier) == 0) {

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
                    if ((m_rescaleModifiers & Qt::ShiftModifier) == 0) {
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
                            m_dBmax = MIN_DB_VALUE+6;
                        }
                    }
                }

                forceUpdateHUD();
                forceUpdateScope();

            } else if (m_rescaleClockDirection == AudioSpectrum::East) {
                // East-West direction: Adjust the maximum frequency
                m_freqMax -= 100*movement.x();
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


        } else {
            // Detect the movement direction here.
            // This algorithm relies on the aspect ratio of dy/dx (size and signum).
            if (movement.manhattanLength() > m_rescaleMinDist) {
                float diff = ((float) movement.y())/movement.x();

                if (abs(diff) > m_rescaleVerticalThreshold || movement.x() == 0) {
                    m_rescaleClockDirection = AudioSpectrum::North;
                } else if (abs(diff) < 1/m_rescaleVerticalThreshold) {
                    m_rescaleClockDirection = AudioSpectrum::East;
                } else if (diff < 0) {
                    m_rescaleClockDirection = AudioSpectrum::Northeast;
                } else {
                    m_rescaleClockDirection = AudioSpectrum::Southeast;
                }
#ifdef DEBUG_AUDIOSPEC
                qDebug() << "Diff is " << diff << "; chose " << directions[m_rescaleClockDirection] << " as direction";
#endif
                m_rescalePropertiesLocked = true;
            }
        }
    } else {
        AbstractAudioScopeWidget::mouseMoveEvent(event);
    }
}

void AudioSpectrum::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Rescaling mode starts
        m_rescaleActive = true;
        m_rescalePropertiesLocked = false;
        m_rescaleFirstRescaleDone = false;
        m_rescaleStartPoint = event->pos();
        m_rescaleModifiers = event->modifiers();

    } else {
        AbstractAudioScopeWidget::mousePressEvent(event);
    }
}

void AudioSpectrum::mouseReleaseEvent(QMouseEvent *event)
{
    m_rescaleActive = false;
    m_rescalePropertiesLocked = false;

    AbstractAudioScopeWidget::mouseReleaseEvent(event);
}

const QString AudioSpectrum::cfgSignature(const int size)
{
    return QString("s%1").arg(size);
}


#ifdef DEBUG_AUDIOSPEC
#undef DEBUG_AUDIOSPEC
#endif

#undef MIN_DB_VALUE
#undef MAX_FREQ_VALUE
#undef MIN_FREQ_VALUE
