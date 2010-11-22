#include "audiospectrum.h"
#include "tools/kiss_fftr.h"

AudioSpectrum::AudioSpectrum(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
        AbstractAudioScopeWidget(projMonitor, clipMonitor, true, parent)
{
    init();
}

QString AudioSpectrum::widgetName() const { return QString("audiospectrum"); }

bool AudioSpectrum::isBackgroundDependingOnInput() const { return false; }
bool AudioSpectrum::isScopeDependingOnInput() const { return false; }
bool AudioSpectrum::isHUDDependingOnInput() const { return false; }

QImage AudioSpectrum::renderBackground(uint) { return QImage(); }
QImage AudioSpectrum::renderScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples)
{
    return QImage();
}
QImage AudioSpectrum::renderHUD(uint) { return QImage(); }

QRect AudioSpectrum::scopeRect() {
    return QRect(0,0,40,40);
}

void AudioSpectrum::readConfig()
{

}
