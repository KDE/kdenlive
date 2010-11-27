#include "audiospectrum.h"
#include "tools/kiss_fftr.h"

#include <QMenu>

//#include <iostream>
//#include <fstream>

bool fileWritten = false;

AudioSpectrum::AudioSpectrum(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
        AbstractAudioScopeWidget(projMonitor, clipMonitor, true, parent)
{
    ui = new Ui::AudioSpectrum_UI;
    ui->setupUi(this);

    m_cfg = kiss_fftr_alloc(512, 0,0,0);

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
    scopeConfig.sync();
}

QString AudioSpectrum::widgetName() const { return QString("audiospectrum"); }

bool AudioSpectrum::isBackgroundDependingOnInput() const { return false; }
bool AudioSpectrum::isScopeDependingOnInput() const { return true; }
bool AudioSpectrum::isHUDDependingOnInput() const { return false; }

QImage AudioSpectrum::renderBackground(uint) { return QImage(); }
QImage AudioSpectrum::renderScope(uint, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples)
{
    QTime start = QTime::currentTime();
    float data[512];

    // The resulting FFT vector is only half as long
    kiss_fft_cpx freqData[256];

    // Copy the first channel's audio into a vector for the FFT display
    // (only one channel handled at the moment)
    for (int i = 0; i < 512; i++) {
        data[i] = (float) audioFrame.data()[i*num_channels];
    }
    // Calculate the Fast Fourier Transform for the input data
    kiss_fftr(m_cfg, data, freqData);

//    qDebug() << num_samples << " samples at " << freq << " Hz";
//    qDebug() << "FFT Freq: " << freqData[0].r << " " << freqData[1].r << ", " << freqData[2].r;
//    qDebug() << "FFT imag: " << freqData[0].i << " " << freqData[1].i << ", " << freqData[2].i;


    float max = 0;
    float maxSignal = 0;
    float min = 1000;
    float val;
    // Get the minimum and the maximum value of the Fourier transformed (for scaling)
    for (int i = 0; i < 256; i++) {
        // sqrt(r² + i²)
        val = pow(pow(fabs(freqData[i].r),2) + pow(fabs(freqData[i].i),2), .5);
        if (maxSignal < val) { maxSignal = val; }
        if (!m_aLin->isChecked()) {
            // Logarithmic scale
            val = log(pow(pow(fabs(freqData[i].r),2) + pow(fabs(freqData[i].i),2), .5)/512.0f);
        }
        max = (max > val) ? max : val;
        min = (min < val) ? min : val;
    }
    qDebug() << "MAX: " << max << " (" << maxSignal << "), MIN: " << min;

    // Scaling factor
    float factor = 100./(max-min);

    // Draw the spectrum
    QImage spectrum(512, 100, QImage::Format_ARGB32);
    spectrum.fill(qRgba(0,0,0,0));
    for (int i = 0; i < 256; i++) {
        if (m_aLin->isChecked()) {
            val = pow(pow(fabs(freqData[i].r),2) + pow(fabs(freqData[i].i),2), .5);
        } else {
            val = log(pow(pow(fabs(freqData[i].r),2) + pow(fabs(freqData[i].i),2), .5)/512.0f);
        }
        //val = val >> 16;
        val = factor * (val-min);
//        qDebug() << val;
        for (int y = 0; y < val && y < 100; y++) {
            spectrum.setPixel(2*i, 99-y, qRgba(225, 182, 255, 255));
            spectrum.setPixel(2*i+1, 99-y, qRgba(225, 182, 255, 255));
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

    return spectrum;
}
QImage AudioSpectrum::renderHUD(uint) { return QImage(); }

QRect AudioSpectrum::scopeRect() {
    return QRect(0,0,40,40);
}
