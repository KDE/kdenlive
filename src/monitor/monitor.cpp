
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
#include "glwidget.h"
#include "recmanager.h"
#include "smallruler.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/bincontroller.h"
#include "kdenlivesettings.h"
#include "timeline/abstractclipitem.h"
#include "timeline/clip.h"
#include "dialogs/profilesdialog.h"
#include "doc/kthumb.h"
#include "utils/KoIconUtils.h"

#include "klocalizedstring.h"
#include <KRecentDirs>
#include <KDualAction>
#include <KSelectAction>
#include <KMessageWidget>
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
#include <QWidgetAction>

#define SEEK_INACTIVE (-1)

QuickEventEater::QuickEventEater(QObject *parent) : QObject(parent)
{
}

bool QuickEventEater::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::DragEnter) {
        QDragEnterEvent *ev = reinterpret_cast< QDragEnterEvent* >(event);
        if (ev->mimeData()->hasFormat("kdenlive/effectslist")) {
            ev->acceptProposedAction();
            return true;
        }
    }
    else if (event->type() == QEvent::DragMove) {
        QDragEnterEvent *ev = reinterpret_cast< QDragEnterEvent* >(event);
        if (ev->mimeData()->hasFormat("kdenlive/effectslist")) {
            ev->acceptProposedAction();
            return true;
        }
    }
    else if (event->type() == QEvent::Drop) {
        QDropEvent *ev = static_cast< QDropEvent* >(event);
        if (ev) {
          const QString effects = QString::fromUtf8(ev->mimeData()->data("kdenlive/effectslist"));
          QDomDocument doc;
          doc.setContent(effects, true);
          emit addEffect(doc.documentElement());
          ev->accept();
          return true;
        }
    }
    return QObject::eventFilter(obj, event);
}



