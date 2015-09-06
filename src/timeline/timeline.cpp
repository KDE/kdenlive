/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2015 by Vincent Pinon (vpinon@kde.org)                  *
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


#include "timeline.h"
#include "track.h"
#include "headertrack.h"
#include "clipitem.h"
#include "transition.h"
#include "timelinecommands.h"
#include "customruler.h"
#include "customtrackview.h"
#include "dialogs/profilesdialog.h"
#include "mltcontroller/clipcontroller.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "doc/kdenlivedoc.h"
#include "project/clipmanager.h"
#include "effectslist/initeffects.h"
#include "mltcontroller/effectscontroller.h"

#include <QScrollBar>
#include <QLocale>

#include <KMessageBox>
#include <KIO/FileCopyJob>
#include <klocalizedstring.h>


Timeline::Timeline(KdenliveDoc *doc, const QList<QAction *> &actions, bool *ok, QWidget *parent) :
    QWidget(parent),
    m_projectTracks(0),
    m_scale(1.0),
    m_doc(doc),
    m_verticalZoom(1)
{
    m_trackActions << actions;
    setupUi(this);
    //    ruler_frame->setMaximumHeight();
    //    size_frame->setMaximumHeight();
    m_scene = new CustomTrackScene(this);
    m_trackview = new CustomTrackView(doc, this, m_scene, parent);
    if (m_doc->setSceneList() == -1) *ok = false;
    else *ok = true;
    m_ruler = new CustomRuler(doc->timecode(), m_trackview);
    connect(m_ruler, SIGNAL(zoneMoved(int,int)), this, SIGNAL(zoneMoved(int,int)));
    connect(m_ruler, SIGNAL(adjustZoom(int)), this, SIGNAL(setZoom(int)));
    connect(m_ruler, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_ruler, SIGNAL(seekCursorPos(int)), m_doc->renderer(), SLOT(seek(int)), Qt::QueuedConnection);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(m_trackview->frameWidth(), 0, 0, 0);
    layout->setSpacing(0);
    ruler_frame->setLayout(layout);
    ruler_frame->setMaximumHeight(m_ruler->height());
    layout->addWidget(m_ruler);

    QHBoxLayout *sizeLayout = new QHBoxLayout;
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(0);
    size_frame->setLayout(sizeLayout);
    size_frame->setMaximumHeight(m_ruler->height());

    QToolButton *butSmall = new QToolButton(this);
    butSmall->setIcon(QIcon::fromTheme("kdenlive-zoom-small"));
    butSmall->setToolTip(i18n("Smaller tracks"));
    butSmall->setAutoRaise(true);
    connect(butSmall, SIGNAL(clicked()), this, SLOT(slotVerticalZoomDown()));
    sizeLayout->addWidget(butSmall);

    QToolButton *butLarge = new QToolButton(this);
    butLarge->setIcon(QIcon::fromTheme("kdenlive-zoom-large"));
    butLarge->setToolTip(i18n("Bigger tracks"));
    butLarge->setAutoRaise(true);
    connect(butLarge, SIGNAL(clicked()), this, SLOT(slotVerticalZoomUp()));
    sizeLayout->addWidget(butLarge);

    QHBoxLayout *tracksLayout = new QHBoxLayout;
    tracksLayout->setContentsMargins(0, 0, 0, 0);
    tracksLayout->setSpacing(0);
    tracks_frame->setLayout(tracksLayout);

    headers_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    headers_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout *headersLayout = new QVBoxLayout;
    headersLayout->setContentsMargins(0, m_trackview->frameWidth(), 0, 0);
    headersLayout->setSpacing(0);
    headers_container->setLayout(headersLayout);
    connect(headers_area->verticalScrollBar(), SIGNAL(valueChanged(int)), m_trackview->verticalScrollBar(), SLOT(setValue(int)));

    tracksLayout->addWidget(m_trackview);
    connect(m_trackview->verticalScrollBar(), SIGNAL(valueChanged(int)), headers_area->verticalScrollBar(), SLOT(setValue(int)));
    connect(m_trackview, SIGNAL(trackHeightChanged()), this, SLOT(slotRebuildTrackHeaders()));
    connect(m_trackview, SIGNAL(tracksChanged()), this, SLOT(slotReloadTracks()));
    connect(m_trackview, SIGNAL(updateTrackHeaders()), this, SLOT(slotRepaintTracks()));
    connect(m_trackview, SIGNAL(showTrackEffects(int,TrackInfo)), this, SIGNAL(showTrackEffects(int,TrackInfo)));
    connect(m_trackview, SIGNAL(updateTrackEffectState(int)), this, SLOT(slotUpdateTrackEffectState(int)));
    Mlt::Service s(m_doc->renderer()->getProducer()->parent().get_service());
    m_tractor = new Mlt::Tractor(s);
    parseDocument(m_doc->toXml());
    connect(m_trackview, SIGNAL(cursorMoved(int,int)), m_ruler, SLOT(slotCursorMoved(int,int)));
    connect(m_trackview, SIGNAL(updateRuler(int)), m_ruler, SLOT(updateRuler(int)), Qt::DirectConnection);

    connect(m_trackview->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_ruler, SLOT(slotMoveRuler(int)));
    connect(m_trackview->horizontalScrollBar(), SIGNAL(rangeChanged(int,int)), this, SLOT(slotUpdateVerticalScroll(int,int)));
    connect(m_trackview, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_trackview, SIGNAL(doTrackLock(int,bool)), this, SLOT(slotChangeTrackLock(int,bool)));
    
    m_trackview->slotUpdateAllThumbs();

    slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
    slotSetZone(m_doc->zone(), false);
}

Timeline::~Timeline()
{
    delete m_ruler;
    delete m_trackview;
    delete m_scene;
    delete m_tractor;
    m_tracks.clear();
}

Track* Timeline::track(int i) 
{
    return m_tracks.at(i);
}

int Timeline::tracksCount() const
{
    return m_tracks.count();
}

