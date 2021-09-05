/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TEMPORARYDATA_H
#define TEMPORARYDATA_H

#include "ui_managecache_ui.h"
#include "definitions.h"
#include <KIO/DirectorySizeJob>
#include <QDir>
#include <QTreeWidgetItem>
#include <QDialog>

class KdenliveDoc;
class QPaintEvent;
class QLabel;
class QGridLayout;
class QTreeWidget;
class QPushButton;
class QToolButton;

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
    void paintEvent(QPaintEvent *event) override;

private:
    QList<int> m_segments;
};

/** @class TreeWidgetItem
    @brief Manage custom sort order for size.
  */
class TreeWidgetItem : public QTreeWidgetItem
{
public:
    TreeWidgetItem(QTreeWidget *parent)
        : QTreeWidgetItem(parent)
    {
    }

private:
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        switch (column) {
            case 1:
                return data(column, Qt::UserRole).toULongLong() < other.data(column, Qt::UserRole).toULongLong();
                break;
            case 2:
                return data(column, Qt::UserRole).toDateTime() < other.data(column, Qt::UserRole).toDateTime();
                break;
            default:
                return text(column).toLower() < other.text(column).toLower();
                break;
        }
    }
};

/** @class TemporaryData
    @brief Dialog allowing management of cache data.
 */
class TemporaryData : public QDialog, public Ui::ManageCache_UI
{
    Q_OBJECT

public:
    explicit TemporaryData(KdenliveDoc *doc, bool currentProjectOnly, QWidget *parent = nullptr);

private:
    KdenliveDoc *m_doc;
    ChartWidget *m_currentPie;
    ChartWidget *m_globalPie;
    bool m_currentProjectOnly;
    KIO::filesize_t m_totalCurrent;
    KIO::filesize_t m_totalGlobal;
    QList<KIO::filesize_t> m_currentSizes;
    QStringList m_globalDirectories;
    QString m_processingDirectory;
    QDir m_globalDir;
    QStringList m_proxies;
    void updateDataInfo();
    void updateGlobalInfo();
    void updateTotal();
    void processglobalDirectories();
    void processBackupDirectories();
    void processProxyDirectory();
    void deleteCache(QStringList &folders);

private slots:
    void gotPreviewSize(KJob *job);
    void gotProxySize(KIO::filesize_t total);
    void gotAudioSize(KJob *job);
    void gotThumbSize(KJob *job);
    void gotFolderSize(KJob *job);
    void gotBackupSize(KJob *job);
    void gotProjectProxySize(KJob *job);
    void refreshGlobalPie();
    void deletePreview();
    void deleteProjectProxy();
    void deleteProxy();
    void deleteAudio();
    void deleteThumbs();
    void deleteCurrentCacheData();
    void deleteBackup();
    void cleanBackup();
    void openCacheFolder();
    void deleteSelected();
    void cleanCache();
    void cleanProxy();

signals:
    void disableProxies();
    void disablePreview();
};

#endif