Monitor::Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent) :
    AbstractMonitor(id, manager, parent)
    , render(NULL)
    , m_controller(NULL)
    , m_glMonitor(NULL)
    , m_splitEffect(NULL)
    , m_splitProducer(NULL)
    , m_length(2)
    , m_dragStarted(false)
    , m_recManager(NULL)
    , m_loopClipAction(NULL)
    , m_effectCompare(NULL)
    , m_sceneVisibilityAction(NULL)
    , m_contextMenu(NULL)
    , m_selectedClip(NULL)
    , m_loopClipTransition(true)
    , m_editMarker(NULL)
    , m_forceSizeFactor(0)
    , m_rootItem(NULL)
    , m_lastMonitorSceneType(MonitorSceneNone)
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
    m_glMonitor = new GLWidget();
    m_videoWidget = QWidget::createWindowContainer(qobject_cast<QWindow*>(m_glMonitor));
    m_videoWidget->setAcceptDrops(true);
    QuickEventEater *leventEater = new QuickEventEater(this);
    m_videoWidget->installEventFilter(leventEater);
    connect(leventEater, &QuickEventEater::addEffect, this, &Monitor::slotAddEffect);

    glayout->addWidget(m_videoWidget, 0, 0);
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
    connect(m_glMonitor, SIGNAL(switchFullScreen(bool)), this, SLOT(slotSwitchFullScreen(bool)));
    connect(m_glMonitor, SIGNAL(zoomChanged()), this, SLOT(setZoom()));
    connect(m_glMonitor, SIGNAL(effectChanged(QRect)), this, SIGNAL(effectChanged(QRect)));
    connect(m_glMonitor, SIGNAL(effectChanged(QVariantList)), this, SIGNAL(effectChanged(QVariantList)));
    connect(m_glMonitor, SIGNAL(lockMonitor(bool)), this, SLOT(slotLockMonitor(bool)), Qt::DirectConnection);

    if (KdenliveSettings::displayMonitorInfo()) {
        // Load monitor overlay qml
        m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitor.qml"))));
        m_rootItem = m_glMonitor->rootObject();
    }
    connect(m_glMonitor, SIGNAL(showContextMenu(QPoint)), this, SLOT(slotShowMenu(QPoint)));

    m_glWidget->setMinimumSize(QSize(320, 180));
    layout->addWidget(m_glWidget, 10);
    layout->addStretch();

    // Tool bar buttons
    m_toolbar = new QToolBar(this);
    QWidget *sp1 = new QWidget(this);
    sp1->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(sp1);
    if (id == Kdenlive::ClipMonitor) {
        // Add options for recording
        m_recManager = new RecManager(this);
        connect(m_recManager, &RecManager::warningMessage, this, &Monitor::warningMessage);
        connect(m_recManager, &RecManager::addClipToProject, this, &Monitor::addClipToProject);
    }

    if (id != Kdenlive::DvdMonitor) {
        m_toolbar->addAction(manager->getAction("mark_in"));
        m_toolbar->addAction(manager->getAction("mark_out"));
    }
    m_toolbar->addAction(manager->getAction("monitor_seek_backward"));

    QToolButton *playButton = new QToolButton(m_toolbar);
    m_playMenu = new QMenu(i18n("Play..."), this);
    QAction *originalPlayAction = static_cast<KDualAction*> (manager->getAction("monitor_play"));
    m_playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    m_playAction->setInactiveIcon(KoIconUtils::themedIcon("media-playback-start"));
    m_playAction->setActiveIcon(KoIconUtils::themedIcon("media-playback-pause"));

    QString strippedTooltip = m_playAction->toolTip().remove(QRegExp("\\s\\(.*\\)"));
    // append shortcut if it exists for action
    if (originalPlayAction->shortcut() == QKeySequence(0)) {
        m_playAction->setToolTip( strippedTooltip);
    }
    else {
        m_playAction->setToolTip( strippedTooltip + " (" + originalPlayAction->shortcut().toString() + ")");
    }
    m_playMenu->addAction(m_playAction);
    connect(m_playAction, &QAction::triggered, this, &Monitor::slotSwitchPlay);

    playButton->setMenu(m_playMenu);
    playButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(playButton);
    m_toolbar->addAction(manager->getAction("monitor_seek_forward"));

    playButton->setDefaultAction(m_playAction);
    m_configMenu = new QMenu(i18n("Misc..."), this);

    if (id != Kdenlive::DvdMonitor) {
        if (id == Kdenlive::ClipMonitor) {
            m_markerMenu = new QMenu(i18n("Go to marker..."), this);
            m_markerMenu->setEnabled(false);
            m_configMenu->addMenu(m_markerMenu);
            connect(m_markerMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotGoToMarker(QAction*)));
        }
        m_forceSize = new KSelectAction(KoIconUtils::themedIcon("transform-scale"), i18n("Force Monitor Size"), this);
        QAction *fullAction = m_forceSize->addAction(QIcon(), i18n("Force 100%"));
        fullAction->setData(100);
        QAction *halfAction = m_forceSize->addAction(QIcon(), i18n("Force 50%"));
        halfAction->setData(50);
        QAction *freeAction = m_forceSize->addAction(QIcon(), i18n("Free Resize"));
        freeAction->setData(0);
        m_configMenu->addAction(m_forceSize);
        m_forceSize->setCurrentAction(freeAction);
        connect(m_forceSize, SIGNAL(triggered(QAction*)), this, SLOT(slotForceSize(QAction*)));
    }

    // Create Volume slider popup
    m_audioSlider = new QSlider(Qt::Vertical);
    m_audioSlider->setRange(0, 100);
    m_audioSlider->setValue(100);
    connect(m_audioSlider, &QSlider::valueChanged, this, &Monitor::slotSetVolume);
    QWidgetAction *widgetslider = new QWidgetAction(this);
    widgetslider->setText(i18n("Audio volume"));
    widgetslider->setDefaultWidget(m_audioSlider);
    QMenu *menu = new QMenu(this);
    menu->addAction(widgetslider);

    m_audioButton = new QToolButton(this);
    m_audioButton->setMenu(menu);
    m_audioButton->setPopupMode(QToolButton::InstantPopup);
    QIcon icon;
    if (KdenliveSettings::volume() == 0) icon = KoIconUtils::themedIcon("audio-volume-muted");
    else icon = KoIconUtils::themedIcon("audio-volume-medium");
    m_audioButton->setIcon(icon);
    m_toolbar->addWidget(m_audioButton);

    if (id == Kdenlive::ClipMonitor) {
        m_effectCompare = new KDualAction(i18n("Split"), i18n("Unsplit"), this);
        m_effectCompare->setInactiveIcon(KoIconUtils::themedIcon("view-split-effect"));
        m_effectCompare->setActiveIcon(KoIconUtils::themedIcon("view-unsplit-effect"));
        m_effectCompare->setActive(false);
        m_effectCompare->setCheckable(true);
        m_effectCompare->setChecked(false);
        m_effectCompare->setEnabled(false);
        m_toolbar->addSeparator();
        m_toolbar->addAction(m_effectCompare);
        connect(m_effectCompare, &KDualAction::activeChanged, this, &Monitor::slotSwitchCompare);
    }
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setLayout(layout);
    setMinimumHeight(200);

    render = new Render(m_id, m_monitorManager->binController(), m_glMonitor, this);

    // Monitor ruler
    m_ruler = new SmallRuler(this, render);
    if (id == Kdenlive::DvdMonitor) m_ruler->setZone(-3, -2);
    layout->addWidget(m_ruler);

    connect(render, SIGNAL(durationChanged(int,int)), this, SLOT(adjustRulerSize(int,int)));
    connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));
    connect(m_glMonitor, SIGNAL(analyseFrame(QImage)), render, SLOT(emitFrameUpdated(QImage)));
    connect(m_glMonitor, SIGNAL(audioSamplesSignal(const audioShortVector&,int,int,int)), render, SIGNAL(audioSamplesSignal(const audioShortVector&,int,int,int)));

    if (id != Kdenlive::ClipMonitor) {
        connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SIGNAL(zoneUpdated(QPoint)));
    } else {
        connect(m_ruler, SIGNAL(zoneChanged(QPoint)), this, SLOT(setClipZone(QPoint)));
    }


    if (id == Kdenlive::ProjectMonitor) {
        m_sceneVisibilityAction = new QAction(KoIconUtils::themedIcon("transform-crop"), i18n("Show/Hide edit mode"), this);
        m_sceneVisibilityAction->setCheckable(true);
        m_sceneVisibilityAction->setChecked(KdenliveSettings::showOnMonitorScene());
        connect(m_sceneVisibilityAction, SIGNAL(triggered(bool)), this, SLOT(slotEnableEffectScene(bool)));
        m_toolbar->addAction(m_sceneVisibilityAction);
    }

    m_toolbar->addSeparator();
    m_timePos = new TimecodeDisplay(m_monitorManager->timecode(), this);
    m_toolbar->addWidget(m_timePos);

    QToolButton *configButton = new QToolButton(m_toolbar);
    configButton->setIcon(KoIconUtils::themedIcon("system-run"));
    configButton->setMenu(m_configMenu);
    configButton->setPopupMode(QToolButton::QToolButton::InstantPopup);
    m_toolbar->addWidget(configButton);
    if (m_recManager) m_toolbar->addAction(m_recManager->switchAction());
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotSeek()));
    layout->addWidget(m_toolbar);
    if (m_recManager) layout->addWidget(m_recManager->toolbar());
    // Info message widget
    m_infoMessage = new KMessageWidget(this);
    layout->addWidget(m_infoMessage);
    m_infoMessage->hide();
}

