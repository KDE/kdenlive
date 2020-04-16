/*
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2017  Nicolas Carion
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QtConcurrent>
#include <type_traits>
template <typename T, typename... Args>
int JobManager::startJob(const std::vector<QString> &binIds, int parentId, QString undoString,
                         std::function<std::shared_ptr<T>(const QString &, Args...)> createFn, Args &&... args)
{
    static_assert(std::is_base_of<AbstractClipJob, T>::value, "Your job must inherit from AbstractClipJob");
    // QWriteLocker locker(&m_lock);
    int jobId = m_currentId++;
    std::shared_ptr<Job_t> job(new Job_t());
    job->m_completionMutex.lock();
    job->m_undoString = std::move(undoString);
    job->m_id = jobId;
    for (const auto &id : binIds) {
        job->m_job.push_back(createFn(id, args...));
        job->m_progress.push_back(0);
        job->m_indices[id] = size_t(int(job->m_job.size()) - 1);
        job->m_type = job->m_job.back()->jobType();
        m_jobsByClip[id].push_back(jobId);
    }
    m_lock.lockForWrite();
    int insertionRow = static_cast<int>(m_jobs.size());
    beginInsertRows(QModelIndex(), insertionRow, insertionRow);
    Q_ASSERT(m_jobs.count(jobId) == 0);
    m_jobs[jobId] = job;
    endInsertRows();
    m_lock.unlock();
    if (parentId == -1 || m_jobs.count(parentId) == 0 || m_jobs[parentId]->m_completionMutex.tryLock()) {
        if (parentId != -1 && m_jobs.count(parentId) > 0) {
            m_jobs[parentId]->m_completionMutex.unlock();
        }
        QtConcurrent::run(this, &JobManager::createJob, job);
    } else {
        m_jobsByParents[parentId].push_back(jobId);
    }
    return jobId;
}

// we must specialize the second version of startjob depending on the type (some types requires to use a prepareJob method). Because we cannot use partial
// specialization for functions, we resort to a static method of a class in this impl namespace we must specialize the second version of startjob depending on
// the type (some types requires to use a prepareJob method). Because we cannot use partial specialization for functions, we resort to a static method of a
// dummy struct in a namespace

namespace impl {

// This is a simple member detector borrowed from https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector
template <typename T> class Detect_prepareJob
{
    // clang-format off
    struct Fallback {int prepareJob;}; // add member name "prepareJob"
    struct Derived : T, Fallback {};
    // clang-format on

    template <typename U, U> struct Check;

    typedef char ArrayOfOne[1]; // typedef for an array of size one.
    typedef char ArrayOfTwo[2]; // typedef for an array of size two.

    template <typename U> static ArrayOfOne &func(Check<int Fallback::*, &U::prepareJob> *);
    template <typename U> static ArrayOfTwo &func(...);

public:
    typedef Detect_prepareJob type; // NOLINT
    enum { value = sizeof(func<Derived>(nullptr)) == 2 };
};

struct dummy
{

    template <typename T, bool Noprepare, typename... Args>
    static typename std::enable_if<!Detect_prepareJob<T>::value || Noprepare, int>::type
    exec(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString, Args &&... args)
    {
        auto defaultCreate = [](const QString &id, Args... local_args) { return AbstractClipJob::make<T>(id, std::forward<Args>(local_args)...); };
        using local_createFn_t = std::function<std::shared_ptr<T>(const QString &, Args...)>;
        return ptr->startJob<T, Args...>(binIds, parentId, std::move(undoString), local_createFn_t(std::move(defaultCreate)), std::forward<Args>(args)...);
    }
    template <typename T, bool Noprepare, typename... Args>
    static typename std::enable_if<Detect_prepareJob<T>::value && !Noprepare, int>::type
    exec(std::shared_ptr<JobManager> ptr, const std::vector<QString> &binIds, int parentId, QString undoString, Args &&... args)
    {
        // For job stabilization, there is a custom preparation function
        return T::prepareJob(ptr, binIds, parentId, std::move(undoString), std::forward<Args>(args)...);
    }
};

} // namespace impl

template <typename T, typename... Args> int JobManager::startJob(const std::vector<QString> &binIds, int parentId, QString undoString, Args &&... args)
{
    return impl::dummy::exec<T, false, Args...>(shared_from_this(), binIds, parentId, std::move(undoString), std::forward<Args>(args)...);
}

template <typename T, typename... Args>
int JobManager::startJob_noprepare(const std::vector<QString> &binIds, int parentId, QString undoString, Args &&... args)
{
    return impl::dummy::exec<T, true, Args...>(shared_from_this(), binIds, parentId, std::move(undoString), std::forward<Args>(args)...);
}
