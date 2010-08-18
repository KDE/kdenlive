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


#include "monitor.h"
#include "renderer.h"
#include "monitormanager.h"
#include "smallruler.h"
#include "docclipbase.h"
#include "monitorscene.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KLocale>
#include <KFileDialog>
#include <KApplication>
#include <KMessageBox>

#include <QMouseEvent>
#include <QStylePainter>
#include <QMenu>
#include <QToolButton>
#include <QToolBar>
#include <QDesktopWidget>
#include <QLabel>
#include <QIntValidator>
#include <QVBoxLayout>
#include <QGraphicsView>


Monitor::Monitor(QString name, MonitorManager *manager, QString profile, QWidget *parent) :
        QWidget(parent),
        render(NULL),
        m_name(name),
        m_monitorManager(manager),
        m_currentClip(NULL),
        m_ruler(new SmallRuler(m_monitorManager)),
        m_overlay(NULL),
        m_isActive(false),
        m_scale(1),
        m_length(0),
        m_dragStarted(false),
        m_effectScene(NULL),
        m_effectView(NULL)
{
    m_ui.setupUi(this);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_ruler);
    m_ui.ruler_frame->setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(200);
    QToolBar *toolbar = new QToolBar(name, this);
    QVBoxLayout *layout2 = new QVBoxLayout;
    layout2->setContentsMargins(0, 0, 0, 0);

    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");

    if (name != "chapter") {
        toolbar->addAction(KIcon("kdenlive-zone-start"), i18n("Set zone start"), this, SLOT(slotSetZoneStart()));
        toolbar->addAction(KIcon("kdenlive-zone-end"), i18n("Set zone end"), this, SLOT(slotSetZoneEnd()));
    } else {
        m_ruler->setZone(-3, -2);
    }

    toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"), this, SLOT(slotRewind()));
    //toolbar->addAction(KIcon("media-skip-backward"), i18n("Rewind 1 frame"), this, SLOT(slotRewindOneFrame()));

    QToolButton *playButton = new QToolButton(toolbar);
    m_playMenu = new QMenu(i18n("Play..."), this);
    m_playAction = m_playMenu->addAction(m_playIcon, i18n("Play"));
    m_playAction->setCheckable(true);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlay()));

    playButton->setMenu(m_playMenu);
    playButton->setPopupMode(QToolButton::MenuButtonPopup);
    toolbar->addWidget(playButton);

    //toolbar->addAction(KIcon("media-skip-forward"), i18n("Forward 1 frame"), this, SLOT(slotForwardOneFrame()));
    toolbar->addAction(KIcon("media-seek-forward"), i18n("Forward"), this, SLOT(slotForward()));

    playButton->setDefaultAction(m_playAction);

    if (name != "chapter") {
        QToolButton *configButton = new QToolButton(toolbar);
        m_configMenu = new QMenu(i18n("Misc..."), this);
        configButton->setIcon(KIcon("system-run"));
        configButton->setMenu(m_configMenu);
        configButton->setPopupMode(QToolButton::QToolButton::InstantPopup);
        toolbar->addWidget(configButton);

        if (name == "clip") {
            m_markerMenu = new QMenu(i18n("Go to marker..."), this);
            m_markerMenu->setEnabled(false);
            m_configMenu->addMenu(m_markerMenu);
            connect(m_markerMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotGoToMarker(QAction *)));
        }
        m_configMenu->addAction(KIcon("transform-scale"), i18n("Resize (100%)"), this, SLOT(slotSetSizeOneToOne()));
        m_configMenu->addAction(KIcon("transform-scale"), i18n("Resize (50%)"), this, SLOT(slotSetSizeOneToTwo()));
    }

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    toolbar->addWidget(spacer);
    m_timePos = new TimecodeDisplay(m_monitorManager->timecode(), this);
    toolbar->addWidget(m_timePos);
    connect(m_timePos, SIGNAL(editingFinished()), this, SLOT(slotSeek()));

    layout2->addWidget(toolbar);
    m_ui.button_frame->setLayout(layout2);
    const int toolHeight = toolbar->height();
    m_ui.button_frame->setMinimumHeight(toolHeight);

    //m_ruler->setPixelPerMark(3);

    if (profile.isEmpty()) profile = KdenliveSettings::current_profile();

    QVBoxLayout *rendererBox = new QVBoxLayout(m_ui.video_frame);
    rendererBox->setContentsMargins(0, 0, 0, 0);