Monitor::~Monitor()
{
    render->stop();
    delete m_glMonitor;
    delete m_videoWidget;
    delete m_glWidget;
    delete m_ruler;
    delete m_timePos;
    delete render;
}

void Monitor::slotGetCurrentImage()
{
    m_monitorManager->activateMonitor(m_id, true);
    m_glMonitor->sendFrameForAnalysis = true;
    refreshMonitor();
    // Update analysis state
    QTimer::singleShot(500, m_monitorManager, SIGNAL(checkScopes()));
}

void Monitor::slotAddEffect(QDomElement effect)
{
    if (m_id == Kdenlive::ClipMonitor) {
        if (m_controller) emit addMasterEffect(m_controller->clipId(), effect);
    }
    else emit addEffect(effect);
}

void Monitor::refreshIcons()
{
    QList<QAction *> allMenus = this->findChildren<QAction *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QAction *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) continue;
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
    QList<KDualAction *> allButtons = this->findChildren<KDualAction *>();
    for (int i = 0; i < allButtons.count(); i++) {
        KDualAction *m = allButtons.at(i);
        QIcon ic = m->activeIcon();
        if (ic.isNull() || ic.name().isEmpty()) continue;
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setActiveIcon(newIcon);
        ic = m->inactiveIcon();
        if (ic.isNull() || ic.name().isEmpty()) continue;
        newIcon = KoIconUtils::themedIcon(ic.name());
        m->setInactiveIcon(newIcon);
    }
}

QAction *Monitor::recAction()
{
    if (m_recManager) return m_recManager->switchAction();
    return NULL;
}

void Monitor::slotLockMonitor(bool lock)
{
    m_monitorManager->lockMonitor(m_id, lock);
}

void Monitor::setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu, QAction *loopClip)
{
    m_contextMenu = new QMenu(this);
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
	m_contextMenu->addAction(KoIconUtils::themedIcon("document-save"), i18n("Save zone"), this, SLOT(slotSaveZone()));
        QAction *extractZone = m_configMenu->addAction(KoIconUtils::themedIcon("document-new"), i18n("Extract Zone"), this, SLOT(slotExtractCurrentZone()));
        m_contextMenu->addAction(extractZone);
    }
    QAction *extractFrame = m_configMenu->addAction(KoIconUtils::themedIcon("document-new"), i18n("Extract frame"), this, SLOT(slotExtractCurrentFrame()));
    m_contextMenu->addAction(extractFrame);

    if (m_id == Kdenlive::ProjectMonitor) {
        QAction *splitView = m_contextMenu->addAction(KoIconUtils::themedIcon("view-split-left-right"), i18n("Split view"), render, SLOT(slotSplitView(bool)));
        splitView->setCheckable(true);
        m_configMenu->addAction(splitView);
    } else if (m_id == Kdenlive::ClipMonitor) {
        QAction *setThumbFrame = m_contextMenu->addAction(KoIconUtils::themedIcon("document-new"), i18n("Set current image as thumbnail"), this, SLOT(slotSetThumbFrame()));
        m_configMenu->addAction(setThumbFrame);
    }

    QAction *overlayAudio = m_contextMenu->addAction(QIcon(), i18n("Overlay audio waveform"));
    overlayAudio->setCheckable(true);
    connect(overlayAudio, SIGNAL(toggled(bool)), m_glMonitor, SLOT(slotSwitchAudioOverlay(bool)));
    overlayAudio->setChecked(KdenliveSettings::displayAudioOverlay());

    if (m_effectCompare) {
        m_configMenu->addSeparator();
        m_configMenu->addAction(m_effectCompare);
    }
    m_configMenu->addAction(overlayAudio);
}

