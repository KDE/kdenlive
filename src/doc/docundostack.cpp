/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#include "docundostack.hpp"
#include <QUndoCommand>
#include <QUndoGroup>

DocUndoStack::DocUndoStack(QUndoGroup *parent)
    : QUndoStack(parent)
{
}

// TODO: custom undostack everywhere do that
void DocUndoStack::push(QUndoCommand *cmd)
{
    if (index() < count()) {
        emit invalidate(index());
    }
    QUndoStack::push(cmd);
}
