/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef NOTESPLUGIN_H
#define NOTESPLUGIN_H

#include <QObject>

class NotesWidget;
class KdenliveDoc;
class ProjectManager;
class QDockWidget;
class QToolBar;

/**
 * @class NotesPlugin
 * @brief Handles connection of NotesWidget
 *
 * Supposed to become a plugin/ProjectPart (@see refactoring branch).
 */

class NotesPlugin : public QObject
{
    Q_OBJECT

public:
    explicit NotesPlugin(ProjectManager *projectManager);
    NotesWidget *widget();
    void clear();
    void showDock();

private slots:
    void setProject(KdenliveDoc *document);
    /** @brief Insert current timecode/cursor position into the widget. */
    void slotInsertTimecode();
    /** @brief Re-assign timestamps to current Bin Clip. */
    void slotReAssign(QStringList anchors, QList <QPoint> points);

private:
    NotesWidget *m_widget;
    QDockWidget *m_notesDock;
    QToolBar *m_tb;
};

#endif
