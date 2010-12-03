#include "audiospectrum.h"
#include "tools/kiss_fftr.h"

#include <QMenu>
#include <QPainter>

// Linear interpolation.
//#include <iostream>
//#include <fstream>

bool fileWritten = false;

AudioSpectrum::AudioSpectrum(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
        AbstractAudioScopeWidget(projMonitor, clipMonitor, true, parent)
{
    ui = new Ui::AudioSpectrum_UI;
    ui->setupUi(this);

    m_distance = QSize(65, 30);
    m_dBmin = -120;
    m_dBmax = 0;
    m_freqMax = 10000;


    m_aLin = new QAction(i18n("Linear scale"), this);
    m_aLin->setCheckable(true);
    m_aLog = new QAction(i18n("Logarithmic scale"), this);
    m_aLog->setCheckable(true);

    m_agScale = new QActionGroup(this);
    m_agScale->addAction(m_aLin);
    m_agScale->addAction(m_aLog);

    m_menu->addSeparator()->setText(i18n("Scale"));
    m_menu->addAction(m_aLin);
    m_menu->addAction(m_aLog);

    ui->windowSize->addItem("256", QVariant(256));
    ui->windowSize->addItem("512", QVariant(512));
    ui->windowSize->addItem("1024", QVariant(1024));
    ui->windowSize->addItem("2048", QVariant(2048));

    m_cfg = kiss_fftr_alloc(ui->windowSize->itemData(ui->windowSize->currentIndex()).toInt(), 0,0,0);


    bool b = true;
    b &= connect(ui->windowSize, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateCfg()));
    Q_ASSERT(b);

    init();
}
AudioSpectrum::~AudioSpectrum()
{
    free(m_cfg);
    delete m_agScale;
    delete m_aLin;
    delete m_aLog;
}

void AudioSpectrum::readConfig()
{
    AbstractAudioScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    QString scale = scopeConfig.readEntry("scale");
    if (scale == "lin") {
        m_aLin->setChecked(true);
    } else {
        m_aLog->setChecked(true);
    }
    ui->windowSize->setCurrentIndex(scopeConfig.readEntry("windowSize", 0));

}
void AudioSpectrum::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    QString scale;
    if (m_aLin->isChecked()) {
        scale = "lin";
    } else {
        scale = "log";
    }
    scopeConfig.writeEntry("scale", scale);
    scopeConfig.writeEntry("windowSize", ui->windowSize->currentIndex());
    scopeConfig.sync();
}

QString AudioSpectrum::widgetName() const { return QString("audiospectrum"); }

bool AudioSpectrum::isBackgroundDependingOnInput() const { return false; }
bool AudioSpectrum::isScopeDependingOnInput() const { return true; }
bool AudioSpectrum::isHUDDependingOnInput() const { return false; }

QImage AudioSpectrum::renderBackground(uint) { return QImage(); }
QImage AudioSpectrum::renderScope(uint, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples)
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
        qDebug() << "Max audio signal is " << maxSig;

        // The resulting FFT vector is only half as long
        kiss_fft_cpx freqData[fftWindow/2];

        // Copy the first channel's audio into a vector for the FFT display
        // (only one channel handled at the moment)
        for (int i = 0; i < fftWindow; i++) {
            // Normalize signals to [0,1] to get correct dB values later on
            data[i] = (float) audioFrame.data()[i*num_channels] / 32767.0f;
        }
        // Calculate the Fast Fourier Transform for the input data
        kiss_fftr(m_cfg, data, freqData);


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
    //        qDebug() << val;
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
            qDebug() << val << "/" <<  (1 - (val - m_dBmax)/(m_dBmin-m_dBmax));
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
        */

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
        davinci.drawText(rect.width() + textDist, y + 8, i18n("%1 dB", db));
    }


    qDebug() << "max freq: " << m_freqMax;
    const uint hzDiff = ceil( ((float)minDistX)/rect.width() * m_freqMax / 1000 ) * 1000;
    qDebug() << hzDiff;
    int x;
    for (int hz = hzDiff; hz < m_freqMax; hz += hzDiff) {
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