//virtual
void Timeline::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Up) {
        m_trackview->slotTrackUp();
        event->accept();
    } else if (event->key() == Qt::Key_Down) {
        m_trackview->slotTrackDown();
        event->accept();
    } else QWidget::keyPressEvent(event);
}

int Timeline::duration() const
{
    return m_trackview->duration();
}

bool Timeline::checkProjectAudio()
{
    bool hasAudio = false;
    int max = m_tractor->count();
    for (int i = 0; i < max; i++) {
        Track *sourceTrack = track(i);
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        int state = track->get_int("hide");
        if (sourceTrack->hasAudio() && !(state & 2)) {
            hasAudio = true;
            break;
        }
    }
    return hasAudio;
}

int Timeline::inPoint() const
{
    return m_ruler->inPoint();
}

int Timeline::outPoint() const
{
    return m_ruler->outPoint();
}

void Timeline::slotSetZone(const QPoint &p, bool updateDocumentProperties)
{
    m_ruler->setZone(p);
    if (updateDocumentProperties) m_doc->setZone(p.x(), p.y());
}

void Timeline::setDuration(int dur)
{
    m_trackview->setDuration(dur);
    m_ruler->setDuration(dur);
}

int Timeline::getTracks() {
    int trackIndex = 0;
    int duration = 1;
    qDeleteAll<>(m_tracks);
    m_tracks.clear();
    m_projectTracks = m_tractor->count();
    for (int i = 0; i < m_projectTracks; ++i) {
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        QString playlist_name = track->get("id");
        if (playlist_name == "black_track" || playlist_name == "playlistmain") continue;
        // check track effects
        Mlt::Playlist playlist(*track);
        int trackduration;
        int audio = playlist.get_int("kdenlive:audio_track");
        trackduration = addTrack(m_tractor->count() - 1 - i, playlist);
        m_tracks.prepend(new Track(playlist, audio == 1 ? AudioTrack : VideoTrack, m_doc->fps()));
        if (trackduration > duration) duration = trackduration;
        if (playlist.filter_count()) getEffects(playlist, NULL, 0);
        ++trackIndex;
        connect(m_tracks[0], &Track::newTrackDuration, this, &Timeline::checkDuration);
	connect(m_tracks[0], SIGNAL(storeSlowMotion(QString,Mlt::Producer *)), m_doc->renderer(), SLOT(storeSlowmotionProducer(QString,Mlt::Producer *)));
    }
    checkTrackHeight();
    return duration;
}

void Timeline::checkDuration(int duration) {
    m_doc->renderer()->mltCheckLength(m_tractor);
    return;
    //FIXME
    for (int i = 1; i < m_tractor->count(); ++i) {
        QScopedPointer<Mlt::Producer> tk(m_tractor->track(i));
        int len = tk->get_playtime() - 1;
        if (len > duration) duration = len;
    }
    QScopedPointer<Mlt::Producer> tk1(m_tractor->track(0));
    Mlt::Service s(tk1->get_service());
    Mlt::Playlist blackTrack(s);
    if (blackTrack.get_playtime() - 1 != duration) {
        QScopedPointer<Mlt::Producer> blackClip(blackTrack.get_clip(0));
        if (blackClip->parent().get_length() <= duration) {
            blackClip->parent().set("length", duration + 1);
            blackClip->parent().set("out", duration);
            blackClip->set("length", duration + 1);
        }
        blackTrack.resize_clip(0, 0, duration);
    }
    //TODO: rewind consumer if beyond duration / emit durationChanged
}

void Timeline::getTransitions() {
    mlt_service service = mlt_service_get_producer(m_tractor->get_service());
    while (service) {
        Mlt::Properties prop(MLT_SERVICE_PROPERTIES(service));
        if (QString(prop.get("mlt_type")) != "transition")
            break;
        //skip automatic mix
        if (QString(prop.get("internal_added")) == "237"
            || QString(prop.get("mlt_service")) == "mix") {
            service = mlt_service_producer(service);
            continue;
        }

        int a_track = prop.get_int("a_track");
        int b_track = prop.get_int("b_track");
        ItemInfo transitionInfo;
        transitionInfo.startPos = GenTime(prop.get_int("in"), m_doc->fps());
        transitionInfo.endPos = GenTime(prop.get_int("out") + 1, m_doc->fps());
        transitionInfo.track = m_projectTracks - 1 - b_track;
        // When adding composite transition, check if it is a wipe transition
        if (prop.get("kdenlive_id") == NULL && QString(prop.get("mlt_service")) == "composite" && isSlide(prop.get("geometry")))
            prop.set("kdenlive_id", "slide");
        QDomElement base = MainWindow::transitions.getEffectByTag(prop.get("mlt_service"), prop.get("kdenlive_id")).cloneNode().toElement();
        //check invalid parameters
        if (a_track > m_projectTracks - 1) {
            m_documentErrors.append(i18n("Transition %1 had an invalid track: %2 > %3", prop.get("id"), a_track, m_projectTracks - 1) + '\n');
            prop.set("a_track", m_projectTracks - 1);
        }
        if (b_track > m_projectTracks - 1) {
            m_documentErrors.append(i18n("Transition %1 had an invalid track: %2 > %3", prop.get("id"), b_track, m_projectTracks - 1) + '\n');
            prop.set("b_track", m_projectTracks - 1);
        }
        if (a_track == b_track || b_track <= 0
            || transitionInfo.startPos >= transitionInfo.endPos
            || base.isNull()
            //|| !m_trackview->canBePastedTo(transitionInfo, TransitionWidget)
           ) {
            // invalid transition, remove it
            m_documentErrors.append(i18n("Removed invalid transition: %1", prop.get("id")) + '\n');
            mlt_service disconnect = service;
            service = mlt_service_producer(service);
            mlt_field_disconnect_service(m_tractor->field()->get_field(), disconnect);
        } else {
            QDomNodeList params = base.elementsByTagName("parameter");
            for (int i = 0; i < params.count(); ++i) {
                QDomElement e = params.item(i).toElement();
                QString value = prop.get(e.attribute("tag").toUtf8().constData());
                if (value.isEmpty()) continue;
                if (e.attribute("type") == "double")
                    adjustDouble(e, value.toDouble());
                else
                    e.setAttribute("value", value);
            }
            Transition *tr = new Transition(transitionInfo, a_track, m_doc->fps(), base, QString(prop.get("automatic")) == "1");
            if (QString(prop.get("force_track")) == "1") tr->setForcedTrack(true, a_track);
            if (isTrackLocked(b_track - 1)) tr->setItemLocked(true);
            m_scene->addItem(tr);
            service = mlt_service_producer(service);
        }
    }
}

