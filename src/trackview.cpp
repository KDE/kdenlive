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


#include <QScrollBar>

#include <KDebug>
#include <KMessageBox>

#include "definitions.h"
#include "headertrack.h"
#include "trackview.h"
#include "clipitem.h"
#include "transition.h"
#include "kdenlivesettings.h"
#include "clipmanager.h"
#include "customruler.h"
#include "kdenlivedoc.h"
#include "mainwindow.h"
#include "customtrackview.h"


TrackView::TrackView(KdenliveDoc *doc, QWidget *parent)
        : QWidget(parent), m_doc(doc), m_scale(1.0), m_projectTracks(0) {

    view = new Ui::TimeLine_UI();
    view->setupUi(this);

    m_scene = new CustomTrackScene(doc);
    m_trackview = new CustomTrackView(doc, m_scene, parent);
    m_trackview->scale(1, 1);
    m_trackview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    //m_scene->addRect(QRectF(0, 0, 100, 100), QPen(), QBrush(Qt::red));

    m_ruler = new CustomRuler(doc->timecode(), m_trackview);
    connect(m_ruler, SIGNAL(zoneMoved(int, int)), this, SIGNAL(zoneMoved(int, int)));
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

    parseDocument(m_doc->toXml());

    connect(m_trackview, SIGNAL(cursorMoved(int, int)), m_ruler, SLOT(slotCursorMoved(int, int)));
    connect(m_trackview->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_ruler, SLOT(slotMoveRuler(int)));
    connect(m_trackview, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_trackview, SIGNAL(transitionItemSelected(Transition*, bool)), this, SLOT(slotTransitionItemSelected(Transition*, bool)));
    slotChangeZoom(m_doc->zoom());
}


int TrackView::duration() const {
    return m_trackview->duration();
}

int TrackView::tracksNumber() const {
    return m_projectTracks - 1;
}

int TrackView::inPoint() const {
    return m_ruler->inPoint();
}

int TrackView::outPoint() const {
    return m_ruler->outPoint();
}

void TrackView::slotSetZone(QPoint p) {
    m_ruler->setZone(p);
}

void TrackView::slotTransitionItemSelected(Transition *t, bool update) {
    emit transitionItemSelected(t, update);
}

void TrackView::setDuration(int dur) {
    m_trackview->setDuration(dur);
    m_ruler->setDuration(dur);
}

