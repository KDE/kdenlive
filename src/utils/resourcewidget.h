/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef RESOURCEWIDGET_H
#define RESOURCEWIDGET_H


#include "ui_freesound_ui.h"
#include "abstractservice.h"

#include <QDialog>
#include <QProcess>
#include <kio/jobclasses.h>
#include <kdeversion.h>


#if KDE_IS_VERSION(4,4,0)
class KPixmapSequenceOverlayPainter;
#endif
class QAction;

class ResourceWidget : public QDialog, public Ui::FreeSound_UI
{
    Q_OBJECT

public:
    explicit ResourceWidget(const QString & folder, QWidget * parent = 0);
    ~ResourceWidget();


private slots:
    void slotStartSearch(int page = 0);
    void slotUpdateCurrentSound();
    void slotPlaySound();
    void slotDisplayMetaInfo(QMap <QString, QString> metaInfo);
    void slotSaveItem(const QString originalUrl = QString());
    void slotOpenUrl(const QString &url);
    void slotChangeService();
    void slotOnline();
    void slotOffline();
    void slotNextPage();
    void slotPreviousPage();
    void slotOpenLink(const QUrl &url);
    void slotLoadThumb(const QString url);
    /** @brief A file download is finished */
    void slotGotFile(KJob *job);
    void slotSetMetadata(const QString desc);
    void slotSetDescription(const QString desc);
    void slotSetImage(const QString desc);
    void slotSetTitle(const QString desc);

private:
    QString m_folder;
    AbstractService *m_currentService;
    void parseLicense(const QString &);
    OnlineItemInfo m_currentInfo;
#if KDE_IS_VERSION(4,4,0)
    KPixmapSequenceOverlayPainter *m_busyWidget;
#endif
    QAction *m_autoPlay;
    QString m_tmpThumbFile;
    QString m_title;
    QString m_image;
    QString m_desc;
    QString m_meta;
    void updateLayout();
   
signals:
    void addClip(KUrl, QMap <QString, QString> data);
};


#endif

