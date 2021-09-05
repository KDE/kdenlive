/*
 *   SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef NOTESWIDGET_H
#define NOTESWIDGET_H

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

protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void insertFromMimeData(const QMimeData *source) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

public slots:
    void createMarkers();
    void assignProjectNote();

private:
    QAction *m_markerAction;
    void createMarker(QStringList anchors);
    QPair <QStringList, QList <QPoint> > getSelectedAnchors();

signals:
    void insertNotesTimecode();
    void insertTextNote(const QString &text);
    void seekProject(int);
    void reAssign(QStringList anchors, QList <QPoint> points);
};

#endif