bool Timeline::isSlide(QString geometry) {
    if (geometry.count(';') != 1) return false;

    geometry.remove(QChar('%'), Qt::CaseInsensitive);
    geometry.replace(QChar('x'), QChar(':'), Qt::CaseInsensitive);
    geometry.replace(QChar(','), QChar(':'), Qt::CaseInsensitive);
    geometry.replace(QChar('/'), QChar(':'), Qt::CaseInsensitive);

    QString start = geometry.section('=', 0, 0).section(':', 0, -2) + ':';
    start.append(geometry.section('=', 1, 1).section(':', 0, -2));
    QStringList numbers = start.split(':', QString::SkipEmptyParts);
    for (int i = 0; i < numbers.size(); ++i) {
        int checkNumber = qAbs(numbers.at(i).toInt());
        if (checkNumber != 0 && checkNumber != 100) {
            return false;
        }
    }
    return true;
}

void Timeline::adjustDouble(QDomElement &e, double value) {
    QString factor = e.attribute("factor", "1");
    double offset = e.attribute("offset", "0").toDouble();
    double fact = 1;
    if (factor.contains('%')) fact = EffectsController::getStringEval(m_doc->getProfileInfo(), factor);
    else fact = factor.toDouble();

    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    e.setAttribute("value", locale.toString(offset + value * fact));
}

void Timeline::parseDocument(const QDomDocument &doc)
{
    //int cursorPos = 0;
    m_documentErrors.clear();
    m_replacementProducerIds.clear();

    // parse project tracks
    QDomElement mlt = doc.firstChildElement("mlt");
    /*QDomElement tractor = mlt.firstChildElement("tractor");
    QDomNodeList tracks = tractor.elementsByTagName("track");
    QDomNodeList playlists = doc.elementsByTagName("playlist");*/
    m_projectTracks = m_tractor->count();// tracks.count();
    for (int i = 0; i < m_projectTracks; i++) {
        QScopedPointer<Mlt::Producer> track(m_tractor->multitrack()->track(i));
        QString trackId = track->get("id");
        if (trackId == "black_track") {
            // Background track, do not show
            continue;
        }
    }
    QDomElement e;
    QDomElement p;
    m_trackview->setDuration(getTracks());
    getTransitions();


    QDomElement infoXml = mlt.firstChildElement("kdenlivedoc");

    /*QDomElement propsXml = infoXml.firstChildElement("documentproperties");
    
    int currentPos = propsXml.attribute("position").toInt();
    if (currentPos > 0) m_trackview->initCursorPos(currentPos);*/
    // Rebuild groups
    QDomDocument groupsDoc;
    groupsDoc.setContent(m_doc->renderer()->getBinProperty("kdenlive:clipgroups"));
    QDomNodeList groups = groupsDoc.elementsByTagName("group");
    m_trackview->loadGroups(groups);
    
    // Load custom effects
    QDomDocument effectsDoc;
    effectsDoc.setContent(m_doc->renderer()->getBinProperty("kdenlive:customeffects"));
    QDomNodeList effects = effectsDoc.elementsByTagName("effect");
    if (!effects.isEmpty()) {
        m_doc->saveCustomEffects(effects);
    }
    // Remove Kdenlive extra info from xml doc before sending it to MLT
    mlt.removeChild(infoXml);

    if (!m_documentErrors.isNull()) KMessageBox::sorry(this, m_documentErrors);
    if (mlt.hasAttribute("upgraded") || mlt.hasAttribute("modified")) {
        // Our document was upgraded, create a backup copy just in case
        QString baseFile = m_doc->url().path().section(".kdenlive", 0, 0);
        int ct = 0;
        QString backupFile = baseFile + "_backup" + QString::number(ct) + ".kdenlive";
        while (QFile::exists(backupFile)) {
            ct++;
            backupFile = baseFile + "_backup" + QString::number(ct) + ".kdenlive";
        }
        QString message;
        if (mlt.hasAttribute("upgraded"))
            message = i18n("Your project file was upgraded to the latest Kdenlive document version.\nTo make sure you don't lose data, a backup copy called %1 was created.", backupFile);
        else
            message = i18n("Your project file was modified by Kdenlive.\nTo make sure you don't lose data, a backup copy called %1 was created.", backupFile);
        
        KIO::FileCopyJob *copyjob = KIO::file_copy(m_doc->url(), QUrl::fromLocalFile(backupFile));
        if (copyjob->exec())
            KMessageBox::information(this, message);
        else
            KMessageBox::information(this, i18n("Your project file was upgraded to the latest Kdenlive document version, but it was not possible to create the backup copy %1.", backupFile));
    }
}

void Timeline::slotDeleteClip(const QString &clipId, QUndoCommand *deleteCommand)
{
    m_trackview->deleteClip(clipId, deleteCommand);
}

void Timeline::setCursorPos(int pos)
{
    m_trackview->setCursorPos(pos);
}

void Timeline::moveCursorPos(int pos)
{
    m_trackview->setCursorPos(pos);
}

void Timeline::slotChangeZoom(int horizontal, int vertical)
{
    m_ruler->setPixelPerMark(horizontal);
    m_scale = (double) m_trackview->getFrameWidth() / m_ruler->comboScale[horizontal];

    if (vertical == -1) {
        // user called zoom
        m_doc->setZoom(horizontal, m_verticalZoom);
        m_trackview->setScale(m_scale, m_scene->scale().y());
    } else {
        m_verticalZoom = vertical;
        if (m_verticalZoom == 0)
            m_trackview->setScale(m_scale, 0.5);
        else
            m_trackview->setScale(m_scale, m_verticalZoom);
        adjustTrackHeaders();
    }
}

