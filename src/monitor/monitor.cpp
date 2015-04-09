
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
#include "monitorscene.h"
#include "glwidget.h"
#include "smallruler.h"
#include "mltcontroller/clipcontroller.h"
#include "kdenlivesettings.h"
#include "timeline/abstractclipitem.h"
#include "twostateaction.h"
#include "doc/kthumb.h"

#include "klocalizedstring.h"
#include <KRecentDirs>
#include <KMessageBox>

#include <QDebug>
#include <QMouseEvent>
#include <QMenu>
#include <QToolButton>
#include <QToolBar>
#include <QDesktopWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QDrag>
#include <QFileDialog>
#include <QMimeData>
#include <QQuickItem>
#include <QScrollBar>

#define SEEK_INACTIVE (-1)


Monitor::Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent) :
    AbstractMonitor(id, manager, parent)
    , render(NULL)
    , m_controller(NULL)
    , m_length(2)
    , m_dragStarted(false)
    , m_loopClipAction(NULL)
    , m_contextMenu(NULL)
    , m_selectedClip(NULL)
    , m_loopClipTransition(true)
    , m_editMarker(NULL)
    , m_glMonitor(NULL)
    , m_rootItem(NULL)
    , m_showEffectScene(false)
    , m_splitEffect(NULL)
    , m_splitProducer(NULL)
{
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create container widget
    m_glWidget = new QWidget;
    QGridLayout* glayout = new QGridLayout(m_glWidget);
    glayout->setSpacing(0);
    glayout->setContentsMargins(0, 0, 0, 0);

    // Create QML OpenGL widget
    m_glMonitor = new GLWidget;//(id == Kdenlive::ProjectMonitor);
    QWidget *videoWidget = QWidget::createWindowContainer(qobject_cast<QWindow*>(m_glMonitor), this);
    glayout->addWidget(videoWidget, 0, 0);
    m_verticalScroll = new QScrollBar(Qt::Vertical);
    glayout->addWidget(m_verticalScroll, 0, 1);
    m_verticalScroll->hide();
    m_horizontalScroll = new QScrollBar(Qt::Horizontal);
    glayout->addWidget(m_horizontalScroll, 1, 0);
    m_horizontalScroll->hide();
    connect(m_horizontalScroll, SIGNAL(valueChanged(int)), m_glMonitor, SLOT(setOffsetX(int)));
    connect(m_verticalScroll, SIGNAL(valueChanged(int)), m_glMonitor, SLOT(setOffsetY(int)));
    connect(m_glMonitor, SIGNAL(frameDisplayed(const SharedFrame&)), this, SLOT(onFrameDisplayed(const SharedFrame&)));
    connect(m_glMonitor, SIGNAL(mouseSeek(int,bool)), this, SLOT(slotMouseSeek(int,bool)));
    connect(m_glMonitor, SIGNAL(monitorPlay()), this, SLOT(slotPlay()));
    connect(m_glMonitor, SIGNAL(startDrag()), this, SLOT(slotStartDrag()));
    connect(m_glMonitor, SIGNAL(switchFullScreen()), this, SLOT(slotSwitchFullScreen()));
    connect(m_glMonitor, SIGNAL(zoomChanged()), this, SLOT(setZoom()));
    connect(m_glMonitor, SIGNAL(effectChanged(QRect)), this, SIGNAL(effectChanged(QRect)));
    m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitor.qml"))));
    m_rootItem = m_glMonitor->rootObject();

    m_glWidget->setMinimumSize(QSize(320, 180));
    layout->addWidget(m_glWidget, 10);
    
    // Video widget holder
    //layout->addWidget(videoBox, 10);
    layout->addStretch();

    // Get base size for icons
    int s = style()->pixelMetric(QStyle::PM_SmallIconSize);


    // Tool bar buttons
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(s, s));

    m_playIcon = QIcon::fromTheme("media-playback-start");
    m_pauseIcon = QIcon::fromTheme("media-playback-pause");


    if (id != Kdenlive::DvdMonitor) {
        m_toolbar->addAction(QIcon::fromTheme("kdenlive-zone-start"), i18n("Set zone start"), this, SLOT(slotSetZoneStart()));
        m_toolbar->addAction(QIcon::fromTheme("kdenlive-zone-end"), i18n("Set zone end"), this, SLOT(slotSetZoneEnd()));
    }

    m_toolbar->addAction(QIcon::fromTheme("media-seek-backward"), i18n("Rewind"), this, SLOT(slotRewind()));
    //m_toolbar->addAction(QIcon::fromTheme("media-skip-backward"), i18n("Rewind 1 frame"), this, SLOT(slotRewindOneFrame()));

    QToolButton *playButton = new QToolButton(m_toolbar);
    m_playMenu = new QMenu(i18n("Play..."), this);
    m_playAction = new TwostateAction(m_playIcon, i18n("Play"), m_pauseIcon, i18n("Pause"));
    m_playMenu->addAction(m_playAction);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlay()));

    playButton->setMenu(m_playMenu);
    playButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(playButton);

    //m_toolbar->addAction(QIcon::fromTheme("media-skip-forward"), i18n("Forward 1 frame"), this, SLOT(slotForwardOneFrame()));
    m_toolbar->addAction(QIcon::fromTheme("media-seek-forward"), i18n("Forward"), this, SLOT(slotForward()));

    playButton->setDefaultAction(m_playAction);

    if (id != Kdenlive::DvdMonitor) {
        QToolButton *configButton = new QToolButton(m_toolbar);
        m_configMenu = new QMenu(i18n("Misc..."), this);
        configButton->setIcon(QIcon::fromTheme("system-run"));
        configButton->setMenu(m_configMenu);
        configButton->setPopupMode(QToolButton::QToolButton::InstantPopup);
        m_toolbar->addWidget(configButton);

        if (id == Kdenlive::ClipMonitor) {
            m_markerMenu = new QMenu(i18n("Go to marker..."), this);
            m_markerMenu->setEnabled(false);
            m_configMenu->addMenu(m_markerMenu);
            connect(m_markerMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotGoToMarker(QAction*)));
        }
        m_configMenu->addAction(QIcon::fromTheme("transform-scale"), i18n("Resize (100%)"), this, SLOT(slotSetSizeOneToOne()));
        m_configMenu->addAction(QIcon::fromTheme("transform-scale"), i18n("Resize (50%)"), this, SLOT(slotSetSizeOneToTwo()));
    }

    // Create Volume slider popup
    m_volumePopup = new QFrame(this, Qt::Popup);
    QVBoxLayout *poplayout = new QVBoxLayout;
    poplayout->setContentsMargins(0, 0, 0, 0);
    m_audioSlider = new QSlider(Qt::Vertical);
    m_audioSlider->setRange(0, 100);
    poplayout->addWidget(m_audioSlider);
    m_volumePopup->setLayout(poplayout);
    QIcon icon;
    if (KdenliveSettings::volume() == 0) icon = QIcon::fromTheme("audio-volume-muted");
    else icon = QIcon::fromTheme("audio-volume-medium");

    m_volumeWidget = m_toolbar->widgetForAction(m_toolbar->addAction(icon, i18n("Audio volume"), this, SLOT(slotShowVolume())));

    // we need to show / hide the popup once so that it's geometry can be calculated in slotShowVolume
    m_volumePopup->show();
    m_volumePopup->hide();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setLayout(layout);
    setMinimumHeight(200);

    render = new Render(m_id, m_monitorManager->binController(), m_glMonitor, this);

    // Monitor ruler
    m_ruler = new SmallRuler(this, render);
    if (id == Kdenlive::DvdMonitor) m_ruler->setZone(-3, -2);
    layout->addWidget(m_ruler);
    
    connect(m_audioSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSetVolume(int)));
    connect(render, SIGNAL(durationChanged(int,int)), this, SLOT(adjustRulerSize(int,int)));
    connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));
    connect(m_glMonitor, SIGNAL(analyseFrame(QImage)), render, SLOT(emitFrameUpdated(QImage)));
    if (id != Kdenlive::ClipMonitor) {
        connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SIGNAL(zoneUpdated(QPoint)));
    } else {
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SLOT(setClipZone(QPoint)));
    }


    if (id == Kdenlive::ProjectMonitor) {
        QAction *visibilityAction = new QAction(QIcon::fromTheme("video-display"), i18n("Show/Hide edit mode"), this);
        visibilityAction->setCheckable(true);
        visibilityAction->setChecked(KdenliveSettings::showOnMonitorScene());
        connect(visibilityAction, SIGNAL(triggered(bool)), this, SLOT(slotEnableEffectScene(bool)));
        //visibilityAction->setVisible(false);
        m_toolbar->addAction(visibilityAction);
        /*m_effectWidget = new MonitorEditWidget(render, m_glWidget);
	connect(m_effectWidget, SIGNAL(showEdit(bool,bool)), this, SLOT(slotShowEffectScene(bool,bool)));
        m_toolbar->addAction(m_effectWidget->getVisibilityAction());
        videoBox->layout()->addWidget(m_effectWidget);
        m_effectWidget->hide();*/
    }

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
    m_timePos = new TimecodeDisplay(m_monitorManager->timecode(), this);
    m_toolbar->addWidget(m_timePos);
    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotSeek()));
    m_toolbar->setMaximumHeight(m_timePos->height());
    layout->addWidget(m_toolbar);
}

