/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */
#ifndef DOCUNDOSTACK_H
#define DOCUNDOSTACK_H

#include <QUndoCommand>

class QUndoGroup;
class QUndoCommand;

class DocUndoStack : public QUndoStack
{
    Q_OBJECT
public:
    explicit DocUndoStack(QUndoGroup *parent = Q_NULLPTR);
    void push(QUndoCommand *cmd);
signals:
    void invalidate(int ix);
};

#endif