#ifdef Q_WS_MAC
    m_glWidget = new VideoGLWidget(m_ui.video_frame);
    rendererBox->addWidget(m_glWidget);
    render = new Render(m_name, (int) m_ui.video_frame->winId(), -1, profile, this);
    m_glWidget->setImageAspectRatio(render->dar());
    m_glWidget->setBackgroundColor(KdenliveSettings::window_background());
    m_glWidget->resize(m_ui.video_frame->size());
    connect(render, SIGNAL(showImageSignal(QImage)), m_glWidget, SLOT(showImage(QImage)));
    m_monitorRefresh = 0;
#else
    m_monitorRefresh = new MonitorRefresh(m_ui.video_frame);
    rendererBox->addWidget(m_monitorRefresh);
    render = new Render(m_name, (int) m_monitorRefresh->winId(), -1, profile, this);
    m_monitorRefresh->setRenderer(render);
#endif

    connect(m_ruler, SIGNAL(seekRenderer(int)), this, SLOT(slotSeek(int)));
    connect(render, SIGNAL(durationChanged(int)), this, SLOT(adjustRulerSize(int)));
    connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));

    //render->createVideoXWindow(m_ui.video_frame->winId(), -1);

    if (name != "clip") {
        connect(render, SIGNAL(rendererPosition(int)), this, SIGNAL(renderPosition(int)));
        connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SIGNAL(zoneUpdated(QPoint)));
    } else {
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SLOT(setClipZone(QPoint)));
    }
#ifndef Q_WS_MAC
    m_monitorRefresh->show();
#endif

    if (name == "project") {
        m_effectScene = new MonitorScene(render);
        m_effectView = new QGraphicsView(m_effectScene, m_ui.video_frame);
        rendererBox->addWidget(m_effectView);
        m_effectView->setMouseTracking(true);
        m_effectScene->setUp();
        m_effectView->hide();
    }

    kDebug() << "/////// BUILDING MONITOR, ID: " << m_ui.video_frame->winId();
}

Monitor::~Monitor()
{
    delete m_ruler;
    delete m_timePos;
    delete m_overlay;
    if (m_name == "project") {
        delete m_effectView;
        delete m_effectScene;
    }
    delete m_monitorRefresh;
    delete render;
}

QString Monitor::name() const
{
    return m_name;
}

void Monitor::setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu)
{
    m_contextMenu = new QMenu(this);
    m_contextMenu->addMenu(m_playMenu);
    if (goMenu) m_contextMenu->addMenu(goMenu);
    if (markerMenu) m_contextMenu->addMenu(markerMenu);

    m_playMenu->addAction(playZone);
    m_playMenu->addAction(loopZone);

    //TODO: add save zone to timeline monitor when fixed
    if (m_name == "clip") {
        m_contextMenu->addMenu(m_markerMenu);
        m_contextMenu->addAction(KIcon("document-save"), i18n("Save zone"), this, SLOT(slotSaveZone()));
    }
    QAction *extractFrame = m_configMenu->addAction(KIcon("document-new"), i18n("Extract frame"), this, SLOT(slotExtractCurrentFrame()));
    m_contextMenu->addAction(extractFrame);

    if (m_name != "clip") {
#ifndef Q_WS_MAC
        QAction *splitView = m_contextMenu->addAction(KIcon("view-split-left-right"), i18n("Split view"), render, SLOT(slotSplitView(bool)));
        splitView->setCheckable(true);
        m_configMenu->addAction(splitView);
#endif
    } else {
        QAction *setThumbFrame = m_contextMenu->addAction(KIcon("document-new"), i18n("Set current image as thumbnail"), this, SLOT(slotSetThumbFrame()));
        m_configMenu->addAction(setThumbFrame);
    }

    QAction *showTips = m_contextMenu->addAction(KIcon("help-hint"), i18n("Monitor overlay infos"));
    showTips->setCheckable(true);
    connect(showTips, SIGNAL(toggled(bool)), this, SLOT(slotSwitchMonitorInfo(bool)));
    showTips->setChecked(KdenliveSettings::displayMonitorInfo());

    QAction *dropFrames = m_contextMenu->addAction(KIcon(), i18n("Real time (drop frames)"));
    dropFrames->setCheckable(true);
    dropFrames->setChecked(true);
    connect(dropFrames, SIGNAL(toggled(bool)), this, SLOT(slotSwitchDropFrames(bool)));

    m_configMenu->addAction(showTips);
    m_configMenu->addAction(dropFrames);

}

