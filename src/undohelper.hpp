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

#ifndef UNDOHELPER_H
#define UNDOHELPER_H
#include <functional>

using Fun = std::function<bool(void)>;

/* @brief this macro executes an operation after a given lambda
 */
#define PUSH_LAMBDA(operation, lambda)                                                                                                                         \
    lambda = [lambda, operation]() {                                                                                                                           \
        bool v = lambda();                                                                                                                                     \
        return v && operation();                                                                                                                               \
    };

/* @brief this macro executes an operation before a given lambda
 */
#define PUSH_FRONT_LAMBDA(operation, lambda)                                                                                                                   \
    lambda = [lambda, operation]() {                                                                                                                           \
        bool v = operation();                                                                                                                                  \
        return v && lambda();                                                                                                                                  \
    };

#include <QUndoCommand>

/*@brief this is a generic class that takes fonctors as undo and redo actions. It just executes them when required by Qt
  Note that QUndoStack actually executes redo() when we push the undoCommand to the stack
  This is bad for us because we execute the command as we construct the undo Function. So to prevent it to be executed twice, there is a small hack in this
  command that prevent redoing if it has not been undone before.
 */
class FunctionalUndoCommand : public QUndoCommand
{
public:
    FunctionalUndoCommand(Fun undo, Fun redo, const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Fun m_undo, m_redo;
    bool m_undone;
};

#endif
