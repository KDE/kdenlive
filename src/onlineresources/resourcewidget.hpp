/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "providersrepository.hpp"
#include "ui_resourcewidget_ui.h"

#include <QListWidgetItem>
#include <QNetworkReply>
#include <QProcess>
#include <QSlider>
#include <QTemporaryFile>
#include <QUrl>
#include <QWidget>
#include <QtNetworkAuth>

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

private slots:
    void slotChangeProvider();
    void slotOpenUrl(const QString &url);
    void slotStartSearch();
    void slotSearchFinished(QList<ResourceItemInfo> &list, const int pageCount);
    void slotUpdateCurrentItem();
    void slotSetIconSize(int size);
    void slotPreviewItem();
    void slotChooseVersion(const QStringList &urls, const QStringList &labels, const QString &accessToken = QString());
    void slotSaveItem(const QString &originalUrl = QString(), const QString &accessToken = QString());
    void slotGotFile(KJob *job);
    void slotAccessTokenReceived(const QString &accessToken);

private:
    std::unique_ptr<ProviderModel> *m_currentProvider{nullptr};
    QListWidgetItem *m_currentItem{nullptr};
    QTemporaryFile *m_tmpThumbFile;
    /** @brief Default icon size for the views. */
    QSize m_iconSize;
    int wheelAccumulatedDelta;
    bool m_showloadingWarning;
    ResourceItemInfo getItemById(const QString &id);
    void loadConfig();
    void saveConfig();
    void blockUI(bool block);
    QString licenseNameFromUrl(const QString &licenseUrl, const bool shortName);

signals:
    void addClip(const QUrl &, const QString &);
    void addLicenseInfo(const QString &);
    void previewClip(const QString &path, const QString &title);
};
