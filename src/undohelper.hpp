/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <functional>

using Fun = std::function<bool(void)>;

/** @brief this macro executes an operation after a given lambda
 */
#define PUSH_LAMBDA(operation, lambda)                                                                                                                         \
    lambda = [lambda, operation]() {                                                                                                                           \
        bool v = lambda();                                                                                                                                     \
        return v && operation();                                                                                                                               \
    };

/** @brief this macro executes an operation before a given lambda
 */
#define PUSH_FRONT_LAMBDA(operation, lambda)                                                                                                                   \
    lambda = [lambda, operation]() {                                                                                                                           \
        bool v = operation();                                                                                                                                  \
        return v && lambda();                                                                                                                                  \
    };

#include <QUndoCommand>

/** @brief this is a generic class that takes fonctors as undo and redo actions. It just executes them when required by Qt
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
