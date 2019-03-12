/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef MACROS_H
#define MACROS_H

/*  This file contains a collection of macros that can be used in model related classes.
    The class only needs to have the following members:
    - For Push_undo : std::weak_ptr<DocUndoStack> m_undoStack;  this is a pointer to the undoStack
    - For Update_undo_redo: mutable QReadWriteLock m_lock; This is a lock that ensures safety in case of concurrent access. Note that the mutex must be
   recursive.

    See for example TimelineModel.

    Note that there also exists a version of update_undo_redo without the need for a lock (but prefer the mutex version where applicable)
*/

/* This convenience macro adds lock/unlock ability to a given lambda function
   Note that it is automatically called when you push the lambda so you shouldn't have
   to call it directly yourself
*/
#define LOCK_IN_LAMBDA(lambda)                                                                                                                                 \
    lambda = [this, lambda]() {                                                                                                                                \
        m_lock.lockForWrite();                                                                                                                                 \
        bool res_lambda = lambda();                                                                                                                            \
        m_lock.unlock();                                                                                                                                       \
        return res_lambda;                                                                                                                                     \
    };

/*This convenience macro locks the mutex for reading.
Note that it might happen that a thread is executing a write operation that requires
reading a Read-protected property. In that case, we try to write lock it first (this will be granted since the lock is recursive)
*/
#define READ_LOCK()                                                                                                                                            \
    std::unique_ptr<QReadLocker> rlocker(new QReadLocker(nullptr));                                                                                            \
    std::unique_ptr<QWriteLocker> wlocker(new QWriteLocker(nullptr));                                                                                          \
    if (m_lock.tryLockForWrite()) {                                                                                                                            \
        /*we yield ownership of the lock to the WriteLocker*/                                                                                                  \
        m_lock.unlock();                                                                                                                                       \
        wlocker.reset(new QWriteLocker(&m_lock));                                                                                                              \
    } else {                                                                                                                                                   \
        rlocker.reset(new QReadLocker(&m_lock));                                                                                                               \
    }

/* @brief This macro takes some lambdas that represent undo/redo for an operation and the text (name) associated with this operation
   The lambdas are transformed to make sure they lock access to the class they operate on.
   Then they are added on the undoStack
*/
#define PUSH_UNDO(undo, redo, text)                                                                                                                            \
    if (auto ptr = m_undoStack.lock()) {                                                                                                                       \
        ptr->push(new FunctionalUndoCommand(undo, redo, text));                                                                                                \
    } else {                                                                                                                                                   \
        qDebug() << "ERROR : unable to access undo stack";                                                                                                     \
        Q_ASSERT(false);                                                                                                                                       \
    }

/* @brief This macro takes as parameter one atomic operation and its reverse, and update
   the undo and redo functional stacks/queue accordingly
   This should be used in the rare case where we don't need a lock mutex. In general, prefer the other version
*/
#define UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo)                                                                                                \
    undo = [reverse, undo]() {                                                                                                                                 \
        bool v = reverse();                                                                                                                                    \
        return undo() && v;                                                                                                                                    \
    };                                                                                                                                                         \
    redo = [operation, redo]() {                                                                                                                               \
        bool v = redo();                                                                                                                                       \
        return operation() && v;                                                                                                                               \
    };
/* @brief This macro takes as parameter one atomic operation and its reverse, and update
   the undo and redo functional stacks/queue accordingly
   It will also ensure that operation and reverse are dealing with mutexes
*/
#define UPDATE_UNDO_REDO(operation, reverse, undo, redo)                                                                                                       \
    LOCK_IN_LAMBDA(operation)                                                                                                                                  \
    LOCK_IN_LAMBDA(reverse)                                                                                                                                    \
    UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo)

#endif
