/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef AUDIOGRAPHSPECTRUM_H
#define AUDIOGRAPHSPECTRUM_H

#include "scopewidget.h"
#include "sharedframe.h"

#include <QPixmap>
#include <QVector>
#include <QWidget>

namespace Mlt {
class Filter;
}

class MonitorManager;

/*class EqualizerWidget : public QWidget
{
    Q_OBJECT
public:
    EqualizerWidget(QWidget *parent = nullptr);

};*/

/** @class AudioGraphWidget
    @brief \@todo Describe class AudioGraphWidget
    @todo Describe class AudioGraphWidget
 */
class AudioGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioGraphWidget(QWidget *parent = nullptr);
    void drawBackground();

public slots:
    void showAudio(const QVector<float> &bands);

protected:
    void paintEvent(QPaintEvent *pe) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QVector<float> m_levels;
    QVector<int> m_dbLabels;
    QStringList m_freqLabels;
    QPixmap m_pixmap;
    QRect m_rect;
    int m_maxDb;
    void drawDbLabels(QPainter &p, const QRect &rect);
    void drawChanLabels(QPainter &p, const QRect &rect, int barWidth);
};

/** @class AudioGraphSpectrum
    @brief An audio spectrum
    @author Jean-Baptiste Mardelle
 */
class AudioGraphSpectrum : public ScopeWidget
{
    Q_OBJECT
public:
    AudioGraphSpectrum(MonitorManager *manager, QWidget *parent = nullptr);
    ~AudioGraphSpectrum() override;
    void dockVisible(bool visible);

private:
    MonitorManager *m_manager;
    Mlt::Filter *m_filter;
    AudioGraphWidget *m_graphWidget;
    // EqualizerWidget *m_equalizer;
    void processSpectrum();
    void refreshScope(const QSize &size, bool full) override;

public slots:
    void refreshPixmap();

private slots:
    void activate(bool enable);
};

#endif
