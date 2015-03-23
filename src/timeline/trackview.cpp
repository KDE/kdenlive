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


#include "trackview.h"
#include "headertrack.h"
#include "clipitem.h"
#include "transition.h"
#include "timelinecommands.h"
#include "customruler.h"
#include "customtrackview.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "doc/kdenlivedoc.h"
#include "project/clipmanager.h"
#include "effectslist/initeffects.h"
#include "dialogs/profilesdialog.h"

#include <QDebug>
#include <QScrollBar>

#include <KMessageBox>
#include <KIO/FileCopyJob>
#include <klocalizedstring.h>


TrackView::TrackView(KdenliveDoc *doc, const QList<QAction *> &actions, bool *ok, QWidget *parent) :
    QWidget(parent),
    m_scale(1.0),
    m_projectTracks(0),
    m_doc(doc),
    m_verticalZoom(1)
{
    m_trackActions << actions;
    setupUi(this);
    //    ruler_frame->setMaximumHeight();
    //    size_frame->setMaximumHeight();
    m_scene = new CustomTrackScene(doc);
    m_trackview = new CustomTrackView(doc, m_scene, parent);
    m_trackview->scale(1, 1);
    m_trackview->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_ruler = new CustomRuler(doc->timecode(), m_trackview);
    connect(m_ruler, SIGNAL(zoneMoved(int,int)), this, SIGNAL(zoneMoved(int,int)));
    connect(m_ruler, SIGNAL(adjustZoom(int)), this, SIGNAL(setZoom(int)));
    connect(m_ruler, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
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

    if (m_doc->setSceneList() == -1) *ok = false;
    else *ok = true;
    

    //TODO: loading the timeline clips should be done directly from MLT's track playlist and not by reading xml
    parseDocument(m_doc->toXml());
    connect(m_trackview, SIGNAL(cursorMoved(int,int)), m_ruler, SLOT(slotCursorMoved(int,int)));
    connect(m_trackview, SIGNAL(updateRuler()), m_ruler, SLOT(updateRuler()));

    connect(m_trackview->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_ruler, SLOT(slotMoveRuler(int)));
    connect(m_trackview->horizontalScrollBar(), SIGNAL(rangeChanged(int,int)), this, SLOT(slotUpdateVerticalScroll(int,int)));
    connect(m_trackview, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_trackview, SIGNAL(doTrackLock(int,bool)), this, SLOT(slotChangeTrackLock(int,bool)));

    slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
    slotSetZone(m_doc->zone(), false);
}

TrackView::~TrackView()
{
    delete m_ruler;
    delete m_trackview;
    delete m_scene;
}

//virtual
void TrackView::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Up) {
        m_trackview->slotTrackUp();
        event->accept();
    } else if (event->key() == Qt::Key_Down) {
        m_trackview->slotTrackDown();
        event->accept();
    } else QWidget::keyPressEvent(event);
}

int TrackView::duration() const
{
    return m_trackview->duration();
}

bool TrackView::checkProjectAudio() const
{
    bool hasAudio = false;
    const QList <TrackInfo> list = m_doc->tracksList();
    int max = list.count();
    for (int i = 0; i < max; ++i) {
        TrackInfo info = list.at(max - i - 1);
        if (!info.isMute && m_trackview->hasAudio(i)) {
            hasAudio = true;
            break;
        }
    }
    return hasAudio;
}

int TrackView::inPoint() const
{
    return m_ruler->inPoint();
}

int TrackView::outPoint() const
{
    return m_ruler->outPoint();
}

void TrackView::slotSetZone(const QPoint &p, bool updateDocumentProperties)
{
    m_ruler->setZone(p);
    if (updateDocumentProperties) m_doc->setZone(p.x(), p.y());
}

void TrackView::setDuration(int dur)
{
    m_trackview->setDuration(dur);
    m_ruler->setDuration(dur);
}

