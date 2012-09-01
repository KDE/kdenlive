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
#include "smallruler.h"
#include "docclipbase.h"
#include "abstractclipitem.h"
#include "monitorscene.h"
#include "monitoreditwidget.h"
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


#define SEEK_INACTIVE (-1)


Monitor::Monitor(Kdenlive::MONITORID id, MonitorManager *manager, QString profile, QWidget *parent) :
    AbstractMonitor(id, manager, parent),
    render(NULL),
    m_currentClip(NULL),
    m_overlay(NULL),
    m_scale(1),
    m_length(0),
    m_dragStarted(false),
    m_contextMenu(NULL),
    m_effectWidget(NULL),
    m_selectedClip(NULL),
    m_loopClipTransition(true),
    m_editMarker(NULL)
{
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Video widget holder
    layout->addWidget(videoBox, 10);
    layout->addStretch();

    // Get base size for icons
    int s = style()->pixelMetric(QStyle::PM_SmallIconSize);


    // Tool bar buttons
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(s, s));

    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");


    if (id != Kdenlive::dvdMonitor) {
        m_toolbar->addAction(KIcon("kdenlive-zone-start"), i18n("Set zone start"), this, SLOT(slotSetZoneStart()));
        m_toolbar->addAction(KIcon("kdenlive-zone-end"), i18n("Set zone end"), this, SLOT(slotSetZoneEnd()));
    }

    m_toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"), this, SLOT(slotRewind()));
    //m_toolbar->addAction(KIcon("media-skip-backward"), i18n("Rewind 1 frame"), this, SLOT(slotRewindOneFrame()));

    QToolButton *playButton = new QToolButton(m_toolbar);
    m_playMenu = new QMenu(i18n("Play..."), this);
    m_playAction = m_playMenu->addAction(m_playIcon, i18n("Play"));
    //m_playAction->setCheckable(true);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlay()));

    playButton->setMenu(m_playMenu);
    playButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(playButton);

    //m_toolbar->addAction(KIcon("media-skip-forward"), i18n("Forward 1 frame"), this, SLOT(slotForwardOneFrame()));
    m_toolbar->addAction(KIcon("media-seek-forward"), i18n("Forward"), this, SLOT(slotForward()));

    playButton->setDefaultAction(m_playAction);

    if (id != Kdenlive::dvdMonitor) {
        QToolButton *configButton = new QToolButton(m_toolbar);
        m_configMenu = new QMenu(i18n("Misc..."), this);
        configButton->setIcon(KIcon("system-run"));
        configButton->setMenu(m_configMenu);
        configButton->setPopupMode(QToolButton::QToolButton::InstantPopup);
        m_toolbar->addWidget(configButton);

        if (id == Kdenlive::clipMonitor) {
            m_markerMenu = new QMenu(i18n("Go to marker..."), this);
            m_markerMenu->setEnabled(false);
            m_configMenu->addMenu(m_markerMenu);
            connect(m_markerMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotGoToMarker(QAction *)));
        }
        m_configMenu->addAction(KIcon("transform-scale"), i18n("Resize (100%)"), this, SLOT(slotSetSizeOneToOne()));
        m_configMenu->addAction(KIcon("transform-scale"), i18n("Resize (50%)"), this, SLOT(slotSetSizeOneToTwo()));
    }

    // Create Volume slider popup
    m_volumePopup = new QFrame(this, Qt::Popup);
    QVBoxLayout *poplayout = new QVBoxLayout;
    poplayout->setContentsMargins(0, 0, 0, 0);
    m_audioSlider = new QSlider(Qt::Vertical);
    m_audioSlider->setRange(0, 100);
    poplayout->addWidget(m_audioSlider);
    m_volumePopup->setLayout(poplayout);
    KIcon icon;
    if (KdenliveSettings::volume() == 0) icon = KIcon("audio-volume-muted");
    else icon = KIcon("audio-volume-medium");

    m_volumeWidget = m_toolbar->widgetForAction(m_toolbar->addAction(icon, i18n("Audio volume"), this, SLOT(slotShowVolume())));

    // we need to show / hide the popup once so that it's geometry can be calculated in slotShowVolume
    m_volumePopup->show();
    m_volumePopup->hide();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setLayout(layout);
    setMinimumHeight(200);

    if (profile.isEmpty()) profile = KdenliveSettings::current_profile();

    bool monitorCreated = false;
