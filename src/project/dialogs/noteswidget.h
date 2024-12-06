/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QTextEdit>

/** @class NotesWidget
    @brief A small text editor to create project notes.
    @author Jean-Baptiste Mardelle
 */
class NotesWidget : public QTextEdit
{
    Q_OBJECT
public:
    explicit NotesWidget(QWidget *parent = nullptr);
    ~NotesWidget() override;
    /** @brief insert current timeline timecode and focus widget to allow entering quick note */
    void addProjectNote();
    /** @brief insert given text and focus widget to allow entering quick note
     * @param text the text
    */
    void addTextNote(const QString &text);
    void insertMarkDown(const QString &md);

protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void insertFromMimeData(const QMimeData *source) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool event(QEvent *event) override;

public Q_SLOTS:
    void createMarkers();
    void assignProjectNote();
    void switchMarkDownEditing(bool enable);

private:
    void createMarker(const QStringList &anchors);
    QPair <QStringList, QList <QPoint> > getSelectedAnchors();

Q_SIGNALS:
    void insertNotesTimecode();
    void insertTextNote(const QString &text);
    void seekProject(const QString);
    void reAssign(QStringList anchors, QList <QPoint> points);
};