Monitor::~Monitor()
{
    delete m_ruler;
    delete m_timePos;
    delete render;
}

void Monitor::setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu, QAction *loopClip, QWidget* parent)
{
    m_contextMenu = new QMenu(parent);
    m_contextMenu->addMenu(m_playMenu);
    if (goMenu)
        m_contextMenu->addMenu(goMenu);
    if (markerMenu) {
        m_contextMenu->addMenu(markerMenu);
        QList <QAction *>list = markerMenu->actions();
        for (int i = 0; i < list.count(); ++i) {
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
    if (m_id == Kdenlive::ClipMonitor) {
        m_contextMenu->addMenu(m_markerMenu);
        m_contextMenu->addAction(QIcon::fromTheme("document-save"), i18n("Save zone"), this, SLOT(slotSaveZone()));
        QAction *extractZone = m_configMenu->addAction(QIcon::fromTheme("document-new"), i18n("Extract Zone"), this, SLOT(slotExtractCurrentZone()));
        m_contextMenu->addAction(extractZone);
    }
    QAction *extractFrame = m_configMenu->addAction(QIcon::fromTheme("document-new"), i18n("Extract frame"), this, SLOT(slotExtractCurrentFrame()));
    m_contextMenu->addAction(extractFrame);

    if (m_id != Kdenlive::ClipMonitor) {
        QAction *splitView = m_contextMenu->addAction(QIcon::fromTheme("view-split-left-right"), i18n("Split view"), render, SLOT(slotSplitView(bool)));
        splitView->setCheckable(true);
        m_configMenu->addAction(splitView);
    } else {
        QAction *setThumbFrame = m_contextMenu->addAction(QIcon::fromTheme("document-new"), i18n("Set current image as thumbnail"), this, SLOT(slotSetThumbFrame()));
        m_configMenu->addAction(setThumbFrame);
    }

    QAction *showTips = m_contextMenu->addAction(QIcon::fromTheme("help-hint"), i18n("Monitor overlay infos"));
    showTips->setCheckable(true);
    connect(showTips, SIGNAL(toggled(bool)), this, SLOT(slotSwitchMonitorInfo(bool)));
    showTips->setChecked(KdenliveSettings::displayMonitorInfo());

    QAction *dropFrames = m_contextMenu->addAction(QIcon(), i18n("Real time (drop frames)"));
    dropFrames->setCheckable(true);
    dropFrames->setChecked(true);
    connect(dropFrames, SIGNAL(toggled(bool)), this, SLOT(slotSwitchDropFrames(bool)));
    
    QAction *overlayAudio = m_contextMenu->addAction(QIcon(), i18n("Overlay audio waveform"));
    overlayAudio->setCheckable(true);
    connect(overlayAudio, SIGNAL(toggled(bool)), m_glMonitor, SLOT(slotSwitchAudioOverlay(bool)));
    overlayAudio->setChecked(KdenliveSettings::displayAudioOverlay());

    QAction *effectCompare = m_contextMenu->addAction(QIcon(), i18n("Compare effect"));
    effectCompare->setCheckable(true);
    connect(effectCompare, SIGNAL(toggled(bool)), this, SLOT(slotSwitchCompare(bool)));
    
    m_configMenu->addAction(showTips);
    m_configMenu->addAction(dropFrames);
    m_configMenu->addAction(effectCompare);
    m_configMenu->addAction(overlayAudio);
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
    //qDebug() << "// render info: " << width << 'x' << height;
    while (width >= maxWidth || height >= maxHeight) {
        width = width * 0.8;
        height = height * 0.8;
    }
    //qDebug() << "// MONITOR; set SIZE: " << width << ", " << height;
    m_glWidget->setFixedSize(width, height);
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
    //qDebug() << "// render info: " << width << 'x' << height;
    while (width >= maxWidth || height >= maxHeight) {
        width = width * 0.8;
        height = height * 0.8;
    }
    //qDebug() << "// MONITOR; set SIZE: " << width << ", " << height;
    m_glWidget->setFixedSize(width, height);
    updateGeometry();
    adjustSize();
    //m_ui.video_frame->setMinimumSize(0, 0);
    emit adjustMonitorSize();
}

void Monitor::resetSize()
{
    m_glWidget->setMinimumSize(0, 0);
}

QString Monitor::getTimecodeFromFrames(int pos)
{
    return m_monitorManager->timecode().getTimecodeFromFrames(pos);
}

double Monitor::fps() const
{
    return m_monitorManager->timecode().fps();
}

void Monitor::updateMarkers()
{
    if (m_controller) {
        m_markerMenu->clear();
        QList <CommentedTime> markers = m_controller->commentedSnapMarkers();
        if (!markers.isEmpty()) {
            QList <int> marks;
            for (int i = 0; i < markers.count(); ++i) {
                int pos = (int) markers.at(i).time().frames(m_monitorManager->timecode().fps());
                marks.append(pos);
                QString position = m_monitorManager->timecode().getTimecode(markers.at(i).time()) + ' ' + markers.at(i).comment();
                QAction *go = m_markerMenu->addAction(position);
                go->setData(pos);
            }
        }
	m_ruler->setMarkers(markers);
        m_markerMenu->setEnabled(!m_markerMenu->isEmpty());
    }
}

void Monitor::setMarkers(const QList<CommentedTime> &markers)
{
    m_ruler->setMarkers(markers);
}

void Monitor::slotSeekToPreviousSnap()
{
    if (m_controller) slotSeek(getSnapForPos(true).frames(m_monitorManager->timecode().fps()));
}

void Monitor::slotSeekToNextSnap()
{
    if (m_controller) slotSeek(getSnapForPos(false).frames(m_monitorManager->timecode().fps()));
}

GenTime Monitor::position()
{
    return render->seekPosition();
}

GenTime Monitor::getSnapForPos(bool previous)
{
    QList <GenTime> snaps;
    QList < GenTime > markers = m_controller->snapMarkers();
    for (int i = 0; i < markers.size(); ++i) {
        GenTime t = markers.at(i);
        snaps.append(t);
    }
    QPoint zone = m_ruler->zone();
    snaps.append(GenTime(zone.x(), m_monitorManager->timecode().fps()));
    snaps.append(GenTime(zone.y(), m_monitorManager->timecode().fps()));
    snaps.append(GenTime());
    snaps.append(m_controller->getPlaytime());
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
    setClipZone(m_ruler->zone());
    checkOverlay();
}

void Monitor::slotSetZoneStart()
{
    m_ruler->setZoneStart();
    emit zoneUpdated(m_ruler->zone());
    setClipZone(m_ruler->zone());
    checkOverlay();
}

void Monitor::slotSetZoneEnd()
{
    m_ruler->setZoneEnd();
    emit zoneUpdated(m_ruler->zone());
    setClipZone(m_ruler->zone());
    checkOverlay();
}

// virtual
void Monitor::mousePressEvent(QMouseEvent * event)
{
    if (render) render->setActiveMonitor();
    if (event->button() != Qt::RightButton) {
        if (m_glWidget->geometry().contains(event->pos())) {
            m_dragStarted = true;
            m_DragStartPosition = event->pos();
        }
    } else if (m_contextMenu) {
        m_contextMenu->popup(event->globalPos());
    }
}

void Monitor::resizeEvent(QResizeEvent *event)
{
    if (m_glMonitor->zoom() > 0.0f) {
        float horizontal = float(m_horizontalScroll->value()) / m_horizontalScroll->maximum();
        float vertical = float(m_verticalScroll->value()) / m_verticalScroll->maximum();
        adjustScrollBars(horizontal, vertical);
    } else {
        m_horizontalScroll->hide();
        m_verticalScroll->hide();
    }
}

void Monitor::adjustScrollBars(float horizontal, float vertical)
{
    if (m_glMonitor->zoom() > 1.0f) {
        m_horizontalScroll->setPageStep(m_glWidget->width());
        m_horizontalScroll->setMaximum(m_glMonitor->profileSize().width() * m_glMonitor->zoom()
                                       - m_horizontalScroll->pageStep());
        m_horizontalScroll->setValue(qRound(horizontal * m_horizontalScroll->maximum()));
        emit m_horizontalScroll->valueChanged(m_horizontalScroll->value());
        m_horizontalScroll->show();
    } else {
        int max = m_glMonitor->profileSize().width() * m_glMonitor->zoom() - m_glWidget->width();
        emit m_horizontalScroll->valueChanged(qRound(0.5 * max));
        m_horizontalScroll->hide();
    }

    if (m_glMonitor->zoom() > 1.0f) {
        m_verticalScroll->setPageStep(m_glWidget->height());
        m_verticalScroll->setMaximum(m_glMonitor->profileSize().height() * m_glMonitor->zoom()
                                     - m_verticalScroll->pageStep());
        m_verticalScroll->setValue(qRound(vertical * m_verticalScroll->maximum()));
        emit m_verticalScroll->valueChanged(m_verticalScroll->value());
        m_verticalScroll->show();
    } else {
        int max = m_glMonitor->profileSize().height() * m_glMonitor->zoom() - m_glWidget->height();
        emit m_verticalScroll->valueChanged(qRound(0.5 * max));
        m_verticalScroll->hide();
    }
}

void Monitor::setZoom()
{
    //emit zoomChanged(factor);
    //Settings.setPlayerZoom(factor);
    if (m_rootItem->objectName() != "root") {
        // we are not in main view, ignore
        return;
    }
    if (m_glMonitor->zoom() == 1.0f) {
       /* m_zoomButton->setIcon(icon);
        m_zoomButton->setChecked(false);*/
        m_horizontalScroll->hide();
        m_verticalScroll->hide();
    } else {
        adjustScrollBars(0.5f, 0.5f);
    }
    QRect r;
    r.setSize(QSize((int) (m_glMonitor->rect().width() * m_glMonitor->zoom() + 0.5), (int) (m_glMonitor->rect().height() * m_glMonitor->zoom() + 0.5)));
    QSize s = m_glMonitor->size() / 2 - r.size() / 2;
    r.moveTopLeft(QPoint(s.width(), s.height()));
    m_rootItem->setProperty("framesize", r);
}

void Monitor::slotSwitchFullScreen()
{
    switchFullScreen();
}

// virtual
void Monitor::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_dragStarted && event->button() != Qt::RightButton) {
        if (m_glMonitor->geometry().contains(event->pos())) {
            if (isActive()) slotPlay();
            else slotActivateMonitor();
        } //else event->ignore(); //QWidget::mouseReleaseEvent(event);
    }
    m_dragStarted = false;
    event->accept();
}