#ifdef Q_WS_MAC
    createOpenGlWidget(videoBox, profile);
    monitorCreated = true;
    //m_glWidget->setFixedSize(width, height);
#elif defined(USE_OPENGL)
    if (KdenliveSettings::openglmonitors()) {
        monitorCreated = createOpenGlWidget(videoBox, profile);
    }
#endif
    if (!monitorCreated) {
	createVideoSurface();
        render = new Render(m_id, (int) videoSurface->winId(), profile, this);
	connect(videoSurface, SIGNAL(refreshMonitor()), render, SLOT(doRefresh()));
    }
#ifdef USE_OPENGL
    else if (m_glWidget) {
	QVBoxLayout *lay = new QVBoxLayout;
	lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_glWidget);
        videoBox->setLayout(lay);
    }
#endif

    // Monitor ruler
    m_ruler = new SmallRuler(m_monitorManager, render);
    if (id == Kdenlive::dvdMonitor) m_ruler->setZone(-3, -2);
    layout->addWidget(m_ruler);
    
    connect(m_audioSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSetVolume(int)));
    connect(m_ruler, SIGNAL(seekRenderer(int)), this, SLOT(slotSeek(int)));
    connect(render, SIGNAL(durationChanged(int)), this, SLOT(adjustRulerSize(int)));
    connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));
    connect(render, SIGNAL(rendererPosition(int)), this, SLOT(seekCursor(int)));

    if (id != Kdenlive::clipMonitor) {
        connect(render, SIGNAL(rendererPosition(int)), this, SIGNAL(renderPosition(int)));
        connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SIGNAL(zoneUpdated(QPoint)));
    } else {
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SLOT(setClipZone(QPoint)));
    }

    if (videoSurface) videoSurface->show();

    if (id == Kdenlive::projectMonitor) {
        m_effectWidget = new MonitorEditWidget(render, videoBox);
	connect(m_effectWidget, SIGNAL(showEdit(bool, bool)), this, SLOT(slotShowEffectScene(bool, bool)));
        m_toolbar->addAction(m_effectWidget->getVisibilityAction());
        videoBox->layout()->addWidget(m_effectWidget);
        m_effectWidget->hide();
    }

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
    m_timePos = new TimecodeDisplay(m_monitorManager->timecode(), this);
    m_toolbar->addWidget(m_timePos);
    connect(m_timePos, SIGNAL(editingFinished()), this, SLOT(slotSeek()));
    m_toolbar->setMaximumHeight(s * 1.5);
    layout->addWidget(m_toolbar);
}

Monitor::~Monitor()
{
    delete m_ruler;
    delete m_timePos;
    delete m_overlay;
    if (m_effectWidget)
        delete m_effectWidget;
    delete render;
}

QWidget *Monitor::container()
{
    return videoBox;
}

#ifdef USE_OPENGL
bool Monitor::createOpenGlWidget(QWidget *parent, const QString profile)
{
    render = new Render(id(), 0, profile, this);
    m_glWidget = new VideoGLWidget(parent);
    if (m_glWidget == NULL) {
        // Creation failed, we are in trouble...
        return false;
    }
    m_glWidget->setImageAspectRatio(render->dar());
    m_glWidget->setBackgroundColor(KdenliveSettings::window_background());
    connect(render, SIGNAL(showImageSignal(QImage)), m_glWidget, SLOT(showImage(QImage)));
    return true;
}
#endif