void TrackView::parseDocument(QDomDocument doc) {
    int cursorPos = 0;
    m_documentErrors = QString();
    // kDebug() << "//// DOCUMENT: " << doc.toString();
    QDomNode props = doc.elementsByTagName("properties").item(0);
    if (!props.isNull()) {
        cursorPos = props.toElement().attribute("timeline_position").toInt();
    }

    // parse project tracks
    QDomNodeList tracks = doc.elementsByTagName("track");
    QDomNodeList playlists = doc.elementsByTagName("playlist");
    int duration = 300;
    m_projectTracks = tracks.count();
    int trackduration = 0;
    QDomElement e;
    QDomElement p;
    bool videotrack;

    int pos = m_projectTracks - 1;


    for (int i = 0; i < m_projectTracks; i++) {
        e = tracks.item(i).toElement();
        QString playlist_name = e.attribute("producer");
        if (playlist_name != "black_track" && playlist_name != "playlistmain") {
            // find playlist related to this track
            p = QDomElement();
            for (int j = 0; j < m_projectTracks; j++) {
                p = playlists.item(j).toElement();
                if (p.attribute("id") == playlist_name) break;
            }
            videotrack = (e.attribute("hide") != "video");
            trackduration = slotAddProjectTrack(pos, p, videotrack);
            pos--;
            //kDebug() << " PRO DUR: " << trackduration << ", TRACK DUR: " << duration;
            if (trackduration > duration) duration = trackduration;
        } else {
            // background black track
            for (int j = 0; j < m_projectTracks; j++) {
                p = playlists.item(j).toElement();
                if (p.attribute("id") == playlist_name) break;
            }
            int black_clips = p.childNodes().count();
            for (int i = 0; i < black_clips; i++)
                m_doc->loadingProgressed();
            qApp->processEvents();
            pos--;
        }
    }

    // parse transitions
    QDomNodeList transitions = doc.elementsByTagName("transition");
    int projectTransitions = transitions.count();
    //kDebug() << "//////////// TIMELINE FOUND: " << projectTransitions << " transitions";
    for (int i = 0; i < projectTransitions; i++) {
        e = transitions.item(i).toElement();
        QDomNodeList transitionparams = e.childNodes();
        bool transitionAdd = true;
        int a_track = 0;
        int b_track = 0;
        bool isAutomatic = false;
        bool forceTrack = false;
        QString mlt_geometry;
        QString mlt_service;
        for (int k = 0; k < transitionparams.count(); k++) {
            p = transitionparams.item(k).toElement();
            if (!p.isNull()) {
                QString paramName = p.attribute("name");
                // do not add audio mixing transitions
                if (paramName == "internal_added" && p.text() == "237") {
                    transitionAdd = false;
                    //kDebug() << "//  TRANSITRION " << i << " IS NOT VALID (INTERN ADDED)";
                    //break;
                } else if (paramName == "a_track") a_track = p.text().toInt();
                else if (paramName == "b_track") b_track = p.text().toInt();
                else if (paramName == "mlt_service") mlt_service = p.text();
                else if (paramName == "geometry") mlt_geometry = p.text();
                else if (paramName == "automatic" && p.text() == "1") isAutomatic = true;
                else if (paramName == "force_track" && p.text() == "1") forceTrack = true;
            }
        }
        if (transitionAdd || mlt_service != "mix") {
            // Transition should be added to the scene
            ItemInfo transitionInfo;
            QString transitionId;
            if (mlt_service == "composite") {
                // When adding composite transition, check if it is a wipe transition
                if (mlt_geometry == "0%,0%:100%x100%") transitionId = "alphatransparency";
                else if (mlt_geometry.count(';') == 1) {
                    mlt_geometry.remove(QChar('%'), Qt::CaseInsensitive);
                    mlt_geometry.replace(QChar('x'), QChar(','), Qt::CaseInsensitive);
                    QString start = mlt_geometry.section(';', 0, 0);
                    start = start.section(':', 0, 1);
                    start.replace(QChar(':'), QChar(','), Qt::CaseInsensitive);
                    QString end = mlt_geometry.section('=', 1, 1);
                    end = end.section(':', 0, 1);
                    end.replace(QChar(':'), QChar(','), Qt::CaseInsensitive);
                    start.append(',' + end);
                    QStringList numbers = start.split(',', QString::SkipEmptyParts);
                    bool isWipeTransition = true;
                    int checkNumber;
                    for (int i = 0; i < numbers.size(); ++i) {
                        checkNumber = qAbs(numbers.at(i).toInt());
                        if (checkNumber != 0 && checkNumber != 100) {
                            isWipeTransition = false;
                            break;
                        }
                    }
                    if (isWipeTransition) transitionId = "wipe";
                }
            }
            QDomElement base = MainWindow::transitions.getEffectByTag(mlt_service, transitionId).cloneNode().toElement();

            for (int k = 0; k < transitionparams.count(); k++) {
                p = transitionparams.item(k).toElement();
                if (!p.isNull()) {
                    QString paramName = p.attribute("name");
                    QString paramValue = p.text();

                    QDomNodeList params = base.elementsByTagName("parameter");
                    if (paramName != "a_track" && paramName != "b_track") for (int i = 0; i < params.count(); i++) {
                            QDomElement e = params.item(i).toElement();
                            if (!e.isNull() && e.attribute("tag") == paramName) {
                                if (e.attribute("type") == "double") {
                                    QString factor = e.attribute("factor", "1");
                                    if (factor != "1") {
                                        double val = paramValue.toDouble() * factor.toDouble();
                                        paramValue = QString::number(val);
                                    }
                                }
                                e.setAttribute("value", paramValue);
                                break;
                            }
                        }
                }
            }

            /*QDomDocument doc;
            doc.appendChild(doc.importNode(base, true));
            kDebug() << "///////  TRANSITION XML: "<< doc.toString();*/

            transitionInfo.startPos = GenTime(e.attribute("in").toInt(), m_doc->fps());
            transitionInfo.endPos = GenTime(e.attribute("out").toInt() + 1, m_doc->fps());
            transitionInfo.track = m_projectTracks - 1 - b_track;
            //kDebug() << "///////////////   +++++++++++  ADDING TRANSITION ON TRACK: " << b_track << ", TOTAL TRKA: " << m_projectTracks;
            Transition *tr = new Transition(transitionInfo, a_track, m_doc->fps(), base, isAutomatic);
            if (forceTrack) tr->setForcedTrack(true, a_track);
            m_scene->addItem(tr);
        }
    }

    // Add guides
    QDomNodeList guides = doc.elementsByTagName("guide");
    for (int i = 0; i < guides.count(); i++) {
        e = guides.item(i).toElement();
        const QString comment = e.attribute("comment");
        const GenTime pos = GenTime(e.attribute("time").toDouble());
        m_trackview->addGuide(pos, comment);
    }

    m_trackview->setDuration(duration);
    kDebug() << "///////////  TOTAL PROJECT DURATION: " << duration;
    slotRebuildTrackHeaders();
    if (!m_documentErrors.isNull()) KMessageBox::sorry(this, m_documentErrors);
    //m_trackview->setCursorPos(cursorPos);
    //m_scrollBox->setGeometry(0, 0, 300 * zoomFactor(), m_scrollArea->height());
}

