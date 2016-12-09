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

class ProjectClip;

/**
 * @class CutClipJob
 * @brief This job class will either transcode or render a part of a clip through FFmpeg or LibAV
 *
 */

class CutClipJob : public AbstractClipJob
{
    Q_OBJECT

public:
    /** @brief Creates the Job.
     *  @param cType the Clip Type (AV, PLAYLIST, AUDIO, ...) as defined in definitions.h. Some jobs will act differently depending on clip type
     *  @param id the id of the clip that requested this clip job
     *  @param parameters StringList that should contain: destination file << source file << in point (optional) << out point (optional)
     * */
    CutClipJob(ClipType cType, const QString &id, const QStringList &parameters);
    virtual ~ CutClipJob();
    const QString destination() const Q_DECL_OVERRIDE;
    void startJob() Q_DECL_OVERRIDE;
    stringMap cancelProperties() Q_DECL_OVERRIDE;
    const QString statusMessage() Q_DECL_OVERRIDE;
    bool isExclusive() Q_DECL_OVERRIDE;
    static QHash<ProjectClip *, AbstractClipJob *> prepareTranscodeJob(double fps, const QList<ProjectClip *> &ids,  const QStringList &parameters);
    static QHash<ProjectClip *, AbstractClipJob *> prepareCutClipJob(double fps, double originalFps, ProjectClip *clip);
    static QHash<ProjectClip *, AbstractClipJob *> prepareAnalyseJob(double fps, const QList<ProjectClip *> &clips, const QStringList &parameters);
    static QList<ProjectClip *> filterClips(const QList<ProjectClip *> &clips, const QStringList &params);

private:
    QString m_dest;
    QString m_src;
    QString m_start;
    QString m_end;
    QString m_cutExtraParams;
    int m_jobDuration;

    void processLogInfo() Q_DECL_OVERRIDE;
    void analyseLogInfo();
    void processAnalyseLog();

signals:
    /** @brief When user requested a to process an Mlt::Filter, this will send back all necessary infos. */
    void gotFilterJobResults(const QString &id, int startPos, int track, const stringMap &result, const stringMap &extra);
};

#endif
