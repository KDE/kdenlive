/***************************************************************************
 *                                                                         *
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

#ifndef MELTJOB
#define MELTJOB

#include <QObject>
#include <QProcess>

#include "abstractclipjob.h"

class KUrl;

namespace Mlt{
        class Profile;
        class Producer;
        class Consumer;
        class Filter;
        class Event;
};

class MeltJob : public AbstractClipJob
{
    Q_OBJECT

public:
    MeltJob(CLIPTYPE cType, const QString &id, QStringList parameters, QMap <QString, QString> extraParams = QMap <QString, QString>());
    virtual ~ MeltJob();
    const QString destination() const;
    void startJob();
    stringMap cancelProperties();
    bool addClipToProject;
    const QString statusMessage();
    void setProducer(Mlt::Producer *producer, KUrl url);
    void emitFrameNumber();
    /** Make the job work on a project tree clip. */
    bool isProjectFilter() const;
    
private:
    Mlt::Producer *m_producer;
    Mlt::Profile *m_profile;
    Mlt::Consumer *m_consumer;
    Mlt::Event *m_showFrameEvent;
    QStringList m_params;
    QString m_dest;
    QString m_url;
    int m_length;
    QMap <QString, QString> m_extra;

signals:
    void gotFilterJobResults(const QString &id, int startPos, int track, stringMap result, stringMap extra);
};

#endif
