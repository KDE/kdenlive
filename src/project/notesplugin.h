/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

class NotesWidget;
class KdenliveDoc;
class ProjectManager;
class QDockWidget;
class QToolBar;
class QAction;

/** @class NotesPlugin
    @brief Handles connection of NotesWidget
 
    Supposed to become a plugin/ProjectPart (\@see refactoring branch).
 */
class NotesPlugin : public QObject
{
    Q_OBJECT

public:
    explicit NotesPlugin(QObject *parent);
    NotesWidget *widget();
    void clear();
    void showDock();
    void loadNotes(QString text);

private Q_SLOTS:
    void setProject(KdenliveDoc *document);
    /** @brief Insert current timecode/cursor position into the widget. */
    void slotInsertTimecode();
    /** @brief Re-assign timestamps to current Bin Clip. */
    void slotReAssign(const QStringList &anchors, const QList <QPoint> &points);
    /** @brief Insert the given text into the widget. */
    void slotInsertText(const QString &text);

private:
    NotesWidget *m_widget;
    QDockWidget *m_notesDock;
    QToolBar *m_tb;
    QAction *m_markDownEdit;
};
