/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "undohelper.hpp"
#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
#endif
#include <QDebug>
#include <QTime>
#include <utility>
FunctionalUndoCommand::FunctionalUndoCommand(Fun undo, Fun redo, const QString &text, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_undo(std::move(undo))
    , m_redo(std::move(redo))
    , m_undone(false)
{
    setText(QString("%1 %2").arg(QTime::currentTime().toString("hh:mm")).arg(text));
}

void FunctionalUndoCommand::undo()
{
    // qDebug() << "UNDOING " <<text();
#ifdef CRASH_AUTO_TEST
    Logger::log_undo(true);
#endif
    m_undone = true;
    bool res = m_undo();
    Q_ASSERT(res);
    QUndoCommand::undo();
}

void FunctionalUndoCommand::redo()
{
    if (m_undone) {
        // qDebug() << "REDOING " <<text();
#ifdef CRASH_AUTO_TEST
        Logger::log_undo(false);
#endif
        bool res = m_redo();
        Q_ASSERT(res);
    }
    QUndoCommand::redo();
}