int Timeline::fitZoom() const
{
    int zoom = (int)((duration() + 20 / m_scale) * m_trackview->getFrameWidth() / m_trackview->width());
    int i;
    for (i = 0; i < 13; ++i)
        if (m_ruler->comboScale[i] > zoom) break;
    return i;
}

KdenliveDoc *Timeline::document()
{
    return m_doc;
}

void Timeline::refresh()
{
    m_trackview->viewport()->update();
}

void Timeline::slotRepaintTracks()
{
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    for (int i = 0; i < widgets.count(); ++i) {
        if (widgets.at(i)) widgets.at(i)->setSelectedIndex(m_trackview->selectedTrack());
    }
}

void Timeline::slotReloadTracks()
{
    slotRebuildTrackHeaders();
    emit updateTracksInfo();
}

TrackInfo Timeline::getTrackInfo(int ix)
{
    Track *tk = track(ix);
    return tk->info();
}

void Timeline::setTrackInfo(int ix, TrackInfo info)
{
    if (ix < 0 || ix > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *tk = track(ix);
    tk->setInfo(info);
}


QList <TrackInfo> Timeline::getTracksInfo()
{
    QList <TrackInfo> tracks;
    for (int i = 0; i < tracksCount(); i++) {
        tracks << track(i)->info();
    }
    return tracks;
}

void Timeline::lockTrack(int ix, bool lock)
{
    if (ix < 0 || ix > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *tk = track(ix);
    tk->setProperty("kdenlive:locked_track", lock ? 1 : 0);
}

bool Timeline::isTrackLocked(int ix)
{
    if (ix < 0 || ix > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return false;
    }
    Track *tk = track(ix);
    int locked = tk->getIntProperty("kdenlive:locked_track");
    return locked == 1;
}

void Timeline::updateTrackState(int ix, int state)
{
    QScopedPointer<Mlt::Producer> track(m_tractor->track(ix));
    int currentState = track->get_int("hide");
    if (state == currentState) return;
    if (state == 0) {
        // Show all
        if (currentState & 1) {
            switchTrackVideo(ix, false);
        }
        if (currentState & 2) {
            switchTrackAudio(ix, false);
        }
    }
    else if (state == 1) {
        // Mute video
        if (currentState & 2) {
            switchTrackAudio(ix, false);
        }
        switchTrackVideo(ix, true);
    }
    else if (state == 2) {
        // Mute audio
        if (currentState & 1) {
            switchTrackVideo(ix, false);
        }
        switchTrackAudio(ix, true);
    }
    else {
        switchTrackVideo(ix, true);
        switchTrackAudio(ix, true);
    }
}

void Timeline::switchTrackVideo(int ix, bool hide)
{
    if (ix < 0 || ix > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track* tk = track(ix);
    int state = tk->state();
    if (hide && (state & 1)) {
        // Video is already muted
        return;
    }
    int newstate = 0;
    if (hide) {
        if (state & 2) {
            newstate = 3;
        }
        else {
            newstate = 1;
        }
    }
    else {
        if (state & 2) {
            newstate = 2;
        }
        else {
            newstate = 0;
        }
    }
    tk->setState(newstate);
    refreshTractor();
}

void Timeline::refreshTractor()
{
    m_tractor->multitrack()->refresh();
    m_tractor->refresh();
}

void Timeline::switchTrackAudio(int ix, bool mute)
{
    if (ix < 0 || ix > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track* tk = track(ix);
    int state = tk->state();
    bool audioMixingBroken = false;
    if (mute && (state & 2)) {
        // audio is already muted
        return;
    }
    if (mute && state < 2 ) {
        // We mute a track with sound
        if (ix == getLowestNonMutedAudioTrack()) audioMixingBroken = true;
    }
    else if (!mute && state > 1 ) {
        // We un-mute a previously muted track
        if (ix < getLowestNonMutedAudioTrack()) audioMixingBroken = true;
    }
    int newstate;
    if (mute) {
        if (state & 1) newstate = 3;
        else newstate = 2;
    } else if (state & 1) {
        newstate = 1;
    } else {
        newstate = 0;
    }
    tk->setState(newstate);
    if (audioMixingBroken) fixAudioMixing();
    m_tractor->multitrack()->refresh();
    m_tractor->refresh();
}

int Timeline::getLowestNonMutedAudioTrack()
{
    for (int i = 0; i < m_tracks.count(); ++i) {
        Track *tk = track(i);
        if (tk->state() < 2) return i;
    }
    return m_tracks.count();
}

void Timeline::fixAudioMixing()
{
    // Make sure the audio mixing transitions are applied to the lowest audible (non muted) track
    int lowestTrack = getLowestNonMutedAudioTrack();
    mlt_service service = mlt_service_get_producer(m_tractor->get_service());
    Mlt::Field *field = m_tractor->field();
    mlt_service_lock(service);
    mlt_service nextservice = mlt_service_get_producer(service);
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString mlt_type = mlt_properties_get(properties, "mlt_type");
    QString resource = mlt_properties_get(properties, "mlt_service");

    mlt_service nextservicetodisconnect;
    // Delete all audio mixing transitions
    while (mlt_type == "transition") {
        if (resource == "mix") {
            nextservicetodisconnect = nextservice;
            nextservice = mlt_service_producer(nextservice);
            mlt_field_disconnect_service(field->get_field(), nextservicetodisconnect);
        }
        else nextservice = mlt_service_producer(nextservice);
        if (nextservice == NULL) break;
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_properties_get(properties, "mlt_type");
        resource = mlt_properties_get(properties, "mlt_service");
    }

    // Re-add correct audio transitions
    for (int i = lowestTrack + 1; i < m_tractor->count(); ++i) {
        Mlt::Transition *transition = new Mlt::Transition(*m_tractor->profile(), "mix");
        transition->set("always_active", 1);
        transition->set("combine", 1);
        transition->set("internal_added", 237);
        field->plant_transition(*transition, lowestTrack, i);
    }
    mlt_service_unlock(service);
}


void Timeline::slotRebuildTrackHeaders()
{
    if (m_tractor == NULL || m_tractor->count() == 0 || m_tracks.isEmpty()) {
        qDebug()<<"+ + + ++ + + + + ++ \nEMPTY TRACTOR WHEN BUKDING HEADERS\n + + ++ + + + + +";
        return;
    }
    /*for (int i = 0; i < m_tractor->count(); i++)
    {
        Mlt::Producer* track = m_tractor->track(i);
        Mlt::Playlist playlist(*track);
        qDebug()<<"++ GOT TRACK: "<<i<<" = "<<playlist.get("kdenlive:track_name");
    }*/
    QLayoutItem *child;
    while ((child = headers_container->layout()->takeAt(0)) != 0) {
        QWidget *wid = child->widget();
        delete child;
        if (wid) wid->deleteLater();
    }
    int max = m_tracks.count();
    int height = KdenliveSettings::trackheight() * m_scene->scale().y() - 1;
    QFrame *frame = NULL;
    int headerWidth = 70;
    for (int i = 0; i < max; i++) {
        frame = new QFrame(headers_container);
        frame->setFrameStyle(QFrame::HLine);
        frame->setFixedHeight(1);
        headers_container->layout()->addWidget(frame);
        TrackInfo info = track(i)->info();
        HeaderTrack *header = new HeaderTrack(i, info, height, m_trackActions, headers_container);
        int currentWidth = header->minimumWidth();
        if (currentWidth > headerWidth) headerWidth = currentWidth;
        header->setSelectedIndex(m_trackview->selectedTrack());
        connect(header, SIGNAL(switchTrackVideo(int,bool)), m_trackview, SLOT(slotSwitchTrackVideo(int,bool)));
        connect(header, SIGNAL(switchTrackAudio(int,bool)), m_trackview, SLOT(slotSwitchTrackAudio(int,bool)));
        connect(header, SIGNAL(switchTrackLock(int,bool)), m_trackview, SLOT(slotSwitchTrackLock(int,bool)));
        connect(header, SIGNAL(selectTrack(int)), m_trackview, SLOT(slotSelectTrack(int)));
        connect(header, SIGNAL(renameTrack(int,QString)), this, SLOT(slotRenameTrack(int,QString)));
        connect(header, SIGNAL(configTrack(int)), this, SIGNAL(configTrack(int)));
        connect(header, SIGNAL(addTrackEffect(QDomElement,int)), m_trackview, SLOT(slotAddTrackEffect(QDomElement,int)));
        connect(header, SIGNAL(showTrackEffects(int)), this, SLOT(slotShowTrackEffects(int)));
        headers_container->layout()->addWidget(header);
    }
    updatePalette();
    headers_container->setFixedWidth(headerWidth);
    frame = new QFrame(this);
    frame->setFrameStyle(QFrame::HLine);
    frame->setFixedHeight(1);
    headers_container->layout()->addWidget(frame);
}


void Timeline::updatePalette()
{
    headers_container->setStyleSheet("");
    setPalette(qApp->palette());
    QPalette p = qApp->palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor norm = scheme.shade(scheme.background(KColorScheme::ActiveBackground).color(), KColorScheme::MidShade);
    p.setColor(QPalette::Button, norm);
    QColor col = scheme.background().color();
    QColor col2 = scheme.foreground().color();
    headers_container->setStyleSheet(QString("QLineEdit { background-color: transparent;color: %1;} QLineEdit:hover{ background-color: %2;} QLineEdit:focus { background-color: %2;}").arg(col2.name()).arg(col.name()));
}

void Timeline::adjustTrackHeaders()
{
    int height = KdenliveSettings::trackheight() * m_scene->scale().y() - 1;
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    for (int i = 0; i < widgets.count(); ++i) {
        if (widgets.at(i)) widgets.at(i)->adjustSize(height);
    }
}

int Timeline::addTrack(int ix, Mlt::Playlist &playlist) {
    // parse track
    int position = 0;
    double fps = m_doc->fps();
    bool locked = playlist.get_int("kdenlive:locked_track") == 1;
    for(int i = 0; i < playlist.count(); ++i) {
        QScopedPointer<Mlt::Producer> clip(playlist.get_clip(i));
        if (clip->is_blank()) {
            position += clip->get_playtime();
            continue;
        }
        // Found a clip
        int in = clip->get_in();
        int out = clip->get_out();
        QString idString = clip->parent().get("id");
        if (in >= out || m_invalidProducers.contains(idString)) {
            m_documentErrors.append(i18n("Invalid clip removed from track %1 at %2\n", ix, position));
            playlist.remove(i);
            --i;
            continue;
        }
        QString id = idString;
        double speed = 1.0;
        int strobe = 1;
        if (idString.endsWith("_video")) {
            // Video only producer, store it in BinController
            m_doc->renderer()->loadExtraProducer(idString, new Mlt::Producer(clip->parent()));
        }
        if (idString.startsWith(QLatin1String("slowmotion"))) {
            QLocale locale;
            locale.setNumberOptions(QLocale::OmitGroupSeparator);
            id = idString.section(':', 1, 1);
            speed = locale.toDouble(idString.section(':', 2, 2));
            strobe = idString.section(':', 3, 3).toInt();
            if (strobe == 0) strobe = 1;
	    // Slowmotion producer, store it for reuse
            m_doc->renderer()->storeSlowmotionProducer(idString, new Mlt::Producer(clip->parent()));
        }
        id = id.section('_', 0, 0);
	int length = out - in + 1;
        ProjectClip *binclip = m_doc->getBinClip(id);
        if (binclip == NULL) {
	    // Warning, unknown clip found, timeline corruption!!
	    
	    //TODO: fix this
	    position += length;
	    continue;
	}
        ItemInfo clipinfo;
        clipinfo.startPos = GenTime(position, fps);
        clipinfo.endPos = GenTime(position + length, fps);
        clipinfo.cropStart = GenTime(in, fps);
        clipinfo.cropDuration = GenTime(length, fps);
        clipinfo.track = ix;
	position += length;
	//qDebug()<<"// Loading clip: "<<idString<<" / SPEED: "<<speed<<"\n++++++++++++++++++++++++";
        ClipItem *item = new ClipItem(binclip, clipinfo, fps, speed, strobe, m_trackview->getFrameWidth(), true);
        item->updateState(idString);
        m_scene->addItem(item);
        if (locked) item->setItemLocked(true);
        if (speed != 1.0 || strobe > 1) {
            QDomElement speedeffect = MainWindow::videoEffects.getEffectByTag(QString(), "speed").cloneNode().toElement();
            EffectsList::setParameter(speedeffect, "speed", QString::number((int)(100 * speed + 0.5)));
            EffectsList::setParameter(speedeffect, "strobe", QString::number(strobe));
            item->addEffect(m_doc->getProfileInfo(), speedeffect, false);
            item->effectsCounter();
        }
        // parse clip effects
        getEffects(*clip, item);
    }
    return position;
}

void Timeline::loadGuides(QMap <double, QString> guidesData)
{
    QMapIterator<double, QString> i(guidesData);
    while (i.hasNext()) {
        i.next();
        const GenTime pos = GenTime(i.key());
        m_trackview->addGuide(pos, i.value(), true);
    }
}

void Timeline::getEffects(Mlt::Service &service, ClipItem *clip, int track) {
    int effectNb = 0;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> effect(service.filter(ix));
        QDomElement clipeffect = getEffectByTag(effect->get("tag"), effect->get("kdenlive_id"));
        if (clipeffect.isNull()) {
            m_documentErrors.append(i18n("Effect %1:%2 not found in MLT, it was removed from this project\n", effect->get("tag"), effect->get("kdenlive_id")));
            service.detach(*effect);
            --ix;
            continue;
        }
        effectNb++;
        QDomElement currenteffect = clipeffect.cloneNode().toElement();
        currenteffect.setAttribute("kdenlive_ix", QString::number(effectNb));
        currenteffect.setAttribute("kdenlive_info", effect->get("kdenlive_info"));
        currenteffect.setAttribute("disable", effect->get("disable"));
        QDomNodeList clipeffectparams = currenteffect.childNodes();

        QDomNodeList params = currenteffect.elementsByTagName("parameter");
	ProfileInfo info = m_doc->getProfileInfo();
        for (int i = 0; i < params.count(); ++i) {
            QDomElement e = params.item(i).toElement();
            if (e.attribute("type") == "keyframe") e.setAttribute("keyframes", getKeyframes(service, ix, e));
            else setParam(info, e, effect->get(e.attribute("name").toUtf8().constData()));
        }

        if (effect->get_out()) { // no keyframes but in/out points
            EffectsList::setParameter(currenteffect, "in",  effect->get("in"));
            EffectsList::setParameter(currenteffect, "out",  effect->get("out"));
            currenteffect.setAttribute("in", effect->get_in());
            currenteffect.setAttribute("out", effect->get_out());
            currenteffect.setAttribute("_sync_in_out", "1");
        }
        if (QString(effect->get("tag")) == "region") getSubfilters(effect.data(), currenteffect);

        if (clip) {
            clip->addEffect(m_doc->getProfileInfo(), currenteffect, false);
        } else {
            addTrackEffect(track, currenteffect);
        }
    }
}

QString Timeline::getKeyframes(Mlt::Service service, int &ix, QDomElement e) {
    QString starttag = e.attribute("starttag", "start");
    QString endtag = e.attribute("endtag", "end");
    double fact, offset = e.attribute("offset", "0").toDouble();
    QString factor = e.attribute("factor", "1");
    if (factor.contains('%')) fact = EffectsController::getStringEval(m_doc->getProfileInfo(), factor);
    else fact = factor.toDouble();
    // retrieve keyframes
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QScopedPointer<Mlt::Filter> effect(service.filter(ix));
    int effectNb = effect->get_int("kdenlive_ix");
    QString keyframes = QString::number(effect->get_in()) + '=' + locale.toString(offset + fact * effect->get_double(starttag.toUtf8().constData())) + ';';
    for (;ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> eff2(service.filter(ix));
        if (eff2->get_int("kdenlive_ix") != effectNb) break;
        if (eff2->get_in() < eff2->get_out()) {
            keyframes.append(QString::number(eff2->get_out()) + '=' + locale.toString(offset + fact * eff2->get_double(endtag.toUtf8().constData())) + ';');
        }
    }
    --ix;
    return keyframes;
}

void Timeline::getSubfilters(Mlt::Filter *effect, QDomElement &currenteffect) {
    for (int i = 0; ; ++i) {
        QString name = "filter" + QString::number(i);
        if (!effect->get(name.toUtf8().constData())) break;
        //identify effect
        QString tag = effect->get(name.append(".tag").toUtf8().constData());
        QString id = effect->get(name.append(".kdenlive_id").toUtf8().constData());
        QDomElement subclipeffect = getEffectByTag(tag, id);
        if (subclipeffect.isNull()) {
            qWarning() << "Region sub-effect not found";
            continue;
        }
        //load effect
        subclipeffect = subclipeffect.cloneNode().toElement();
        subclipeffect.setAttribute("region_ix", i);
        //get effect parameters (prefixed by subfilter name)
        QDomNodeList params = subclipeffect.elementsByTagName("parameter");
	ProfileInfo info = m_doc->getProfileInfo();
        for (int i = 0; i < params.count(); ++i) {
            QDomElement param = params.item(i).toElement();
            setParam(info, param, effect->get((name + "." + param.attribute("name")).toUtf8().constData()));
        }
        currenteffect.appendChild(currenteffect.ownerDocument().importNode(subclipeffect, true));
    }
}

//static
void Timeline::setParam(ProfileInfo info, QDomElement param, QString value) {
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    //get Kdenlive scaling parameters
    double offset = param.attribute("offset", "0").toDouble();
    double fact;
    QString factor = param.attribute("factor", "1");
    if (factor.contains('%')) {
        fact = EffectsController::getStringEval(info, factor);
    } else {
        fact = factor.toDouble();
    }
    //adjust parameter if necessary
    QString type = param.attribute("type");
    if (type == "simplekeyframe") {
        QStringList kfrs = value.split(';');
        for (int l = 0; l < kfrs.count(); ++l) {
            QString fr = kfrs.at(l).section('=', 0, 0);
            double val = locale.toDouble(kfrs.at(l).section('=', 1, 1));
            kfrs[l] = fr + '=' + QString::number((int) (val * fact + offset));
        }
        param.setAttribute("keyframes", kfrs.join(";"));
    } else if (type == "double" || type == "constant") {
        param.setAttribute("value", locale.toDouble(value) * fact + offset);
    } else {
        param.setAttribute("value", value);
    }
}

QDomElement Timeline::getEffectByTag(const QString &effecttag, const QString &effectid)
{
    QDomElement clipeffect = MainWindow::customEffects.getEffectByTag(QString(), effectid);
    if (clipeffect.isNull()) {
        clipeffect = MainWindow::videoEffects.getEffectByTag(effecttag, effectid);
    }
    if (clipeffect.isNull()) {
        clipeffect = MainWindow::audioEffects.getEffectByTag(effecttag, effectid);
    }
    return clipeffect;
}


QGraphicsScene *Timeline::projectScene()
{
    return m_scene;
}

CustomTrackView *Timeline::projectView()
{
    return m_trackview;
}

void Timeline::setEditMode(const QString & editMode)
{
    m_editMode = editMode;
}

const QString & Timeline::editMode() const
{
    return m_editMode;
}

void Timeline::slotChangeTrackLock(int ix, bool lock)
{
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    widgets.at(ix)->setLock(lock);
}


void Timeline::slotVerticalZoomDown()
{
    if (m_verticalZoom == 0) return;
    m_verticalZoom--;
    m_doc->setZoom(m_doc->zoom().x(), m_verticalZoom);
    if (m_verticalZoom == 0)
        m_trackview->setScale(m_scene->scale().x(), 0.5);
    else
        m_trackview->setScale(m_scene->scale().x(), 1);
    adjustTrackHeaders();
    m_trackview->verticalScrollBar()->setValue(headers_area->verticalScrollBar()->value());
}

void Timeline::slotVerticalZoomUp()
{
    if (m_verticalZoom == 2) return;
    m_verticalZoom++;
    m_doc->setZoom(m_doc->zoom().x(), m_verticalZoom);
    if (m_verticalZoom == 2)
        m_trackview->setScale(m_scene->scale().x(), 2);
    else
        m_trackview->setScale(m_scene->scale().x(), 1);
    adjustTrackHeaders();
    m_trackview->verticalScrollBar()->setValue(headers_area->verticalScrollBar()->value());
}

void Timeline::updateProjectFps()
{
    m_ruler->updateProjectFps(m_doc->timecode());
    m_trackview->updateProjectFps();
}

void Timeline::slotRenameTrack(int ix, const QString &name)
{
    QString currentName = track(ix)->getProperty("kdenlive:track_name");
    ConfigTracksCommand *configTracks = new ConfigTracksCommand(this, ix, currentName, name);
    m_doc->commandStack()->push(configTracks);
}

void Timeline::renameTrack(int ix, const QString &name)
{
    track(ix)->setProperty("kdenlive:track_name", name);
    // Make sure header widget displays correct name
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    if (ix < 0 || ix >= widgets.count()) {
        qWarning() << "ERROR, Trying to access a non existent track: " << ix;
        return;
    }
    widgets.at(ix)->renameTrack(name);
}

void Timeline::slotUpdateVerticalScroll(int /*min*/, int max)
{
    int height = 0;
    if (max > 0) height = m_trackview->horizontalScrollBar()->height() - 1;
    headers_container->layout()->setContentsMargins(0, m_trackview->frameWidth(), 0, height);
}

void Timeline::updateRuler()
{
    m_ruler->update();
}

void Timeline::slotShowTrackEffects(int ix)
{
    m_trackview->clearSelection();
    emit showTrackEffects(tracksCount() - ix, getTrackInfo(ix));
}

void Timeline::slotUpdateTrackEffectState(int ix)
{
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    if (ix < 0 || ix >= widgets.count()) {
        qWarning() << "ERROR, Trying to access a non existent track: " << ix;
        return;
    }
    widgets.at(ix)->updateEffectLabel(track(ix)->effectsList.effectNames());
}

void Timeline::slotSaveTimelinePreview(const QString &path)
{
    QImage img(width(), height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(palette().base().color().rgb());
    QPainter painter(&img);
    render(&painter);
    painter.end();
    img = img.scaledToWidth(600, Qt::SmoothTransformation);
    img.save(path);
}

void Timeline::updateProfile()
{
    m_ruler->updateFrameSize();
    m_trackview->updateSceneFrameWidth();
    slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
    slotSetZone(m_doc->zone(), false);
}

void Timeline::checkTrackHeight()
{
    if (m_trackview->checkTrackHeight()) {
        m_doc->clipManager()->clearCache();
        m_ruler->updateFrameSize();
        m_trackview->updateSceneFrameWidth();
        slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
        slotSetZone(m_doc->zone(), false);
    }
}

bool Timeline::moveClip(int startTrack, qreal startPos, int endTrack, qreal endPos, PlaylistState::ClipState state, int mode, bool duplicate)
{
    if (startTrack == endTrack) {
        return track(startTrack)->move(startPos, endPos, mode);
    }
    Track *sourceTrack = track(startTrack);
    int pos = sourceTrack->frame(startPos);
    int clipIndex = sourceTrack->playlist().get_clip_index_at(pos);
    sourceTrack->playlist().lock();
    Mlt::Producer *clipProducer = sourceTrack->playlist().replace_with_blank(clipIndex);
    sourceTrack->playlist().consolidate_blanks();
    if (!clipProducer || clipProducer->is_blank()) {
        qDebug() << "// Cannot get clip at index: "<<clipIndex<<" / "<< startPos;
        sourceTrack->playlist().unlock();
        return false;
    }
    sourceTrack->playlist().unlock();
    Track *destTrack = track(endTrack);
    
    bool success = destTrack->add(endPos, clipProducer, GenTime(clipProducer->get_in(), destTrack->fps()).seconds(), GenTime(clipProducer->get_out(), destTrack->fps()).seconds(), state, duplicate, mode);
    delete clipProducer;
    return success;
}

void Timeline::addTrackEffect(int trackIndex, QDomElement effect)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *sourceTrack = track(trackIndex);
    effect.setAttribute("kdenlive_ix", sourceTrack->effectsList.count() + 1);

    // Init parameter value & keyframes if required
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();

        // Check if this effect has a variable parameter
        if (e.attribute("default").contains('%')) {
            double evaluatedValue = EffectsController::getStringEval(m_doc->getProfileInfo(), e.attribute("default"));
            e.setAttribute("default", evaluatedValue);
            if (e.hasAttribute("value") && e.attribute("value").startsWith('%')) {
                e.setAttribute("value", evaluatedValue);
            }
        }

        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe")) {
            QString def = e.attribute("default");
            // Effect has a keyframe type parameter, we need to set the values
            if (e.attribute("keyframes").isEmpty()) {
                e.setAttribute("keyframes", "0:" + def + ';');
                //qDebug() << "///// EFFECT KEYFRAMES INITED: " << e.attribute("keyframes");
                //break;
            }
        }

        if (effect.attribute("id") == "crop") {
            // default use_profile to 1 for clips with proxies to avoid problems when rendering
            if (e.attribute("name") == "use_profile" && m_doc->useProxy())
                e.setAttribute("value", "1");
        }
    }
    sourceTrack->effectsList.append(effect);
}

void Timeline::removeTrackEffect(int trackIndex, const QDomElement &effect)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    int toRemove = effect.attribute("kdenlive_ix").toInt();
    Track *sourceTrack = track(trackIndex);
    int max = sourceTrack->effectsList.count();
    for (int i = 0; i < max; ++i) {
        int index = sourceTrack->effectsList.at(i).attribute("kdenlive_ix").toInt();
        if (toRemove == index) {
            sourceTrack->effectsList.removeAt(toRemove);
            break;
        }
    }
}

