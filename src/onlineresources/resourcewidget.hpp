/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "providersrepository.hpp"
#include "ui_resourcewidget_ui.h"

#include <KJob>
#include <QElapsedTimer>
#include <QListWidgetItem>
#include <QMutex>
#include <QNetworkReply>
#include <QProcess>
#include <QSet>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QWidget>

const int imageRole = Qt::UserRole;
const int urlRole = Qt::UserRole + 1;
const int downloadRole = Qt::UserRole + 2;
const int durationRole = Qt::UserRole + 3;
const int previewRole = Qt::UserRole + 4;
const int authorRole = Qt::UserRole + 5;
const int authorUrl = Qt::UserRole + 6;
const int infoUrl = Qt::UserRole + 7;
const int infoData = Qt::UserRole + 8;
const int idRole = Qt::UserRole + 9;
const int licenseRole = Qt::UserRole + 10;
const int descriptionRole = Qt::UserRole + 11;
const int widthRole = Qt::UserRole + 12;
const int heightRole = Qt::UserRole + 13;
const int nameRole = Qt::UserRole + 14;
const int singleDownloadRole = Qt::UserRole + 15;
const int filetypeRole = Qt::UserRole + 16;
const int downloadLabelRole = Qt::UserRole + 17;

class ResourceWidget : public QWidget, public Ui::ResourceWidget_UI
{
    Q_OBJECT

public:
    explicit ResourceWidget(QWidget *parent = nullptr);
    ~ResourceWidget() override;

private Q_SLOTS:
    void slotChangeProvider();
    void slotOpenUrl(const QString &url);
    void slotStartSearch();
    void slotLoadImages();
    void slotShowPixmap(const QString &url, const QPixmap &pixmap);
    void slotSearchFinished(const QList<ResourceItemInfo> &list, const int pageCount);
    void slotDisplayError(const QString &message);
    void slotUpdateCurrentItem();
    void slotSetIconSize(int size);
    void slotPreviewItem();
    void slotChooseVersion(const QStringList &urls, const QStringList &labels, const QString &accessToken = QString());
    void slotSaveItem(const QString &originalUrl = QString(), const QString &accessToken = QString());
    void slotGotFile(KJob *job);
    void slotAccessTokenReceived(const QString &accessToken);
    void abortDownload();

private:
    std::unique_ptr<ProviderModel> *m_currentProvider{nullptr};
    QListWidgetItem *m_currentItem{nullptr};
    QStringList m_imagesUrl;
    QMutex m_imageLock;
    /** @brief Default icon size for the views. */
    QSize m_iconSize;
    int wheelAccumulatedDelta;
    bool m_showloadingWarning;
    QSet<QNetworkReply *> m_activeImageReplies;
    QSet<QTimer *> m_imageBackoffTimers;
    int m_backoff = 0;
    QElapsedTimer m_backoffCooldownTimer;
    QAction *m_stopAction;
    QNetworkAccessManager *m_networkManager{nullptr};
    ResourceItemInfo getItemById(const QString &id);
    void loadConfig();
    void saveConfig();
    void blockUI(bool block);
    QString licenseNameFromUrl(const QString &licenseUrl, const bool shortName);
    void downloadImage(const QString &url, QSharedPointer<QMap<QString, int>> retryCount);

Q_SIGNALS:
    void addClip(const QUrl &, const QString &);
    void addLicenseInfo(const QString &);
    void previewClip(const QString &path, const QString &title);
    void gotPixmap(const QString &url, const QPixmap &pix);
};