void Monitor::slotGoToMarker(QAction *action)
{
    int pos = action->data().toInt();
    slotSeek(pos);
}

void Monitor::slotSetSizeOneToOne()
{
    QRect r = QApplication::desktop()->screenGeometry();
    const int maxWidth = r.width() - 20;
    const int maxHeight = r.height() - 20;
    int width = render->renderWidth();
    int height = render->renderHeight();
    kDebug() << "// render info: " << width << "x" << height;
    while (width >= maxWidth || height >= maxHeight) {
        width = width * 0.8;
        height = height * 0.8;
    }
    kDebug() << "// MONITOR; set SIZE: " << width << ", " << height;
    m_ui.video_frame->setFixedSize(width, height);
    updateGeometry();
    adjustSize();
    //m_ui.video_frame->setMinimumSize(0, 0);
    emit adjustMonitorSize();
}

void Monitor::slotSetSizeOneToTwo()
{
    QRect r = QApplication::desktop()->screenGeometry();
    const int maxWidth = r.width() - 20;
    const int maxHeight = r.height() - 20;
    int width = render->renderWidth() / 2;
    int height = render->renderHeight() / 2;
    kDebug() << "// render info: " << width << "x" << height;
    while (width >= maxWidth || height >= maxHeight) {
        width = width * 0.8;
        height = height * 0.8;
    }
    kDebug() << "// MONITOR; set SIZE: " << width << ", " << height;
    m_ui.video_frame->setFixedSize(width, height);
    updateGeometry();
    adjustSize();
    //m_ui.video_frame->setMinimumSize(0, 0);
    emit adjustMonitorSize();
}

void Monitor::resetSize()
{
    m_ui.video_frame->setMinimumSize(0, 0);
}

DocClipBase *Monitor::activeClip()
{
    return m_currentClip;
}

void Monitor::updateMarkers(DocClipBase *source)
{
    if (source == m_currentClip && source != NULL) {
        m_markerMenu->clear();
        QList <CommentedTime> markers = m_currentClip->commentedSnapMarkers();
        if (!markers.isEmpty()) {
            QList <int> marks;
            for (int i = 0; i < markers.count(); i++) {
                int pos = (int) markers.at(i).time().frames(m_monitorManager->timecode().fps());
                marks.append(pos);
                QString position = m_monitorManager->timecode().getTimecode(markers.at(i).time()) + ' ' + markers.at(i).comment();
                QAction *go = m_markerMenu->addAction(position);
                go->setData(pos);
            }
            m_ruler->setMarkers(marks);
        } else m_ruler->setMarkers(QList <int>());
        m_markerMenu->setEnabled(!m_markerMenu->isEmpty());
    }
}

void Monitor::slotSeekToPreviousSnap()
{
    if (m_currentClip) slotSeek(getSnapForPos(true).frames(m_monitorManager->timecode().fps()));
}

void Monitor::slotSeekToNextSnap()
{
    if (m_currentClip) slotSeek(getSnapForPos(false).frames(m_monitorManager->timecode().fps()));
}

GenTime Monitor::position()
{
    return render->seekPosition();
}

GenTime Monitor::getSnapForPos(bool previous)
{
    QList <GenTime> snaps;
    QList < GenTime > markers = m_currentClip->snapMarkers();
    for (int i = 0; i < markers.size(); ++i) {
        GenTime t = markers.at(i);
        snaps.append(t);
    }
    QPoint zone = m_ruler->zone();
    snaps.append(GenTime(zone.x(), m_monitorManager->timecode().fps()));
    snaps.append(GenTime(zone.y(), m_monitorManager->timecode().fps()));
    snaps.append(GenTime());
    snaps.append(m_currentClip->duration());
    qSort(snaps);

    const GenTime pos = render->seekPosition();
    for (int i = 0; i < snaps.size(); ++i) {
        if (previous && snaps.at(i) >= pos) {
            if (i == 0) i = 1;
            return snaps.at(i - 1);
        } else if (!previous && snaps.at(i) > pos) {
            return snaps.at(i);
        }
    }
    return GenTime();
}

void Monitor::slotZoneMoved(int start, int end)
{
    m_ruler->setZone(start, end);
    checkOverlay();
    setClipZone(m_ruler->zone());
}

void Monitor::slotSetZoneStart()
{
    m_ruler->setZone(render->seekFramePosition(), -1);
    emit zoneUpdated(m_ruler->zone());
    checkOverlay();
    setClipZone(m_ruler->zone());
}