void Monitor::slotGoToMarker(QAction *action)
{
    int pos = action->data().toInt();
    slotSeek(pos);
}

void Monitor::slotForceSize(QAction *a)
{
    int resizeType = a->data().toInt();
    int profileWidth = 320;
    int profileHeight = 200;
    if (resizeType > 0) {
        // calculate size
        QRect r = QApplication::desktop()->screenGeometry();
        profileWidth = m_glMonitor->profileSize().width() * resizeType / 100;
        profileHeight = m_glMonitor->profileSize().height() * resizeType / 100;
        if (profileWidth > r.width() * 0.8 || profileHeight > r.height() * 0.7) {
            // reset action to free resize
            QList< QAction * > list = m_forceSize->actions ();
            foreach(QAction *ac, list) {
                if (ac->data().toInt() == m_forceSizeFactor) {
                    m_forceSize->setCurrentAction(ac);
                    break;
                }
            }
            warningMessage(i18n("Your screen resolution is not sufficient for this action"));
            return;
        }
    }
    switch (resizeType) {
      case 100:
      case 50:
          // resize full size
          setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
          m_videoWidget->setMinimumSize(profileWidth, profileHeight);
          m_videoWidget->setMaximumSize(profileWidth, profileHeight);
          setMinimumSize(QSize(profileWidth, profileHeight + m_toolbar->height() + m_ruler->height()));
          break;
      default:
        // Free resize
        m_videoWidget->setMinimumSize(profileWidth, profileHeight);
        setMinimumSize(QSize(profileWidth, profileHeight + m_toolbar->height() + m_ruler->height()));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        break;
    }
    m_forceSizeFactor = resizeType;
    updateGeometry();
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
        checkOverlay();
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
    m_monitorManager->activateMonitor(m_id);
    if (!(event->button() & Qt::RightButton)) {
        if (m_glWidget->geometry().contains(event->pos())) {
            m_dragStarted = true;
            m_DragStartPosition = event->pos();
            event->accept();
        }
    } else if (m_contextMenu) {
        m_contextMenu->popup(event->globalPos());
        event->accept();
    }
}

void Monitor::slotShowMenu(const QPoint pos)
{
    m_contextMenu->popup(pos);
}

void Monitor::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
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
    if (m_glMonitor->zoom() == 1.0f) {
       /* m_zoomButton->setIcon(icon);
        m_zoomButton->setChecked(false);*/
        m_horizontalScroll->hide();
        m_verticalScroll->hide();
    } else {
        adjustScrollBars(0.5f, 0.5f);
    }
}

