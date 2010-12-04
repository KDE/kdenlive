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
#include "tools/kiss_fftr.h"

#include <QMenu>
#include <QPainter>
#include <QMouseEvent>

#include <iostream>
//#include <fstream>

//bool fileWritten = false;

const QString AudioSpectrum::directions[] =  {"North", "Northeast", "East", "Southeast"};

AudioSpectrum::AudioSpectrum(QWidget *parent) :
        AbstractAudioScopeWidget(false, parent),
        m_rescaleMinDist(8),
        m_rescaleVerticalThreshold(2.0f),
        m_rescaleActive(false),
        m_rescalePropertiesLocked(false),
        m_rescaleScale(1)
{
    ui = new Ui::AudioSpectrum_UI;
    ui->setupUi(this);

    m_distance = QSize(65, 30);
    m_freqMax = 10000;


    m_aLin = new QAction(i18n("Linear scale"), this);
    m_aLin->setCheckable(true);
    m_aLog = new QAction(i18n("Logarithmic scale"), this);
    m_aLog->setCheckable(true);

    m_agScale = new QActionGroup(this);
    m_agScale->addAction(m_aLin);
    m_agScale->addAction(m_aLog);

    m_aLockHz = new QAction(i18n("Lock maximum frequency"), this);
    m_aLockHz->setCheckable(true);
    m_aLockHz->setEnabled(false);


//    m_menu->addSeparator()->setText(i18n("Scale"));
//    m_menu->addAction(m_aLin);
//    m_menu->addAction(m_aLog);
    m_menu->addSeparator();
    m_menu->addAction(m_aLockHz);


    ui->windowSize->addItem("256", QVariant(256));
    ui->windowSize->addItem("512", QVariant(512));
    ui->windowSize->addItem("1024", QVariant(1024));
    ui->windowSize->addItem("2048", QVariant(2048));

    m_cfg = kiss_fftr_alloc(ui->windowSize->itemData(ui->windowSize->currentIndex()).toInt(), 0,0,0);


    bool b = true;
    b &= connect(ui->windowSize, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateCfg()));
    Q_ASSERT(b);

    AbstractScopeWidget::init();
}
AudioSpectrum::~AudioSpectrum()
{
    writeConfig();

    free(m_cfg);
    delete m_agScale;
    delete m_aLin;
    delete m_aLog;
    delete m_aLockHz;
}