void Monitor::slotSetZoneEnd()
{
    m_ruler->setZone(-1, render->seekFramePosition());
    emit zoneUpdated(m_ruler->zone());
    checkOverlay();
    setClipZone(m_ruler->zone());
}

// virtual
void Monitor::mousePressEvent(QMouseEvent * event)
{
    if (event->button() != Qt::RightButton) {
        if (m_ui.video_frame->underMouse()) {
            m_dragStarted = true;
            m_DragStartPosition = event->pos();
        }
    } else m_contextMenu->popup(event->globalPos());
}

// virtual
void Monitor::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_dragStarted) {
        if (m_ui.video_frame->underMouse()) {
            if (isActive()) slotPlay();
            else activateMonitor();
        } else QWidget::mouseReleaseEvent(event);
        m_dragStarted = false;
    }
}

// virtual
void Monitor::mouseMoveEvent(QMouseEvent *event)
{
    // kDebug() << "// DRAG STARTED, MOUSE MOVED: ";
    if (!m_dragStarted || m_currentClip == NULL) return;

    if ((event->pos() - m_DragStartPosition).manhattanLength()
            < QApplication::startDragDistance())
        return;

    {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        QStringList list;
        list.append(m_currentClip->getId());
        QPoint p = m_ruler->zone();
        list.append(QString::number(p.x()));
        list.append(QString::number(p.y()));
        QByteArray data;
        data.append(list.join(";").toUtf8());
        mimeData->setData("kdenlive/clip", data);
        drag->setMimeData(mimeData);
        QPixmap pix = m_currentClip->thumbnail();
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(0, 50));
        drag->start(Qt::MoveAction);

        //Qt::DropAction dropAction;
        //dropAction = drag->start(Qt::CopyAction | Qt::MoveAction);

        //Qt::DropAction dropAction = drag->exec();

    }
    //event->accept();
}

/*void Monitor::dragMoveEvent(QDragMoveEvent * event) {
    event->setDropAction(Qt::IgnoreAction);
    event->setDropAction(Qt::MoveAction);
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

Qt::DropActions Monitor::supportedDropActions() const {
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}*/

QStringList Monitor::mimeTypes() const
{
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("kdenlive/clip");
    return qstrList;
}

// virtual
void Monitor::wheelEvent(QWheelEvent * event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        int delta = m_monitorManager->timecode().fps();
        if (event->delta() > 0) delta = 0 - delta;
        slotSeek(render->seekFramePosition() - delta);
    } else {
        if (event->delta() >= 0) slotForwardOneFrame();
        else slotRewindOneFrame();
    }
}

void Monitor::slotSetThumbFrame()
{
    if (m_currentClip == NULL) {
        return;
    }
    m_currentClip->setClipThumbFrame((uint) render->seekFramePosition());
    emit refreshClipThumbnail(m_currentClip->getId());
}

void Monitor::slotExtractCurrentFrame()
{
    QImage frame = render->extractFrame(render->seekFramePosition());
    KFileDialog *fs = new KFileDialog(KUrl(), "image/png", this);
    fs->setOperationMode(KFileDialog::Saving);
    fs->setMode(KFile::File);
#if KDE_IS_VERSION(4,2,0)
    fs->setConfirmOverwrite(true);
#endif
    fs->setKeepLocation(true);
    fs->exec();
    QString path = fs->selectedFile();
    delete fs;
    if (!path.isEmpty()) {
        frame.save(path);
    }
}

bool Monitor::isActive() const
{
    return m_isActive;
}

void Monitor::activateMonitor()
{
    if (!m_isActive) {
        m_monitorManager->slotSwitchMonitors(m_name == "clip");
    }
}

void Monitor::setTimePos(const QString &pos)
{
    m_timePos->setValue(pos);
    slotSeek();
}

void Monitor::slotSeek()
{
    slotSeek(m_timePos->getValue());
}

void Monitor::slotSeek(int pos)
{
    activateMonitor();
    if (render == NULL) return;
    render->seekToFrame(pos);
}