void Monitor::setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu, QAction *loopClip)
{
    m_contextMenu = new QMenu(this);
    m_contextMenu->addMenu(m_playMenu);
    if (goMenu)
        m_contextMenu->addMenu(goMenu);
    if (markerMenu) {
        m_contextMenu->addMenu(markerMenu);
        QList <QAction *>list = markerMenu->actions();
        for (int i = 0; i < list.count(); i++) {
            if (list.at(i)->data().toString() == "edit_marker") {
                m_editMarker = list.at(i);
                break;
            }
        }
    }

    m_playMenu->addAction(playZone);
    m_playMenu->addAction(loopZone);
    if (loopClip) {
        m_loopClipAction = loopClip;
        m_playMenu->addAction(loopClip);
    }

    //TODO: add save zone to timeline monitor when fixed
    if (m_id == Kdenlive::clipMonitor) {
        m_contextMenu->addMenu(m_markerMenu);
        m_contextMenu->addAction(KIcon("document-save"), i18n("Save zone"), this, SLOT(slotSaveZone()));
        QAction *extractZone = m_configMenu->addAction(KIcon("document-new"), i18n("Extract Zone"), this, SLOT(slotExtractCurrentZone()));
        m_contextMenu->addAction(extractZone);
    }
    QAction *extractFrame = m_configMenu->addAction(KIcon("document-new"), i18n("Extract frame"), this, SLOT(slotExtractCurrentFrame()));
    m_contextMenu->addAction(extractFrame);

    if (m_id != Kdenlive::clipMonitor) {
        QAction *splitView = m_contextMenu->addAction(KIcon("view-split-left-right"), i18n("Split view"), render, SLOT(slotSplitView(bool)));
        splitView->setCheckable(true);
        m_configMenu->addAction(splitView);
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
    videoBox->setFixedSize(width, height);
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
    videoBox->setFixedSize(width, height);
    updateGeometry();
    adjustSize();
    //m_ui.video_frame->setMinimumSize(0, 0);
    emit adjustMonitorSize();
}

void Monitor::resetSize()
{
    videoBox->setMinimumSize(0, 0);
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
    m_ruler->setZoneStart();
    emit zoneUpdated(m_ruler->zone());
    checkOverlay();
    setClipZone(m_ruler->zone());
}

void Monitor::slotSetZoneEnd()
{
    m_ruler->setZoneEnd();
    emit zoneUpdated(m_ruler->zone());
    checkOverlay();
    setClipZone(m_ruler->zone());
}

// virtual
void Monitor::mousePressEvent(QMouseEvent * event)
{
    if (event->button() != Qt::RightButton) {
        if (videoBox->geometry().contains(event->pos()) && (!m_overlay || !m_overlay->underMouse())) {
            m_dragStarted = true;
            m_DragStartPosition = event->pos();
        }
    } else if (m_contextMenu && (!m_effectWidget || !m_effectWidget->isVisible())) {
        m_contextMenu->popup(event->globalPos());
    }
}

void Monitor::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    if (render && isVisible() && isActive()) render->doRefresh();
}

void Monitor::slotSwitchFullScreen()
{
    videoBox->switchFullScreen();
}

// virtual
void Monitor::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_dragStarted && event->button() != Qt::RightButton) {
        if (videoBox->geometry().contains(event->pos()) && (!m_effectWidget || !m_effectWidget->isVisible())) {
            if (isActive()) slotPlay();
            else slotActivateMonitor();
        } //else event->ignore(); //QWidget::mouseReleaseEvent(event);
        m_dragStarted = false;
    }
}

// virtual
void Monitor::mouseMoveEvent(QMouseEvent *event)
{
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
        /*QPixmap pix = m_currentClip->thumbnail();
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(0, 50));*/
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
    slotMouseSeek(event->delta(), event->modifiers() == Qt::ControlModifier);
    event->accept();
}

void Monitor::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (!KdenliveSettings::openglmonitors()) {
        videoBox->switchFullScreen();
        event->accept();
    }
}

void Monitor::slotMouseSeek(int eventDelta, bool fast)
{
    if (fast) {
        int delta = m_monitorManager->timecode().fps();
        if (eventDelta > 0) delta = 0 - delta;
	if (render->requestedSeekPosition != SEEK_INACTIVE)
	    slotSeek(render->requestedSeekPosition - delta);
	else slotSeek(render->seekFramePosition() - delta);
    } else {
        if (eventDelta >= 0) slotForwardOneFrame();
        else slotRewindOneFrame();
    }
}

void Monitor::slotSetThumbFrame()
{
    if (m_currentClip == NULL) {
        return;
    }
    m_currentClip->setClipThumbFrame((uint) render->seekFramePosition());
    emit refreshClipThumbnail(m_currentClip->getId(), true);
}

void Monitor::slotExtractCurrentZone()
{
    if (m_currentClip == NULL) return;
    emit extractZone(m_currentClip->getId(), m_ruler->zone());
}