int TrackView::getTracks(Mlt::Tractor &tractor) {
    int trackIndex = 0;
    int duration = 1;
    for (int i = 0; i < tractor.count(); ++i) {
        Mlt::Producer* track = tractor.track(i);
        QString playlist_name = track->get("id");
        if (playlist_name == "black_track" || playlist_name == "playlistmain") continue;
        // check track effects
        Mlt::Playlist playlist(*track);
        if (playlist.filter_count()) getEffects(playlist, NULL, trackIndex);
        ++trackIndex;
        int hide = track->get_int("hide");
        if (hide & 1) m_doc->switchTrackVideo(i - 1, true);
        if (hide & 2) m_doc->switchTrackAudio(i - 1, true);
        int trackduration;
        trackduration = addTrack(tractor.count() - 1 - i, playlist, m_doc->isTrackLocked(i - 1));
        if (trackduration > duration) duration = trackduration;
    }
    return duration;
}

void TrackView::getTransitions(Mlt::Tractor &tractor) {
    mlt_service service = mlt_service_get_producer(tractor.get_service());
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
            mlt_field_disconnect_service(tractor.field()->get_field(), disconnect);
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
            if (m_doc->isTrackLocked(b_track - 1)) tr->setItemLocked(true);
            m_scene->addItem(tr);
            service = mlt_service_producer(service);
        }
    }
}

bool TrackView::isSlide(QString geometry) {
    if (!geometry.count(';') == 1) return false;

    geometry.remove(QChar('%'), Qt::CaseInsensitive);
    geometry.replace(QChar('x'), QChar(':'), Qt::CaseInsensitive);
    geometry.replace(QChar(','), QChar(':'), Qt::CaseInsensitive);
    geometry.replace(QChar('/'), QChar(':'), Qt::CaseInsensitive);

    QString start = geometry.section('=', 0, 0).section(':', 0, -2) + ':';
    start.append(geometry.section('=', 1, 1).section(':', 0, -2));
    QStringList numbers = start.split(':', QString::SkipEmptyParts);
    bool isWipeTransition = true;
    for (int i = 0; i < numbers.size(); ++i) {
        int checkNumber = qAbs(numbers.at(i).toInt());
        if (checkNumber != 0 && checkNumber != 100) {
            return false;
        }
    }
    return true;
}

void TrackView::adjustDouble(QDomElement &e, double value) {
    QString factor = e.attribute("factor", "1");
    double offset = e.attribute("offset", "0").toDouble();
    double fact = 1;
    if (factor.contains('%')) fact = ProfilesDialog::getStringEval(m_doc->mltProfile(), factor);
    else fact = factor.toDouble();

    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    e.setAttribute("value", locale.toString(offset + value * fact));
}

