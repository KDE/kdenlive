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


#include <QMouseEvent>
#include <QStylePainter>
#include <QScrollBar>

#include <KDebug>

#include "definitions.h"
#include "headertrack.h"
#include "trackview.h"
#include "clipitem.h"

TrackView::TrackView(KdenliveDoc *doc, QWidget *parent)
        : QWidget(parent), m_doc(doc), m_scale(1.0), m_projectTracks(0), m_projectDuration(0) {
    setMouseTracking(true);
    view = new Ui::TimeLine_UI();
    view->setupUi(this);
    m_ruler = new CustomRuler(doc->timecode());
    QVBoxLayout *layout = new QVBoxLayout;
    view->ruler_frame->setLayout(layout);
    layout->addWidget(m_ruler);

    m_scene = new QGraphicsScene();
    m_trackview = new CustomTrackView(doc, m_scene, this);
    m_trackview->scale(1, 1);
    m_trackview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    //m_scene->addRect(QRectF(0, 0, 100, 100), QPen(), QBrush(Qt::red));

    m_headersLayout = new QVBoxLayout;
    m_headersLayout->setContentsMargins(0, 0, 0, 0);
    view->headers_frame->setLayout(m_headersLayout);

    QVBoxLayout *tracksLayout = new QVBoxLayout;
    tracksLayout->setContentsMargins(0, 0, 0, 0);
    view->tracks_frame->setLayout(tracksLayout);
    tracksLayout->addWidget(m_trackview);

    parseDocument(doc->toXml());
    /*
      TrackPanelClipMoveFunction *m_moveFunction = new TrackPanelClipMoveFunction(this);
      registerFunction("move", m_moveFunction);
      setEditMode("move");*/

    connect(view->horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(slotChangeZoom(int)));
    connect(m_ruler, SIGNAL(cursorMoved(int)), this, SLOT(setCursorPos(int)));
    connect(m_trackview, SIGNAL(cursorMoved(int)), this, SLOT(slotCursorMoved(int)));
    connect(m_trackview, SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(m_trackview, SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(m_trackview->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_ruler, SLOT(slotMoveRuler(int)));
    connect(m_trackview, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_trackview, SIGNAL(clipItemSelected(ClipItem*)), this, SLOT(slotClipItemSelected(ClipItem*)));
    view->horizontalSlider->setValue(4);
    m_currentZoom = view->horizontalSlider->value();
    m_trackview->initView();
}


int TrackView::duration() {
    return m_projectDuration;
}

int TrackView::tracksNumber() {
    return m_projectTracks;
}

void TrackView::slotClipItemSelected(ClipItem*c) {
    emit clipItemSelected(c);
}

void TrackView::parseDocument(QDomDocument doc) {
    int cursorPos = 0;
    kDebug() << "//// DOCUMENT: " << doc.toString();
    QDomNode props = doc.elementsByTagName("properties").item(0);
    if (!props.isNull()) {
        cursorPos = props.toElement().attribute("timeline_position").toInt();
    }
    QDomNodeList tracks = doc.elementsByTagName("playlist");
    m_projectDuration = 300;
    m_projectTracks = tracks.count();
    int duration = 0;
    kDebug() << "//////////// TIMELINE FOUND: " << m_projectTracks << " tracks";
    for (int i = 0; i < m_projectTracks; i++) {
        if (tracks.item(i).toElement().attribute("hide", QString::null) == "video") {
            // this is an audio track
            duration = slotAddAudioTrack(i, tracks.item(i).toElement());
        } else if (!tracks.item(i).toElement().attribute("id", QString::null).isEmpty())
            duration = slotAddVideoTrack(i, tracks.item(i).toElement());
        kDebug() << " PRO DUR: " << m_projectDuration << ", TRACK DUR: " << duration;
        if (duration > m_projectDuration) m_projectDuration = duration;
    }
    m_trackview->setDuration(m_projectDuration);
    slotCursorMoved(cursorPos, true);
    //m_scrollBox->setGeometry(0, 0, 300 * zoomFactor(), m_scrollArea->height());
}

void TrackView::slotDeleteClip(int clipId) {
    m_trackview->deleteClip(clipId);
}

void TrackView::setCursorPos(int pos) {
    emit cursorMoved();
    m_trackview->setCursorPos(pos * m_scale);
}

void TrackView::moveCursorPos(int pos) {
    m_trackview->setCursorPos(pos * m_scale, false);
    m_ruler->slotNewValue(pos * FRAME_SIZE, false);
}

void TrackView::slotCursorMoved(int pos, bool emitSignal) {
    m_ruler->slotNewValue(pos * FRAME_SIZE / m_scale, emitSignal); //(int) m_trackview->mapToScene(QPoint(pos, 0)).x());
    //m_trackview->setCursorPos(pos);
    //m_trackview->invalidateScene(QRectF(), QGraphicsScene::ForegroundLayer);
}

void TrackView::slotChangeZoom(int factor) {
    double pos = m_trackview->cursorPos() / m_scale;
    m_ruler->setPixelPerMark(factor);
    m_scale = (double) FRAME_SIZE / m_ruler->comboScale[factor]; // m_ruler->comboScale[m_currentZoom] /
    m_currentZoom = factor;
    m_trackview->setScale(m_scale);
    m_trackview->setCursorPos(pos * m_scale, false);
    m_ruler->slotNewValue(pos * FRAME_SIZE, false);
    m_trackview->centerOn(QPointF(m_trackview->cursorPos(), 50));
}

const double TrackView::zoomFactor() const {
    return m_scale;
}

void TrackView::slotZoomIn() {
    view->horizontalSlider->setValue(view->horizontalSlider->value() - 1);
}

void TrackView::slotZoomOut() {
    view->horizontalSlider->setValue(view->horizontalSlider->value() + 1);
}

const int TrackView::mapLocalToValue(int x) const {
    return (int) x * zoomFactor();
}

KdenliveDoc *TrackView::document() {
    return m_doc;
}

void TrackView::refresh() {
    m_trackview->viewport()->update();
}

int TrackView::slotAddAudioTrack(int ix, QDomElement xml) {
    kDebug() << "*************  ADD AUDIO TRACK " << ix;
    m_trackview->addTrack();
    HeaderTrack *header = new HeaderTrack();
    //m_tracksAreaLayout->addWidget(track); //, ix, Qt::AlignTop);
    m_headersLayout->addWidget(header); //, ix, Qt::AlignTop);
    //documentTracks.insert(ix, track);
    return 0;
    //track->show();
}

int TrackView::slotAddVideoTrack(int ix, QDomElement xml) {
    m_trackview->addTrack();
    HeaderTrack *header = new HeaderTrack();
    int trackTop = 50 * ix;
    int trackBottom = trackTop + 50;
    // parse track
    int position = 0;
    for (QDomNode n = xml.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement elem = n.toElement();
        if (elem.tagName() == "blank") {
            position += elem.attribute("length", 0).toInt();
        } else if (elem.tagName() == "entry") {
            int in = elem.attribute("in", 0).toInt();
            int id = elem.attribute("producer", 0).toInt();
            DocClipBase *clip = m_doc->clipManager()->getClipById(id);
            int out = elem.attribute("out", 0).toInt() - in;
            //kDebug()<<"++++++++++++++\n\n / / /ADDING CLIP: "<<clip.cropTime<<", out: "<<clip.duration<<", Producer: "<<clip.producer<<"\n\n++++++++++++++++++++";
            ClipItem *item = new ClipItem(clip, ix, position, QRectF(position * m_scale, trackTop + 1, out * m_scale, 49), out);
            m_scene->addItem(item);
            position += out;

            //m_clipList.append(clip);
        }
    }
    //m_trackDuration = position;

    //m_tracksAreaLayout->addWidget(track); //, ix, Qt::AlignTop);
    m_headersLayout->addWidget(header); //, ix, Qt::AlignTop);
    //documentTracks.insert(ix, track);
    kDebug() << "*************  ADD VIDEO TRACK " << ix << ", DURATION: " << position;
    return position;
    //track->show();
}

QGraphicsScene *TrackView::projectScene() {
    return m_scene;
}

CustomTrackView *TrackView::projectView() {
    return m_trackview;
}

void TrackView::setEditMode(const QString & editMode) {
    m_editMode = editMode;
}

const QString & TrackView::editMode() const {
    return m_editMode;
}

#include "trackview.moc"
