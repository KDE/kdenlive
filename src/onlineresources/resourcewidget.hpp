/***************************************************************************
 *   Copyright (C) 2021 by Julius Künzel (jk.kdedev@smartlab.uber.space)   *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.*
 ***************************************************************************/

#ifndef REOURCEWIDGET_H
#define REOURCEWIDGET_H

#include "ui_resourcewidget_ui.h"
#include "providersrepository.hpp"

#include <QWidget>
#include <QUrl>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QSlider>
#include <QListWidgetItem>
#include <QProcess>
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

#endif // REOURCEWIDGET_H
