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

#include <KIO/CopyJob>
#include <QTemporaryFile>
#include <kio/global.h>

#include <QDialog>
#include <QDomDocument>
#include <QFuture>
#include <memory>

class KJob;
class KArchive;

/**
 * @class ArchiveWidget
 * @brief A widget allowing to archive a project (copy all project files to a new location)
 * @author Jean-Baptiste Mardelle
 */

class KMessageWidget;

class ArchiveWidget : public QDialog, public Ui::ArchiveWidget_UI
{
    Q_OBJECT

public:
    ArchiveWidget(const QString &projectName, const QString xmlData, const QStringList &luma_list, QWidget *parent = nullptr);
    // Constructor for extracting widget
    explicit ArchiveWidget(QUrl url, QWidget *parent = nullptr);
    ~ArchiveWidget() override;

    QString extractedProjectFile() const;

private slots:
    void slotCheckSpace();
    bool slotStartArchiving(bool firstPass = true);
    void slotArchivingFinished(KJob *job = nullptr, bool finished = false);
    void slotArchivingProgress(KJob *, qulonglong);
    void done(int r) Q_DECL_OVERRIDE;
    bool closeAccepted();
    void createArchive();
    void slotArchivingIntProgress(int);
    void slotArchivingBoolFinished(bool result);
    void slotStartExtracting();
    void doExtracting();
    void slotExtractingFinished();
    void slotExtractProgress();
    void slotGotProgress(KJob *);
    void openArchiveForExtraction();
    void slotDisplayMessage(const QString &icon, const QString &text);
    void slotJobResult(bool success, const QString &text);
    void slotProxyOnly(int onlyProxy);

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    KIO::filesize_t m_requestedSize;
    KIO::CopyJob *m_copyJob;
    QMap<QUrl, QUrl> m_duplicateFiles;
    QMap<QUrl, QUrl> m_replacementList;
    QString m_name;
    QDomDocument m_doc;
    QTemporaryFile *m_temp;
    bool m_abortArchive;
    QFuture<void> m_archiveThread;
    QStringList m_foldersList;
    QMap<QString, QString> m_filesList;
    bool m_extractMode;
    QUrl m_extractUrl;
    QString m_projectName;
    QTimer *m_progressTimer;
    KArchive *m_extractArchive;
    int m_missingClips;
    KMessageWidget *m_infoMessage;

    /** @brief Generate tree widget subitems from a string list of urls. */
    void generateItems(QTreeWidgetItem *parentItem, const QStringList &items);
    /** @brief Generate tree widget subitems from a map of clip ids / urls. */
    void generateItems(QTreeWidgetItem *parentItem, const QMap<QString, QString> &items);
    /** @brief Replace urls in project file. */
    bool processProjectFile();

signals:
    void archivingFinished(bool);
    void archiveProgress(int);
    void extractingFinished();
    void showMessage(const QString &, const QString &);
};

#endif