void TrackView::slotDeleteClip(const QString &clipId) {
    m_trackview->deleteClip(clipId);
}

void TrackView::setCursorPos(int pos) {
    m_trackview->setCursorPos(pos);
}

void TrackView::moveCursorPos(int pos) {
    m_trackview->setCursorPos(pos, false);
}

void TrackView::slotChangeZoom(int factor) {
    m_doc->setZoom(factor);
    m_ruler->setPixelPerMark(factor);
    m_scale = (double) FRAME_SIZE / m_ruler->comboScale[factor]; // m_ruler->comboScale[m_currentZoom] /
    m_trackview->setScale(m_scale);
}

int TrackView::fitZoom() const {
    int zoom = (int)((duration() + 20 / m_scale) * FRAME_SIZE / m_trackview->width());
    int i;
    for (i = 0; i < 13; i++)
        if (m_ruler->comboScale[i] > zoom) break;
    return i;
}

const double TrackView::zoomFactor() const {
    return m_scale;
}

const int TrackView::mapLocalToValue(int x) const {
    return (int)(x * zoomFactor());
}

KdenliveDoc *TrackView::document() {
    return m_doc;
}

void TrackView::refresh() {
    m_trackview->viewport()->update();
}

void TrackView::slotRebuildTrackHeaders() {
    QList <TrackInfo> list = m_trackview->tracksList();
    QList<HeaderTrack *> widgets = this->findChildren<HeaderTrack *>();
    for (int i = 0; i < widgets.count(); i++)
        delete widgets.at(i);
    int max = list.count();
    for (int i = 0; i < max; i++) {
        HeaderTrack *header = new HeaderTrack(i, list.at(max - i - 1), this);
        connect(header, SIGNAL(switchTrackVideo(int)), m_trackview, SLOT(slotSwitchTrackVideo(int)));
        connect(header, SIGNAL(switchTrackAudio(int)), m_trackview, SLOT(slotSwitchTrackAudio(int)));

        //TODO: re-enable when add / remove track is implemented
        connect(header, SIGNAL(deleteTrack(int)), m_trackview, SLOT(slotDeleteTrack(int)));
        connect(header, SIGNAL(insertTrack(int)), m_trackview, SLOT(slotInsertTrack(int)));
        m_headersLayout->addWidget(header);
    }
    view->headers_container->adjustSize();
}

