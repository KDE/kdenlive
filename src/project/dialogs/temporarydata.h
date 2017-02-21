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

#include <QWidget>
#include <QDir>

class KdenliveDoc;
class KJob;
class QPaintEvent;
class QLabel;
class QGridLayout;
class QTreeWidget;
class QPushButton;

/**
 * @class ChartWidget
 * @brief Chart drawing widget.
 *
 */
class ChartWidget : public QWidget
{
public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void setSegments(const QList<int> &segments);

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

private:
    QList<int> m_segments;
};

/**
 * @class TreeWidgetItem
 * @brief Manage custom sort order for size.
 *
 */

class TreeWidgetItem : public QTreeWidgetItem
{
public:
    TreeWidgetItem(QTreeWidget *parent): QTreeWidgetItem(parent) {}
private:
    bool operator<(const QTreeWidgetItem &other)const Q_DECL_OVERRIDE
    {
        int column = treeWidget()->sortColumn();
        switch (column) {
        case 0:
            return text(column).toLower() < other.text(column).toLower();
            break;
        default:
            return data(column, Qt::UserRole) < other.data(column, Qt::UserRole);
            break;
        }
    }
};

/**
 * @class TemporaryData
 * @brief Dialog allowing management of project's temporary data.
 *
 */

class TemporaryData : public QWidget
{
    Q_OBJECT

public:
    explicit TemporaryData(KdenliveDoc *doc, bool currentProjectOnly, QWidget *parent = nullptr);

private:
    KdenliveDoc *m_doc;
    ChartWidget *m_currentPie;
    ChartWidget *m_globalPie;
    QLabel *m_previewSize;
    QLabel *m_proxySize;
    QLabel *m_audioSize;
    QLabel *m_thumbSize;
    QLabel *m_currentSize;
    QLabel *m_globalSize;
    QLabel *m_selectedSize;
    QWidget *m_currentPage;
    QWidget *m_globalPage;
    QTreeWidget *m_listWidget;
    QGridLayout *m_grid;
    qulonglong m_totalCurrent;
    qulonglong m_totalGlobal;
    QList<qulonglong> mCurrentSizes;
    QList<qulonglong> mGlobalSizes;
    QStringList m_globalDirectories;
    QString m_processingDirectory;
    QDir m_globalDir;
    QStringList m_proxies;
    QPushButton *m_globalDelete;
    void updateDataInfo();
    void updateGlobalInfo();
    void updateTotal();
    void buildGlobalCacheDialog(int minHeight);
    void processglobalDirectories();

private slots:
    void gotPreviewSize(KJob *job);
    void gotProxySize(qint64 total);
    void gotAudioSize(KJob *job);
    void gotThumbSize(KJob *job);
    void gotFolderSize(KJob *job);
    void refreshGlobalPie();
    void deletePreview();
    void deleteProxy();
    void deleteAudio();
    void deleteThumbs();
    void deleteCurrentCacheData();
    void openCacheFolder();
    void deleteSelected();

signals:
    void disableProxies();
    void disablePreview();
};

#endif