void TrackView::parseDocument(const QDomDocument &doc)
{
    //int cursorPos = 0;
    m_documentErrors.clear();
    m_replacementProducerIds.clear();

    // parse project tracks
    QDomElement mlt = doc.firstChildElement("mlt");
    QDomElement tractor = mlt.firstChildElement("tractor");
    QDomNodeList tracks = tractor.elementsByTagName("track");
    QDomNodeList playlists = doc.elementsByTagName("playlist");
    int duration = 1;
    m_projectTracks = tracks.count();
    int trackduration = 0;
    QDomElement e;
    QDomElement p;

    int pos = m_projectTracks - 1;

    Mlt::Service svce(m_doc->renderer()->getProducer()->parent().get_service());
    Mlt::Tractor ttor(svce);
    m_trackview->setDuration(getTracks(ttor));
    getTransitions(ttor);


    QDomElement infoXml = mlt.firstChildElement("kdenlivedoc");

    QDomElement propsXml = infoXml.firstChildElement("documentproperties");
    
    int currentPos = propsXml.attribute("position").toInt();
    if (currentPos > 0) m_trackview->initCursorPos(currentPos);

    // Rebuild groups
    QDomNodeList groups = infoXml.elementsByTagName("group");
    m_trackview->loadGroups(groups);

    // Remove Kdenlive extra info from xml doc before sending it to MLT
    mlt.removeChild(infoXml);

    slotRebuildTrackHeaders();
    if (!m_documentErrors.isNull()) KMessageBox::sorry(this, m_documentErrors);
    if (infoXml.hasAttribute("upgraded") || infoXml.hasAttribute("modified")) {
        // Our document was upgraded, create a backup copy just in case
        QString baseFile = m_doc->url().path().section(".kdenlive", 0, 0);
        int ct = 0;
        QString backupFile = baseFile + "_backup" + QString::number(ct) + ".kdenlive";
        while (QFile::exists(backupFile)) {
            ct++;
            backupFile = baseFile + "_backup" + QString::number(ct) + ".kdenlive";
        }
        QString message;
        if (infoXml.hasAttribute("upgraded"))
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

void TrackView::slotDeleteClip(const QString &clipId, QUndoCommand *deleteCommand)
{
    m_trackview->deleteClip(clipId, deleteCommand);
}

void TrackView::setCursorPos(int pos)
{
    m_trackview->setCursorPos(pos);
}

void TrackView::moveCursorPos(int pos)
{
    m_trackview->setCursorPos(pos);
}

void TrackView::slotChangeZoom(int horizontal, int vertical)
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

int TrackView::fitZoom() const
{
    int zoom = (int)((duration() + 20 / m_scale) * m_trackview->getFrameWidth() / m_trackview->width());
    int i;
    for (i = 0; i < 13; ++i)
        if (m_ruler->comboScale[i] > zoom) break;
    return i;
}

KdenliveDoc *TrackView::document()
{
    return m_doc;
}

void TrackView::refresh()
{
    m_trackview->viewport()->update();
}

void TrackView::slotRepaintTracks()
{
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    for (int i = 0; i < widgets.count(); ++i) {
        if (widgets.at(i)) widgets.at(i)->setSelectedIndex(m_trackview->selectedTrack());
    }
}

void TrackView::slotReloadTracks()
{
    slotRebuildTrackHeaders();
    emit updateTracksInfo();
}

void TrackView::slotRebuildTrackHeaders()
{
    const QList <TrackInfo> list = m_doc->tracksList();
    QLayoutItem *child;
    while ((child = headers_container->layout()->takeAt(0)) != 0) {
        QWidget *wid = child->widget();
        delete child;
        if (wid) wid->deleteLater();
    }
    int max = list.count();
    int height = KdenliveSettings::trackheight() * m_scene->scale().y() - 1;
    QFrame *frame = NULL;
    updatePalette();
    int headerWidth = 70;
    for (int i = 0; i < max; ++i) {
        frame = new QFrame(headers_container);
        frame->setFrameStyle(QFrame::HLine);
        frame->setFixedHeight(1);
        headers_container->layout()->addWidget(frame);
        TrackInfo info = list.at(max - i - 1);
        HeaderTrack *header = new HeaderTrack(i, info, height, m_trackActions, headers_container);
        int currentWidth = header->minimumWidth();
        if (currentWidth > headerWidth) headerWidth = currentWidth;
        header->setSelectedIndex(m_trackview->selectedTrack());
        connect(header, SIGNAL(switchTrackVideo(int)), m_trackview, SLOT(slotSwitchTrackVideo(int)));
        connect(header, SIGNAL(switchTrackAudio(int)), m_trackview, SLOT(slotSwitchTrackAudio(int)));
        connect(header, SIGNAL(switchTrackLock(int)), m_trackview, SLOT(slotSwitchTrackLock(int)));
        connect(header, SIGNAL(selectTrack(int)), m_trackview, SLOT(slotSelectTrack(int)));
        connect(header, SIGNAL(renameTrack(int,QString)), this, SLOT(slotRenameTrack(int,QString)));
        connect(header, SIGNAL(configTrack(int)), this, SIGNAL(configTrack(int)));
        connect(header, SIGNAL(addTrackEffect(QDomElement,int)), m_trackview, SLOT(slotAddTrackEffect(QDomElement,int)));
        connect(header, SIGNAL(showTrackEffects(int)), this, SLOT(slotShowTrackEffects(int)));
        headers_container->layout()->addWidget(header);
    }
    headers_container->setFixedWidth(headerWidth);
    frame = new QFrame(this);
    frame->setFrameStyle(QFrame::HLine);
    frame->setFixedHeight(1);
    headers_container->layout()->addWidget(frame);
}


void TrackView::updatePalette()
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
    m_trackview->updatePalette();
    m_ruler->updatePalette();
    
}

void TrackView::adjustTrackHeaders()
{
    int height = KdenliveSettings::trackheight() * m_scene->scale().y() - 1;
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    for (int i = 0; i < widgets.count(); ++i) {
        if (widgets.at(i)) widgets.at(i)->adjustSize(height);
    }
}

int TrackView::addTrack(int ix, Mlt::Playlist &playlist, bool locked) {
    // parse track
    int position = 0;
    for(int i = 0; i < playlist.count(); ++i) {
        Mlt::Producer *clip = playlist.get_clip(i);
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
        if (idString.startsWith(QLatin1String("slowmotion"))) {
            QLocale locale;
            locale.setNumberOptions(QLocale::OmitGroupSeparator);
            id = idString.section(':', 1, 1);
            speed = locale.toDouble(idString.section(':', 2, 2));
            strobe = idString.section(':', 3, 3).toInt();
            if (strobe == 0) strobe = 1;
        }
        id = id.section('_', 0, 0);
        ProjectClip *binclip = m_doc->getBinClip(id);
        if (binclip == NULL) continue;
        double fps = m_doc->fps();
        int length = out - in + 1;
        ItemInfo clipinfo;
        clipinfo.startPos = GenTime(position, fps);
        clipinfo.endPos = GenTime(position + length, fps);
        clipinfo.cropStart = GenTime(in, fps);
        clipinfo.cropDuration = GenTime(length, fps);
        clipinfo.track = ix;
        //qDebug() << "// INSERTING CLIP: " << in << 'x' << out << ", track: " << ix << ", ID: " << id << ", SCALE: " << m_scale << ", FPS: " << m_doc->fps();
        ClipItem *item = new ClipItem(binclip, clipinfo, fps, speed, strobe, m_trackview->getFrameWidth(), false);
        if (idString.endsWith(QLatin1String("_video"))) item->setVideoOnly(true);
        else if (idString.endsWith(QLatin1String("_audio"))) item->setAudioOnly(true);
        m_scene->addItem(item);
        if (locked) item->setItemLocked(true);
        position += length;
        if (speed != 1.0 || strobe > 1) {
            QDomElement speedeffect = MainWindow::videoEffects.getEffectByTag(QString(), "speed").cloneNode().toElement();
            EffectsList::setParameter(speedeffect, "speed", QString::number((int)(100 * speed + 0.5)));
            EffectsList::setParameter(speedeffect, "strobe", QString::number(strobe));
            item->addEffect(speedeffect, false);
            item->effectsCounter();
        }
        // parse clip effects
        getEffects(*clip, item);
    }
    return position;
}

void TrackView::loadGuides(QMap <double, QString> guidesData)
{
    QMapIterator<double, QString> i(guidesData);
    while (i.hasNext()) {
        i.next();
        const GenTime pos = GenTime(i.key());
        m_trackview->addGuide(pos, i.value());
    }
}

void TrackView::getEffects(Mlt::Service &service, ClipItem *clip, int track) {
    int effectNb = 0;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        Mlt::Filter *effect = service.filter(ix);
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

        //TODO: rewrite other way around:
        //instead of looping in Mlt::Filter params to fill XML params
        //loop over XML and use getter functions to directly catch values
        //+split functions with offset/factor, keyframes...
        //+do all this in a FilterView wrapper class?

        // if effect is key-framable, get keyframes from each MLT filter instance
        if (MainWindow::videoEffects.hasKeyFrames(currenteffect)) {
            // effect tag names / default values
            QString starttag, endtag, factor;
            double offset = 0;
            QDomNodeList params = currenteffect.elementsByTagName("parameter");
            for (int i = 0; i < params.count(); ++i) {
                QDomElement e = params.item(i).toElement();
                if (e.attribute("type") == "keyframe") {
                    starttag = e.attribute("starttag", "start");
                    endtag = e.attribute("endtag", "end");
                    factor = e.attribute("factor", "1");
                    offset = e.attribute("offset", "0").toDouble();
                    break;
                }
            }
            double fact;
            if (factor.contains('%')) {
                fact = ProfilesDialog::getStringEval(m_doc->mltProfile(), factor);
            } else {
                fact = factor.toDouble();
            }
            // retrieve keyframes
            QString keyframes = QString::number(effect->get_in()) + '=' + locale.toString(offset + fact * effect->get_double(starttag.toUtf8().constData())) + ';';
            for (;ix < service.filter_count(); ++ix) {
                effect = service.filter(ix);
                if (effect->get_int("kdenlive_ix") != effectNb) break;
                if (effect->get_in() < effect->get_out()) {
                    keyframes.append(QString::number(effect->get_out()) + '=' + locale.toString(offset + fact * effect->get_double(endtag.toUtf8().constData())) + ';');
                }
            }
            params = currenteffect.elementsByTagName("parameter");
            for (int i = 0; i < params.count(); ++i) {
                QDomElement e = params.item(i).toElement();
                if (e.attribute("type") == "keyframe") e.setAttribute("keyframes", keyframes);
            }
        } else if (effect->get_out()) { // no keyframes but in/out points
            EffectsList::setParameter(currenteffect, "in",  effect->get("in"));
            EffectsList::setParameter(currenteffect, "out",  effect->get("out"));
            currenteffect.setAttribute("in", effect->get("in"));
            currenteffect.setAttribute("out", effect->get("out"));
            currenteffect.setAttribute("_sync_in_out", "1");
        }
        // adjust effect parameters
        bool regionFilter = QString(effect->get("tag")) == "region";
        int regionix = 0; // search for "filter0"
        for (int i = 0; i < effect->count(); ++i) {
            QString pname = effect->get_name(i);
            // Special case, region filter embeds other effects
            if (regionFilter && pname == QString("filter%1").arg(regionix)) {
                QString subfilter = pname;
                QDomElement subclipeffect = getEffectByTag(
                    effect->get(QString(subfilter + ".tag").toUtf8().constData()),
                    effect->get(QString(subfilter + ".kdenlive_id").toUtf8().constData()))
                    .cloneNode().toElement();
                if (subclipeffect.isNull()) { qWarning() << "Region sub-effect not found in MLT";}
                subclipeffect.setAttribute("region_ix", regionix);
                QDomNodeList subclipeffectparams = subclipeffect.childNodes();
                for (;;) { // forward read the subfilter parameters
                    ++i; pname = effect->get_name(i);
                    if (pname.startsWith(subfilter)) {
                        adjustparameterValue(subclipeffectparams, pname.section('.', 1, -1),
                            effect->get(pname.toUtf8().constData()));
                    } else {
                        ++regionix; // search for next subfilter
                        --i; // don't skip this param in for loop
                        break;
                    }
                }
                if (!subclipeffect.isNull())
                    currenteffect.appendChild(currenteffect.ownerDocument().importNode(subclipeffect, true));
            } else {
                // try to find this parameter in the effect xml and set its value
                adjustparameterValue(clipeffectparams, pname, effect->get(pname.toUtf8().constData()));
            }
        }
        if (clip) {
            clip->addEffect(currenteffect, false);
        } else {
            m_doc->addTrackEffect(track, currenteffect);
        }
    }
}

