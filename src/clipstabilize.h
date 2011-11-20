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


#ifndef CLIPSTABILIZE_H
#define CLIPSTABILIZE_H


#include "ui_clipstabilize_ui.h"

#include <KUrl>
#include <mlt++/Mlt.h>
#include <QProcess>

class ClipStabilize : public QDialog, public Ui::ClipStabilize_UI
{
    Q_OBJECT

public:
    ClipStabilize(KUrl::List urls, const QString &params, QWidget * parent = 0);
    ~ClipStabilize();


private slots:
    void slotShowStabilizeInfo();
    void slotStartStabilize();
    void slotStabilizeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotUpdateParams(int ix = -1);

private:
    QProcess m_stabilizeProcess;
	QString filtername;
	Mlt::Profile profile;
    KUrl::List m_urls;
    int m_duration;

signals:
    void addClip(KUrl url);
};


#endif

