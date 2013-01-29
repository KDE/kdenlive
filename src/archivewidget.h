/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef ARCHIVEWIDGET_H
#define ARCHIVEWIDGET_H


#include "ui_archivewidget_ui.h"
#include "docclipbase.h"

#include <kio/global.h>
#include <KIO/CopyJob>
#include <KTemporaryFile>
#include <kdeversion.h>

#include <QLabel>
#include <QDialog>
#include <QList>


class KJob;
class KArchive;

/**
 * @class ArchiveWidget
 * @brief A widget allowing to archive a project (copy all project files to a new location)
 * @author Jean-Baptiste Mardelle
 */

#if KDE_IS_VERSION(4,7,0)
    class KMessageWidget;
#endif

class ArchiveWidget : public QDialog, public Ui::ArchiveWidget_UI
{
    Q_OBJECT

public:
    ArchiveWidget(QString projectName, QDomDocument doc, QList <DocClipBase*> list, QStringList luma_list, QWidget * parent = 0);
    // Constructor for extracting widget
    explicit ArchiveWidget(const KUrl &url, QWidget * parent = 0);
    ~ArchiveWidget();

    QString extractedProjectFile();
    
private slots:
    void slotCheckSpace();
    bool slotStartArchiving(bool firstPass = true);
    void slotArchivingFinished(KJob *job = NULL, bool finished = false);
    void slotArchivingProgress(KJob *, qulonglong);
    virtual void done ( int r );
    bool closeAccepted();
    void createArchive();
    void slotArchivingProgress(int);
    void slotArchivingFinished(bool result);
    void slotStartExtracting();
    void doExtracting();
    void slotExtractingFinished();
    void slotExtractProgress();
    void slotGotProgress(KJob*);
    void openArchiveForExtraction();
    void slotDisplayMessage(const QString &icon, const QString &text);
    void slotJobResult(bool success, const QString &text);
    void slotProxyOnly(int onlyProxy);

protected:
    virtual void closeEvent ( QCloseEvent * e );
    
private:
    KIO::filesize_t m_requestedSize;
    KIO::CopyJob *m_copyJob;
    QMap <KUrl, KUrl> m_duplicateFiles;
    QMap <KUrl, KUrl> m_replacementList;
    QString m_name;
    QDomDocument m_doc;
    KTemporaryFile *m_temp;
    bool m_abortArchive;
    QFuture<void> m_archiveThread;
    QStringList m_foldersList;
    QMap <QString, QString> m_filesList;
    bool m_extractMode;
    KUrl m_extractUrl;
    QString m_projectName;
    QTimer *m_progressTimer;
    KArchive *m_extractArchive;
    int m_missingClips;
    
#if KDE_IS_VERSION(4,7,0)
    KMessageWidget *m_infoMessage;
#endif

    /** @brief Generate tree widget subitems from a string list of urls. */
    void generateItems(QTreeWidgetItem *parentItem, QStringList items);
    /** @brief Generate tree widget subitems from a map of clip ids / urls. */
    void generateItems(QTreeWidgetItem *parentItem, QMap <QString, QString> items);
    /** @brief Replace urls in project file. */
    bool processProjectFile();

signals:
    void archivingFinished(bool);
    void archiveProgress(int);
    void extractingFinished();
    void showMessage(const QString &, const QString &);
};


#endif

