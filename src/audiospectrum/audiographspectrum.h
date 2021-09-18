/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle (jb@kdenlive.org)
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef AUDIOGRAPHSPECTRUM_H
#define AUDIOGRAPHSPECTRUM_H

#include "../monitor/scopes/sharedframe.h"

#include <QPixmap>
#include <QVector>
#include <QWidget>

namespace Mlt {
class Filter;
}

class MonitorManager;

class EqualizerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EqualizerWidget(QWidget *parent = nullptr);
};

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
    void paintEvent(QPaintEvent *pe);
    void resizeEvent(QResizeEvent *event);

private:
    QVector<double> m_levels;
    QVector<int> m_dbLabels;
    QStringList m_freqLabels;
    QPixmap m_pixmap;
    QRect m_rect;
    int m_maxDb;
    void drawDbLabels(QPainter &p, const QRect &rect);
    void drawChanLabels(QPainter &p, const QRect &rect, int barWidth);
};

/** @class AudioGraphSpectrum
    @brief An audio spectrum allowing to edit audio level / equalize clips or tracks
    @author Jean-Baptiste Mardelle
 */
class AudioGraphSpectrum : public QWidget
{
    Q_OBJECT
public:
    AudioGraphSpectrum(MonitorManager *manager, QWidget *parent = nullptr);
    virtual ~AudioGraphSpectrum();

private:
    MonitorManager *m_manager;
    Mlt::Filter *m_filter;
    AudioGraphWidget *m_graphWidget;
    EqualizerWidget *m_equalizer;

public slots:
    void processSpectrum(const SharedFrame &frame);
    void refreshPixmap();
};

#endif