void Monitor::slotStartDrag()
{
    if (m_id == Kdenlive::ProjectMonitor) {
        // dragging is only allowed for clip monitor
        return;
    }
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;

    QStringList list;
    list.append(m_controller->clipId());
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
}

// virtual
void Monitor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragStarted || m_controller == NULL) return;

    if ((event->pos() - m_DragStartPosition).manhattanLength()
            < QApplication::startDragDistance())
        return;

    {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        QStringList list;
        list.append(m_controller->clipId());
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
	/*Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
        Qt::DropAction dropAction;
        dropAction = drag->start(Qt::CopyAction | Qt::MoveAction);*/

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
    switchFullScreen();
    event->accept();
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
    if (m_controller == NULL) {
        return;
    }
    //TODO
    /*m_controller->setClipThumbFrame((uint) render->seekFramePosition());
    emit refreshClipThumbnail(m_controller->clipId(), true);*/
}

void Monitor::slotExtractCurrentZone()
{
    if (m_controller == NULL) return;
    emit extractZone(m_controller->clipId());
}

ClipController *Monitor::currentController() const
{
    return m_controller;
}

void Monitor::slotExtractCurrentFrame()
{
    QImage frame;
    // check if we are using a proxy
    if (m_controller && !m_controller->property("kdenlive:proxy").isEmpty() && m_controller->property("kdenlive:proxy") != "-") {
        // using proxy, use original clip url to get frame
        frame = render->extractFrame(render->seekFramePosition(), m_controller->property("resource"));
    }
    else frame = render->extractFrame(render->seekFramePosition());
    QString framesFolder = KRecentDirs::dir(":KdenliveFramesFolder");
    if (framesFolder.isEmpty()) framesFolder = QDir::homePath();
    
    QPointer<QFileDialog> fs = new QFileDialog(this, i18n("Save Image"), framesFolder);
    fs->setMimeTypeFilters(QStringList() << "image/png");
    fs->setAcceptMode(QFileDialog::AcceptSave);
    if (fs->exec()) {
        QStringList path = fs->selectedFiles();
        if (!path.isEmpty()) {
            KRecentDirs::add(":KdenliveFramesFolder", fs->selectedUrls().first().adjusted(QUrl::RemoveFilename).path());
            frame.save(path.first());
        }
    }
    delete fs;
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
    if (m_rootItem->objectName() != "root") {
        // we are not in main view, ignore
        return;
    }
    QString overlayText;
    int pos = m_timePos->getValue();//render->seekFramePosition();
    QPoint zone = m_ruler->zone();
    if (pos == zone.x())
        overlayText = i18n("In Point");
    else if (pos == zone.y())
        overlayText = i18n("Out Point");
    else if (m_controller) {
        overlayText = m_controller->markerComment(GenTime(pos, m_monitorManager->timecode().fps()));
    }
    if (overlayText!= m_rootItem->property("comment")) m_rootItem->setProperty("comment", overlayText);
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

int Monitor::getZoneStart()
{
  return m_ruler->zone().x();
}

int Monitor::getZoneEnd()
{
  return m_ruler->zone().y();
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
	if (currentspeed >= 0) render->play(-1);
	else switch((int) currentspeed) {
	    case -1:
		render->play(-2);
		break;
	    case -2:
		render->play(-3);
		break;
	    case -3:
		render->play(-5);
		break;
	    default:
		render->play(-8);
	}
    } else render->play(speed);
    m_playAction->setActive(true);
}

