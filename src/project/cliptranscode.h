/*
 *   SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef CLIPTRANSCODE_H
#define CLIPTRANSCODE_H

#include "ui_cliptranscode_ui.h"

#include <KMessageWidget>
#include <QUrl>

#include <QProcess>

class ClipTranscode : public QDialog, public Ui::ClipTranscode_UI
{
    Q_OBJECT

public:
    ClipTranscode(QStringList urls, const QString &params, QStringList postParams, const QString &description, QString folderInfo = QString(),
                  bool automaticMode = false, QWidget *parent = nullptr);
    ~ClipTranscode() override;

public slots:
    void slotStartTransCode();

private slots:
    void slotShowTranscodeInfo();
    void slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotUpdateParams(int ix = -1);

private:
    QProcess m_transcodeProcess;
    QStringList m_urls;
    QString m_folderInfo;
    int m_duration;
    bool m_automaticMode;
    /** @brief The path for destination transcoded file. */
    QString m_destination;
    QStringList m_postParams;
    KMessageWidget *m_infoMessage;

signals:
    void addClip(const QUrl &url, const QString &folderInfo = QString());
    void transcodedClip(const QUrl &source, const QUrl &result);
};

#endif
