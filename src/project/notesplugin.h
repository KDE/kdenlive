/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef NOTESPLUGIN_H
#define NOTESPLUGIN_H

#include <QObject>

class NotesWidget;
class KdenliveDoc;
class ProjectManager;
class QDockWidget;
class QToolBar;

/** @class NotesPlugin
    @brief Handles connection of NotesWidget
 
    Supposed to become a plugin/ProjectPart (\@see refactoring branch).
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
    /** @brief Insert the given text into the widget. */
    void slotInsertText(const QString &text);

private:
    NotesWidget *m_widget;
    QDockWidget *m_notesDock;
    QToolBar *m_tb;
};

#endif