void Timeline::setTrackEffect(int trackIndex, int effectIndex, QDomElement effect)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *sourceTrack = track(trackIndex);
    int max = sourceTrack->effectsList.count();
    if (effectIndex <= 0 || effectIndex > (max) || effect.isNull()) {
        //qDebug() << "Invalid effect index: " << effectIndex;
        return;
    }
    sourceTrack->effectsList.removeAt(effect.attribute("kdenlive_ix").toInt());
    effect.setAttribute("kdenlive_ix", effectIndex);
    sourceTrack->effectsList.insert(effect);
}

void Timeline::enableTrackEffects(int trackIndex, const QList <int> &effectIndexes, bool disable)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *sourceTrack = track(trackIndex);
    EffectsList list = sourceTrack->effectsList;
    QDomElement effect;
    for (int i = 0; i < effectIndexes.count(); ++i) {
        effect = list.itemFromIndex(effectIndexes.at(i));
        if (!effect.isNull()) effect.setAttribute("disable", (int) disable);
    }
}

const EffectsList Timeline::getTrackEffects(int trackIndex)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return EffectsList();
    }
    Track *sourceTrack = track(trackIndex);
    return sourceTrack->effectsList;
}

QDomElement Timeline::getTrackEffect(int trackIndex, int effectIndex)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return QDomElement();
    }
    Track *sourceTrack = track(trackIndex);
    EffectsList list = sourceTrack->effectsList;
    if (effectIndex > list.count() || effectIndex < 1 || list.itemFromIndex(effectIndex).isNull()) return QDomElement();
    return list.itemFromIndex(effectIndex).cloneNode().toElement();
}

