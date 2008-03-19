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
#include "kdenlivesettings.h"

TrackView::TrackView(KdenliveDoc *doc, QWidget *parent)
        : QWidget(parent), m_doc(doc), m_scale(1.0), m_projectTracks(0), m_currentZoom(4) {

    view = new Ui::TimeLine_UI();
    view->setupUi(this);

    m_scene = new QGraphicsScene();
    m_trackview = new CustomTrackView(doc, m_scene, parent);
    m_trackview->scale(1, 1);
    m_trackview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    //m_scene->addRect(QRectF(0, 0, 100, 100), QPen(), QBrush(Qt::red));

    m_ruler = new CustomRuler(doc->timecode(), m_trackview);
    QHBoxLayout *layout = new QHBoxLayout;
    view->ruler_frame->setLayout(layout);
    int left_margin;
    int right_margin;
    layout->getContentsMargins(&left_margin, 0, &right_margin, 0);
    layout->setContentsMargins(left_margin, 0, right_margin, 0);
    layout->addWidget(m_ruler);

    QHBoxLayout *tracksLayout = new QHBoxLayout;
    tracksLayout->setContentsMargins(0, 0, 0, 0);
    view->tracks_frame->setLayout(tracksLayout);

    view->headers_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->headers_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_headersLayout = new QVBoxLayout;
    m_headersLayout->setContentsMargins(0, 0, 0, 0);
    m_headersLayout->setSpacing(0);
    view->headers_container->setLayout(m_headersLayout);

    connect(view->headers_area->verticalScrollBar(), SIGNAL(valueChanged(int)), m_trackview->verticalScrollBar(), SLOT(setValue(int)));

    tracksLayout->addWidget(m_trackview);

    connect(m_trackview->verticalScrollBar(), SIGNAL(valueChanged(int)), view->headers_area->verticalScrollBar(), SLOT(setValue(int)));
    connect(m_trackview, SIGNAL(trackHeightChanged()), this, SLOT(slotRebuildTrackHeaders()));

    parseDocument(doc->toXml());

    connect(m_trackview, SIGNAL(cursorMoved(int, int)), m_ruler, SLOT(slotCursorMoved(int, int)));
    connect(m_trackview->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_ruler, SLOT(slotMoveRuler(int)));
    connect(m_trackview, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_trackview, SIGNAL(clipItemSelected(ClipItem*)), this, SLOT(slotClipItemSelected(ClipItem*)));
    connect(m_trackview, SIGNAL(transitionItemSelected(Transition*)), this, SLOT(slotTransitionItemSelected(Transition*)));
    slotChangeZoom(m_currentZoom);
}

int TrackView::currentZoom() const {
    return m_currentZoom;
}

int TrackView::duration() const {
    return m_trackview->duration();
}

int TrackView::tracksNumber() const {
    return m_projectTracks;
}

int TrackView::inPoint() const {
    return m_ruler->inPoint();
}

int TrackView::outPoint() const {
    return m_ruler->outPoint();
}

void TrackView::slotClipItemSelected(ClipItem*c) {
    emit clipItemSelected(c);
}

void TrackView::slotTransitionItemSelected(Transition *t) {
    emit transitionItemSelected(t);
}

void TrackView::setDuration(int dur) {
    m_trackview->setDuration(dur);
    m_ruler->setDuration(dur);
}

void TrackView::parseDocument(QDomDocument doc) {
    int cursorPos = 0;
    kDebug() << "//// DOCUMENT: " << doc.toString();
    QDomNode props = doc.elementsByTagName("properties").item(0);
    if (!props.isNull()) {
        cursorPos = props.toElement().attribute("timeline_position").toInt();
    }
    QDomNodeList tracks = doc.elementsByTagName("playlist");
    int duration = 300;
    m_projectTracks = tracks.count();
    int trackduration = 0;
    kDebug() << "//////////// TIMELINE FOUND: " << m_projectTracks << " tracks";
    for (int i = 0; i < m_projectTracks; i++) {
        if (tracks.item(i).toElement().attribute("hide", QString::null) == "video") {
            // this is an audio track
            trackduration = slotAddAudioTrack(i, tracks.item(i).toElement());
        } else if (!tracks.item(i).toElement().attribute("id", QString::null).isEmpty())
            trackduration = slotAddVideoTrack(i, tracks.item(i).toElement());
        kDebug() << " PRO DUR: " << trackduration << ", TRACK DUR: " << duration;
        if (trackduration > duration) duration = trackduration;
    }
    m_trackview->setDuration(duration);
    slotRebuildTrackHeaders();
    //m_trackview->setCursorPos(cursorPos);
    //m_scrollBox->setGeometry(0, 0, 300 * zoomFactor(), m_scrollArea->height());
}

void TrackView::slotDeleteClip(int clipId) {
    m_trackview->deleteClip(clipId);
}

void TrackView::setCursorPos(int pos) {
    m_trackview->setCursorPos(pos);
}

void TrackView::moveCursorPos(int pos) {
    m_trackview->setCursorPos(pos, false);
}

void TrackView::slotChangeZoom(int factor) {

    m_ruler->setPixelPerMark(factor);
    m_scale = (double) FRAME_SIZE / m_ruler->comboScale[factor]; // m_ruler->comboScale[m_currentZoom] /
    m_currentZoom = factor;
    m_trackview->setScale(m_scale);
}

int TrackView::fitZoom() const {
    int zoom = (duration() + 20 / m_scale) * FRAME_SIZE / m_trackview->width();
    int i;
    for (i = 0; i < 13; i++)
        if (m_ruler->comboScale[i] > zoom) break;
    return i;
}

const double TrackView::zoomFactor() const {
    return m_scale;
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

void TrackView::slotRebuildTrackHeaders() {
    QList <TRACKTYPE> list = m_trackview->tracksList();
    QList<HeaderTrack *> widgets = this->findChildren<HeaderTrack *>();
    for (int i = 0; i < widgets.count(); i++)
        delete widgets.at(i);
    int max = list.count();
    for (int i = 0; i < max; i++) {
        HeaderTrack *header = new HeaderTrack(i, list.at(max - i - 1), this);
        m_headersLayout->addWidget(header);
    }
    view->headers_container->adjustSize();
}

int TrackView::slotAddAudioTrack(int ix, QDomElement xml) {
    kDebug() << "*************  ADD AUDIO TRACK " << ix;
    m_trackview->addTrack(AUDIOTRACK);
    //documentTracks.insert(ix, track);
    return 0;
    //track->show();
}

int TrackView::slotAddVideoTrack(int ix, QDomElement xml) {
    m_trackview->addTrack(VIDEOTRACK);

    int trackTop = KdenliveSettings::trackheight() * ix;
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
            ClipItem *item = new ClipItem(clip, ix, GenTime(position, m_doc->fps()), QRectF(position * m_scale, trackTop + 1, out * m_scale, KdenliveSettings::trackheight() - 1), GenTime(out, m_doc->fps()), m_doc->fps());
            m_scene->addItem(item);
            position += out;

            //m_clipList.append(clip);
        }
    }
    //m_trackDuration = position;


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