void Monitor::checkOverlay()
{
    if (m_overlay == NULL) return;
    int pos = render->seekFramePosition();
    QPoint zone = m_ruler->zone();
    if (pos == zone.x())
        m_overlay->setOverlayText(i18n("In Point"));
    else if (pos == zone.y())
        m_overlay->setOverlayText(i18n("Out Point"));
    else {
        if (m_currentClip) {
            QString markerComment = m_currentClip->markerComment(GenTime(pos, m_monitorManager->timecode().fps()));
            if (markerComment.isEmpty())
                m_overlay->setHidden(true);
            else
                m_overlay->setOverlayText(markerComment, false);
        } else m_overlay->setHidden(true);
    }
}

void Monitor::slotStart()
{
    activateMonitor();
    render->play(0);
    render->seekToFrame(0);
}

void Monitor::slotEnd()
{
    activateMonitor();
    render->play(0);
    render->seekToFrame(render->getLength());
}

void Monitor::slotZoneStart()
{
    activateMonitor();
    render->play(0);
    render->seekToFrame(m_ruler->zone().x());
}

void Monitor::slotZoneEnd()
{
    activateMonitor();
    render->play(0);
    render->seekToFrame(m_ruler->zone().y());
}

void Monitor::slotRewind(double speed)
{
    activateMonitor();
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed >= 0) render->play(-2);
        else render->play(currentspeed * 2);
    } else render->play(speed);
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotForward(double speed)
{
    activateMonitor();
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed <= 1) render->play(2);
        else render->play(currentspeed * 2);
    } else render->play(speed);
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotRewindOneFrame(int diff)
{
    activateMonitor();
    render->play(0);
    render->seekToFrameDiff(-diff);
}

void Monitor::slotForwardOneFrame(int diff)
{
    activateMonitor();
    render->play(0);
    render->seekToFrameDiff(diff);
}

void Monitor::seekCursor(int pos)
{
    activateMonitor();
    if (m_ruler->slotNewValue(pos)) {
        checkOverlay();
        m_timePos->setValue(pos);
    }
}

void Monitor::rendererStopped(int pos)
{
    if (m_ruler->slotNewValue(pos)) {
        checkOverlay();
        m_timePos->setValue(pos);
    }
    disconnect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlay()));
    m_playAction->setChecked(false);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlay()));
    m_playAction->setIcon(m_playIcon);
}

void Monitor::initMonitor()
{
    kDebug() << "/////// INITING MONITOR, ID: " << m_ui.video_frame->winId();
}

// virtual
/*void Monitor::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    adjustRulerSize(-1);
    if (render && m_isActive) render->doRefresh();
    //
}*/

void Monitor::adjustRulerSize(int length)
{
    if (length > 0) m_length = length;
    m_ruler->adjustScale(m_length);
    if (m_currentClip != NULL) {
        QPoint zone = m_currentClip->zone();
        m_ruler->setZone(zone.x(), zone.y());
    }
}

void Monitor::stop()
{
    m_isActive = false;
    disconnect(render, SIGNAL(rendererPosition(int)), this, SLOT(seekCursor(int)));
    if (render) render->stop();
}

void Monitor::start()
{
    m_isActive = true;
    if (render) render->start();
    connect(render, SIGNAL(rendererPosition(int)), this, SLOT(seekCursor(int)));
}

void Monitor::refreshMonitor(bool visible)
{
    if (visible && render && !m_isActive) {
        activateMonitor();
        render->doRefresh(); //askForRefresh();
    }
}

