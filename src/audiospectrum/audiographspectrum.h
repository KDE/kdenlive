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

#include "../monitor/sharedframe.h"

#include <QWidget>
#include <QVector>
#include <QSemaphore>

namespace Mlt {
class Filter;
class Consumer;
}

class MonitorManager;

class AudioGraphWidget : public QWidget
{
    Q_OBJECT
public:
    AudioGraphWidget(QWidget *parent = 0);

public slots:
    void showAudio(const QVector<double>&bands);

protected:
    void paintEvent(QPaintEvent *pe);

private:
    QVector<double> m_levels;
    QVector<int> m_dbLabels;
    QStringList m_freqLabels;
    int m_maxDb;
    void drawDbLabels(QPainter& p, const QRect &rect);
    void drawChanLabels(QPainter& p, const QRect &rect, int barWidth);
};


class AudioGraphSpectrum : public QWidget
{
    Q_OBJECT
public:
    AudioGraphSpectrum(MonitorManager *manager, QWidget *parent = 0);
    virtual ~AudioGraphSpectrum();

private:
    MonitorManager *m_manager;
    Mlt::Filter* m_filter;
    AudioGraphWidget *m_graphWidget;
    QSemaphore m_semaphore;

public slots:
    void processSpectrum(const SharedFrame&frame);

};

#endif
