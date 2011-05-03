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

#include <KDialog>
#include <kio/global.h>
#include <QLabel>
#include <QList>
#include <KIO/Job>
#include <KIO/CopyJob>

class KJob;

/**
 * @class ArchiveWidget
 * @brief A widget allowing to archive a project (copy all project files to a new location)
 * @author Jean-Baptiste Mardelle
 */

class ArchiveWidget : public QDialog, public Ui::ArchiveWidget_UI
{
    Q_OBJECT

public:
    ArchiveWidget(QList <DocClipBase*> list, QStringList lumas, QWidget * parent = 0);
    ~ArchiveWidget();
    
private slots:
    void slotCheckSpace();
    void slotStartArchiving();
    void slotArchivingFinished(KJob *job);
    void slotArchivingProgress(KJob *, qulonglong);

private:
    KIO::filesize_t m_requestedSize;
    KIO::CopyJob *m_copyJob;

signals:

};


#endif