void Monitor::pause()
{
    if (render == NULL) return;
    activateMonitor();
    render->pause();
    //m_playAction->setChecked(true);
    //m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotPlay()
{
    if (render == NULL) return;
    activateMonitor();
    if (render->playSpeed() == 0) {
        m_playAction->setChecked(true);
        m_playAction->setIcon(m_pauseIcon);
    } else {
        m_playAction->setChecked(false);
        m_playAction->setIcon(m_playIcon);
    }
    render->switchPlay();
}

void Monitor::slotPlayZone()
{
    if (render == NULL) return;
    activateMonitor();
    QPoint p = m_ruler->zone();
    render->playZone(GenTime(p.x(), m_monitorManager->timecode().fps()), GenTime(p.y(), m_monitorManager->timecode().fps()));
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotLoopZone()
{
    if (render == NULL) return;
    activateMonitor();
    QPoint p = m_ruler->zone();
    render->loopZone(GenTime(p.x(), m_monitorManager->timecode().fps()), GenTime(p.y(), m_monitorManager->timecode().fps()));
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotSetXml(DocClipBase *clip, QPoint zone, const int position)
{
    if (render == NULL) return;
    if (clip == NULL && m_currentClip != NULL) {
        m_currentClip = NULL;
        m_length = -1;
        render->setProducer(NULL, -1);
        return;
    }
    if (m_currentClip != NULL) activateMonitor();
    if (clip != m_currentClip) {
        m_currentClip = clip;
        updateMarkers(clip);
        if (render->setProducer(clip->producer(), position) == -1) {
            // MLT CONSUMER is broken
            kDebug(QtWarningMsg) << "ERROR, Cannot start monitor";
        }
    } else if (position != -1) render->seek(GenTime(position, m_monitorManager->timecode().fps()));
    if (!zone.isNull()) {
        m_ruler->setZone(zone.x(), zone.y());
        render->seek(GenTime(zone.x(), m_monitorManager->timecode().fps()));
    }
}

void Monitor::slotOpenFile(const QString &file)
{
    if (render == NULL) return;
    activateMonitor();
    QDomDocument doc;
    QDomElement mlt = doc.createElement("mlt");
    doc.appendChild(mlt);
    QDomElement prod = doc.createElement("producer");
    mlt.appendChild(prod);
    prod.setAttribute("mlt_service", "avformat");
    prod.setAttribute("resource", file);
    render->setSceneList(doc, 0);
}

void Monitor::slotSaveZone()
{
    if (render == NULL) return;
    emit saveZone(render, m_ruler->zone());

    //render->setSceneList(doc, 0);
}

void Monitor::resetProfile(const QString profile)
{
    m_timePos->updateTimeCode(m_monitorManager->timecode());
    if (render == NULL) return;
    render->resetProfile(profile);
    if (m_effectScene) {
        m_effectScene->resetProfile();
    }
}

void Monitor::saveSceneList(QString path, QDomElement info)
{
    if (render == NULL) return;
    render->saveSceneList(path, info);
}

const QString Monitor::sceneList()
{
    if (render == NULL) return QString();
    return render->sceneList();
}

void Monitor::setClipZone(QPoint pos)
{
    if (m_currentClip == NULL) return;
    m_currentClip->setZone(pos);
}

void Monitor::slotSwitchDropFrames(bool show)
{
    render->setDropFrames(show);
}

void Monitor::slotSwitchMonitorInfo(bool show)
{
    KdenliveSettings::setDisplayMonitorInfo(show);
    if (show) {
        if (m_overlay) return;
#ifndef Q_WS_MAC
        m_overlay = new Overlay(m_monitorRefresh);
        m_overlay->raise();
        m_overlay->setHidden(true);
#else
        m_overlay = new Overlay(m_glWidget);
#endif
    } else {
        delete m_overlay;
        m_overlay = NULL;
    }
}

void Monitor::updateTimecodeFormat()
{
    m_timePos->slotUpdateTimeCodeFormat();
}

QStringList Monitor::getZoneInfo() const
{
    QStringList result;
    if (m_currentClip == NULL) return result;
    result << m_currentClip->getId();
    QPoint zone = m_ruler->zone();
    result << QString::number(zone.x()) << QString::number(zone.y());
    return result;
}

void Monitor::slotEffectScene(bool show)
{
    if (m_name == "project") {
#ifdef Q_WS_MAC
        m_glWidget->setVisible(!show);
#else
        m_monitorRefresh->setVisible(!show);
#endif
        m_effectView->setVisible(show);
        if (show) {
            render->doRefresh();
            m_effectScene->slotZoomFit();
        }
    }
}

MonitorScene * Monitor::getEffectScene()
{
    return m_effectScene;
}

MonitorRefresh::MonitorRefresh(QWidget* parent) : \
        QWidget(parent),
        m_renderer(NULL)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    //setAttribute(Qt::WA_NoSystemBackground);
}

void MonitorRefresh::setRenderer(Render* render)
{
    m_renderer = render;
}

void MonitorRefresh::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_renderer) m_renderer->doRefresh();
}

Overlay::Overlay(QWidget* parent) :
        QLabel(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    //setAttribute(Qt::WA_OpaquePaintEvent);
    //setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
}

void Overlay::setOverlayText(const QString &text, bool isZone)
{
    setHidden(true);
    m_isZone = isZone;
    QPalette p;
    p.setColor(QPalette::Text, Qt::white);
    if (m_isZone) p.setColor(QPalette::Base, QColor(200, 0, 0));
    else p.setColor(QPalette::Base, QColor(0, 0, 200));
    setPalette(p);
    setText(' ' + text + ' ');
    setHidden(false);
    update();
}

#include "monitor.moc"