void Monitor::slotSwitchFullScreen(bool minimizeOnly)
{
    // TODO: disable screensaver?
    if (!m_glWidget->isFullScreen() && !minimizeOnly) {
        // Check if we have a multiple monitor setup
        int monitors = QApplication::desktop()->screenCount();
        int screen = -1;
        if (monitors > 1) {
            QRect screenres;
            // Move monitor widget to the second screen (one screen for Kdenlive, the other one for the Monitor widget
            //int currentScreen = QApplication::desktop()->screenNumber(this);
            for (int i = 0; screen == -1 && i < QApplication::desktop()->screenCount(); i++) {
            if (i != QApplication::desktop()->screenNumber(this->parentWidget()->parentWidget()))
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

void Monitor::reparent()
{
    m_glWidget->setParent(NULL);
    m_glWidget->showMinimized();
    m_glWidget->showNormal();
    QVBoxLayout *lay = (QVBoxLayout *) layout();
    lay->insertWidget(0, m_glWidget, 10);
}


// virtual
void Monitor::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_dragStarted && event->button() != Qt::RightButton) {
        if (m_glMonitor->geometry().contains(event->pos())) {
            if (isActive()) slotPlay();
            else {
              slotActivateMonitor();
            }
        } //else event->ignore(); //QWidget::mouseReleaseEvent(event);
    }
    m_dragStarted = false;
    event->accept();
}


void Monitor::slotStartDrag()
{
    if (m_id == Kdenlive::ProjectMonitor || m_controller == NULL) {
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
    event->accept();
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
    slotSwitchFullScreen();
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
    m_controller->setProperty("kdenlive:thumbnailFrame", (int) render->seekFramePosition());
    emit refreshClipThumbnail(m_controller->clipId());
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

void Monitor::slotExtractCurrentFrame(QString path)
{
    QString framesFolder = KRecentDirs::dir(":KdenliveFramesFolder");
    if (framesFolder.isEmpty()) framesFolder = QDir::homePath();
    QPointer<QFileDialog> fs = new QFileDialog(this, i18n("Save Image"), framesFolder);
    fs->setMimeTypeFilters(QStringList() << "image/png");
    fs->setAcceptMode(QFileDialog::AcceptSave);
    fs->selectFile(path);
    if (fs->exec()) {
        if (!fs->selectedFiles().isEmpty()) {
            QUrl savePath = fs->selectedUrls().first();
            if (QFile::exists(savePath.toLocalFile()) && KMessageBox::warningYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", savePath.toLocalFile())) == KMessageBox::No) {
                delete fs;
                slotExtractCurrentFrame(savePath.fileName());
                return;
            }
            // Create Qimage with frame
            QImage frame;
            // check if we are using a proxy
            if (m_controller && !m_controller->property("kdenlive:proxy").isEmpty() && m_controller->property("kdenlive:proxy") != "-") {
                // using proxy, use original clip url to get frame
                frame = render->extractFrame(render->seekFramePosition(), m_controller->property("resource"));
            }
            else frame = render->extractFrame(render->seekFramePosition());
            frame.save(savePath.toLocalFile());
            KRecentDirs::add(":KdenliveFramesFolder", savePath.adjusted(QUrl::RemoveFilename).path());
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
    if (!m_rootItem || m_rootItem->objectName() != "root") {
        // we are not in main view, ignore
        return;
    }
    QString overlayText;
    int pos = m_timePos->getValue();
    QPoint zone = m_ruler->zone();
    if (pos == zone.x())
        overlayText = i18n("In Point");
    else if (pos == zone.y())
        overlayText = i18n("Out Point");
    else if (m_controller) {
        overlayText = m_controller->markerComment(GenTime(pos, m_monitorManager->timecode().fps()));
    }
    if (overlayText != m_rootItem->property("comment")) m_rootItem->setProperty("comment", overlayText);
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
    render->seekToFrameDiff(-diff);
    m_ruler->update();
}

void Monitor::slotForwardOneFrame(int diff)
{
    slotActivateMonitor();
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
    m_playAction->setActive(false);
    if (render) {
        render->stop();
    }
}

void Monitor::mute(bool mute, bool updateIconOnly)
{
    if (render) {
        // TODO: we should set the "audio_off" property to 1 to mute the consumer instead of changing volume
        QIcon icon;
        if (mute || KdenliveSettings::volume() == 0) icon = KoIconUtils::themedIcon("audio-volume-muted");
        else icon = KoIconUtils::themedIcon("audio-volume-medium");
        m_audioButton->setIcon(icon);
        if (!updateIconOnly) render->setVolume(mute ? 0 : (double)KdenliveSettings::volume() / 100.0 );
    }
}

void Monitor::start()
{
    if (!isVisible() || !isActive()) return;
    if (render) {
        render->startConsumer();
    }
}

void Monitor::refreshMonitor(bool visible)
{
    if (visible && isActive()) {
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

void Monitor::switchPlay(bool play)
{
    m_playAction->setActive(play);
    render->switchPlay(play);
}

void Monitor::slotSwitchPlay(bool triggered)
{
    if (render == NULL) return;
    slotActivateMonitor();
    render->switchPlay(m_playAction->isActive());
}

void Monitor::slotPlay()
{
    if (render == NULL) return;
    slotActivateMonitor();
    m_playAction->setActive(!m_playAction->isActive());
    render->switchPlay(m_playAction->isActive());
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
    if (render->setProducer(prod, -1, false))
        prod->set_speed(1.0);
}

void Monitor::updateClipProducer(const QString &playlist)
{
    if (render == NULL) return;
    Mlt::Producer *prod = new Mlt::Producer(*m_glMonitor->profile(), playlist.toUtf8().constData());
    render->setProducer(prod, render->seekFramePosition(), true);
    render->play(1.0);
}

void Monitor::slotSeekController(ClipController *controller, int pos)
{
    if (controller != m_controller) {
        slotOpenClip(controller, pos, -1);
    }
    else slotSeek(pos);
}

void Monitor::slotOpenClip(ClipController *controller, int in, int out)
{
    if (render == NULL) return;
    bool sameClip = controller == m_controller && controller != NULL;
    m_controller = controller;
    if (m_rootItem && m_rootItem->objectName() != QLatin1String("root") && !sameClip) {
        // changed clip, disable split effect
        if (m_splitProducer) {
            m_effectCompare->blockSignals(true);
            m_effectCompare->setActive(false);
            m_effectCompare->setChecked(false);
            m_effectCompare->blockSignals(false);
            delete m_splitEffect;
            m_splitProducer = NULL;
            m_splitEffect = NULL;
        }
        loadMasterQml();
    }
    //bool hasEffects;
    if (controller) {
	if (m_recManager->toolbar()->isVisible()) {
	      // we are in record mode, don't display clip
	      return;
	}
        updateMarkers();
        render->setProducer(m_controller->masterProducer(), in, isActive());
	if (out > -1) {
	    m_ruler->setZone(in, out);
	    setClipZone(QPoint(in, out));
	}
	//hasEffects =  controller->hasEffects();
    }
    else {
        render->setProducer(NULL, -1, isActive());
        //hasEffects = false;
    }
}

void Monitor::enableCompare(int effectsCount)
{
    if (!m_effectCompare) return;
    if (effectsCount == 0) {
        m_effectCompare->setEnabled(false);
        m_effectCompare->setChecked(false);
        m_effectCompare->setActive(false);
        if (m_splitProducer) {
            delete m_splitEffect;
            m_splitProducer = NULL;
            m_splitEffect = NULL;
            loadMasterQml();
        }
    } else {
        m_effectCompare->setEnabled(true);
    }
}

const QString Monitor::activeClipId()
{
    if (m_controller) {
        return m_controller->clipId();
    }
    return QString();
}

void Monitor::slotOpenDvdFile(const QString &file)
{
    if (render == NULL) return;
    m_glMonitor->initializeGL();
    render->loadUrl(file);
}

void Monitor::slotSaveZone()
{
    render->saveZone(m_ruler->zone());
}

void Monitor::setCustomProfile(const QString &profile, const Timecode &tc)
{
    m_timePos->updateTimeCode(tc);
    if (render == NULL) return;
    slotActivateMonitor();
    render->prepareProfileReset();
    m_glMonitor->resetProfile(ProfilesDialog::getVideoProfile(profile));
}

void Monitor::resetProfile(MltVideoProfile profile)
{
    m_timePos->updateTimeCode(m_monitorManager->timecode());
    if (render == NULL) return;

    render->prepareProfileReset();
    m_glMonitor->resetProfile(profile);

    if (m_rootItem && m_rootItem->objectName() == "rooteffectscene") {
        // we are not in main view, ignore
        m_rootItem->setProperty("framesize", QRect(0, 0, m_glMonitor->profileSize().width(), m_glMonitor->profileSize().height()));
    }
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

void Monitor::switchDropFrames(bool drop)
{
    render->setDropFrames(drop);
}

void Monitor::switchMonitorInfo(bool show)
{
    KdenliveSettings::setDisplayMonitorInfo(show);
    if (m_rootItem && m_rootItem->objectName() != "root") {
        // we are not in main view, ignore
        return;
    }
    loadMasterQml();
}

void Monitor::updateMonitorGamma()
{
    if (isActive()) {
        stop();
        m_glMonitor->updateGamma();
        start();
    }
    else {
        m_glMonitor->updateGamma();
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
    MonitorSceneType sceneType = enable ? m_lastMonitorSceneType : MonitorSceneNone;
    slotShowEffectScene(sceneType, true);
    if (enable) {
        emit renderPosition(render->seekFramePosition());
    }
}

void Monitor::slotShowEffectScene(MonitorSceneType sceneType, bool temporary)
{
    if (!temporary) m_lastMonitorSceneType = sceneType;
    if (sceneType == MonitorSceneGeometry) {
        if (!m_rootItem || m_rootItem->objectName() != QLatin1String("rooteffectscene")) {
            m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitoreffectscene.qml"))));
            m_rootItem = m_glMonitor->rootObject();
            QObject::connect(m_rootItem, SIGNAL(addKeyframe()), this, SIGNAL(addKeyframe()), Qt::UniqueConnection);
            QObject::connect(m_rootItem, SIGNAL(seekToKeyframe()), this, SLOT(slotSeekToKeyFrame()), Qt::UniqueConnection);
            m_glMonitor->slotShowEffectScene(sceneType);
        }
    }
    else if (sceneType == MonitorSceneCorners) {
        if (!m_rootItem || m_rootItem->objectName() != QLatin1String("rootcornerscene")) {
            m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitorcornerscene.qml"))));
            m_rootItem = m_glMonitor->rootObject();
            QObject::connect(m_rootItem, SIGNAL(addKeyframe()), this, SIGNAL(addKeyframe()), Qt::UniqueConnection);
            QObject::connect(m_rootItem, SIGNAL(seekToKeyframe()), this, SLOT(slotSeekToKeyFrame()), Qt::UniqueConnection);
            m_glMonitor->slotShowEffectScene(sceneType);
        }
    }
    else if (sceneType == MonitorSceneRoto) {
        // TODO
        if (!m_rootItem || m_rootItem->objectName() != QLatin1String("rootcornerscene")) {
            m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitorcornerscene.qml"))));
            m_rootItem = m_glMonitor->rootObject();
            QObject::connect(m_rootItem, SIGNAL(addKeyframe()), this, SIGNAL(addKeyframe()), Qt::UniqueConnection);
            QObject::connect(m_rootItem, SIGNAL(seekToKeyframe()), this, SLOT(slotSeekToKeyFrame()), Qt::UniqueConnection);
            m_glMonitor->slotShowEffectScene(sceneType);
        }
    }
    else if (m_rootItem && m_rootItem->objectName() != QLatin1String("root") && m_rootItem->objectName() != QLatin1String("rootsplit"))  {
        if (m_effectCompare && m_effectCompare->isEnabled() && m_effectCompare->isActive()) {
            if (m_rootItem->objectName() != QLatin1String("rootsplit")) {
                slotSwitchCompare(true);
            }
        } 
        else loadMasterQml();
    }
    if (m_sceneVisibilityAction) m_sceneVisibilityAction->setChecked(sceneType != MonitorSceneNone);
}


void Monitor::slotSeekToKeyFrame()
{
    if (m_rootItem && m_rootItem->objectName() == "rooteffectscene") {
        // Adjust splitter pos
        int kfr = m_rootItem->property("requestedKeyFrame").toInt();
        emit seekToKeyframe(kfr);
    }
}

void Monitor::setUpEffectGeometry(QRect r, QVariantList list)
{
    if (!m_rootItem) return;
    if (!list.isEmpty()) {
        m_rootItem->setProperty("centerPoints", list);
    }
    if (!r.isEmpty()) m_rootItem->setProperty("framesize", r);
}

QRect Monitor::effectRect() const
{
    if (!m_rootItem) {
	return QRect();
    }
    return m_rootItem->property("framesize").toRect();
}

QVariantList Monitor::effectPolygon() const
{
    if (!m_rootItem) {
	return QVariantList();
    }
    return m_rootItem->property("centerPoints").toList();
}

void Monitor::setEffectKeyframe(bool enable)
{
    if (m_rootItem) {
	m_rootItem->setProperty("iskeyframe", enable);
    }
}

bool Monitor::effectSceneDisplayed(MonitorSceneType effectType)
{
    if (!m_rootItem) return false;
    switch (effectType) {
      case MonitorSceneGeometry:
          return m_rootItem->objectName() == "rooteffectscene";
          break;
      case MonitorSceneCorners:
          return m_rootItem->objectName() == "rootcornerscene";
          break;
      case MonitorSceneRoto:
          return m_rootItem->objectName() == "rootrotoscene";
          break;
      default:
          return m_rootItem->objectName() == "root";
          break;
    }
}

void Monitor::slotSetVolume(int volume)
{
    KdenliveSettings::setVolume(volume);
    QIcon icon;
    double renderVolume = render->volume();
    render->setVolume((double) volume / 100.0);
    if (renderVolume > 0 && volume > 0) return;
    if (volume == 0) icon = KoIconUtils::themedIcon("audio-volume-muted");
    else icon = KoIconUtils::themedIcon("audio-volume-medium");
    m_audioButton->setIcon(icon);
}

void Monitor::sendFrameForAnalysis(bool analyse)
{
    m_glMonitor->sendFrameForAnalysis = analyse;
}

void Monitor::updateAudioForAnalysis()
{
    m_glMonitor->updateAudioForAnalysis();
}

void Monitor::onFrameDisplayed(const SharedFrame& frame)
{
    int position = frame.get_position();
    seekCursor(position);
    if (!render->checkFrameNumber(position)) {
        m_playAction->setActive(false);
    }
    /*
    // display frame number in monitor overlay, currently disabled
    if (m_rootItem && m_rootItem->objectName() == "root") {
        // we are in main view, show frame
        m_rootItem->setProperty("framenum", QString::number(position));
    }*/
    if (position >= m_length) {
        m_playAction->setActive(false);
    }
}

AbstractRender *Monitor::abstractRender()
{
    return render;
}

void Monitor::reloadProducer(const QString &id)
{
    if (!m_controller) return;
    if (m_controller->clipId() == id)
        slotOpenClip(m_controller);
}

QString Monitor::getMarkerThumb(GenTime pos)
{
    if (!m_controller) return QString();
    if (!m_controller->getClipHash().isEmpty()) {
	QString url = m_monitorManager->getProjectFolder() + "thumbs/" + m_controller->getClipHash() + '#' + QString::number((int) pos.frames(m_monitorManager->timecode().fps())) + ".png";
        if (QFile::exists(url)) return url;
    }
    return QString();
}

const QString Monitor::projectFolder() const
{
      return m_monitorManager->getProjectFolder();
}

void Monitor::setPalette ( const QPalette & p)
{
    QWidget::setPalette(p);
    if (m_ruler) m_ruler->updatePalette();
}


void Monitor::warningMessage(const QString &text)
{
    m_infoMessage->setMessageType(KMessageWidget::Warning);
    m_infoMessage->setText(text);
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->animatedShow();
    QTimer::singleShot(5000, m_infoMessage, SLOT(animatedHide()));
 
}

void Monitor::slotSwitchCompare(bool enable)
{
    if (m_controller == NULL || !m_controller->hasEffects()) {
        // disable split effect
        enableCompare(0);
        if (m_controller) {
            warningMessage(i18n("Clip has no effects"));
        }
        else {
            warningMessage(i18n("Select a clip in project bin to compare effect"));
        }
        return;
    }
    int pos = position().frames(m_monitorManager->timecode().fps());
    if (enable) {
        if ((m_rootItem && m_rootItem->objectName() == QLatin1String("rootsplit"))) {
            // Split scene is already active
            return;
        }
        m_splitEffect = new Mlt::Filter(*profile(), "frei0r.scale0tilt");
        if (m_splitEffect && m_splitEffect->is_valid()) {
            m_splitEffect->set("Clip left", 0.5);
        } else {
            // frei0r.scal0tilt is not available
            warningMessage(i18n("The scal0tilt filter is required for that feature, please install frei0r and restart Kdenlive"));
            return;
        }
        QString splitTransition = KdenliveSettings::gpu_accel() ? "movit.overlay" : "frei0r.cairoblend";
        Mlt::Transition t(*profile(), splitTransition.toUtf8().constData());
        if (!t.is_valid()) {
            delete m_splitEffect;
            warningMessage(i18n("The cairoblend transition is required for that feature, please install frei0r and restart Kdenlive"));
            return;
        }
        Mlt::Producer *original = m_controller->masterProducer();
        Mlt::Tractor trac(*profile());
	Clip clp(*original);
        Mlt::Producer *clone = clp.clone();
	Clip clp2(*clone);
	clp2.deleteEffects();
        trac.set_track(*original, 0);
        trac.set_track(*clone, 1);
        clone->attach(*m_splitEffect);
        trac.plant_transition(t, 0, 1);
	delete clone;
	delete original;
        m_splitProducer = new Mlt::Producer(trac.get_producer());
        render->setProducer(m_splitProducer, pos, isActive());
        m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitorsplit.qml"))));
        m_rootItem = m_glMonitor->rootObject();
        QObject::connect(m_rootItem, SIGNAL(qmlMoveSplit()), this, SLOT(slotAdjustEffectCompare()), Qt::UniqueConnection);
    }
    else if (m_splitEffect) {
        render->setProducer(m_controller->masterProducer(), pos, isActive());
        delete m_splitEffect;
        m_splitProducer = NULL;
        m_splitEffect = NULL;
        loadMasterQml();
    }
    slotActivateMonitor();
}

void Monitor::loadMasterQml()
{
    if (!KdenliveSettings::displayMonitorInfo()) {
        // We don't want info overlay
        if (m_rootItem) {
            m_glMonitor->setSource(QUrl());
            m_rootItem = NULL;
        }
        return;
    }
    if ((m_rootItem && m_rootItem->objectName() == "root")) {
        // Root scene is already active
        return;
    }
    m_glMonitor->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivemonitor.qml"))));
    m_glMonitor->slotShowEffectScene(MonitorSceneNone);
    m_rootItem = m_glMonitor->rootObject();
}

void Monitor::slotAdjustEffectCompare()
{
    QRect r = m_glMonitor->rect();
    double percent = 0.5;
    if (m_rootItem && m_rootItem->objectName() == "rootsplit") {
        // Adjust splitter pos
        percent = (m_rootItem->property("splitterPos").toInt() - r.left()) / (double) r.width();
        // Store real frame percentage for resize events
        m_rootItem->setProperty("realpercent", percent);
    }
    if (m_splitEffect) m_splitEffect->set("Clip left", percent);
    render->doRefresh();
}

ProfileInfo Monitor::profileInfo() const
{
    ProfileInfo info;
    info.profileSize = QSize(render->frameRenderWidth(), render->renderHeight());
    info.profileFps = render->fps();
    return info;
}

Mlt::Profile *Monitor::profile()
{
    return m_glMonitor->profile();
}

void Monitor::slotSwitchRec(bool enable)
{
    if (!m_recManager) return;
    if (enable) {
        m_toolbar->setVisible(false);
        m_recManager->toolbar()->setVisible(true);
    }
    else if (m_recManager->toolbar()->isVisible()) {
        m_recManager->stop();
        m_toolbar->setVisible(true);
	emit refreshCurrentClip();
    }
}

bool Monitor::startCapture(const QString &params, const QString &path, Mlt::Producer *p)
{
    m_controller = NULL;
    if (render->updateProducer(p)) {
        m_glMonitor->reconfigureMulti(params, path, p->profile());
        return true;
    } else return false;
}

bool Monitor::stopCapture()
{
    m_glMonitor->stopCapture();
    slotOpenClip(NULL);
    m_glMonitor->reconfigure(profile());
    return true;
}


