/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <KRuler>
#include <QGroupBox>
#include <QGraphicsScene>
#include <QGraphicsLineItem>

#define FRAME_SIZE 90

#include "ui_timeline_ui.h"
#include "customruler.h"
#include "kdenlivedoc.h"
#include "customtrackview.h"
class ClipItem;

class TrackView : public QWidget {
    Q_OBJECT

public:
    TrackView(KdenliveDoc *doc, QWidget *parent = 0);

    const double zoomFactor() const;
    const int mapLocalToValue(int x) const;
    void setEditMode(const QString & editMode);
    const QString & editMode() const;
    QGraphicsScene *projectScene();
    CustomTrackView *projectView();
    int duration() const;
    int tracksNumber() const;
    KdenliveDoc *document();
    void refresh() ;
    int outPoint() const;
    int inPoint() const;
    int currentZoom() const;
    int fitZoom() const;

public slots:
    void slotDeleteClip(int clipId);
    void slotChangeZoom(int factor);
    void setDuration(int dur);

private:
    Ui::TimeLine_UI *view;
    CustomRuler *m_ruler;
    CustomTrackView *m_trackview;
    double m_scale;
    int m_projectTracks;
    QString m_editMode;
    QGraphicsScene *m_scene;
    uint m_currentZoom;

    KdenliveDoc *m_doc;
    QVBoxLayout *m_tracksLayout;
    QVBoxLayout *m_headersLayout;
    QScrollArea *m_scrollArea;
    QFrame *m_scrollBox;
    QVBoxLayout *m_tracksAreaLayout;
    void parseDocument(QDomDocument doc);
    int slotAddAudioTrack(int ix, QDomElement xml);
    int slotAddVideoTrack(int ix, QDomElement xml);

private slots:
    void setCursorPos(int pos);
    void moveCursorPos(int pos);
    void slotClipItemSelected(ClipItem*);

signals:
    void mousePosition(int);
    void cursorMoved();
    void clipItemSelected(ClipItem*);
};

#endif
