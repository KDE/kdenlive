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

#ifndef ABSTRACTCLIPJOB
#define ABSTRACTCLIPJOB

#include <QObject>

#include "definitions.h"
#include "undohelper.hpp"
#include <memory>

/**
 * @class AbstractClipJob
 * @brief This is the base class for all Kdenlive clip jobs.
 *
 */

struct Job_t;
class AbstractClipJob : public QObject
{
    Q_OBJECT

public:
    enum JOBTYPE {
        NOJOBTYPE = 0,
        PROXYJOB = 1,
        CUTJOB = 2,
        STABILIZEJOB = 3,
        TRANSCODEJOB = 4,
        FILTERCLIPJOB = 5,
        THUMBJOB = 6,
        ANALYSECLIPJOB = 7,
        LOADJOB = 8,
        AUDIOTHUMBJOB = 9,
        SPEEDJOB = 10,
        CACHEJOB = 11
    };
    AbstractClipJob(JOBTYPE type, QString id, QObject *parent = nullptr);
    ~AbstractClipJob() override;

    template <typename T, typename... Args> static std::shared_ptr<T> make(const QString &binId, Args &&... args)
    {
        auto m = std::make_shared<T>(binId, std::forward<Args>(args)...);
        return m;
    }

    /** @brief Returns the id of the bin clip that this job is working on. */
    const QString clipId() const;
    const QString getErrorMessage() const;
    const QString getLogDetails() const;
    virtual const QString getDescription() const = 0;

    virtual bool startJob() = 0;

    /** @brief This is to be called after the job finished.
        By design, the job should store the result of the computation but not share it with the rest of the code. This happens when we call commitResult
        This methods return true on success
    */
    virtual bool commitResult(Fun &undo, Fun &redo) = 0;

    // brief run a given job
    static bool execute(const std::shared_ptr<AbstractClipJob> &job);

    /* @brief return the type of this job */
    JOBTYPE jobType() const;

protected:
    QString m_clipId;
    QString m_errorMessage;
    QString m_logDetails;
    int m_addClipToProject;
    JOBTYPE m_jobType;
    int m_inPoint;
    int m_outPoint;
    bool m_resultConsumed{false};

signals:
    // send an int between 0 and 100 to reflect computation progress
    void jobProgress(int);
    void jobCanceled();
};

#endif
