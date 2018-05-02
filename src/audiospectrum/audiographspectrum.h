/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

/*!
 * @class AudioGraphSpectrum
 * @brief An audio spectrum allowing to edit audio level / equalize clips or tracks
 * @author Jean-Baptiste Mardelle
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

class AudioGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioGraphWidget(QWidget *parent = nullptr);
    void drawBackground();

public slots:
    void showAudio(const QVector<double> &bands);

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
