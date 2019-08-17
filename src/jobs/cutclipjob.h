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

#ifndef CUTCLIPJOB
#define CUTCLIPJOB

#include "abstractclipjob.h"
#include <QProcess>

/**
 * @class CutClipJob
 * @brief This job class will either transcode or render a part of a clip through FFmpeg or LibAV
 *
 */
class JobManager;

class CutClipJob : public AbstractClipJob
{
    Q_OBJECT

public:
    /** @brief Extract part of a clip with ffmpeg whithout re-encoding
     */
    CutClipJob(const QString &binId, const QString sourcePath, GenTime inTime, GenTime outTime, const QString destPath, QStringList encodingParams);

    // This is a special function that prepares the stabilize job for a given list of clips.
    // Namely, it displays the required UI to configure the job and call startJob with the right set of parameters
    bool startJob() override;

    // Then the job is automatically put in queue. Its id is returned
    static int prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString, GenTime inTime, GenTime outTime);

    bool commitResult(Fun &undo, Fun &redo) override;
    const QString getDescription() const override;

private slots:
    void processLogInfo();

protected:
    QString m_sourceUrl;
    QString m_destUrl;
    bool m_done;
    std::unique_ptr<QProcess>m_jobProcess;
    GenTime m_in;
    GenTime m_out;
    QStringList m_encodingParams;
    int m_jobDuration;
};

#endif
