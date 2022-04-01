/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
