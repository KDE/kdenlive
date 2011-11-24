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
#include <QProcess>
#include <QFuture>

class QTimer;
namespace Mlt{
	class Profile;
	class Playlist;
	class Consumer;
	class Filter;
};

class ClipStabilize : public QDialog, public Ui::ClipStabilize_UI
{
    Q_OBJECT

public:
    ClipStabilize(KUrl::List urls, const QString &params, Mlt::Filter* filter,QWidget * parent = 0);
    ~ClipStabilize();


private slots:
    void slotShowStabilizeInfo();
    void slotStartStabilize();
    void slotStabilizeFinished(bool success);
	void slotRunStabilize();
	void slotAbortStabilize();
	void slotUpdateParams();

private:
	QFuture<void> m_stabilizeRun;
	QString filtername;
	Mlt::Profile *m_profile;
	Mlt::Consumer *m_consumer;
	Mlt::Playlist *m_playlist;
    KUrl::List m_urls;
    int m_duration;
	Mlt::Filter* m_filter;
	QTimer *m_timer;
	QHash<QString,QHash<QString,QString> > m_ui_params;
	QVBoxLayout *vbox;
	void fillParamaters(QStringList);

signals:
    void addClip(KUrl url);
};


#endif