void TrackView::adjustparameterValue(QDomNodeList clipeffectparams, const QString &paramname, const QString &paramvalue)
{
    QDomElement e;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    for (int k = 0; k < clipeffectparams.count(); ++k) {
        e = clipeffectparams.item(k).toElement();
        if (!e.isNull() && e.tagName() == "parameter" && e.attribute("name") == paramname) {
            QString type = e.attribute("type");
            QString factor = e.attribute("factor", "1");
            double fact;
            if (factor.contains('%')) {
                fact = ProfilesDialog::getStringEval(m_doc->mltProfile(), factor);
            } else {
                fact = factor.toDouble();
            }
            double offset = e.attribute("offset", "0").toDouble();
            if (type == "simplekeyframe") {
                QStringList kfrs = paramvalue.split(';');
                for (int l = 0; l < kfrs.count(); ++l) {
                    QString fr = kfrs.at(l).section('=', 0, 0);
                    double val = locale.toDouble(kfrs.at(l).section('=', 1, 1));
                    //kfrs[l] = fr + ':' + locale.toString((int)(val * fact));
                    kfrs[l] = fr + '=' + QString::number((int) (offset + val * fact));
                }
                e.setAttribute("keyframes", kfrs.join(";"));
            } else if (type == "double" || type == "constant") {
                bool ok;
                e.setAttribute("value", offset + locale.toDouble(paramvalue, &ok) * fact);
                if (!ok)
                    e.setAttribute("value", paramvalue);
            } else {
                e.setAttribute("value", paramvalue);
            }
            break;
        }
    }
}


