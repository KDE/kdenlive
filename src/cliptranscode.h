/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef CLIPTRANSCODE_H
#define CLIPTRANSCODE_H


#include "ui_cliptranscode_ui.h"

#include <KUrl>
#include <kdeversion.h>
#if KDE_IS_VERSION(4,7,0)
#include <KMessageWidget>
#endif

#include <QProcess>

class ClipTranscode : public QDialog, public Ui::ClipTranscode_UI
{
    Q_OBJECT

public:
    ClipTranscode(KUrl::List urls, const QString &params, const QStringList &postParams, const QString &description, bool automaticMode = false, QWidget * parent = 0);
    ~ClipTranscode();


private slots:
    void slotShowTranscodeInfo();
    void slotStartTransCode();
    void slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotUpdateParams(int ix = -1);

private:
    QProcess m_transcodeProcess;
    KUrl::List m_urls;
    int m_duration;
    bool m_automaticMode;
    /** @brief The path for destination transcoded file. */
    QString m_destination;
    QStringList m_postParams;

#if KDE_IS_VERSION(4,7,0)
    KMessageWidget *m_infoMessage;
#endif
    
signals:
    void addClip(KUrl url);
    void transcodedClip(KUrl source, KUrl result);
};


#endif