void Monitor::slotExtractCurrentFrame()
{
    QImage frame;
    // check if we are using a proxy
    if (m_currentClip && !m_currentClip->getProperty("proxy").isEmpty() && m_currentClip->getProperty("proxy") != "-") {
        // using proxy, use original clip url to get frame
        frame = render->extractFrame(render->seekFramePosition(), m_currentClip->fileURL().path());
    }
    else frame = render->extractFrame(render->seekFramePosition());
    QPointer<KFileDialog> fs = new KFileDialog(KUrl(), "image/png", this);
    fs->setOperationMode(KFileDialog::Saving);
    fs->setMode(KFile::File);
    fs->setConfirmOverwrite(true);
    fs->setKeepLocation(true);
    fs->exec();
    QString path;
    if (fs) path = fs->selectedFile();
    delete fs;
    if (!path.isEmpty()) {
        frame.save(path);
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
    if (render == NULL) return;
    slotActivateMonitor();
    render->seekToFrame(pos);
    m_ruler->update();
}

void Monitor::checkOverlay()
{
    if (m_overlay == NULL) return;
    QString overlayText;
    int pos = render->seekFramePosition();
    QPoint zone = m_ruler->zone();
    if (pos == zone.x())
        overlayText = i18n("In Point");
    else if (pos == zone.y())
        overlayText = i18n("Out Point");
    else {
        if (m_currentClip) {
            overlayText = m_currentClip->markerComment(GenTime(pos, m_monitorManager->timecode().fps()));
	    if (!overlayText.isEmpty()) {
		m_overlay->setOverlayText(overlayText, false);
		return;
	    }
	}
    }
    if (m_overlay->isVisible() && overlayText.isEmpty()) m_overlay->setOverlayText(QString(), false);
    else m_overlay->setOverlayText(overlayText);
}

void Monitor::slotStart()
{
    slotActivateMonitor();
    render->play(0);
    render->seekToFrame(0);
}

void Monitor::slotEnd()
{
    slotActivateMonitor();
    render->play(0);
    render->seekToFrame(render->getLength());
}

void Monitor::slotZoneStart()
{
    slotActivateMonitor();
    render->play(0);
    render->seekToFrame(m_ruler->zone().x());
}

void Monitor::slotZoneEnd()
{
    slotActivateMonitor();
    render->play(0);
    render->seekToFrame(m_ruler->zone().y());
}

void Monitor::slotRewind(double speed)
{
    slotActivateMonitor();
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed >= 0) render->play(-2);
        else render->play(currentspeed * 2);
    } else render->play(speed);
    //m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotForward(double speed)
{
    slotActivateMonitor();
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed <= 1) render->play(2);
        else render->play(currentspeed * 2);
    } else render->play(speed);
    //m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotRewindOneFrame(int diff)
{
    slotActivateMonitor();
    render->play(0);
    render->seekToFrameDiff(-diff);
    m_ruler->update();
}

void Monitor::slotForwardOneFrame(int diff)
{
    slotActivateMonitor();
    render->play(0);
    render->seekToFrameDiff(diff);
    m_ruler->update();
}

void Monitor::seekCursor(int pos)
{
    //slotActivateMonitor();
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
    m_playAction->setIcon(m_playIcon);
}

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
    if (render) render->stop();
}

void Monitor::start()
{
    if (!isVisible() || !isActive()) return;
    if (render) render->doRefresh();// start();
}

void Monitor::refreshMonitor(bool visible)
{
    if (visible && render) {
        if (!slotActivateMonitor()) {
            // the monitor was already active, simply refreshClipThumbnail
            render->doRefresh();
        }
    }
}

void Monitor::refreshMonitor()
{
    if (isActive()) {
        render->doRefresh();
    }
}

void Monitor::pause()
{
    if (render == NULL) return;
    slotActivateMonitor();
    render->pause();
    //m_playAction->setChecked(true);
    m_playAction->setIcon(m_playIcon);
}

void Monitor::unpause()
{
}

void Monitor::slotPlay()
{
    if (render == NULL) return;
    slotActivateMonitor();
    if (render->playSpeed() == 0.0) {
        m_playAction->setIcon(m_pauseIcon);
        render->switchPlay(true);
    } else {
        m_playAction->setIcon(m_playIcon);
        render->switchPlay(false);
    }
}

