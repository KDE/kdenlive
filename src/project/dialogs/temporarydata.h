/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TEMPORARYDATA_H
#define TEMPORARYDATA_H

#include "definitions.h"

#include <QDialog>

class KdenliveDoc;
class KJob;
class QPaintEvent;
class QLabel;
class QGridLayout;

/**
 * @class ChartWidget
 * @brief Chart drawing widget.
 *
 */
class ChartWidget : public QWidget
{
public:
    explicit ChartWidget(QWidget *parent = 0);
    void setSegments(QList <int> segments);

protected:
    void paintEvent(QPaintEvent *event);

private:
    QList <int> m_segments;
};

/**
 * @class TemporaryData
 * @brief Dialog allowing management of project's temporary data.
 *
 */

class TemporaryData : public QDialog
{
    Q_OBJECT

public:
    explicit TemporaryData(KdenliveDoc *doc, bool currentProjectOnly, QWidget * parent = 0);

private:
    KdenliveDoc *m_doc;
    ChartWidget *m_currentPie;
    QLabel *m_previewSize;
    QLabel *m_proxySize;
    QLabel *m_audioSize;
    QLabel *m_thumbSize;
    QLabel *m_currentSize;
    QWidget *m_currentPage;
    QGridLayout *m_grid;
    qulonglong m_totalCurrent;
    QList <qulonglong> mCurrentSizes;
    void updateDataInfo();
    void updateTotal();

private slots:
    void gotPreviewSize(KJob *job);
    void gotProxySize(KJob *job);
    void gotAudioSize(KJob *job);
    void gotThumbSize(KJob *job);
    void deletePreview();
    void deleteProxy();
    void deleteAudio();
    void deleteThumbs();
    void deleteAll();
};

#endif
