/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2011 by Marco Gittler (marco@gitma.de)                  *
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


#ifndef FREESOUND_H
#define FREESOUND_H


#include "ui_freesound_ui.h"

#include <QDialog>
#include <QProcess>
#include <kio/jobclasses.h>

enum SERVICETYPE { FREESOUND = 0, OPENCLIPART = 1 };

class FreeSound : public QDialog, public Ui::FreeSound_UI
{
    Q_OBJECT

public:
    FreeSound(const QString & folder, QWidget * parent = 0);
    ~FreeSound();


private slots:
    void slotStartSearch(int page = 0);
    void slotDataIsHere(KIO::Job *,const QByteArray & data );
    void slotShowResults();
    void slotUpdateCurrentSound();
    void slotPlaySound();
    void slotForcePlaySound(bool play);
    void slotPreviewStatusChanged(QProcess::ProcessState state);
    void slotSaveSound();
    void slotOpenUrl(const QString &url);
    void slotChangeService();
    void slotOnline();
    void slotOffline();
    void slotNextPage();
    void slotPreviousPage();

private:
    QString m_folder;
    QByteArray m_result;
    QVariant m_data;
    QString m_currentPreview;
    QString m_currentUrl;
    QProcess *m_previewProcess;
    SERVICETYPE m_service;
   
signals:
    void addClip(KUrl, const QString &);
};


#endif

