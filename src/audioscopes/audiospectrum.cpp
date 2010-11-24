#include "audiospectrum.h"
#include "tools/kiss_fftr.h"

AudioSpectrum::AudioSpectrum(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
        AbstractAudioScopeWidget(projMonitor, clipMonitor, true, parent)
{
    init();
    m_cfg = kiss_fftr_alloc(512, 0,0,0);
}
AudioSpectrum::~AudioSpectrum()
{
    free(m_cfg);
}

QString AudioSpectrum::widgetName() const { return QString("audiospectrum"); }

bool AudioSpectrum::isBackgroundDependingOnInput() const { return false; }
bool AudioSpectrum::isScopeDependingOnInput() const { return true; }
bool AudioSpectrum::isHUDDependingOnInput() const { return false; }

QImage AudioSpectrum::renderBackground(uint) { return QImage(); }
QImage AudioSpectrum::renderScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples)
{
    float data[512];
    kiss_fft_cpx freqData[512];
    for (int i = 0; i < 512; i++) {
        data[i] = (float) audioFrame.data()[i];
    }
    kiss_fftr(m_cfg, data, freqData);
    qDebug() << freqData[0].r << " " << freqData[1].r << " " << freqData[2].r;
    return QImage();
}
QImage AudioSpectrum::renderHUD(uint) { return QImage(); }

QRect AudioSpectrum::scopeRect() {
    return QRect(0,0,40,40);
}

void AudioSpectrum::readConfig()
{

}
