/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <kddockwidgets/DockWidget.h>

class NotesWidget;
class KdenliveDoc;
class ProjectManager;
class QToolBar;
class QFrame;
class QLineEdit;
class QToolButton;
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

private Q_SLOTS:
    void setProject(KdenliveDoc *document);
    /** @brief Insert current timecode/cursor position into the widget. */
    void slotInsertTimecode();
    /** @brief Re-assign timestamps to current Bin Clip. */
    void slotReAssign(const QStringList &anchors, const QList<QPoint> &points, QString binId = QString(), int offset = 0);
    /** @brief Insert the given text into the widget. */
    void slotInsertText(const QString &text);
    /** @brief Show / hide the find toolbar. */
    void find();
    /** @brief Find next occurrence of a search. */
    void findNext();
    /** @brief Find previous occurrence of a search. */
    void findPrevious();
    /** @brief Check if editor has a selection and timecodes in it. */
    void checkSelection();

private:
    NotesWidget *m_widget;
    KDDockWidgets::QtWidgets::DockWidget *m_notesDock;
    QToolBar *m_tb;
    QFrame *m_searchFrame;
    QLineEdit *m_searchLine;
    QToolButton *m_button_next;
    QToolButton *m_button_prev;
    QToolButton *m_showSearch;
    QAction *m_findAction;
    QAction *m_createFromSelection;
    QAction *m_reassingToBin;
    QAction *m_reassingToTimeline;
    void clipRenamed(const QString &uuid, const QString &newName, QPair<QStringList, QList<QPoint>> anchors);
};
