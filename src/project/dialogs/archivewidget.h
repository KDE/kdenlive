/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle (jb@kdenlive.org)
    SPDX-FileCopyrightText: 2021 Julius Künzel (jk.kdedev@smartalb.uber.space)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef ARCHIVEWIDGET_H
#define ARCHIVEWIDGET_H

#include "ui_archivewidget_ui.h"
#include "timeline2/model/timelinemodel.hpp"

#include <KIO/CopyJob>
#include <QTemporaryFile>
#include <kio/global.h>

#include <QDialog>
#include <QDomDocument>
#include <QFuture>
#include <memory>

class KJob;
class KArchive;

class KMessageWidget;

/** @class ArchiveWidget
    @brief A widget allowing to archive a project (copy all project files to a new location)
    @author Jean-Baptiste Mardelle
 */
class ArchiveWidget : public QDialog, public Ui::ArchiveWidget_UI
{
    Q_OBJECT

public:
    ArchiveWidget(const QString &projectName, const QString xmlData, const QStringList &luma_list, const QStringList &other_list, QWidget *parent = nullptr);
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
    void onlyTimelineItems(int onlyTimeline);

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    enum {
        ClipIdRole = Qt::UserRole + 1,
        SlideshowImagesRole,
        SlideshowSizeRole,
        IsInTimelineRole,
    };
    KIO::filesize_t m_requestedSize, m_timelineSize;
    KIO::CopyJob *m_copyJob;
    QMap<QUrl, QUrl> m_duplicateFiles;
    QMap<QUrl, QUrl> m_replacementList;
    QString m_name;
    QString m_archiveName;
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
    /** @brief Make urls in the given playlist file (*.mlt) relative.
    * @param filename the url of the *.mlt file
    * @returns the files content with replaced urls
    */
    QString processPlaylistFile(const QString &filename);
    /** @brief Make urls in project file relative */
    bool processProjectFile();
    /** @brief Replace urls in mlt doc.
    * @param doc the xml document
    * @param destPrefix (optional) prefix to put before each new file path
    * @returns the doc's content with replaced urls
    */
    QString processMltFile(QDomDocument doc, const QString &destPrefix = QString());
    /** @brief If the given element contains the property its content (url) will be converted to a relative file path
     *  @param e the dom element  that might contains the property
     *  @param propertyName name of the property that should be checked
     *  @param root rootpath of the parent mlt document
    */
    void propertyProcessUrl(QDomElement e, QString propertyName, QString root);

signals:
    void archivingFinished(bool);
    void archiveProgress(int);
    void extractingFinished();
    void showMessage(const QString &, const QString &);
};

#endif