void Monitor::slotPlayZone()
{
    if (render == NULL) return;
    slotActivateMonitor();
    QPoint p = m_ruler->zone();
    render->playZone(GenTime(p.x(), m_monitorManager->timecode().fps()), GenTime(p.y(), m_monitorManager->timecode().fps()));
    //m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotLoopZone()
{
    if (render == NULL) return;
    slotActivateMonitor();
    QPoint p = m_ruler->zone();
    render->loopZone(GenTime(p.x(), m_monitorManager->timecode().fps()), GenTime(p.y(), m_monitorManager->timecode().fps()));
    //m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotLoopClip()
{
    if (render == NULL || m_selectedClip == NULL)
        return;
    slotActivateMonitor();
    render->loopZone(m_selectedClip->startPos(), m_selectedClip->endPos());
    //m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::updateClipProducer(Mlt::Producer *prod)
{
    if (render == NULL) return;
   render->setProducer(prod, render->seekFramePosition());
}

void Monitor::slotSetClipProducer(DocClipBase *clip, QPoint zone, bool forceUpdate, int position)
{
    if (render == NULL) return;
    if (clip == NULL && m_currentClip != NULL) {
        kDebug()<<"// SETTING NULL CLIP MONITOR";
        m_currentClip = NULL;
        m_length = -1;
        render->setProducer(NULL, -1);
        return;
    }

    if (clip != m_currentClip || forceUpdate) {
        m_currentClip = clip;
        if (m_currentClip) slotActivateMonitor();
        updateMarkers(clip);
        Mlt::Producer *prod = NULL;
        if (clip) prod = clip->getCloneProducer();
        if (render->setProducer(prod, position) == -1) {
            // MLT CONSUMER is broken
            kDebug(QtWarningMsg) << "ERROR, Cannot start monitor";
        }
    } else {
        if (m_currentClip) {
            slotActivateMonitor();
            if (position == -1) position = render->seekFramePosition();
            render->seek(position);
        }
    }
    if (!zone.isNull()) {
        m_ruler->setZone(zone.x(), zone.y());
        render->seek(zone.x());
    }
}

void Monitor::slotOpenFile(const QString &file)
{
    if (render == NULL) return;
    slotActivateMonitor();
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
    emit saveZone(render, m_ruler->zone(), m_currentClip);

    //render->setSceneList(doc, 0);
}

void Monitor::resetProfile(const QString &profile)
{
    m_timePos->updateTimeCode(m_monitorManager->timecode());
    if (render == NULL) return;
    if (!render->hasProfile(profile)) {
        slotActivateMonitor();
        render->resetProfile(profile);
    }
    if (m_effectWidget)
        m_effectWidget->resetProfile(render);
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
        if (videoSurface == NULL) {
            // Using OpenGL display
#ifdef USE_OPENGL
            if (m_glWidget->layout()) delete m_glWidget->layout();
            m_overlay = new Overlay();
            connect(m_overlay, SIGNAL(editMarker()), this, SLOT(slotEditMarker()));
            QVBoxLayout *layout = new QVBoxLayout;
            layout->addStretch(10);
            layout->addWidget(m_overlay);
            m_glWidget->setLayout(layout);
#endif
        } else {
            if (videoSurface->layout()) delete videoSurface->layout();
            m_overlay = new Overlay();
            connect(m_overlay, SIGNAL(editMarker()), this, SLOT(slotEditMarker()));
            QVBoxLayout *layout = new QVBoxLayout;
            layout->addStretch(10);
            layout->addWidget(m_overlay);
            videoSurface->setLayout(layout);
            m_overlay->raise();
            m_overlay->setHidden(true);
        }
        checkOverlay();
    } else {
        delete m_overlay;
        m_overlay = NULL;
    }
}

void Monitor::slotEditMarker()
{
    if (m_editMarker) m_editMarker->trigger();
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

void Monitor::slotSetSelectedClip(AbstractClipItem* item)
{
    if (item) {
        m_loopClipAction->setEnabled(true);
        m_selectedClip = item;
    } else {
        m_loopClipAction->setEnabled(false);
    }
}

void Monitor::slotSetSelectedClip(ClipItem* item)
{
    if (item || (!item && !m_loopClipTransition)) {
        m_loopClipTransition = false;
        slotSetSelectedClip((AbstractClipItem*)item);
    }
}

void Monitor::slotSetSelectedClip(Transition* item)
{
    if (item || (!item && m_loopClipTransition)) {
        m_loopClipTransition = true;
        slotSetSelectedClip((AbstractClipItem*)item);
    }
}


void Monitor::slotShowEffectScene(bool show, bool manuallyTriggered)
{
    if (m_id == Kdenlive::projectMonitor) {
        if (!m_effectWidget->getVisibilityAction()->isChecked())
            show = false;
        if (m_effectWidget->isVisible() == show)
            return;
        setUpdatesEnabled(false);
        if (show) {
            if (videoSurface) {
                videoSurface->setVisible(false);
                // Preview is handeled internally through the Render::showFrame method
                render->disablePreview(true);
#ifdef USE_OPENGL
            } else {
                m_glWidget->setVisible(false);
#endif
            }
            m_effectWidget->setVisible(true);
            m_effectWidget->getScene()->slotZoomFit();
            emit requestFrameForAnalysis(true);
        } else {    
            m_effectWidget->setVisible(false);
            emit requestFrameForAnalysis(false);
            if (videoSurface) {
                videoSurface->setVisible(true);
                // Preview is handeled internally through the Render::showFrame method
                render->disablePreview(false);
            
#ifdef USE_OPENGL
            } else {
                m_glWidget->setVisible(true);
#endif
            }
        }
        if (!manuallyTriggered)
            m_effectWidget->showVisibilityButton(show);
        setUpdatesEnabled(true);
        videoBox->setEnabled(show);
        //render->doRefresh();
    }
}

MonitorEditWidget* Monitor::getEffectEdit()
{
    return m_effectWidget;
}

bool Monitor::effectSceneDisplayed()
{
    return m_effectWidget->isVisible();
}

void Monitor::slotSetVolume(int volume)
{
    KdenliveSettings::setVolume(volume);
    KIcon icon;
    if (volume == 0) icon = KIcon("audio-volume-muted");
    else icon = KIcon("audio-volume-medium");
    static_cast <QToolButton *>(m_volumeWidget)->setIcon(icon);
    render->slotSetVolume(volume);
}

void Monitor::slotShowVolume()
{
    m_volumePopup->move(mapToGlobal(m_toolbar->geometry().topLeft()) + QPoint(mapToParent(m_volumeWidget->geometry().bottomLeft()).x(), -m_volumePopup->height()));
    int vol = render->volume();
    // Disable widget if we cannot get the volume
    m_volumePopup->setEnabled(vol != -1);
    m_audioSlider->blockSignals(true);
    m_audioSlider->setValue(vol);
    m_audioSlider->blockSignals(false);
    m_volumePopup->show();
}

AbstractRender *Monitor::abstractRender()
{
    return render;
}

void Monitor::reloadProducer(const QString &id)
{
    if (!m_currentClip) return;
    if (m_currentClip->getId() == id)
        slotSetClipProducer(m_currentClip, m_currentClip->zone(), true);
}

void Monitor::setPalette ( const QPalette & p)
{
    QWidget::setPalette(p);
    if (m_ruler) m_ruler->updatePalette();
    
}

Overlay::Overlay(QWidget* parent) :
    QLabel(parent)
{
    //setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setCursor(Qt::PointingHandCursor);
}

// virtual
void Overlay::mouseReleaseEvent ( QMouseEvent * event )
{
    event->ignore();
}

// virtual
void Overlay::mousePressEvent( QMouseEvent * event )
{
    event->ignore();
}

// virtual
void Overlay::mouseDoubleClickEvent ( QMouseEvent * event )
{
    emit editMarker();
    event->ignore();
}

void Overlay::setOverlayText(const QString &text, bool isZone)
{
    if (text.isEmpty()) {
	QPalette p;
	p.setColor(QPalette::Base, KdenliveSettings::window_background());
	setPalette(p);
	setText(QString());
	repaint();
	setHidden(true);
	return;
    }
    setHidden(true);
    QPalette p;
    p.setColor(QPalette::Text, Qt::white);
    if (isZone) p.setColor(QPalette::Base, QColor(200, 0, 0));
    else p.setColor(QPalette::Base, QColor(0, 0, 200));
    setPalette(p);
    setText(' ' + text + ' ');
    setHidden(false);
}


#include "monitor.moc"