int Timeline::hasTrackEffect(int trackIndex, const QString &tag, const QString &id) 
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return -1;
    }
    Track *sourceTrack = track(trackIndex);
    EffectsList list = sourceTrack->effectsList;
    return list.hasEffect(tag, id);
}

MltVideoProfile Timeline::mltProfile() const
{
    return ProfilesDialog::getVideoProfile(*m_tractor->profile());
}

double Timeline::fps() const
{
    return m_doc->fps();
}


QPoint Timeline::getTracksCount()
{
    int audioTracks = 0;
    int videoTracks = 0;
    int max = m_tracks.count();
    for (int i = 0; i < max; i++) {
        Track *tk = track(i);
        if (tk->type == AudioTrack) audioTracks++;
        else videoTracks++;
    }
    return QPoint(videoTracks, audioTracks);
}

int Timeline::getTrackSpaceLength(int trackIndex, int pos, bool fromBlankStart)
{
    if (trackIndex < 0 || trackIndex > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return 0;
    }
    return track(trackIndex)->getBlankLength(pos, fromBlankStart);
}

void Timeline::updateClipProperties(const QString &id, QMap <QString, QString> properties)
{
    for (int i = 0; i< m_tracks.count(); i++) {
        track(i)->updateClipProperties(id, properties);
    }
}

int Timeline::changeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, double speed, int strobe, Mlt::Producer *originalProd)
{
    QLocale locale;
    QString url = QString::fromUtf8(originalProd->get("resource"));
    url.append('?' + locale.toString(speed));
    if (strobe > 1) url.append("&strobe=" + QString::number(strobe));
    Mlt::Producer *prod = m_doc->renderer()->getSlowmotionProducer(url);
    Mlt::Properties passProperties;
    Mlt::Properties original(originalProd->get_properties());
    passProperties.pass_list(original, ClipController::getPassPropertiesList());
    return track(info.track)->changeClipSpeed(info, speedIndependantInfo, speed, strobe, prod, passProperties);
}