void AudioSpectrum::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());
    QString scale = scopeConfig.readEntry("scale");
    if (scale == "lin") {
        m_aLin->setChecked(true);
    } else {
        m_aLog->setChecked(true);
    }
    m_aLockHz->setChecked(scopeConfig.readEntry("lockHz", false));
    ui->windowSize->setCurrentIndex(scopeConfig.readEntry("windowSize", 0));
    m_dBmax = scopeConfig.readEntry("dBmax", 0);
    m_dBmin = scopeConfig.readEntry("dBmin", -70);

}
void AudioSpectrum::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, AbstractScopeWidget::configName());
    QString scale;
    if (m_aLin->isChecked()) {
        scale = "lin";
    } else {
        scale = "log";
    }
    scopeConfig.writeEntry("scale", scale);
    scopeConfig.writeEntry("windowSize", ui->windowSize->currentIndex());
    scopeConfig.writeEntry("lockHz", m_aLockHz->isChecked());
    scopeConfig.writeEntry("dBmax", m_dBmax);
    scopeConfig.writeEntry("dBmin", m_dBmin);
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
        m_freqMax = freq / 2;

        QTime start = QTime::currentTime();

        bool customCfg = false;
        kiss_fftr_cfg myCfg = m_cfg;
        int fftWindow = ui->windowSize->itemData(ui->windowSize->currentIndex()).toInt();
        if (fftWindow > num_samples) {
            fftWindow = num_samples;
            customCfg = true;
        }
        if ((fftWindow & 1) == 1) {
            fftWindow--;
            customCfg = true;
        }
        if (customCfg) {
            myCfg = kiss_fftr_alloc(fftWindow, 0,0,0);
        }

        float data[fftWindow];
        float freqSpectrum[fftWindow/2];

        int16_t maxSig = 0;
        for (int i = 0; i < fftWindow; i++) {
            if (audioFrame.data()[i*num_channels] > maxSig) {
                maxSig = audioFrame.data()[i*num_channels];
            }
        }

        // The resulting FFT vector is only half as long
        kiss_fft_cpx freqData[fftWindow/2];


        // Copy the first channel's audio into a vector for the FFT display
        // (only one channel handled at the moment)
        if (num_samples < fftWindow) {
            std::fill(&data[num_samples], &data[fftWindow-1], 0);
        }
        for (int i = 0; i < num_samples && i < fftWindow; i++) {
            // Normalize signals to [0,1] to get correct dB values later on
            data[i] = (float) audioFrame.data()[i*num_channels] / 32767.0f;
        }

        // Calculate the Fast Fourier Transform for the input data
        kiss_fftr(myCfg, data, freqData);


        float val;
        // Get the minimum and the maximum value of the Fourier transformed (for scaling)
        for (int i = 0; i < fftWindow/2; i++) {
            if (m_aLog->isChecked()) {
                // Logarithmic scale: 20 * log ( 2 * magnitude / N )
                // with N = FFT size (after FFT, 1/2 window size)
                val = 20*log(pow(pow(fabs(freqData[i].r),2) + pow(fabs(freqData[i].i),2), .5)/((float)fftWindow/2.0f))/log(10);
            } else {
                // sqrt(r² + i²)
                val = pow(pow(fabs(freqData[i].r),2) + pow(fabs(freqData[i].i),2), .5);
            }
            freqSpectrum[i] = val;
        }



        // Draw the spectrum
        QImage spectrum(scopeRect().size(), QImage::Format_ARGB32);
        spectrum.fill(qRgba(0,0,0,0));
        uint w = scopeRect().size().width();
        uint h = scopeRect().size().height();
        float x;
        for (uint i = 0; i < w; i++) {

            x = i/((float) w) * fftWindow/2;

            // Use linear interpolation in order to get smoother display
            if (i == 0 || i == w-1) {
                val = freqSpectrum[i];
            } else {
                // Use floor(x)+1 instead of ceil(x) as floor(x) == ceil(x) is possible.
                val = (floor(x)+1 - x)*freqSpectrum[(int) floor(x)] + (x-floor(x))*freqSpectrum[(int) floor(x)+1];
            }

            // freqSpectrum values range from 0 to -inf as they are relative dB values.
            for (uint y = 0; y < h*(1 - (val - m_dBmax)/(m_dBmin-m_dBmax)) && y < h; y++) {
                spectrum.setPixel(i, h-y-1, qRgba(225, 182, 255, 255));
            }
        }

        emit signalScopeRenderingFinished(start.elapsed(), 1);

        /*
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
        //*/

        if (customCfg) {
            free(myCfg);
        }

        return spectrum;
    } else {
        emit signalScopeRenderingFinished(0, 1);
        return QImage();
    }
}
QImage AudioSpectrum::renderHUD(uint)
{
    QTime start = QTime::currentTime();

    const QRect rect = scopeRect();
    // Minimum distance between two lines
    const uint minDistY = 30;
    const uint minDistX = 40;
    const uint textDist = 5;
    const uint dbDiff = ceil((float)minDistY/rect.height() * (m_dBmax-m_dBmin));

    QImage hud(AbstractAudioScopeWidget::rect().size(), QImage::Format_ARGB32);
    hud.fill(qRgba(0,0,0,0));

    QPainter davinci(&hud);
    davinci.setPen(AbstractAudioScopeWidget::penLight);

    int y;
    for (int db = -dbDiff; db > m_dBmin; db -= dbDiff) {
        y = rect.height() * ((float)db)/(m_dBmin - m_dBmax);
        davinci.drawLine(0, y, rect.width()-1, y);
        davinci.drawText(rect.width() + textDist, y + 8, i18n("%1 dB", m_dBmax + db));
    }


    const uint hzDiff = ceil( ((float)minDistX)/rect.width() * m_freqMax / 1000 ) * 1000;
    int x;
    for (uint hz = hzDiff; hz < m_freqMax; hz += hzDiff) {
        x = rect.width() * ((float)hz)/m_freqMax;
        davinci.drawLine(x, 0, x, rect.height()+4);
        davinci.drawText(x-4, rect.height() + 20, QVariant(hz/1000).toString());
    }
    davinci.drawText(rect.width(), rect.height() + 20, "[kHz]");


    emit signalHUDRenderingFinished(start.elapsed(), 1);
    return hud;
}

QRect AudioSpectrum::scopeRect() {
    return QRect(QPoint(0, 0), AbstractAudioScopeWidget::rect().size() - m_distance);
}


void AudioSpectrum::slotUpdateCfg()
{
    free(m_cfg);
    m_cfg = kiss_fftr_alloc(ui->windowSize->itemData(ui->windowSize->currentIndex()).toInt(), 0,0,0);
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

                // Ensure the dB values lie in [-100, 0]
                // 0 is the upper bound, everything below -70 dB is most likely noise
                if (m_dBmax > 0) {
                    m_dBmax = 0;
                }
                if (m_dBmin < -100) {
                    m_dBmin = -100;
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
                        if (m_dBmin < -100) {
                            m_dBmin = -100;
                            m_dBmax = -100+6;
                        }
                    }
                }

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
//                qDebug() << "Diff is " << diff << "; chose " << directions[m_rescaleClockDirection] << " as direction";
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