QDomElement TrackView::getEffectByTag(const QString &effecttag, const QString &effectid)
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


QGraphicsScene *TrackView::projectScene()
{
    return m_scene;
}

CustomTrackView *TrackView::projectView()
{
    return m_trackview;
}

void TrackView::setEditMode(const QString & editMode)
{
    m_editMode = editMode;
}

const QString & TrackView::editMode() const
{
    return m_editMode;
}

void TrackView::slotChangeTrackLock(int ix, bool lock)
{
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    widgets.at(ix)->setLock(lock);
}


void TrackView::slotVerticalZoomDown()
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

void TrackView::slotVerticalZoomUp()
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

void TrackView::updateProjectFps()
{
    m_ruler->updateProjectFps(m_doc->timecode());
    m_trackview->updateProjectFps();
}

void TrackView::slotRenameTrack(int ix, const QString &name)
{
    int tracknumber = m_doc->tracksCount() - ix;
    QList <TrackInfo> tracks = m_doc->tracksList();
    tracks[tracknumber - 1].trackName = name;
    ConfigTracksCommand *configTracks = new ConfigTracksCommand(m_trackview, m_doc->tracksList(), tracks);
    m_doc->commandStack()->push(configTracks);
    m_doc->setModified(true);
}

void TrackView::slotUpdateVerticalScroll(int /*min*/, int max)
{
    int height = 0;
    if (max > 0) height = m_trackview->horizontalScrollBar()->height() - 1;
    headers_container->layout()->setContentsMargins(0, m_trackview->frameWidth(), 0, height);
}