void Monitor::slotForward(double speed)
{
    slotActivateMonitor();
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed <= 0) render->play(1);
        else switch((int) currentspeed) {
        case 1:
            render->play(2);
            break;
        case 2:
            render->play(3);
            break;
        case 3:
            render->play(5);
            break;
        default:
            render->play(8);
        }
    } else {
        render->play(speed);
    }
    m_playAction->setActive(true);
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
    if (m_ruler->slotNewValue(pos)) {
        m_timePos->setValue(pos);
	checkOverlay();
        if (m_id != Kdenlive::ClipMonitor) {
            emit renderPosition(pos);
        }
    }
}

void Monitor::rendererStopped(int pos)
{
    if (m_ruler->slotNewValue(pos)) {
        m_timePos->setValue(pos);
        checkOverlay();
    }
    m_playAction->setActive(false);
}

void Monitor::adjustRulerSize(int length, int offset)
{
    if (length > 0) m_length = length;
    m_ruler->adjustScale(m_length, offset);
    m_timePos->setRange(offset, offset + length);
    if (m_controller != NULL) {
        QPoint zone = m_controller->zone();
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
    if (render) render->startConsumer();
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
    m_playAction->setActive(false);
}

void Monitor::slotPlay()
{
    if (render == NULL) return;
    slotActivateMonitor();
    if (render->isPlaying()) {
        m_playAction->setActive(false);
        render->switchPlay(false);
    }
    else {
        m_playAction->setActive(true);
        render->switchPlay(true);
    }
    m_ruler->refreshRuler();
}

void Monitor::slotPlayZone()
{
    if (render == NULL) return;
    slotActivateMonitor();
    QPoint p = m_ruler->zone();
    bool ok = render->playZone(GenTime(p.x(), m_monitorManager->timecode().fps()), GenTime(p.y(), m_monitorManager->timecode().fps()));
    if (ok) {
        m_playAction->setActive(true);
    }
}

void Monitor::slotLoopZone()
{
    if (render == NULL) return;
    slotActivateMonitor();
    QPoint p = m_ruler->zone();
    render->loopZone(GenTime(p.x(), m_monitorManager->timecode().fps()), GenTime(p.y(), m_monitorManager->timecode().fps()));
    m_playAction->setActive(true);
}

void Monitor::slotLoopClip()
{
    if (render == NULL || m_selectedClip == NULL)
        return;
    slotActivateMonitor();
    render->loopZone(m_selectedClip->startPos(), m_selectedClip->endPos());
    m_playAction->setActive(true);
}

void Monitor::updateClipProducer(Mlt::Producer *prod)
{
    if (render == NULL) return;
    render->setProducer(prod, render->seekFramePosition(), true);
}

void Monitor::openClip(ClipController *controller)
{
    if (render == NULL) return;
    m_controller = controller;
    if (controller) {
        updateMarkers();
        render->setProducer(m_controller->masterProducer(), -1, isActive());
    }
    else {
        render->setProducer(NULL, -1, isActive());
    }
}

void Monitor::openClipZone(ClipController *controller, int in, int out)
{
    if (render == NULL) return;
    m_controller = controller;
    if (controller) {
        render->setProducer(m_controller->zoneProducer(in, out), -1, isActive());
    }
    else {
        render->setProducer(NULL, -1, isActive());
    }
}

const QString Monitor::activeClipId()
{
    if (m_controller) {
        return m_controller->clipId();
    }
    return QString();
}

/*
void Monitor::slotSetClipProducer(DocClipBase *clip, QPoint zone, bool forceUpdate, int position)
{
    if (render == NULL) return;
    if (clip == NULL && m_currentClip != NULL) {
	m_currentClip->lastSeekPosition = render->seekFramePosition();
        m_currentClip = NULL;
        m_length = -1;
        render->setProducer(NULL, -1);
        return;
    }

    if (clip != m_currentClip || forceUpdate) {
	if (m_currentClip) m_currentClip->lastSeekPosition = render->seekFramePosition();
        m_currentClip = clip;
	if (position == -1) position = clip->lastSeekPosition;
        updateMarkers(clip);
  	if (render->setMonitorProducer(clip->getId(), position) == -1) {
            // MLT CONSUMER is broken
            qWarning() << "ERROR, Cannot start monitor";
        } else start();
    } else {
        if (m_currentClip) {
            slotActivateMonitor();
            if (position == -1) position = render->seekFramePosition();
            render->seek(position);
	    if (zone.isNull()) {
		zone = m_currentClip->zone();
		m_ruler->setZone(zone.x(), zone.y());
		return;
	    }
        }
    }
    if (!zone.isNull()) {
        m_ruler->setZone(zone.x(), zone.y());
        render->seek(zone.x());
    }
}
*/

void Monitor::slotOpenFile(const QString &file)
{
    if (render == NULL) return;
    slotActivateMonitor();
    render->loadUrl(file);
}

void Monitor::slotSaveZone()
{
    if (render == NULL) return;
    //TODO
    //emit saveZone(render, m_ruler->zone(), m_currentClip);

    //render->setSceneList(doc, 0);
}

void Monitor::setCustomProfile(const QString &profile, const Timecode &tc)
{
    m_timePos->updateTimeCode(tc);
    if (render == NULL) return;
    if (!render->hasProfile(profile)) {
        slotActivateMonitor();
        render->resetProfile(profile);
    }
}

void Monitor::resetProfile(const QString &profile)
{
    m_timePos->updateTimeCode(m_monitorManager->timecode());
    if (render == NULL) return;
    if (!render->hasProfile(profile)) {
        slotActivateMonitor();
        render->resetProfile(profile);
    }
    if (m_rootItem->objectName() != "root") {
        // we are not in main view, ignore
        return;
    }
    m_rootItem->setProperty("framesize", QRect(0, 0, m_glMonitor->profileSize().width(), m_glMonitor->profileSize().height()));
}

void Monitor::saveSceneList(const QString &path, const QDomElement &info)
{
    if (render == NULL) return;
    render->saveSceneList(path, info);
}

const QString Monitor::sceneList()
{
    if (render == NULL) return QString();
    return render->sceneList();
}

void Monitor::setClipZone(const QPoint &pos)
{
    if (m_controller == NULL) return;
    m_controller->setZone(pos);
}

void Monitor::slotSwitchDropFrames(bool show)
{
    render->setDropFrames(show);
}

void Monitor::slotSwitchMonitorInfo(bool show)
{
    if (m_rootItem->objectName() != "root") {
        // we are not in main view, ignore
        return;
    }
    KdenliveSettings::setDisplayMonitorInfo(show);
    if (show) {
        m_rootItem = m_glMonitor->rootObject();
        m_rootItem->setProperty("visible", true);
    }
    else {
        m_rootItem->setProperty("visible", false);
        m_rootItem = NULL;
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
    if (m_controller == NULL) return result;
    result << m_controller->clipId();
    QPoint zone = m_ruler->zone();
    result << QString::number(zone.x()) << QString::number(zone.y());
    return result;
}

void Monitor::slotSetSelectedClip(AbstractClipItem* item)
{
    if (item) {
        if (m_loopClipAction) m_loopClipAction->setEnabled(true);
        m_selectedClip = item;
    } else {
        if (m_loopClipAction) m_loopClipAction->setEnabled(false);
    }
}

void Monitor::slotSetSelectedClip(ClipItem* item)
{
    if (item || (!item && !m_loopClipTransition)) {
        m_loopClipTransition = false;
        slotSetSelectedClip((AbstractClipItem*)item); //FIXME static_cast fails!
    }
}

void Monitor::slotSetSelectedClip(Transition* item)
{
    if (item || (!item && m_loopClipTransition)) {
        m_loopClipTransition = true;
        slotSetSelectedClip((AbstractClipItem*)item); //FIXME static_cast fails!
    }
}


void Monitor::slotEnableEffectScene(bool enable)
{
    KdenliveSettings::setShowOnMonitorScene(enable);
    if (m_showEffectScene) {
        slotShowEffectScene(true);
    }
}

void Monitor::slotShowEffectScene(bool show, bool manuallyTriggered)
{
    m_showEffectScene = show;
    m_glMonitor->slotShowEffectScene(show && KdenliveSettings::showOnMonitorScene());
}

void Monitor::setUpEffectGeometry(QRect r)
{
    m_glMonitor->rootObject()->setProperty("framesize", r);
}

void Monitor::setUpEffectGeometry(int x, int y, int w, int h)
{
    m_glMonitor->rootObject()->setProperty("framesize", QRect(x, y, w, h));
    QRect res = m_glMonitor->rootObject()->property("framesize").toRect();
}

QRect Monitor::effectRect() const
{
    return m_glMonitor->rootObject()->property("framesize").toRect();
}

void Monitor::setEffectKeyframe(bool enable)
{
    m_glMonitor->rootObject()->setProperty("iskeyframe", enable);
}

bool Monitor::effectSceneDisplayed()
{
    return m_glMonitor->effectSceneVisible();
}

void Monitor::slotSetVolume(int volume)
{
    KdenliveSettings::setVolume(volume);
    QIcon icon;
    if (volume == 0) icon = QIcon::fromTheme("audio-volume-muted");
    else icon = QIcon::fromTheme("audio-volume-medium");
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


void Monitor::sendFrameForAnalysis(bool analyse)
{
    m_glMonitor->sendFrameForAnalysis = analyse;
}


void Monitor::onFrameDisplayed(const SharedFrame& frame)
{
    int position = frame.get_position();
    render->checkFrameNumber(position);
    if (m_rootItem->objectName() == "root") {
        // we are in main view, show frame
        m_rootItem->setProperty("framenum", QString::number(position));
    }
    seekCursor(position);
    if (position >= m_length)
        render->pause();
        //emit endOfStream();
}

AbstractRender *Monitor::abstractRender()
{
    return render;
}

void Monitor::reloadProducer(const QString &id)
{
    if (!m_controller) return;
    if (m_controller->clipId() == id)
        openClip(m_controller);
}

QString Monitor::getMarkerThumb(GenTime pos)
{
    //TODO
    /*if (!m_controller) return QString();
    if (!m_controller->getClipHash().isEmpty()) {
	QString url = m_monitorManager->getProjectFolder() + "thumbs/" + m_currentClip->getClipHash() + '#' + QString::number((int) pos.frames(m_monitorManager->timecode().fps())) + ".png";
        if (QFile::exists(url)) return url;
    }*/
    return QString();
}

void Monitor::setPalette ( const QPalette & p)
{
    QWidget::setPalette(p);
    if (m_ruler) m_ruler->updatePalette();
    
}

void Monitor::switchFullScreen()
{
    // TODO: disable screensaver?
    if (!m_glWidget->isFullScreen()) {
        // Check if we have a multiple monitor setup
        int monitors = QApplication::desktop()->screenCount();
        int screen = -1;
        if (monitors > 1) {
            QRect screenres;
            // Move monitor widget to the second screen (one screen for Kdenlive, the other one for the Monitor widget
            //int currentScreen = QApplication::desktop()->screenNumber(this);
            for (int i = 0; screen == -1 && i < QApplication::desktop()->screenCount(); i++) {
            if (i != QApplication::desktop()->screenNumber(this))
                screen = i;
            }
        }
        m_glWidget->setParent(QApplication::desktop()->screen(screen));
        m_glWidget->move(QApplication::desktop()->screenGeometry(screen).bottomLeft());
        m_glWidget->showFullScreen();
    } else {
        m_glWidget->showNormal();
        QVBoxLayout *lay = (QVBoxLayout *) layout();
        lay->insertWidget(0, m_glWidget, 10);
    }
}


void Monitor::slotSwitchCompare(bool enable)
{
    int pos = position().frames(m_monitorManager->timecode().fps());
    if (enable) {
        Mlt::Producer *original = m_controller->masterProducer();
        Mlt::Tractor trac(*original->profile());
        Mlt::Producer p2(*original->profile(), original->get("resource"));
        trac.set_track(*original, 0);
        trac.set_track(p2, 1);
        m_splitEffect = new Mlt::Filter(*original->profile(), "frei0r.scale0tilt");
        m_splitEffect->set("Clip left", 0.5);
        p2.attach(*m_splitEffect);
        Mlt::Transition t(*original->profile(), "movit.overlay");
        trac.plant_transition(t, 0, 1);
        m_splitProducer = new Mlt::Producer(trac.get_producer());
        //m_splitProducer->seek(pos);
        render->setProducer(m_splitProducer, pos, isActive());
        m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitorsplit.qml"))));
        m_rootItem = m_glMonitor->rootObject();
        QObject::connect(m_rootItem, SIGNAL(qmlMoveSplit(double)), this, SLOT(slotAdjustEffectCompare(double)), Qt::UniqueConnection);
    }
    else {
        render->setProducer(m_controller->masterProducer(), pos, isActive());
        loadMasterQml();
        delete m_splitProducer;
        delete m_splitEffect;
    }
    refreshMonitor();
}

void Monitor::loadMasterQml()
{
    m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitor.qml"))));
    m_rootItem = m_glMonitor->rootObject();
}

void Monitor::slotAdjustEffectCompare(double percent)
{
    if (m_splitEffect) m_splitEffect->set("Clip left", percent);
    refreshMonitor();
}

