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
#include "documenttrack.h"
#include "trackpanelfunctionfactory.h"
#include "trackpanelfunction.h"
#include "customtrackview.h"

class TrackView : public QWidget
{
  Q_OBJECT
  
  public:
    TrackView(KdenliveDoc *doc, QWidget *parent=0);

	/** This event occurs when the mouse has been moved. */
    void mouseMoveEvent(QMouseEvent * event);

    const double zoomFactor() const;
    DocumentTrack *panelAt(int y);
    const int mapLocalToValue(int x) const;
    void setEditMode(const QString & editMode);
    const QString & editMode() const;
    QGraphicsScene *projectScene();
    CustomTrackView *projectView();
    int duration();
    int tracksNumber();
    KdenliveDoc *document();

  public slots:
    void slotDeleteClip(int clipId);

  private:
    Ui::TimeLine_UI *view;
    CustomRuler *m_ruler;
    CustomTrackView *m_trackview;
    double m_scale;
    QList <DocumentTrack*> documentTracks;
    int m_projectDuration;
    int m_projectTracks;
    TrackPanelFunctionFactory m_factory;
    DocumentTrack *m_panelUnderMouse;
	/** The currently applied function. This lasts from mousePressed until mouseRelease. */
    TrackPanelFunction *m_function;
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
    void registerFunction(const QString & name, TrackPanelFunction * function);
    TrackPanelFunction *getApplicableFunction(DocumentTrack * panel, const QString & editMode, QMouseEvent * event);

  private slots:
    void slotChangeZoom(int factor);
    void slotCursorMoved(int pos, bool slotCursorMoved = false);
    void slotZoomIn();
    void slotZoomOut();
    void setCursorPos(int pos);
    void moveCursorPos(int pos);

  signals:
    void mousePosition(int);
    void cursorMoved();
};

#endif
