#ifndef AUDIOSPECTRUM_H
#define AUDIOSPECTRUM_H

#include <QtCore>

#include "abstractaudioscopewidget.h"
#include "ui_audiospectrum_ui.h"
#include "tools/kiss_fftr.h"

class AudioSpectrum_UI;

class AudioSpectrum : public AbstractAudioScopeWidget {
    Q_OBJECT

public:
    AudioSpectrum(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    ~AudioSpectrum();

    // Implemented virtual methods
    QString widgetName() const;

protected:
    ///// Implemented methods /////
    QRect scopeRect();
    QImage renderHUD(uint accelerationFactor);
    QImage renderScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples);
    QImage renderBackground(uint accelerationFactor);
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;
    virtual void readConfig();
    void writeConfig();

private:
    Ui::AudioSpectrum_UI *ui;
    kiss_fftr_cfg m_cfg;

    QAction *m_aLin;
    QAction *m_aLog;
    QActionGroup *m_agScale;

    QSize m_distance;

    /** Lower bound for the dB value to display */
    int m_dBmin;
    /** Upper bound (max: 0) */
    int m_dBmax;

    uint m_freqMax;

private slots:
    void slotUpdateCfg();

};

#endif // AUDIOSPECTRUM_H