int TrackView::slotAddProjectTrack(int ix, QDomElement xml, bool videotrack) {
    TrackInfo info;

    if (videotrack) {
        info.type = VIDEOTRACK;
        info.isMute = false;
        info.isBlind = false;
    } else {
        info.type = AUDIOTRACK;
        info.isMute = false;
        info.isBlind = false;
    }

    m_trackview->addTrack(info);

    int trackTop = KdenliveSettings::trackheight() * ix;
    // parse track
    int position = 0;
    for (QDomNode n = xml.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement elem = n.toElement();
        if (elem.tagName() == "blank") {
            position += elem.attribute("length").toInt();
        } else if (elem.tagName() == "entry") {
            m_doc->loadingProgressed();
            qApp->processEvents();
            // Found a clip
            int in = elem.attribute("in").toInt();
            QString idString = elem.attribute("producer");
            QString id = idString;
            bool hasSpeedAttribute = false;
            double speed;
            if (idString.startsWith("slowmotion")) {
                hasSpeedAttribute = true;
                id = idString.section(":", 1, 1);
                speed = idString.section(":", 2, 2).toDouble();
            } else id = id.section('_', 0, 0);
            DocClipBase *clip = m_doc->clipManager()->getClipById(id);
            if (clip != NULL) {
                int out = elem.attribute("out").toInt();

                ItemInfo clipinfo;
                clipinfo.startPos = GenTime(position, m_doc->fps());
                clipinfo.endPos = clipinfo.startPos + GenTime(out - in + 1, m_doc->fps());
                clipinfo.cropStart = GenTime(in, m_doc->fps());
                clipinfo.track = ix;
                //kDebug() << "// INSERTING CLIP: " << in << "x" << out << ", track: " << ix << ", ID: " << id << ", SCALE: " << m_scale << ", FPS: " << m_doc->fps();
                ClipItem *item = new ClipItem(clip, clipinfo, m_doc->fps(), false);
                if (hasSpeedAttribute) item->setSpeed(speed);
                m_scene->addItem(item);
                clip->addReference();
                position += (out - in + 1);

                // parse clip effects
                QDomNodeList effects = elem.childNodes();
                for (int ix = 0; ix < effects.count(); ix++) {
                    QDomElement effect = effects.at(ix).toElement();
                    if (effect.tagName() == "filter") {
                        // add effect to clip
                        QString effecttag;
                        QString effectid;
                        QString effectindex;
                        // Get effect tag & index
                        for (QDomNode n3 = effect.firstChild(); !n3.isNull(); n3 = n3.nextSibling()) {
                            // parse effect parameters
                            QDomElement effectparam = n3.toElement();
                            if (effectparam.attribute("name") == "tag") {
                                effecttag = effectparam.text();
                            } else if (effectparam.attribute("name") == "kdenlive_id") {
                                effectid = effectparam.text();
                            } else if (effectparam.attribute("name") == "kdenlive_ix") {
                                effectindex = effectparam.text();
                            }
                        }
                        //kDebug() << "+ + CLIP EFF FND: " << effecttag << ", " << effectid << ", " << effectindex;
                        // get effect standard tags
                        QDomElement clipeffect = MainWindow::customEffects.getEffectByTag(QString(), effectid);
                        if (clipeffect.isNull()) clipeffect = MainWindow::videoEffects.getEffectByTag(effecttag, effectid);
                        if (clipeffect.isNull()) clipeffect = MainWindow::audioEffects.getEffectByTag(effecttag, effectid);
                        if (clipeffect.isNull()) {
                            kDebug() << "///  WARNING, EFFECT: " << effecttag << ": " << effectid << " not found, removing it from project";
                            m_documentErrors.append(i18n("Effect %1:%2 not found in MLT, it was removed from this project\n", effecttag, effectid));
                            elem.removeChild(effects.at(ix));
                            ix--;
                        } else {
                            QDomElement currenteffect = clipeffect.cloneNode().toElement();
                            currenteffect.setAttribute("kdenlive_ix", effectindex);
                            QDomNodeList clipeffectparams = currenteffect.childNodes();

                            if (MainWindow::videoEffects.hasKeyFrames(currenteffect)) {
                                //kDebug() << " * * * * * * * * * * ** CLIP EFF WITH KFR FND  * * * * * * * * * * *";
                                // effect is key-framable, read all effects to retrieve keyframes
                                double factor;
                                QString starttag;
                                QString endtag;
                                QDomNodeList params = currenteffect.elementsByTagName("parameter");
                                for (int i = 0; i < params.count(); i++) {
                                    QDomElement e = params.item(i).toElement();
                                    if (e.attribute("type") == "keyframe") {
                                        starttag = e.attribute("starttag", "start");
                                        endtag = e.attribute("endtag", "end");
                                        factor = e.attribute("factor", "1").toDouble();
                                        break;
                                    }
                                }
                                QString keyframes;
                                int effectin = effect.attribute("in").toInt();
                                int effectout = effect.attribute("out").toInt();
                                double startvalue;
                                double endvalue;
                                for (QDomNode n3 = effect.firstChild(); !n3.isNull(); n3 = n3.nextSibling()) {
                                    // parse effect parameters
                                    QDomElement effectparam = n3.toElement();
                                    if (effectparam.attribute("name") == starttag)
                                        startvalue = effectparam.text().toDouble() * factor;
                                    if (effectparam.attribute("name") == endtag)
                                        endvalue = effectparam.text().toDouble() * factor;
                                }
                                // add first keyframe
                                keyframes.append(QString::number(effectin) + ":" + QString::number(startvalue) + ";" + QString::number(effectout) + ":" + QString::number(endvalue) + ";");
                                QDomNode lastParsedEffect;
                                ix++;
                                QDomNode n2 = effects.at(ix);
                                bool continueParsing = true;
                                for (; !n2.isNull() && continueParsing; n2 = n2.nextSibling()) {
                                    // parse all effects
                                    QDomElement kfreffect = n2.toElement();
                                    int effectout = kfreffect.attribute("out").toInt();

                                    for (QDomNode n4 = kfreffect.firstChild(); !n4.isNull(); n4 = n4.nextSibling()) {
                                        // parse effect parameters
                                        QDomElement subeffectparam = n4.toElement();
                                        if (subeffectparam.attribute("name") == "kdenlive_ix" && subeffectparam.text() != effectindex) {
                                            //We are not in the same effect, stop parsing
                                            lastParsedEffect = n2.previousSibling();
                                            ix--;
                                            continueParsing = false;
                                            break;
                                        } else if (subeffectparam.attribute("name") == endtag) {
                                            endvalue = subeffectparam.text().toDouble() * factor;
                                            break;
                                        }
                                    }
                                    if (continueParsing) {
                                        keyframes.append(QString::number(effectout) + ":" + QString::number(endvalue) + ";");
                                        ix++;
                                    }
                                }

                                params = currenteffect.elementsByTagName("parameter");
                                for (int i = 0; i < params.count(); i++) {
                                    QDomElement e = params.item(i).toElement();
                                    if (e.attribute("type") == "keyframe") e.setAttribute("keyframes", keyframes);
                                }
                                if (!continueParsing) {
                                    n2 = lastParsedEffect;
                                }
                            } else {
                                // Check if effect has in/out points
                                if (effect.hasAttribute("in")) {
                                    EffectsList::setParameter(currenteffect, "in",  effect.attribute("in"));
                                }
                                if (effect.hasAttribute("out")) {
                                    EffectsList::setParameter(currenteffect, "out",  effect.attribute("out"));
                                }
                            }

                            // adjust effect parameters
                            for (QDomNode n3 = effect.firstChild(); !n3.isNull(); n3 = n3.nextSibling()) {
                                // parse effect parameters
                                QDomElement effectparam = n3.toElement();
                                QString paramname = effectparam.attribute("name");
                                QString paramvalue = effectparam.text();

                                // try to find this parameter in the effect xml
                                QDomElement e;
                                for (int k = 0; k < clipeffectparams.count(); k++) {
                                    e = clipeffectparams.item(k).toElement();
                                    if (!e.isNull() && e.tagName() == "parameter" && e.attribute("name") == paramname) {
                                        e.setAttribute("value", paramvalue);
                                        break;
                                    }
                                }
                            }
                            item->addEffect(currenteffect, false);
                            item->effectsCounter();
                        }
                    }
                }
            } else kWarning() << "CANNOT INSERT CLIP " << id;
            //m_clipList.append(clip);
        }
    }
    //m_trackDuration = position;


    //documentTracks.insert(ix, track);
    kDebug() << "*************  ADD DOC TRACK " << ix << ", DURATION: " << position;
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