void TrackView::updateRuler()
{
    m_ruler->update();
}

void TrackView::slotShowTrackEffects(int ix)
{
    m_trackview->clearSelection();
    emit showTrackEffects(m_doc->tracksCount() - ix, m_doc->trackInfoAt(m_doc->tracksCount() - ix - 1));
}

void TrackView::slotUpdateTrackEffectState(int ix)
{
    QList<HeaderTrack *> widgets = findChildren<HeaderTrack *>();
    if (ix < 0 || ix >= widgets.count()) {
        //qDebug() << "ERROR, Trying to access a non existent track: " << ix;
        return;
    }
    widgets.at(m_doc->tracksCount() - ix - 1)->updateEffectLabel(m_doc->trackInfoAt(ix).effectsList.effectNames());
}

void TrackView::slotSaveTimelinePreview(const QString &path)
{
    QImage img(width(), height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(palette().base().color().rgb());
    QPainter painter(&img);
    render(&painter);
    painter.end();
    img = img.scaledToWidth(600, Qt::SmoothTransformation);
    img.save(path);
}

void TrackView::updateProfile()
{
    m_ruler->updateFrameSize();
    m_trackview->updateSceneFrameWidth();
    slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
    slotSetZone(m_doc->zone(), false);
}

void TrackView::checkTrackHeight()
{
    if (m_trackview->checkTrackHeight()) {
        m_doc->clipManager()->clearCache();
        m_ruler->updateFrameSize();
        m_trackview->updateSceneFrameWidth();
        slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
        slotSetZone(m_doc->zone(), false);
    }
}





