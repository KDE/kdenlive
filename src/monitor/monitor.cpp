/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "monitor.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "capture/mediacapture.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "jobs/cuttask.h"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "lib/localeHandling.h"
#include "mainwindow.h"
#include "mltcontroller/clipcontroller.h"
#include "project/dialogs/guideslist.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include "videowidget.h"
#if defined(Q_OS_WIN)
#include "d3dvideowidget.h"
#include "openglvideowidget.h"
#elif defined(Q_OS_MACOS)
#include "metalvideowidget.h"
#else
// Linux
#include "openglvideowidget.h"
#endif
#else // Qt6
#include "glwidget.h"
#endif

#include "bin/model/markersortmodel.h"
#include "monitormanager.h"
#include "monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "qmlmanager.h"
#include "recmanager.h"
#include "scopes/monitoraudiolevel.h"
#include "timeline2/model/snapmodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "transitions/transitionsrepository.hpp"
#include "utils/thumbnailcache.hpp"

#include "KLocalizedString"
#include <KActionMenu>
#include <KDualAction>
#include <KFileWidget>
#include <KMessageBox>
#include <KMessageWidget>
#include <KRecentDirs>
#include <KSelectAction>
#include <KWindowConfig>
#include <kio_version.h>
#include <kwidgetsaddons_version.h>

#include "kdenlive_debug.h"
#include <QCheckBox>
#include <QDrag>
#include <QFontDatabase>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QQuickItem>
#include <QScreen>
#include <QScrollBar>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <utility>

#define SEEK_INACTIVE (-1)

VolumeAction::VolumeAction(QObject *parent)
    : QWidgetAction(parent)
{
}

QWidget *VolumeAction::createWidget(QWidget *parent)
{
    auto *hlay = new QHBoxLayout(parent);
    auto *iconLabel = new QLabel();
    iconLabel->setToolTip(i18n("Audio volume"));
    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(0, 100);
    auto *percentLabel = new QLabel(parent);
    connect(slider, &QSlider::valueChanged, this, [percentLabel, iconLabel](int value) {
        percentLabel->setText(i18n("%1%", value));
        int h = 16;
        QString iconName(QStringLiteral("audio-volume-high"));
        if (value == 0) {
            iconName = QStringLiteral("audio-volume-muted");
        } else if (value < 33) {
            iconName = QStringLiteral("audio-volume-low");
        } else if (value < 66) {
            iconName = QStringLiteral("audio-volume-medium");
        }
        iconLabel->setPixmap(QIcon::fromTheme(iconName).pixmap(h, h));
    });
    slider->setValue(KdenliveSettings::volume());
    connect(slider, &QSlider::valueChanged, this, &VolumeAction::volumeChanged);
    hlay->addWidget(iconLabel);
    hlay->addWidget(slider);
    hlay->addWidget(percentLabel);
    auto w = new QWidget(parent);
    w->setLayout(hlay);
    return w;
}

Monitor::Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent)
    : AbstractMonitor(id, manager, parent)
    , m_controller(nullptr)
    , m_glMonitor(nullptr)
    , m_snaps(new SnapModel())
    , m_splitEffect(nullptr)
    , m_splitProducer(nullptr)
    , m_dragStarted(false)
    , m_recManager(nullptr)
    , m_loopClipAction(nullptr)
    , m_contextMenu(nullptr)
    , m_markerMenu(nullptr)
    , m_audioChannels(nullptr)
    , m_loopClipTransition(true)
    , m_editMarker(nullptr)
    , m_forceSizeFactor(0)
    , m_lastMonitorSceneType(MonitorSceneDefault)
    , m_displayingCountdown(true)
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    // Create container widget
    m_glWidget = new QWidget(this);
    m_glWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    auto *glayout = new QGridLayout(m_glWidget);
    glayout->setSpacing(0);
    glayout->setContentsMargins(0, 0, 0, 0);
    // Create QML OpenGL widget
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WIN)
    if (QSGRendererInterface::Direct3D11 == QQuickWindow::graphicsApi())
        m_glMonitor = new D3DVideoWidget(id, this);
    else
        m_glMonitor = new OpenGLVideoWidget(id, this);
#elif defined(Q_OS_MACOS)
    m_glMonitor = new MetalVideoWidget(id, this);
#else
    m_glMonitor = new OpenGLVideoWidget(id, this);
#endif
    //  The m_glMonitor quickWindow() can be destroyed on undock with some graphics interface (Windows/Mac), so reconnect on destroy
    auto rebuildViewConnection = [this]() {
        connect(m_glMonitor->quickWindow(), &QQuickWindow::sceneGraphInitialized, m_glMonitor, &VideoWidget::initialize, Qt::DirectConnection);
        connect(m_glMonitor->quickWindow(), &QQuickWindow::beforeRendering, m_glMonitor, &VideoWidget::beforeRendering, Qt::DirectConnection);
        connect(m_glMonitor->quickWindow(), &QQuickWindow::beforeRenderPassRecording, m_glMonitor, &VideoWidget::renderVideo, Qt::DirectConnection);
        m_glMonitor->setClearColor(KdenliveSettings::window_background());
        // Enforce geometry recalculation
        m_glMonitor->refreshZoom = true;
        m_glMonitor->reconnectWindow();
    };

    connect(m_glMonitor, &VideoWidget::reconnectWindow, [this, rebuildViewConnection]() {
        connect(m_glMonitor->quickWindow(), &QQuickWindow::destroyed, [rebuildViewConnection]() { rebuildViewConnection(); });
    });

    rebuildViewConnection();
#else
    m_glMonitor = new VideoWidget(id, this);
#endif
    connect(m_glMonitor, &VideoWidget::passKeyEvent, this, &Monitor::doKeyPressEvent);
    connect(m_glMonitor, &VideoWidget::panView, this, &Monitor::panView);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::requestSeek, this, &Monitor::processSeek, Qt::DirectConnection);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::positionChanged, this, &Monitor::slotSeekPosition);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::addTimelineEffect, this, &Monitor::addTimelineEffect);

    m_qmlManager = new QmlManager(m_glMonitor);
    connect(m_qmlManager, &QmlManager::effectChanged, this, &Monitor::effectChanged);
    connect(m_qmlManager, &QmlManager::effectPointsChanged, this, &Monitor::effectPointsChanged);
    connect(m_qmlManager, &QmlManager::activateTrack, this, [&](int ix) { Q_EMIT activateTrack(ix, false); });

    glayout->addWidget(m_glMonitor, 0, 0);
    m_verticalScroll = new QScrollBar(Qt::Vertical);
    glayout->addWidget(m_verticalScroll, 0, 1);
    m_verticalScroll->hide();
    m_horizontalScroll = new QScrollBar(Qt::Horizontal);
    glayout->addWidget(m_horizontalScroll, 1, 0);
    m_horizontalScroll->hide();
    connect(m_horizontalScroll, &QAbstractSlider::valueChanged, this, &Monitor::setOffsetX);
    connect(m_verticalScroll, &QAbstractSlider::valueChanged, this, &Monitor::setOffsetY);
    connect(m_glMonitor, &VideoWidget::frameDisplayed, this, &Monitor::onFrameDisplayed, Qt::DirectConnection);
    connect(m_glMonitor, &VideoWidget::mouseSeek, this, &Monitor::slotMouseSeek);
    connect(m_glMonitor, &VideoWidget::switchFullScreen, this, &Monitor::slotSwitchFullScreen);
    connect(m_glMonitor, &VideoWidget::zoomChanged, this, &Monitor::setZoom);
    connect(m_glMonitor, SIGNAL(lockMonitor(bool)), this, SLOT(slotLockMonitor(bool)), Qt::DirectConnection);
    connect(m_glMonitor, &VideoWidget::showContextMenu, this, &Monitor::slotShowMenu);
    connect(m_glMonitor, &VideoWidget::gpuNotSupported, this, &Monitor::gpuError);

    m_glWidget->setMinimumSize(QSize(320, 180));
    layout->addWidget(m_glWidget, 10);
    layout->addStretch();

    // Tool bar buttons
    m_toolbar = new QToolBar(this);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);
    m_toolbar->setIconSize(iconSize);

    auto *scalingAction = new QComboBox(this);
    scalingAction->setToolTip(i18n("Preview resolution - lower resolution means faster preview"));
    scalingAction->setWhatsThis(xi18nc("@info:whatsthis", "Sets the preview resolution of the project/clip monitor. One can select between 1:1, 720p, 540p, "
                                                          "360p, 270p (the lower the resolution the faster the preview)."));
    // Combobox padding is bad, so manually add a space before text
    scalingAction->addItems({QStringLiteral(" ") + i18n("1:1"), QStringLiteral(" ") + i18n("720p"), QStringLiteral(" ") + i18n("540p"),
                             QStringLiteral(" ") + i18n("360p"), QStringLiteral(" ") + i18n("270p")});
    connect(scalingAction, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        switch (index) {
        case 1:
            KdenliveSettings::setPreviewScaling(2);
            break;
        case 2:
            KdenliveSettings::setPreviewScaling(4);
            break;
        case 3:
            KdenliveSettings::setPreviewScaling(8);
            break;
        case 4:
            KdenliveSettings::setPreviewScaling(16);
            break;
        default:
            KdenliveSettings::setPreviewScaling(0);
        }
        Q_EMIT m_monitorManager->scalingChanged();
        Q_EMIT m_monitorManager->updatePreviewScaling();
        m_monitorManager->refreshMonitors();
    });

    connect(manager, &MonitorManager::updatePreviewScaling, this, [this, scalingAction]() {
        m_glMonitor->updateScaling();
        switch (KdenliveSettings::previewScaling()) {
        case 2:
            scalingAction->setCurrentIndex(1);
            break;
        case 4:
            scalingAction->setCurrentIndex(2);
            break;
        case 8:
            scalingAction->setCurrentIndex(3);
            break;
        case 16:
            scalingAction->setCurrentIndex(4);
            break;
        default:
            scalingAction->setCurrentIndex(0);
            break;
        }
    });
    scalingAction->setFrame(false);
    scalingAction->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_toolbar->addWidget(scalingAction);
    m_toolbar->addSeparator();

    if (id == Kdenlive::ClipMonitor) {
        // Add options for recording
        m_recManager = new RecManager(this);
        connect(m_recManager, &RecManager::warningMessage, this, &Monitor::warningMessage);
        connect(m_recManager, &RecManager::addClipToProject, this, &Monitor::addClipToProject);
        connect(m_glMonitor, &VideoWidget::startDrag, this, &Monitor::slotStartDrag);
        connect(pCore.get(), &Core::binClipDeleted, m_glMonitor->getControllerProxy(), &MonitorProxy::clipDeleted);
        // Show timeline clip usage
        connect(pCore.get(), &Core::clipInstanceResized, this, [this](const QString &binId) {
            if (m_controller && activeClipId() == binId) {
                m_controller->checkClipBounds();
            }
        });

        m_toolbar->addAction(manager->getAction(QStringLiteral("insert_project_tree")));
        m_toolbar->addSeparator();
        m_streamsButton = new QToolButton(this);
        m_streamsButton->setPopupMode(QToolButton::InstantPopup);
        m_streamsButton->setIcon(QIcon::fromTheme(QStringLiteral("speaker")));
        m_streamsButton->setToolTip(i18n("Audio streams"));
        m_streamAction = m_toolbar->addWidget(m_streamsButton);
        m_audioChannels = new QMenu(this);
        m_streamsButton->setMenu(m_audioChannels);
        m_streamAction->setVisible(false);

        // Connect job data
        connect(&pCore->taskManager, &TaskManager::detailedProgress, m_glMonitor->getControllerProxy(), &MonitorProxy::setJobsProgress);

        connect(m_audioChannels, &QMenu::triggered, this, [this](QAction *ac) {
            // m_audioChannels->show();
            QList<QAction *> actions = m_audioChannels->actions();
            QMap<int, QString> enabledStreams;
            QVector<int> streamsList;
            if (ac->data().toInt() == INT_MAX) {
                // Merge stream selected, clear all others
                enabledStreams.clear();
                enabledStreams.insert(INT_MAX, i18n("Merged streams"));
                // Disable all other streams
                QSignalBlocker bk(m_audioChannels);
                for (auto act : qAsConst(actions)) {
                    if (act->isChecked() && act != ac) {
                        act->setChecked(false);
                    }
                    if (act->data().toInt() != INT_MAX) {
                        streamsList << act->data().toInt();
                    }
                }
            } else {
                for (auto act : qAsConst(actions)) {
                    if (act->isChecked()) {
                        // Audio stream is selected
                        if (act->data().toInt() == INT_MAX) {
                            QSignalBlocker bk(m_audioChannels);
                            act->setChecked(false);
                        } else {
                            enabledStreams.insert(act->data().toInt(), act->text().remove(QLatin1Char('&')));
                        }
                    }
                    if (act->data().toInt() != INT_MAX) {
                        streamsList << act->data().toInt();
                    }
                }
            }
            if (!enabledStreams.isEmpty()) {
                // Only 1 stream wanted, easy
                QMap<QString, QString> props;
                props.insert(QStringLiteral("audio_index"), QString::number(enabledStreams.firstKey()));
                props.insert(QStringLiteral("astream"), QString::number(streamsList.indexOf(enabledStreams.firstKey())));
                QList<int> streams = enabledStreams.keys();
                QStringList astreams;
                for (const int st : qAsConst(streams)) {
                    astreams << QString::number(st);
                }
                props.insert(QStringLiteral("kdenlive:active_streams"), astreams.join(QLatin1Char(';')));
                m_controller->setProperties(props, true);
            } else {
                // No active stream
                QMap<QString, QString> props;
                props.insert(QStringLiteral("audio_index"), QStringLiteral("-1"));
                props.insert(QStringLiteral("astream"), QStringLiteral("-1"));
                props.insert(QStringLiteral("kdenlive:active_streams"), QStringLiteral("-1"));
                m_controller->setProperties(props, true);
            }
        });
    } else if (id == Kdenlive::ProjectMonitor) {
        // JBM - This caused the track audio levels to go blank on pause, doesn't seem to have another use
        // connect(m_glMonitor, &VideoWidget::paused, m_monitorManager, &MonitorManager::cleanMixer);
    }

    m_markIn = new QAction(QIcon::fromTheme(QStringLiteral("zone-in")), i18n("Set Zone In"), this);
    m_markOut = new QAction(QIcon::fromTheme(QStringLiteral("zone-out")), i18n("Set Zone Out"), this);
    m_toolbar->addAction(m_markIn);
    m_toolbar->addAction(m_markOut);
    connect(m_markIn, &QAction::triggered, this, [&, manager]() {
        m_monitorManager->activateMonitor(m_id);
        manager->getAction(QStringLiteral("mark_in"))->trigger();
    });
    connect(m_markOut, &QAction::triggered, this, [&, manager]() {
        m_monitorManager->activateMonitor(m_id);
        manager->getAction(QStringLiteral("mark_out"))->trigger();
    });
    // Per monitor rewind action
    QAction *rewind = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-backward")), i18n("Rewind"), this);
    m_toolbar->addAction(rewind);
    connect(rewind, &QAction::triggered, this, [&]() { Monitor::slotRewind(); });

    auto *playButton = new QToolButton(m_toolbar);
    m_playMenu = new QMenu(i18n("Play"), this);
    connect(m_playMenu, &QMenu::aboutToShow, this, &Monitor::slotActivateMonitor);
    QAction *originalPlayAction = static_cast<KDualAction *>(manager->getAction(QStringLiteral("monitor_play")));
    m_playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    m_playAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_playAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    connect(m_glMonitor, &VideoWidget::monitorPlay, m_playAction, &QAction::trigger);

    QString strippedTooltip = m_playAction->toolTip().remove(QRegularExpression(QStringLiteral("\\s\\(.*\\)")));
    // append shortcut if it exists for action
    if (originalPlayAction->shortcut() == QKeySequence(0)) {
        m_playAction->setToolTip(strippedTooltip);
    } else {
        m_playAction->setToolTip(strippedTooltip + QStringLiteral(" (") + originalPlayAction->shortcut().toString() + QLatin1Char(')'));
    }
    m_playMenu->addAction(m_playAction);
    connect(m_playAction, &QAction::triggered, this, &Monitor::slotSwitchPlay);

    playButton->setMenu(m_playMenu);
    playButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(playButton);

    // Per monitor forward action
    QAction *forward = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-forward")), i18n("Forward"), this);
    m_toolbar->addAction(forward);
    connect(forward, &QAction::triggered, this, [this]() { Monitor::slotForward(); });

    m_configMenuAction = new KActionMenu(QIcon::fromTheme(QStringLiteral("application-menu")), i18n("More Options…"), m_toolbar);
    m_configMenuAction->setWhatsThis(xi18nc("@info:whatsthis", "Opens the list of project/clip monitor options (e.g. audio volume, monitor size)."));
    m_configMenuAction->setPopupMode(QToolButton::InstantPopup);
    connect(m_configMenuAction->menu(), &QMenu::aboutToShow, this, &Monitor::updateMarkers);

    playButton->setDefaultAction(m_playAction);
    auto *volumeAction = new VolumeAction(this);
    connect(volumeAction, &VolumeAction::volumeChanged, this, &Monitor::slotSetVolume);
    m_configMenuAction->addAction(volumeAction);

    m_markerMenu = new KActionMenu(id == Kdenlive::ClipMonitor ? i18n("Go to Marker…") : i18n("Go to Guide…"), this);
    m_markerMenu->setEnabled(false);
    m_configMenuAction->addAction(m_markerMenu);
    connect(m_markerMenu->menu(), &QMenu::triggered, this, &Monitor::slotGoToMarker);
    m_forceSize = new KSelectAction(QIcon::fromTheme(QStringLiteral("transform-scale")), i18n("Force Monitor Size"), this);
    QAction *fullAction = m_forceSize->addAction(QIcon(), i18n("Force 100%"));
    fullAction->setData(100);
    QAction *halfAction = m_forceSize->addAction(QIcon(), i18n("Force 50%"));
    halfAction->setData(50);
    QAction *freeAction = m_forceSize->addAction(QIcon(), i18n("Free Resize"));
    freeAction->setData(0);
    m_configMenuAction->addAction(m_forceSize);
    m_forceSize->setCurrentAction(freeAction);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
    connect(m_forceSize, &KSelectAction::actionTriggered, this, &Monitor::slotForceSize);
#else
    connect(m_forceSize, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &Monitor::slotForceSize);
#endif

    if (m_id == Kdenlive::ClipMonitor) {
        m_background = new KSelectAction(QIcon::fromTheme(QStringLiteral("paper-color")), i18n("Background Color"), this);
        QAction *blackAction = m_background->addAction(QIcon(), i18n("Black"));
        blackAction->setData("black");
        QAction *whiteAction = m_background->addAction(QIcon(), i18n("White"));
        whiteAction->setData("white");
        QAction *pinkAction = m_background->addAction(QIcon(), i18n("Pink"));
        pinkAction->setData("#ff00ff");
        m_configMenuAction->addAction(m_background);
        if (KdenliveSettings::monitor_background() == whiteAction->data().toString()) {
            m_background->setCurrentAction(whiteAction);
        } else if (KdenliveSettings::monitor_background() == pinkAction->data().toString()) {
            m_background->setCurrentAction(pinkAction);
        } else {
            m_background->setCurrentAction(blackAction);
        }
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
        connect(m_background, &KSelectAction::actionTriggered, this, [this](QAction *a) {
#else
        connect(m_background, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, [this](QAction *a) {
#endif
            KdenliveSettings::setMonitor_background(a->data().toString());
            buildBackgroundedProducer(position());
        });
    }

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setLayout(layout);
    setMinimumHeight(200);

    connect(this, &Monitor::scopesClear, m_glMonitor, &VideoWidget::releaseAnalyse, Qt::DirectConnection);
    connect(m_glMonitor, &VideoWidget::analyseFrame, this, &Monitor::frameUpdated);
    m_timePos = new TimecodeDisplay(this);

    if (id == Kdenlive::ProjectMonitor) {
        connect(m_glMonitor->getControllerProxy(), &MonitorProxy::saveZone, this, &Monitor::zoneUpdated);
        connect(m_glMonitor->getControllerProxy(), &MonitorProxy::saveZoneWithUndo, this, &Monitor::zoneUpdatedWithUndo);
    } else if (id == Kdenlive::ClipMonitor) {
        connect(m_glMonitor->getControllerProxy(), &MonitorProxy::saveZone, this, &Monitor::updateClipZone);
    }
    m_glMonitor->getControllerProxy()->setTimeCode(m_timePos);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::triggerAction, pCore.get(), &Core::triggerAction);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::seekNextKeyframe, this, &Monitor::seekToNextKeyframe);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::seekPreviousKeyframe, this, &Monitor::seekToPreviousKeyframe);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::addRemoveKeyframe, this, &Monitor::addRemoveKeyframe);
    connect(m_glMonitor->getControllerProxy(), &MonitorProxy::seekToKeyframe, this, &Monitor::slotSeekToKeyFrame);

    m_toolbar->addAction(manager->getAction(QStringLiteral("monitor_editmode")));

    m_toolbar->addSeparator();
    m_toolbar->addWidget(m_timePos);
    m_toolbar->addAction(m_configMenuAction);
    m_toolbar->addSeparator();
    QMargins mrg = m_toolbar->contentsMargins();
    m_audioMeterWidget = new MonitorAudioLevel(m_toolbar->height() - mrg.top() - mrg.bottom(), this);
    m_toolbar->addWidget(m_audioMeterWidget);
    if (!m_audioMeterWidget->isValid) {
        KdenliveSettings::setMonitoraudio(0x01);
        m_audioMeterWidget->setVisibility(false);
    } else {
        m_audioMeterWidget->setVisibility((KdenliveSettings::monitoraudio() & m_id) != 0);
        if (id == Kdenlive::ProjectMonitor) {
            connect(m_audioMeterWidget, &MonitorAudioLevel::audioLevelsAvailable, pCore.get(), &Core::audioLevelsAvailable);
        }
    }

    // Trimming tool bar buttons
    m_trimmingbar = new QToolBar(this);
    m_trimmingbar->setIconSize(iconSize);

    m_trimmingOffset = new QLabel();
    m_trimmingbar->addWidget(m_trimmingOffset);

    m_fiveLess = new QAction(i18n("-5"), this);
    m_trimmingbar->addAction(m_fiveLess);
    connect(m_fiveLess, &QAction::triggered, this, [&]() {
        slotTrimmingPos(-5);
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(-5, true);
    });
    m_oneLess = new QAction(i18n("-1"), this);
    m_trimmingbar->addAction(m_oneLess);
    connect(m_oneLess, &QAction::triggered, this, [&]() {
        slotTrimmingPos(-1);
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(-1, true);
    });
    m_oneMore = new QAction(i18n("+1"), this);
    m_trimmingbar->addAction(m_oneMore);
    connect(m_oneMore, &QAction::triggered, this, [&]() {
        slotTrimmingPos(1);
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(1, true);
    });
    m_fiveMore = new QAction(i18n("+5"), this);
    m_trimmingbar->addAction(m_fiveMore);
    connect(m_fiveMore, &QAction::triggered, this, [&]() {
        slotTrimmingPos(5);
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(5, true);
    });

    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotSeek()));
    layout->addWidget(m_toolbar);
    if (m_recManager) {
        layout->addWidget(m_recManager->toolbar());
    }
    layout->addWidget(m_trimmingbar);
    m_trimmingbar->setVisible(false);

    // Load monitor overlay qml
    loadQmlScene(MonitorSceneDefault);

    // Monitor dropped fps timer
    m_droppedTimer.setInterval(1000);
    m_droppedTimer.setSingleShot(false);
    connect(&m_droppedTimer, &QTimer::timeout, this, &Monitor::checkDrops);

    // Info message widget
    m_infoMessage = new KMessageWidget(this);
    layout->addWidget(m_infoMessage);
    m_infoMessage->hide();
    if (m_id == Kdenlive::ProjectMonitor) {
        if (!KdenliveSettings::project_monitor_fullscreen().isEmpty()) {
            slotSwitchFullScreen();
        }
    } else if (!KdenliveSettings::clip_monitor_fullscreen().isEmpty()) {
        slotSwitchFullScreen();
    }
}

Monitor::~Monitor()
{
    m_markerModel.reset();
    delete m_audioMeterWidget;
    delete m_glMonitor;
    delete m_glWidget;
    delete m_timePos;
}

void Monitor::setOffsetX(int x)
{
    m_glMonitor->setOffsetX(x, m_horizontalScroll->maximum());
}

void Monitor::setOffsetY(int y)
{
    m_glMonitor->setOffsetY(y, m_verticalScroll->maximum());
}

void Monitor::slotGetCurrentImage(bool request)
{
    m_glMonitor->sendFrameForAnalysis = request;
    if (request) {
        slotActivateMonitor();
        refreshMonitor(true);
        // Update analysis state
        QTimer::singleShot(500, m_monitorManager, &MonitorManager::checkScopes);
    } else {
        m_glMonitor->releaseAnalyse();
    }
}

void Monitor::refreshIcons()
{
    QList<QAction *> allMenus = this->findChildren<QAction *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QAction *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
    QList<KDualAction *> allButtons = this->findChildren<KDualAction *>();
    for (int i = 0; i < allButtons.count(); i++) {
        KDualAction *m = allButtons.at(i);
        QIcon ic = m->activeIcon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setActiveIcon(newIcon);
        ic = m->inactiveIcon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        newIcon = QIcon::fromTheme(ic.name());
        m->setInactiveIcon(newIcon);
    }
}

QAction *Monitor::recAction()
{
    if (m_recManager) {
        return m_recManager->recAction();
    }
    return nullptr;
}

void Monitor::slotLockMonitor(bool lock)
{
    m_monitorManager->lockMonitor(m_id, lock);
}

void Monitor::setupMenu(QMenu *goMenu, QMenu *overlayMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu, QAction *loopClip)
{
    delete m_contextMenu;
    m_contextMenu = new QMenu(this);
    m_contextMenu->addMenu(m_playMenu);
    if (goMenu) {
        m_contextMenu->addMenu(goMenu);
    }

    if (markerMenu) {
        m_contextMenu->addMenu(markerMenu);
        QList<QAction *> list = markerMenu->actions();
        for (int i = 0; i < list.count(); ++i) {
            if (list.at(i)->objectName() == QLatin1String("edit_marker")) {
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

    m_contextMenu->addAction(m_markerMenu);
    if (m_id == Kdenlive::ClipMonitor) {
        // m_contextMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), i18n("Save zone"), this, SLOT(slotSaveZone()));
        auto *extractZone = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Extract Zone"), this);
        connect(extractZone, &QAction::triggered, this, &Monitor::slotExtractCurrentZone);
        m_configMenuAction->addAction(extractZone);
        m_contextMenu->addAction(extractZone);
        m_contextMenu->addAction(m_monitorManager->getAction(QStringLiteral("insert_project_tree")));
    }
    m_contextMenu->addAction(m_monitorManager->getAction(QStringLiteral("extract_frame")));
    m_contextMenu->addAction(m_monitorManager->getAction(QStringLiteral("extract_frame_to_project")));
    m_contextMenu->addAction(m_monitorManager->getAction(QStringLiteral("add_project_note")));

    m_contextMenu->addAction(m_markIn);
    m_contextMenu->addAction(m_markOut);
    QAction *setThumbFrame =
        m_contextMenu->addAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Set current image as thumbnail"), this, SLOT(slotSetThumbFrame()));
    m_configMenuAction->addAction(setThumbFrame);
    if (m_id == Kdenlive::ProjectMonitor) {
        m_contextMenu->addAction(m_monitorManager->getAction(QStringLiteral("monitor_multitrack")));
    } else if (m_id == Kdenlive::ClipMonitor) {
        QAction *alwaysShowAudio = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-show-audiothumb")), i18n("Always show audio thumbnails"), this);
        alwaysShowAudio->setCheckable(true);
        connect(alwaysShowAudio, &QAction::triggered, this, [this](bool checked) {
            KdenliveSettings::setAlwaysShowMonitorAudio(checked);
            m_glMonitor->rootObject()->setProperty("permanentAudiothumb", checked);
        });
        alwaysShowAudio->setChecked(KdenliveSettings::alwaysShowMonitorAudio());
        m_contextMenu->addAction(alwaysShowAudio);
        m_configMenuAction->addAction(alwaysShowAudio);
    }

    if (overlayMenu) {
        m_contextMenu->addMenu(overlayMenu);
    }

    m_configMenuAction->addAction(m_monitorManager->getAction("mlt_scrub"));

    QAction *switchAudioMonitor = new QAction(i18n("Show Audio Levels"), this);
    connect(switchAudioMonitor, &QAction::triggered, this, &Monitor::slotSwitchAudioMonitor);
    m_configMenuAction->addAction(switchAudioMonitor);
    switchAudioMonitor->setCheckable(true);
    switchAudioMonitor->setChecked((KdenliveSettings::monitoraudio() & m_id) != 0);

    if (m_id == Kdenlive::ClipMonitor) {
        QAction *recordTimecode = new QAction(i18n("Show Source Timecode"), this);
        recordTimecode->setCheckable(true);
        connect(recordTimecode, &QAction::triggered, this, &Monitor::slotSwitchRecTimecode);
        recordTimecode->setChecked(KdenliveSettings::rectimecode());
        m_configMenuAction->addAction(recordTimecode);
    }

    // For some reason, the frame in QAbstracSpinBox (base class of TimeCodeDisplay) needs to be displayed once, then hidden
    // or it will never appear (supposed to appear on hover).
    m_timePos->setFrame(false);
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
        QRect r = QApplication::primaryScreen()->geometry();
        profileHeight = m_glMonitor->profileSize().height() * resizeType / 100;
        profileWidth = int(pCore->getCurrentProfile()->dar() * profileHeight);
        if (profileWidth > r.width() * 0.8 || profileHeight > r.height() * 0.7) {
            // reset action to free resize
            const QList<QAction *> list = m_forceSize->actions();
            for (QAction *ac : list) {
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
        profileHeight += m_glMonitor->m_displayRulerHeight;
        m_glMonitor->setMinimumSize(profileWidth, profileHeight);
        m_glMonitor->setMaximumSize(profileWidth, profileHeight);
        setMinimumSize(QSize(profileWidth, profileHeight + m_toolbar->height()));
        break;
    default:
        // Free resize
        m_glMonitor->setMinimumSize(profileWidth, profileHeight);
        m_glMonitor->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setMinimumSize(QSize(profileWidth, profileHeight + m_toolbar->height() + m_glMonitor->getControllerProxy()->rulerHeight()));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        break;
    }
    m_forceSizeFactor = resizeType;
    updateGeometry();
}

void Monitor::buildBackgroundedProducer(int pos)
{
    if (m_controller == nullptr) {
        return;
    }
    if (KdenliveSettings::monitor_background() != "black") {
        Mlt::Tractor trac(pCore->getProjectProfile());
        QString color = QString("color:%1").arg(KdenliveSettings::monitor_background());
        std::shared_ptr<Mlt::Producer> bg(new Mlt::Producer(*trac.profile(), color.toUtf8().constData()));
        int maxLength = m_controller->originalProducer()->get_length();
        bg->set("length", maxLength);
        bg->set("out", maxLength - 1);
        bg->set("mlt_image_format", "rgba");
        trac.set_track(*bg.get(), 0);
        trac.set_track(*m_controller->originalProducer().get(), 1);
        QString composite = TransitionsRepository::get()->getCompositingTransition();
        std::unique_ptr<Mlt::Transition> transition = TransitionsRepository::get()->getTransition(composite);
        transition->set("always_active", 1);
        transition->set_tracks(0, 1);
        trac.plant_transition(*transition.get(), 0, 1);
        m_glMonitor->setProducer(std::make_shared<Mlt::Producer>(trac), isActive(), pos);
    } else {
        m_glMonitor->setProducer(m_controller->originalProducer(), isActive(), pos);
    }
}

void Monitor::updateMarkers()
{
    if (m_markerMenu) {
        // Fill guide menu
        m_markerMenu->menu()->clear();
        std::shared_ptr<MarkerListModel> model;
        if (m_id == Kdenlive::ClipMonitor && m_controller) {
            model = m_controller->getMarkerModel();
        } else if (m_id == Kdenlive::ProjectMonitor && pCore->currentDoc()) {
            model = pCore->currentDoc()->getGuideModel(pCore->currentTimelineId());
        }
        if (model) {
            QList<CommentedTime> markersList = model->getAllMarkers();
            for (const CommentedTime &mkr : qAsConst(markersList)) {
                QString label = pCore->timecode().getTimecode(mkr.time()) + QLatin1Char(' ') + mkr.comment();
                QAction *a = new QAction(label);
                a->setData(mkr.time().frames(pCore->getCurrentFps()));
                m_markerMenu->addAction(a);
            }
        }
        m_markerMenu->setEnabled(!m_markerMenu->menu()->isEmpty());
    }
}

void Monitor::updateDocumentUuid()
{
    m_glMonitor->rootObject()->setProperty("documentId", pCore->currentDoc()->uuid());
}

void Monitor::slotSeekToPreviousSnap()
{
    if (m_controller) {
        m_glMonitor->getControllerProxy()->setPosition(getSnapForPos(true).frames(pCore->getCurrentFps()));
    }
}

void Monitor::slotSeekToNextSnap()
{
    if (m_controller) {
        m_glMonitor->getControllerProxy()->setPosition(getSnapForPos(false).frames(pCore->getCurrentFps()));
    }
}

int Monitor::position()
{
    return m_glMonitor->getControllerProxy()->getPosition();
}

GenTime Monitor::getSnapForPos(bool previous)
{
    int frame = previous ? m_snaps->getPreviousPoint(m_glMonitor->getCurrentPos()) : m_snaps->getNextPoint(m_glMonitor->getCurrentPos());
    return {frame, pCore->getCurrentFps()};
}

void Monitor::slotLoadClipZone(const QPoint &zone)
{
    m_glMonitor->getControllerProxy()->setZone(zone.x(), zone.y(), false);
    Q_EMIT zoneDurationChanged(zone.y() - zone.x());
    checkOverlay();
}

void Monitor::slotSetZoneStart()
{
    QPoint oldZone = m_glMonitor->getControllerProxy()->zone();
    int currentIn = m_glMonitor->getCurrentPos();
    int updatedZoneOut = -1;
    if (currentIn > oldZone.y()) {
        updatedZoneOut = qMin(m_glMonitor->duration() - 1, currentIn + (oldZone.y() - oldZone.x()));
    }

    Fun undo_zone = [this, oldZone, updatedZoneOut]() {
        m_glMonitor->getControllerProxy()->setZoneIn(oldZone.x());
        if (updatedZoneOut > -1) {
            m_glMonitor->getControllerProxy()->setZoneOut(oldZone.y());
        }
        const QPoint zone = m_glMonitor->getControllerProxy()->zone();
        Q_EMIT zoneDurationChanged(zone.y() - zone.x());
        checkOverlay();
        return true;
    };
    Fun redo_zone = [this, currentIn, updatedZoneOut]() {
        if (updatedZoneOut > -1) {
            m_glMonitor->getControllerProxy()->setZoneOut(updatedZoneOut);
        }
        m_glMonitor->getControllerProxy()->setZoneIn(currentIn);
        const QPoint zone = m_glMonitor->getControllerProxy()->zone();
        Q_EMIT zoneDurationChanged(zone.y() - zone.x());
        checkOverlay();
        return true;
    };
    redo_zone();
    pCore->pushUndo(undo_zone, redo_zone, i18n("Set Zone"));
}

void Monitor::slotSetZoneEnd()
{
    QPoint oldZone = m_glMonitor->getControllerProxy()->zone();
    int currentOut = m_glMonitor->getCurrentPos() + 1;
    int updatedZoneIn = -1;
    if (currentOut < oldZone.x()) {
        updatedZoneIn = qMax(0, currentOut - (oldZone.y() - oldZone.x()));
    }
    Fun undo_zone = [this, oldZone, updatedZoneIn]() {
        m_glMonitor->getControllerProxy()->setZoneOut(oldZone.y());
        if (updatedZoneIn > -1) {
            m_glMonitor->getControllerProxy()->setZoneIn(oldZone.x());
        }
        const QPoint zone = m_glMonitor->getControllerProxy()->zone();
        Q_EMIT zoneDurationChanged(zone.y() - zone.x());
        checkOverlay();
        return true;
    };

    Fun redo_zone = [this, currentOut, updatedZoneIn]() {
        if (updatedZoneIn > -1) {
            m_glMonitor->getControllerProxy()->setZoneIn(updatedZoneIn);
        }
        m_glMonitor->getControllerProxy()->setZoneOut(currentOut);
        const QPoint zone = m_glMonitor->getControllerProxy()->zone();
        Q_EMIT zoneDurationChanged(zone.y() - zone.x());
        checkOverlay();
        return true;
    };
    redo_zone();
    pCore->pushUndo(undo_zone, redo_zone, i18n("Set Zone"));
}

// virtual
void Monitor::mousePressEvent(QMouseEvent *event)
{
    m_monitorManager->activateMonitor(m_id);
    if ((event->button() & Qt::RightButton) == 0u) {
        if (m_glWidget->geometry().contains(event->pos())) {
            m_DragStartPosition = event->pos();
            event->accept();
        }
    } else if (m_contextMenu) {
        slotActivateMonitor();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_contextMenu->popup(event->globalPos());
#else
        m_contextMenu->popup(event->globalPosition().toPoint());
#endif
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void Monitor::slotShowMenu(const QPoint pos)
{
    slotActivateMonitor();
    if (m_contextMenu) {
        updateMarkers();
        m_contextMenu->popup(pos);
    }
}

void Monitor::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    if (m_glMonitor->zoom() > 0.0f) {
        float horizontal = float(m_horizontalScroll->value()) / float(m_horizontalScroll->maximum());
        float vertical = float(m_verticalScroll->value()) / float(m_verticalScroll->maximum());
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
        m_horizontalScroll->setMaximum(int(m_glWidget->width() * m_glMonitor->zoom()));
        m_horizontalScroll->setValue(qRound(horizontal * float(m_horizontalScroll->maximum())));
        Q_EMIT m_horizontalScroll->valueChanged(m_horizontalScroll->value());
        m_horizontalScroll->show();
    } else {
        Q_EMIT m_horizontalScroll->valueChanged(int(0.5f * m_glWidget->width() * m_glMonitor->zoom()));
        m_horizontalScroll->hide();
    }

    if (m_glMonitor->zoom() > 1.0f) {
        m_verticalScroll->setPageStep(m_glWidget->height());
        m_verticalScroll->setMaximum(int(m_glWidget->height() * m_glMonitor->zoom()));
        m_verticalScroll->setValue(int(m_verticalScroll->maximum() * vertical));
        Q_EMIT m_verticalScroll->valueChanged(m_verticalScroll->value());
        m_verticalScroll->show();
    } else {
        Q_EMIT m_verticalScroll->valueChanged(int(0.5f * m_glWidget->height() * m_glMonitor->zoom()));
        m_verticalScroll->hide();
    }
}

void Monitor::setZoom(float zoomRatio)
{
    if (qFuzzyCompare(m_glMonitor->zoom(), 1.0f)) {
        adjustScrollBars(1., 1.);
    } else if (qFuzzyCompare(m_glMonitor->zoom() / zoomRatio, 1.0f)) {
        adjustScrollBars(0.5f, 0.5f);
    } else {
        float horizontal = float(m_horizontalScroll->value()) / float(m_horizontalScroll->maximum());
        float vertical = float(m_verticalScroll->value()) / float(m_verticalScroll->maximum());
        adjustScrollBars(horizontal, vertical);
    }
}

bool Monitor::monitorIsFullScreen() const
{
    return m_glWidget->isFullScreen();
}

void Monitor::slotSwitchFullScreen(bool minimizeOnly)
{
    // TODO: disable screensaver?
    m_glMonitor->refreshZoom = true;
    if (!m_glWidget->isFullScreen() && !minimizeOnly) {
        // Move monitor widget to the second screen (one screen for Kdenlive, the other one for the Monitor widget)
        if (qApp->screens().count() > 1) {
            QString requestedMonitor = KdenliveSettings::fullscreen_monitor();
            if (m_id == Kdenlive::ProjectMonitor) {
                if (!KdenliveSettings::project_monitor_fullscreen().isEmpty()) {
                    requestedMonitor = KdenliveSettings::project_monitor_fullscreen();
                }
            } else {
                if (!KdenliveSettings::clip_monitor_fullscreen().isEmpty()) {
                    requestedMonitor = KdenliveSettings::clip_monitor_fullscreen();
                }
            }
            bool screenFound = false;
            int ix = -1;
            if (!requestedMonitor.isEmpty()) {
                // If the platform does now provide screen serial number, use indexes
                for (const QScreen *screen : qApp->screens()) {
                    ix++;
                    bool match = requestedMonitor == QString("%1:%2").arg(QString::number(ix), screen->serialNumber());
                    // Check if monitor's index changed
                    if (!match && !screen->serialNumber().isEmpty()) {
                        match = requestedMonitor.section(QLatin1Char(':'), 1) == screen->serialNumber();
                    }
                    if (match) {
                        // Match
                        m_glWidget->setParent(nullptr);
                        m_glWidget->move(screen->geometry().topLeft());
                        m_glWidget->resize(screen->geometry().size());
                        screenFound = true;
                        if (m_id == Kdenlive::ProjectMonitor) {
                            KdenliveSettings::setProject_monitor_fullscreen(QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
                        } else {
                            KdenliveSettings::setClip_monitor_fullscreen(QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
                        }
                        break;
                    }
                }
            }
            if (!screenFound) {
                ix = 0;
                for (const QScreen *screen : qApp->screens()) {
                    // Autodetect second monitor
                    QRect screenRect = screen->geometry();
                    if (!screenRect.contains(pCore->window()->geometry().center())) {
                        if (qApp->screens().count() > 2) {
                            // We have 3 monitors, use each
                            if (m_id == Kdenlive::ProjectMonitor) {
                                if (KdenliveSettings::clip_monitor_fullscreen().isEmpty()) {
                                    KdenliveSettings::setProject_monitor_fullscreen(QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
                                } else {
                                    if (KdenliveSettings::clip_monitor_fullscreen() == QString("%1:%2").arg(QString::number(ix), screen->serialNumber())) {
                                        continue;
                                    }
                                }
                            } else {
                                if (KdenliveSettings::project_monitor_fullscreen().isEmpty()) {
                                    KdenliveSettings::setClip_monitor_fullscreen(QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
                                } else {
                                    if (KdenliveSettings::project_monitor_fullscreen() == QString("%1:%2").arg(QString::number(ix), screen->serialNumber())) {
                                        continue;
                                    }
                                }
                            }

                        } else {
                            if (m_id == Kdenlive::ProjectMonitor) {
                                KdenliveSettings::setProject_monitor_fullscreen(QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
                            } else {
                                KdenliveSettings::setClip_monitor_fullscreen(QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
                            }
                        }
                        m_glWidget->setParent(nullptr);
                        m_glWidget->move(screenRect.topLeft());
                        m_glWidget->resize(screenRect.size());
                        screenFound = true;
                        break;
                    }
                    ix++;
                }
            }
            if (!screenFound) {
                m_glWidget->setParent(nullptr);
            }
        } else {
            m_glWidget->setParent(nullptr);
        }
        m_glWidget->showFullScreen();
        setFocus();
    } else {
        m_glWidget->showNormal();
        auto *lay = static_cast<QVBoxLayout *>(layout());
        lay->insertWidget(0, m_glWidget, 10);
        // With some Qt versions, focus was lost after switching back from fullscreen,
        // QApplication::setActiveWindow restores focus to the correct window
        QApplication::setActiveWindow(this);
        if (m_id == Kdenlive::ProjectMonitor) {
            KdenliveSettings::setProject_monitor_fullscreen(QString());
        } else {
            KdenliveSettings::setClip_monitor_fullscreen(QString());
        }
        setFocus();
    }
}

void Monitor::fixFocus()
{
    setFocus();
}

// virtual
void Monitor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragStarted) {
        event->ignore();
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (event->button() != Qt::RightButton) {
        if (m_glMonitor->geometry().contains(event->pos())) {
            if (isActive()) {
                slotPlay();
            } else {
                slotActivateMonitor();
            }
        } // else event->ignore(); //QWidget::mouseReleaseEvent(event);
    }
    m_dragStarted = false;
    event->accept();
    QWidget::mouseReleaseEvent(event);
}

void Monitor::slotStartDrag()
{
    if (m_id == Kdenlive::ProjectMonitor || m_controller == nullptr) {
        // dragging is only allowed for clip monitor
        return;
    }
    auto *drag = new QDrag(this);
    auto *mimeData = new QMimeData;
    QByteArray prodData;
    QPoint p = m_glMonitor->getControllerProxy()->zone();
    if (p.x() == -1 || p.y() == -1) {
        prodData = m_controller->AbstractProjectItem::clipId().toUtf8();
    } else {
        QStringList list;
        list.append(m_controller->AbstractProjectItem::clipId());
        list.append(QString::number(p.x()));
        list.append(QString::number(p.y() - 1));
        prodData.append(list.join(QLatin1Char('/')).toUtf8());
    }
    mimeData->setData(QStringLiteral("text/producerslist"), prodData);
    mimeData->setData(QStringLiteral("text/dragid"), QUuid::createUuid().toByteArray());
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
    Q_EMIT pCore->bin()->processDragEnd();
}

// virtual
void Monitor::wheelEvent(QWheelEvent *event)
{
    slotMouseSeek(event->angleDelta().y(), event->modifiers());
    event->accept();
}

void Monitor::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
    slotSwitchFullScreen();
}

void Monitor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        slotSwitchFullScreen();
        event->accept();
        return;
    }
    if (m_glWidget->isFullScreen()) {
        event->ignore();
        Q_EMIT passKeyPress(event);
        return;
    }
    QWidget::keyPressEvent(event);
}

void Monitor::slotMouseSeek(int eventDelta, uint modifiers)
{
    if ((modifiers & Qt::ControlModifier) != 0u) {
        // Ctrl wheel zooms monitor
        m_glMonitor->slotZoom(eventDelta > 0);
        return;
    } else if ((modifiers & Qt::ShiftModifier) != 0u) {
        // Shift wheel seeks one second
        int delta = qRound(pCore->getCurrentFps());
        if (eventDelta > 0) {
            delta = -delta;
        }
        delta = qBound(0, m_glMonitor->getCurrentPos() + delta, m_glMonitor->duration() - 1);
        m_glMonitor->getControllerProxy()->setPosition(delta);
    } else if ((modifiers & Qt::AltModifier) != 0u) {
        if (eventDelta >= 0) {
            Q_EMIT seekToPreviousSnap();
        } else {
            Q_EMIT seekToNextSnap();
        }
    } else {
        if (eventDelta >= 0) {
            slotRewindOneFrame();
        } else {
            slotForwardOneFrame();
        }
    }
}

void Monitor::slotSetThumbFrame()
{
    pCore->setDocumentModified();
    if (m_controller == nullptr || m_controller->clipType() == ClipType::Timeline) {
        // This is a sequence thumbnail
        pCore->bin()->setSequenceThumbnail(pCore->currentTimelineId(), m_glMonitor->getCurrentPos());
        return;
    }
    m_controller->setProducerProperty(QStringLiteral("kdenlive:thumbnailFrame"), m_glMonitor->getCurrentPos());
    Q_EMIT refreshClipThumbnail(m_controller->AbstractProjectItem::clipId());
}

void Monitor::slotExtractCurrentZone()
{
    if (m_controller == nullptr) {
        return;
    }
    CutTask::start(ObjectId(KdenliveObjectType::BinClip, m_controller->clipId().toInt(), QUuid()), getZoneStart(), getZoneEnd(), this);
}

std::shared_ptr<ProjectClip> Monitor::currentController() const
{
    return m_controller;
}

void Monitor::slotExtractCurrentFrame(QString frameName, bool addToProject)
{
    if (m_playAction->isActive()) {
        // Pause playing
        switchPlay(false);
    }
    if (QFileInfo(frameName).fileName().isEmpty()) {
        // convenience: when extracting an image to be added to the project,
        // suggest a suitable image file name. In the project monitor, this
        // suggestion bases on the project file name; in the clip monitor,
        // the suggestion bases on the clip file name currently shown.
        // Finally, the frame number is added to this suggestion, prefixed
        // with "-f", so we get something like clip-f#.png.
        QString suggestedImageName =
            QFileInfo(currentController() ? currentController()->clipName()
                                          : pCore->currentDoc()->url().isValid() ? pCore->currentDoc()->url().fileName() : i18n("untitled"))
                .completeBaseName() +
            QStringLiteral("-f") + QString::number(m_glMonitor->getCurrentPos()).rightJustified(6, QLatin1Char('0')) + QStringLiteral(".png");
        frameName = QFileInfo(frameName, suggestedImageName).fileName();
    }

    QString framesFolder = KRecentDirs::dir(QStringLiteral(":KdenliveFramesFolder"));
    if (framesFolder.isEmpty()) {
        framesFolder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    QScopedPointer<QDialog> dlg(new QDialog(this));
    QScopedPointer<KFileWidget> fileWidget(new KFileWidget(QUrl::fromLocalFile(framesFolder), dlg.data()));
    dlg->setWindowTitle(addToProject ? i18nc("@title:window", "Save Image to Project") : i18nc("@title:window", "Save Image"));
    auto *layout = new QVBoxLayout;
    layout->addWidget(fileWidget.data());
    QCheckBox *b = nullptr;
    if (m_id == Kdenlive::ClipMonitor && m_controller && m_controller->clipType() != ClipType::Text) {
        QSize fSize = m_controller->getFrameSize();
        if (fSize != pCore->getCurrentFrameSize()) {
            b = new QCheckBox(i18n("Export image using source resolution"), dlg.data());
            b->setChecked(KdenliveSettings::exportframe_usingsourceres());
            fileWidget->setCustomWidget(b);
        }
    }
    fileWidget->setConfirmOverwrite(true);
    fileWidget->okButton()->show();
    fileWidget->cancelButton()->show();
    QObject::connect(fileWidget->okButton(), &QPushButton::clicked, fileWidget.data(), &KFileWidget::slotOk);
    QObject::connect(fileWidget.data(), &KFileWidget::accepted, fileWidget.data(), &KFileWidget::accept);
    QObject::connect(fileWidget.data(), &KFileWidget::accepted, dlg.data(), &QDialog::accept);
    QObject::connect(fileWidget->cancelButton(), &QPushButton::clicked, dlg.data(), &QDialog::reject);
    dlg->setLayout(layout);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    fileWidget->setMimeFilter(QStringList() << QStringLiteral("image/png"));
#else
    fileWidget->setFilters({KFileFilter::fromMimeType(QStringLiteral("image/png"))});
#endif
    fileWidget->setMode(KFile::File | KFile::LocalOnly);
    fileWidget->setOperationMode(KFileWidget::Saving);
    QUrl relativeUrl;
    relativeUrl.setPath(frameName);
    fileWidget->setSelectedUrl(relativeUrl);
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    QWindow *handle = dlg->windowHandle();
    if ((handle != nullptr) && conf->hasGroup("FileDialogSize")) {
        KWindowConfig::restoreWindowSize(handle, conf->group("FileDialogSize"));
        dlg->resize(handle->size());
    }
    if (dlg->exec() == QDialog::Accepted) {
        QString selectedFile = fileWidget->selectedFile();
        bool useSourceResolution = b != nullptr && b->isChecked();
        if (!selectedFile.isEmpty()) {
            if (b != nullptr) {
                KdenliveSettings::setExportframe_usingsourceres(useSourceResolution);
            }
            KRecentDirs::add(QStringLiteral(":KdenliveFramesFolder"), QUrl::fromLocalFile(selectedFile).adjusted(QUrl::RemoveFilename).toLocalFile());
            // check if we are using a proxy
            if ((m_controller != nullptr) && !m_controller->getProducerProperty(QStringLiteral("kdenlive:proxy")).isEmpty() &&
                m_controller->getProducerProperty(QStringLiteral("kdenlive:proxy")) != QLatin1String("-")) {
                // Clip monitor, using proxy. Use original clip url to get frame
                QTemporaryFile src(QDir::temp().absoluteFilePath(QString("XXXXXX.mlt")));
                if (src.open()) {
                    src.setAutoRemove(false);
                    m_controller->cloneProducerToFile(src.fileName());
                    const QStringList pathInfo = {src.fileName(), selectedFile, pCore->bin()->getCurrentFolder()};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    QtConcurrent::run(m_glMonitor->getControllerProxy(), &MonitorProxy::extractFrameToFile, m_glMonitor->getCurrentPos(), pathInfo,
                                      addToProject, useSourceResolution);
#else
                    (void)QtConcurrent::run(&MonitorProxy::extractFrameToFile, m_glMonitor->getControllerProxy(), m_glMonitor->getCurrentPos(), pathInfo,
                                            addToProject, useSourceResolution);
#endif
                } else {
                    // TODO: warn user, cannot open tmp file
                    qDebug() << "Could not create temporary file";
                }
                return;
            } else {
                if (m_id == Kdenlive::ProjectMonitor) {
                    // Create QImage with frame
                    QImage frame;
                    // Disable monitor preview scaling if any
                    int previewScale = KdenliveSettings::previewScaling();
                    if (previewScale > 0) {
                        KdenliveSettings::setPreviewScaling(0);
                        m_glMonitor->updateScaling();
                    }
                    // Check if we have proxied clips at position
                    QStringList proxiedClips = pCore->window()->getCurrentTimeline()->model()->getProxiesAt(m_glMonitor->getCurrentPos());
                    // Temporarily disable proxy on those clips
                    QMap<QString, QString> existingProxies;
                    if (!proxiedClips.isEmpty()) {
                        existingProxies = pCore->currentDoc()->proxyClipsById(proxiedClips, false);
                    }
                    disconnect(m_glMonitor, &VideoWidget::analyseFrame, this, &Monitor::frameUpdated);
                    bool analysisStatus = m_glMonitor->sendFrameForAnalysis;
                    m_glMonitor->sendFrameForAnalysis = true;
                    if (m_captureConnection) {
                        QObject::disconnect(m_captureConnection);
                    }
                    m_captureConnection =
                        connect(m_glMonitor, &VideoWidget::analyseFrame, this,
                                [this, proxiedClips, selectedFile, existingProxies, addToProject, analysisStatus, previewScale](const QImage &img) {
                                    m_glMonitor->sendFrameForAnalysis = analysisStatus;
                                    m_glMonitor->releaseAnalyse();
                                    if (pCore->getCurrentSar() != 1.) {
                                        QImage scaled = img.scaled(pCore->getCurrentFrameDisplaySize());
                                        scaled.save(selectedFile);
                                    } else {
                                        img.save(selectedFile);
                                    }
                                    if (previewScale > 0) {
                                        KdenliveSettings::setPreviewScaling(previewScale);
                                        m_glMonitor->updateScaling();
                                    }
                                    // Re-enable proxy on those clips
                                    if (!proxiedClips.isEmpty()) {
                                        pCore->currentDoc()->proxyClipsById(proxiedClips, true, existingProxies);
                                    }
                                    QObject::disconnect(m_captureConnection);
                                    connect(m_glMonitor, &VideoWidget::analyseFrame, this, &Monitor::frameUpdated);
                                    KRecentDirs::add(QStringLiteral(":KdenliveFramesFolder"),
                                                     QUrl::fromLocalFile(selectedFile).adjusted(QUrl::RemoveFilename).toLocalFile());
                                    if (addToProject) {
                                        QString folderInfo = pCore->bin()->getCurrentFolder();
                                        QMetaObject::invokeMethod(pCore->bin(), "droppedUrls", Qt::QueuedConnection,
                                                                  Q_ARG(QList<QUrl>, {QUrl::fromLocalFile(selectedFile)}), Q_ARG(QString, folderInfo));
                                    }
                                });
                    if (proxiedClips.isEmpty()) {
                        // If there is a proxy, replacing it in timeline will trigger the monitor once replaced
                        refreshMonitor();
                    }
                    return;
                } else {
                    QStringList pathInfo;
                    if (useSourceResolution) {
                        // Create a producer with the original clip
                        QTemporaryFile src(QDir::temp().absoluteFilePath(QString("XXXXXX.mlt")));
                        if (src.open()) {
                            src.setAutoRemove(false);
                            m_controller->cloneProducerToFile(src.fileName());
                            pathInfo = QStringList({src.fileName(), selectedFile, pCore->bin()->getCurrentFolder()});
                        }
                    } else {
                        pathInfo = QStringList({QString(), selectedFile, pCore->bin()->getCurrentFolder()});
                    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    QtConcurrent::run(m_glMonitor->getControllerProxy(), &MonitorProxy::extractFrameToFile, m_glMonitor->getCurrentPos(), pathInfo,
                                      addToProject, useSourceResolution);
#else
                    (void)QtConcurrent::run(&MonitorProxy::extractFrameToFile, m_glMonitor->getControllerProxy(), m_glMonitor->getCurrentPos(), pathInfo,
                                            addToProject, useSourceResolution);
#endif
                }
            }
        }
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
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->getControllerProxy()->setPosition(pos);
    Q_EMIT m_monitorManager->cleanMixer();
}

void Monitor::refreshAudioThumbs()
{
    Q_EMIT m_glMonitor->getControllerProxy()->audioThumbFormatChanged();
    Q_EMIT m_glMonitor->getControllerProxy()->colorsChanged();
}

void Monitor::normalizeAudioThumbs()
{
    Q_EMIT m_glMonitor->getControllerProxy()->audioThumbNormalizeChanged();
}

void Monitor::checkOverlay(int pos)
{
    if (m_qmlManager->sceneType() != MonitorSceneDefault) {
        // we are not in main view, ignore
        return;
    }
    QString overlayText;
    QColor color;
    if (pos == -1) {
        pos = m_timePos->getValue();
    }

    if (m_markerModel) {
        int mid = m_markerModel->markerIdAtFrame(pos);
        if (mid > -1) {
            CommentedTime marker = m_markerModel->markerById(mid);
            overlayText = marker.comment();
            color = pCore->markerTypes.value(marker.markerType()).color;
        }
    }
    m_glMonitor->getControllerProxy()->setMarker(overlayText, color);
}

int Monitor::getZoneStart()
{
    return m_glMonitor->getControllerProxy()->zoneIn();
}

int Monitor::getZoneEnd()
{
    return m_glMonitor->getControllerProxy()->zoneOut();
}

void Monitor::slotZoneStart()
{
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->getControllerProxy()->setPosition(m_glMonitor->getControllerProxy()->zoneIn());
}

void Monitor::slotZoneEnd()
{
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->getControllerProxy()->setPosition(m_glMonitor->getControllerProxy()->zoneOut());
}

void Monitor::slotRewind(double speed)
{
    if (!slotActivateMonitor() || m_trimmingbar->isVisible()) {
        return;
    }
    if (qFuzzyIsNull(speed)) {
        double currentspeed = m_glMonitor->playSpeed();
        if (currentspeed > -1) {
            m_glMonitor->purgeCache();
            speed = -1;
            m_speedIndex = 0;
        } else {
            m_speedIndex++;
            if (m_speedIndex > 5) {
                m_speedIndex = 0;
            }
            speed = -MonitorManager::speedArray[m_speedIndex];
        }
    }
    updatePlayAction(true);
    m_glMonitor->switchPlay(true, speed);
}

void Monitor::slotForward(double speed, bool allowNormalPlay)
{
    if (!slotActivateMonitor() || m_trimmingbar->isVisible()) {
        return;
    }
    if (qFuzzyIsNull(speed)) {
        double currentspeed = m_glMonitor->playSpeed();
        if (currentspeed < 1) {
            m_speedIndex = 0;
            if (allowNormalPlay) {
                m_glMonitor->purgeCache();
                updatePlayAction(true);
                m_glMonitor->switchPlay(true);
                return;
            }
        } else {
            m_speedIndex++;
        }
        if (m_speedIndex > 5) {
            m_speedIndex = 0;
        }
        speed = MonitorManager::speedArray[m_speedIndex];
    }
    updatePlayAction(true);
    m_glMonitor->switchPlay(true, speed);
}

void Monitor::slotRewindOneFrame(int diff)
{
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->getControllerProxy()->setPosition(qMax(0, m_glMonitor->getCurrentPos() - diff));
}

void Monitor::slotForwardOneFrame(int diff)
{
    if (!slotActivateMonitor()) {
        return;
    }
    if (m_id == Kdenlive::ClipMonitor) {
        m_glMonitor->getControllerProxy()->setPosition(qMin(m_glMonitor->duration() - 1, m_glMonitor->getCurrentPos() + diff));
    } else {
        m_glMonitor->getControllerProxy()->setPosition(m_glMonitor->getCurrentPos() + diff);
    }
}

void Monitor::adjustRulerSize(int length, const std::shared_ptr<MarkerSortModel> &markerModel)
{
    if (m_controller != nullptr) {
        m_glMonitor->setRulerInfo(length);
    } else {
        m_glMonitor->setRulerInfo(length, markerModel);
    }
    m_timePos->setRange(0, length);

    if (markerModel) {
        QAbstractItemModel *sourceModel = markerModel->sourceModel();
        connect(sourceModel, SIGNAL(dataChanged(QModelIndex, QModelIndex, QVector<int>)), this, SLOT(checkOverlay()));
        connect(sourceModel, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(checkOverlay()));
        connect(sourceModel, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(checkOverlay()));
    } else {
        // Project simply changed length, update display
        Q_EMIT durationChanged(length);
    }
}

void Monitor::stop()
{
    updatePlayAction(false);
    m_glMonitor->stop();
}

void Monitor::mute(bool mute)
{
    // TODO: we should set the "audio_off" property to 1 to mute the consumer instead of changing volume
    m_glMonitor->setVolume(mute ? 0 : KdenliveSettings::volume() / 100.0);
}

void Monitor::start()
{
    if (!isVisible() || !isActive()) {
        return;
    }
    m_glMonitor->startConsumer();
}

void Monitor::slotRefreshMonitor(bool visible)
{
    if (visible && monitorVisible()) {
        if (slotActivateMonitor()) {
            start();
        }
    }
}

void Monitor::forceMonitorRefresh()
{
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->refresh();
}

void Monitor::refreshMonitor(bool directUpdate)
{
    if (!m_glMonitor->isReady() || isPlaying()) {
        return;
    }
    if (isActive()) {
        if (directUpdate) {
            m_glMonitor->refresh();
        } else {
            m_glMonitor->requestRefresh();
        }
    } else if (monitorVisible()) {
        // Monitor was not active. Check if the other one is visible to re-activate it afterwards
        bool otherMonitorVisible = m_id == Kdenlive::ClipMonitor ? m_monitorManager->projectMonitorVisible() : m_monitorManager->clipMonitorVisible();
        slotActivateMonitor();
        if (isActive()) {
            m_glMonitor->refresh();
            // Monitor was not active, so we activate it, refresh and activate the other monitor once done
            QObject::disconnect(m_switchConnection);
            m_switchConnection = connect(m_glMonitor, &VideoWidget::frameDisplayed, this, [=]() {
                m_monitorManager->activateMonitor(m_id == Kdenlive::ClipMonitor ? Kdenlive::ProjectMonitor : Kdenlive::ClipMonitor, otherMonitorVisible);
                QObject::disconnect(m_switchConnection);
            });
        }
    }
}

bool Monitor::monitorVisible() const
{
    return m_glWidget->isFullScreen() || !m_glWidget->visibleRegion().isEmpty();
}

void Monitor::refreshMonitorIfActive(bool directUpdate)
{
    if (!m_glMonitor->isReady() || !isActive()) {
        return;
    }
    if (directUpdate) {
        m_glMonitor->refresh();
    } else {
        m_glMonitor->requestRefresh();
    }
}

void Monitor::pause()
{
    if (!m_playAction->isActive() || !slotActivateMonitor()) {
        return;
    }
    switchPlay(false);
}

void Monitor::switchPlay(bool play)
{
    if (m_trimmingbar->isVisible()) {
        return;
    }
    m_speedIndex = 0;
    if (!play) {
        m_droppedTimer.stop();
    }
    if (!KdenliveSettings::autoscroll()) {
        Q_EMIT pCore->autoScrollChanged();
    }
    if (!m_glMonitor->switchPlay(play)) {
        play = false;
    }
    m_playAction->setActive(play);
}

void Monitor::updatePlayAction(bool play)
{
    m_playAction->setActive(play);
    if (!play) {
        m_droppedTimer.stop();
    }
    if (!KdenliveSettings::autoscroll()) {
        Q_EMIT pCore->autoScrollChanged();
    }
}

void Monitor::slotSwitchPlay()
{
    if (!slotActivateMonitor() || m_trimmingbar->isVisible()) {
        return;
    }
    if (!KdenliveSettings::autoscroll()) {
        Q_EMIT pCore->autoScrollChanged();
    }
    m_speedIndex = 0;
    bool play = m_playAction->isActive();
    if (pCore->getAudioDevice()->isRecording()) {
        int recState = pCore->getAudioDevice()->recordState();
        if (recState == QMediaRecorder::RecordingState) {
            if (!play) {
                pCore->getAudioDevice()->pauseRecording();
            }
        } else if (recState == QMediaRecorder::PausedState && play) {
            pCore->getAudioDevice()->resumeRecording();
        }
        m_displayingCountdown = true;
    } else if (pCore->getAudioDevice()->isMonitoring()) {
        if (m_displayingCountdown || KdenliveSettings::disablereccountdown()) {
            m_displayingCountdown = false;
            m_playAction->setActive(false);
            pCore->recordAudio(-1, true);
            return;
        }
        pCore->recordAudio(-1, true);
    }
    if (!m_glMonitor->switchPlay(play)) {
        play = false;
        m_playAction->setActive(false);
    }
    bool showDropped = false;
    if (m_id == Kdenlive::ClipMonitor) {
        showDropped = KdenliveSettings::displayClipMonitorInfo() & 0x20;
    } else if (m_id == Kdenlive::ProjectMonitor) {
        showDropped = KdenliveSettings::displayProjectMonitorInfo() & 0x20;
    }
    if (showDropped) {
        m_glMonitor->resetDrops();
        if (play) {
            m_droppedTimer.start();
        } else {
            m_droppedTimer.stop();
        }
    } else {
        m_droppedTimer.stop();
    }
}

void Monitor::slotPlay()
{
    m_playAction->trigger();
}

bool Monitor::isPlaying() const
{
    return m_playAction->isActive();
}

void Monitor::resetPlayOrLoopZone(const QString &binId)
{
    if (activeClipId() == binId) {
        m_glMonitor->resetZoneMode();
    }
}

void Monitor::slotPlayZone()
{
    if (!slotActivateMonitor()) {
        return;
    }
    bool ok = m_glMonitor->playZone();
    if (ok) {
        updatePlayAction(true);
    }
}

void Monitor::slotLoopZone()
{
    if (!slotActivateMonitor()) {
        return;
    }
    bool ok = m_glMonitor->playZone(true);
    if (ok) {
        updatePlayAction(true);
    }
}

void Monitor::slotLoopClip(QPoint inOut)
{
    if (!slotActivateMonitor()) {
        return;
    }
    bool ok = m_glMonitor->loopClip(inOut);
    if (ok) {
        updatePlayAction(true);
    }
}

void Monitor::updateClipProducer(const std::shared_ptr<Mlt::Producer> &prod)
{
    if (m_glMonitor->setProducer(prod, isActive(), -1)) {
        prod->set_speed(1.0);
    }
}

void Monitor::updateClipProducer(const QString &playlist)
{
    Q_UNUSED(playlist)
    // TODO
    // Mlt::Producer *prod = new Mlt::Producer(*m_glMonitor->profile(), playlist.toUtf8().constData());
    // m_glMonitor->setProducer(prod, isActive(), render->seekFramePosition());
    m_glMonitor->switchPlay(true);
}

void Monitor::slotOpenClip(const std::shared_ptr<ProjectClip> &controller, int in, int out)
{
    if (m_controller) {
        m_glMonitor->resetZoneMode();
        // store last audiothumb zoom / position
        double zoomFactor = m_glMonitor->rootObject()->property("zoomFactor").toDouble();
        if (zoomFactor != 1.) {
            double zoomStart = m_glMonitor->rootObject()->property("zoomStart").toDouble();
            m_controller->setProducerProperty(QStringLiteral("kdenlive:thumbZoomFactor"), zoomFactor);
            m_controller->setProducerProperty(QStringLiteral("kdenlive:thumbZoomStart"), zoomStart);
        } else {
            m_controller->resetProducerProperty(QStringLiteral("kdenlive:thumbZoomFactor"));
            m_controller->resetProducerProperty(QStringLiteral("kdenlive:thumbZoomStart"));
        }
        m_controller->setProducerProperty(QStringLiteral("kdenlive:monitorPosition"), position());
        disconnect(m_controller.get(), &ProjectClip::audioThumbReady, this, &Monitor::prepareAudioThumb);
        disconnect(m_controller->getMarkerModel().get(), SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &)), this,
                   SLOT(checkOverlay()));
        disconnect(m_controller->getMarkerModel().get(), SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(checkOverlay()));
        disconnect(m_controller->getMarkerModel().get(), SIGNAL(rowsRemoved(const QModelIndex &, int, int)), this, SLOT(checkOverlay()));
        if (m_controller->hasLimitedDuration()) {
            disconnect(m_controller.get(), &ProjectClip::boundsChanged, m_glMonitor->getControllerProxy(), &MonitorProxy::updateClipBounds);
            disconnect(m_controller.get(), &ProjectClip::registeredClipChanged, m_controller.get(), &ProjectClip::checkClipBounds);
        }
    } else if (controller == nullptr) {
        // Nothing to do
        pCore->taskManager.displayedClip = -1;
        return;
    }
    disconnect(this, &Monitor::seekPosition, this, &Monitor::seekRemap);
    m_controller = controller;
    pCore->taskManager.displayedClip = m_controller ? m_controller->clipId().toInt() : -1;
    m_glMonitor->getControllerProxy()->setAudioStream(QString());
    m_snaps.reset(new SnapModel());
    m_glMonitor->getControllerProxy()->resetZone();
    if (controller) {
        m_markerModel = m_controller->getMarkerModel();
        if (pCore->currentRemap(controller->clipId())) {
            connect(this, &Monitor::seekPosition, this, &Monitor::seekRemap, Qt::UniqueConnection);
        }
        ClipType::ProducerType type = controller->clipType();
        if (type == ClipType::AV || type == ClipType::Video || type == ClipType::SlideShow) {
            m_glMonitor->rootObject()->setProperty("baseThumbPath",
                                                   QString("image://thumbnail/%1/%2/#").arg(controller->clipId(), pCore->currentDoc()->uuid().toString()));
        } else {
            m_glMonitor->rootObject()->setProperty("baseThumbPath", QString());
        }
        m_audioChannels->clear();
        if (m_controller->audioInfo()) {
            QMap<int, QString> audioStreamsInfo = m_controller->audioStreams();
            if (audioStreamsInfo.size() > 1) {
                // Multi stream clip
                QMapIterator<int, QString> i(audioStreamsInfo);
                QMap<int, QString> activeStreams = m_controller->activeStreams();
                if (activeStreams.size() > 1) {
                    m_glMonitor->getControllerProxy()->setAudioStream(i18np("%1 audio stream", "%1 audio streams", activeStreams.size()));
                    // TODO: Mix audio channels
                } else if (!activeStreams.isEmpty()) {
                    m_glMonitor->getControllerProxy()->setAudioStream(activeStreams.first());
                }
                QAction *ac;
                while (i.hasNext()) {
                    i.next();
                    ac = m_audioChannels->addAction(i.value());
                    ac->setData(i.key());
                    ac->setCheckable(true);
                    if (activeStreams.contains(i.key())) {
                        ac->setChecked(true);
                    }
                }
                ac = m_audioChannels->addAction(i18n("Merge all streams"));
                ac->setData(INT_MAX);
                ac->setCheckable(true);
                if (activeStreams.contains(INT_MAX)) {
                    ac->setChecked(true);
                }
                m_streamAction->setVisible(true);
            } else {
                m_streamAction->setVisible(false);
            }
        } else {
            m_streamAction->setVisible(false);
            // m_audioChannels->menuAction()->setVisible(false);
        }
        connect(m_controller.get(), &ProjectClip::audioThumbReady, this, &Monitor::prepareAudioThumb);
        if (m_controller->hasLimitedDuration()) {
            connect(m_controller.get(), &ProjectClip::boundsChanged, m_glMonitor->getControllerProxy(), &MonitorProxy::updateClipBounds);
            connect(m_controller.get(), &ProjectClip::registeredClipChanged, m_controller.get(), &ProjectClip::checkClipBounds);
        }
        connect(m_controller->getMarkerModel().get(), SIGNAL(dataChanged(QModelIndex, QModelIndex, QVector<int>)), this, SLOT(checkOverlay()));
        connect(m_controller->getMarkerModel().get(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(checkOverlay()));
        connect(m_controller->getMarkerModel().get(), SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(checkOverlay()));

        if (m_recManager->toolbar()->isVisible()) {
            // we are in record mode, don't display clip
            return;
        }
        if (KdenliveSettings::rectimecode()) {
            m_timePos->setOffset(m_controller->getRecordTime());
        }
        if (m_controller->statusReady()) {
            m_timePos->setRange(0, int(m_controller->frameDuration() - 1));
            m_glMonitor->setRulerInfo(int(m_controller->frameDuration() - 1), controller->getFilteredMarkerModel());
            double audioScale = m_controller->getProducerDoubleProperty(QStringLiteral("kdenlive:thumbZoomFactor"));
            if (in == out && in == -1) {
                // Only apply on bin clip, not sub clips
                int lastPosition = m_controller->getProducerIntProperty(QStringLiteral("kdenlive:monitorPosition"));
                if (lastPosition > 0 && lastPosition != m_controller->originalProducer()->position()) {
                    m_controller->originalProducer()->seek(lastPosition);
                }
                if (audioScale > 0. && audioScale != 1.) {
                    double audioStart = m_controller->getProducerDoubleProperty(QStringLiteral("kdenlive:thumbZoomStart"));
                    m_glMonitor->rootObject()->setProperty("zoomFactor", audioScale);
                    m_glMonitor->rootObject()->setProperty("zoomStart", audioStart);
                    m_glMonitor->rootObject()->setProperty("showZoomBar", true);
                } else {
                    m_glMonitor->rootObject()->setProperty("zoomFactor", 1);
                    m_glMonitor->rootObject()->setProperty("zoomStart", 0);
                    m_glMonitor->rootObject()->setProperty("showZoomBar", false);
                }
            } else {
                m_glMonitor->rootObject()->setProperty("zoomFactor", 1);
                m_glMonitor->rootObject()->setProperty("zoomStart", 0);
                m_glMonitor->rootObject()->setProperty("showZoomBar", false);
            }
            pCore->guidesList()->setClipMarkerModel(m_controller);
            loadQmlScene(MonitorSceneDefault);
            updateMarkers();
            connect(m_glMonitor->getControllerProxy(), &MonitorProxy::addSnap, this, &Monitor::addSnapPoint, Qt::DirectConnection);
            connect(m_glMonitor->getControllerProxy(), &MonitorProxy::removeSnap, this, &Monitor::removeSnapPoint, Qt::DirectConnection);
            if (out == -1) {
                m_glMonitor->getControllerProxy()->setZone(m_controller->zone(), false);
            } else {
                m_glMonitor->getControllerProxy()->setZone(in, out, false);
            }
            m_snaps->addPoint(int(m_controller->frameDuration() - 1));
            // Loading new clip / zone, stop if playing
            if (m_playAction->isActive()) {
                updatePlayAction(false);
            }
            m_audioMeterWidget->audioChannels = controller->audioInfo() ? controller->audioInfo()->channels() : 0;
            m_controller->getMarkerModel()->registerSnapModel(m_snaps);
            m_glMonitor->getControllerProxy()->setClipProperties(controller->clipId().toInt(), controller->clipType(), controller->hasAudioAndVideo(),
                                                                 controller->clipName());
            if (!m_controller->hasVideo() || KdenliveSettings::displayClipMonitorInfo() & 0x10) {
                if (m_audioMeterWidget->audioChannels == 0 || !m_controller->hasAudio()) {
                    qDebug() << "=======\n\nSETTING AUDIO DATA IN MONITOR EMPTY!!!";
                    m_glMonitor->getControllerProxy()->setAudioThumb();
                } else {
                    QList<int> streamIndexes = m_controller->activeStreams().keys();
                    qDebug() << "=======\n\nSETTING AUDIO DATA IN MONITOR NOT EMPTY!!!";
                    if (streamIndexes.count() == 1 && streamIndexes.at(0) == INT_MAX) {
                        // Display all streams
                        streamIndexes = m_controller->audioStreams().keys();
                    }
                    m_glMonitor->getControllerProxy()->setAudioThumb(streamIndexes, m_controller->activeStreamChannels());
                }
            }
            if (monitorVisible() && !m_monitorManager->projectMonitor()->isPlaying()) {
                slotActivateMonitor();
            }
            buildBackgroundedProducer(in);
        } else {
            qDebug() << "*************** CONTROLLER NOT READY";
        }
        // hasEffects =  controller->hasEffects();
    } else {
        m_markerModel = nullptr;
        loadQmlScene(MonitorSceneDefault);
        m_glMonitor->setProducer(nullptr, isActive(), -1);
        m_glMonitor->getControllerProxy()->setAudioThumb();
        m_glMonitor->rootObject()->setProperty("zoomFactor", 1);
        m_glMonitor->rootObject()->setProperty("zoomStart", 0);
        m_glMonitor->rootObject()->setProperty("showZoomBar", false);
        m_audioMeterWidget->audioChannels = 0;
        m_glMonitor->getControllerProxy()->setClipProperties(-1, ClipType::Unknown, false, QString());
        pCore->guidesList()->setClipMarkerModel(nullptr);
        // m_audioChannels->menuAction()->setVisible(false);
        m_streamAction->setVisible(false);
        if (monitorVisible()) {
            slotActivateMonitor();
        }
    }
    if (isActive()) {
        start();
    }
    checkOverlay();
}

void Monitor::loadZone(int in, int out)
{
    m_glMonitor->getControllerProxy()->setZone({in, out}, false);
}

void Monitor::reloadActiveStream()
{
    if (m_controller) {
        QList<QAction *> acts = m_audioChannels->actions();
        QSignalBlocker bk(m_audioChannels);
        QList<int> activeStreams = m_controller->activeStreams().keys();
        QMap<int, QString> streams = m_controller->audioStreams();
        qDebug() << "==== REFRESHING MONITOR STREAMS: " << activeStreams;
        if (activeStreams.size() > 1) {
            m_glMonitor->getControllerProxy()->setAudioStream(i18np("%1 audio stream", "%1 audio streams", activeStreams.size()));
            // TODO: Mix audio channels
        } else if (!activeStreams.isEmpty()) {
            m_glMonitor->getControllerProxy()->setAudioStream(m_controller->activeStreams().first());
        } else {
            m_glMonitor->getControllerProxy()->setAudioStream(QString());
        }
        prepareAudioThumb();
        for (auto ac : qAsConst(acts)) {
            int val = ac->data().toInt();
            if (streams.contains(val)) {
                // Update stream name in case of renaming
                ac->setText(streams.value(val));
            }
            if (activeStreams.contains(val)) {
                ac->setChecked(true);
            } else {
                ac->setChecked(false);
            }
        }
    }
}

const QString Monitor::activeClipId()
{
    if (m_controller) {
        return m_controller->clipId();
    }
    return QString();
}

void Monitor::slotPreviewResource(const QString &path, const QString &title)
{
    if (isPlaying()) {
        stop();
    }
    QApplication::processEvents();
    slotOpenClip(nullptr);
    m_streamAction->setVisible(false);
    // TODO: direct loading of the producer blocks UI, we should use a task to load the producer
    m_markerModel = nullptr;
    m_glMonitor->setProducer(path);
    m_timePos->setRange(0, m_glMonitor->producer()->get_length() - 1);
    m_glMonitor->getControllerProxy()->setClipProperties(-1, ClipType::Unknown, false, title);
    m_glMonitor->setRulerInfo(m_glMonitor->producer()->get_length() - 1);
    loadQmlScene(MonitorSceneDefault);
    checkOverlay();
    slotStart();
    switchPlay(true);
}

void Monitor::resetProfile()
{
    m_glMonitor->reloadProfile();
    m_glMonitor->rootObject()->setProperty("framesize", QRect(0, 0, m_glMonitor->profileSize().width(), m_glMonitor->profileSize().height()));
    // Update drop frame info
    m_qmlManager->setProperty(QStringLiteral("dropped"), false);
    m_qmlManager->setProperty(QStringLiteral("fps"), QString::number(pCore->getCurrentFps(), 'f', 2));
}

void Monitor::resetConsumer(bool fullReset)
{
    m_glMonitor->resetConsumer(fullReset);
}

void Monitor::updateClipZone(const QPoint zone)
{
    if (m_controller == nullptr) {
        return;
    }
    m_controller->setZone(zone);
}

void Monitor::restart()
{
    m_glMonitor->restart();
}

void Monitor::switchMonitorInfo(int code)
{
    int currentOverlay;
    if (m_id == Kdenlive::ClipMonitor) {
        currentOverlay = KdenliveSettings::displayClipMonitorInfo();
        currentOverlay ^= code;
        KdenliveSettings::setDisplayClipMonitorInfo(currentOverlay);
    } else {
        currentOverlay = KdenliveSettings::displayProjectMonitorInfo();
        currentOverlay ^= code;
        KdenliveSettings::setDisplayProjectMonitorInfo(currentOverlay);
    }
    updateQmlDisplay(currentOverlay);
    if (code == 0x01) {
        // Hide/show ruler
        m_glMonitor->switchRuler(currentOverlay & 0x01);
    }
}

void Monitor::slotEditMarker()
{
    if (m_editMarker) {
        m_editMarker->trigger();
    }
}

void Monitor::updateTimecodeFormat()
{
    m_glMonitor->rootObject()->setProperty("timecode", m_timePos->displayText());
}

QPoint Monitor::getZoneInfo() const
{
    return m_glMonitor->getControllerProxy()->zone();
}

void Monitor::enableEffectScene(bool enable)
{
    KdenliveSettings::setShowOnMonitorScene(enable);
    MonitorSceneType sceneType = enable ? m_lastMonitorSceneType : MonitorSceneDefault;
    slotShowEffectScene(sceneType, true);
    if (enable) {
        Q_EMIT updateScene();
    }
}

void Monitor::slotShowEffectScene(MonitorSceneType sceneType, bool temporary, const QVariant &sceneData)
{
    if (m_trimmingbar->isVisible()) {
        return;
    }
    if (sceneType == MonitorSceneNone) {
        // We just want to revert to normal scene
        if (m_qmlManager->sceneType() == MonitorSceneSplit || m_qmlManager->sceneType() == MonitorSceneDefault) {
            // Ok, nothing to do
            return;
        }
        sceneType = MonitorSceneDefault;
    } else if (m_qmlManager->sceneType() == MonitorSplitTrack) {
        // Don't show another scene type if multitrack mode is active
        loadQmlScene(MonitorSplitTrack, sceneData);
        return;
    }
    if (!temporary) {
        m_lastMonitorSceneType = sceneType;
    }
    loadQmlScene(sceneType, sceneData);
}

void Monitor::slotSeekToKeyFrame()
{
    if (m_qmlManager->sceneType() == MonitorSceneGeometry) {
        // Adjust splitter pos
        int kfr = m_glMonitor->rootObject()->property("requestedKeyFrame").toInt();
        Q_EMIT seekToKeyframe(kfr);
    }
}

void Monitor::setUpEffectGeometry(const QRect &r, const QVariantList &list, const QVariantList &types)
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (!root) {
        return;
    }
    if (!list.isEmpty() || m_qmlManager->sceneType() == MonitorSceneRoto) {
        root->setProperty("centerPointsTypes", types);
        root->setProperty("centerPoints", list);
    }
    if (!r.isEmpty()) {
        root->setProperty("framesize", r);
    }
}

void Monitor::setEffectSceneProperty(const QString &name, const QVariant &value)
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (!root) {
        return;
    }
    root->setProperty(name.toUtf8().constData(), value);
}

QRect Monitor::effectRect() const
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (!root) {
        return {};
    }
    return root->property("framesize").toRect();
}

QVariantList Monitor::effectPolygon() const
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (!root) {
        return QVariantList();
    }
    return root->property("centerPoints").toList();
}

QVariantList Monitor::effectRoto() const
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (!root) {
        return QVariantList();
    }
    QVariantList points = root->property("centerPoints").toList();
    QVariantList controlPoints = root->property("centerPointsTypes").toList();
    // rotoscoping effect needs a list of
    QVariantList mix;
    mix.reserve(points.count() * 3);
    for (int i = 0; i < points.count(); i++) {
        mix << controlPoints.at(2 * i);
        mix << points.at(i);
        mix << controlPoints.at(2 * i + 1);
    }
    return mix;
}

void Monitor::setEffectKeyframe(bool enable)
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (root) {
        root->setProperty("iskeyframe", enable);
    }
}

bool Monitor::effectSceneDisplayed(MonitorSceneType effectType)
{
    return m_qmlManager->sceneType() == effectType;
}

void Monitor::slotSetVolume(int volume)
{
    KdenliveSettings::setVolume(volume);
    double renderVolume = m_glMonitor->volume();
    m_glMonitor->setVolume(volume / 100.0);
    if (renderVolume > 0 && volume > 0) {
        return;
    }
    /*QIcon icon;
    if (volume == 0) {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-muted"));
    } else {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-medium"));
    }*/
}

void Monitor::sendFrameForAnalysis(bool analyse)
{
    m_glMonitor->sendFrameForAnalysis = analyse;
}

void Monitor::updateAudioForAnalysis()
{
    m_glMonitor->updateAudioForAnalysis();
}

void Monitor::onFrameDisplayed(const SharedFrame &frame)
{
    Q_EMIT m_monitorManager->frameDisplayed(frame);
    if (m_id == Kdenlive::ProjectMonitor) {
        Q_EMIT pCore->updateMixerLevels(frame.get_position());
    }
    if (!m_glMonitor->checkFrameNumber(frame.get_position(), m_playAction->isActive())) {
        updatePlayAction(false);
    }
}

void Monitor::checkDrops()
{
    int dropped = m_glMonitor->droppedFrames();
    if (dropped == 0) {
        // No dropped frames since last check
        m_qmlManager->setProperty(QStringLiteral("dropped"), false);
        m_qmlManager->setProperty(QStringLiteral("fps"), QString::number(pCore->getCurrentFps(), 'f', 2));
    } else {
        m_glMonitor->resetDrops();
        dropped = int(pCore->getCurrentFps() - dropped);
        m_qmlManager->setProperty(QStringLiteral("dropped"), true);
        m_qmlManager->setProperty(QStringLiteral("fps"), QString::number(dropped, 'f', 2));
    }
}

void Monitor::reloadProducer(const QString &id)
{
    if (!m_controller) {
        return;
    }
    if (m_controller->AbstractProjectItem::clipId() == id) {
        slotOpenClip(m_controller);
    }
}

QString Monitor::getMarkerThumb(GenTime pos)
{
    if (!m_controller) {
        return QString();
    }
    if (!m_controller->getClipHash().isEmpty()) {
        bool ok = false;
        QDir dir = pCore->currentDoc()->getCacheDir(CacheThumbs, &ok);
        if (ok) {
            QString url = dir.absoluteFilePath(m_controller->getClipHash() + QLatin1Char('#') + QString::number(pos.frames(pCore->getCurrentFps())) +
                                               QStringLiteral(".png"));
            if (QFile::exists(url)) {
                return url;
            }
        }
    }
    return QString();
}

void Monitor::setPalette(const QPalette &p)
{
    QWidget::setPalette(p);
    QList<QToolButton *> allButtons = this->findChildren<QToolButton *>();
    for (int i = 0; i < allButtons.count(); i++) {
        QToolButton *m = allButtons.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
    QQuickItem *root = m_glMonitor->rootObject();
    if (root) {
        QMetaObject::invokeMethod(root, "updatePalette");
    }
    m_audioMeterWidget->refreshPixmap();
}

void Monitor::gpuError()
{
    qCWarning(KDENLIVE_LOG) << " + + + + Error initializing Movit GLSL manager";
    warningMessage(i18n("Cannot initialize Movit's GLSL manager, please disable Movit"), -1);
}

void Monitor::warningMessage(const QString &text, int timeout, const QList<QAction *> &actions)
{
    m_infoMessage->setMessageType(KMessageWidget::Warning);
    m_infoMessage->setText(text);
    for (QAction *action : actions) {
        m_infoMessage->addAction(action);
    }
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->animatedShow();
    if (timeout > 0) {
        QTimer::singleShot(timeout, m_infoMessage, &KMessageWidget::animatedHide);
    }
}

void Monitor::activateSplit()
{
    loadQmlScene(MonitorSceneSplit);
    if (isActive()) {
        m_glMonitor->requestRefresh();
    } else if (slotActivateMonitor()) {
        start();
    }
}

void Monitor::slotSwitchCompare(bool enable)
{
    if (m_id == Kdenlive::ProjectMonitor) {
        if (enable) {
            if (m_qmlManager->sceneType() == MonitorSceneSplit) {
                // Split scene is already active
                return;
            }
            m_splitEffect.reset(new Mlt::Filter(pCore->getProjectProfile(), "frei0r.alphagrad"));
            if ((m_splitEffect != nullptr) && m_splitEffect->is_valid()) {
                m_splitEffect->set("0", 0.5);    // 0 is the Clip left parameter
                m_splitEffect->set("1", 0);      // 1 is gradient width
                m_splitEffect->set("2", -0.747); // 2 is tilt
            } else {
                // frei0r.scal0tilt is not available
                warningMessage(i18n("The alphagrad filter is required for that feature, please install frei0r and restart Kdenlive"));
                return;
            }
            Q_EMIT createSplitOverlay(m_splitEffect);
            return;
        }
        // Delete temp track
        Q_EMIT removeSplitOverlay();
        m_splitEffect.reset();
        loadQmlScene(MonitorSceneDefault);
        if (isActive()) {
            m_glMonitor->requestRefresh();
        } else if (slotActivateMonitor()) {
            start();
        }
        return;
    }
    if (m_controller == nullptr || !m_controller->hasEffects()) {
        // disable split effect
        if (m_controller) {
            pCore->displayMessage(i18n("Clip has no effects"), InformationMessage);
        } else {
            pCore->displayMessage(i18n("Select a clip in project bin to compare effect"), InformationMessage);
        }
        return;
    }
    if (enable) {
        if (m_qmlManager->sceneType() == MonitorSceneSplit) {
            // Split scene is already active
            qDebug() << " . . . .. ALREADY ACTIVE";
            return;
        }
        buildSplitEffect(m_controller->masterProducer());
    } else if (m_splitEffect) {
        // TODO
        m_glMonitor->setProducer(m_controller->originalProducer(), isActive(), position());
        m_splitEffect.reset();
        m_splitProducer.reset();
        loadQmlScene(MonitorSceneDefault);
    }
    slotActivateMonitor();
}

void Monitor::resetScene()
{
    loadQmlScene(MonitorSceneDefault);
}

void Monitor::buildSplitEffect(Mlt::Producer *original)
{
    m_splitEffect.reset(new Mlt::Filter(pCore->getProjectProfile(), "frei0r.alphagrad"));
    if ((m_splitEffect != nullptr) && m_splitEffect->is_valid()) {
        m_splitEffect->set("0", 0.5);    // 0 is the Clip left parameter
        m_splitEffect->set("1", 0);      // 1 is gradient width
        m_splitEffect->set("2", -0.747); // 2 is tilt
    } else {
        // frei0r.scal0tilt is not available
        pCore->displayMessage(i18n("The alphagrad filter is required for that feature, please install frei0r and restart Kdenlive"), ErrorMessage);
        return;
    }
    QString splitTransition = TransitionsRepository::get()->getCompositingTransition();
    Mlt::Transition t(pCore->getProjectProfile(), splitTransition.toUtf8().constData());
    if (!t.is_valid()) {
        m_splitEffect.reset();
        pCore->displayMessage(i18n("The cairoblend transition is required for that feature, please install frei0r and restart Kdenlive"), ErrorMessage);
        return;
    }
    Mlt::Tractor trac(pCore->getProjectProfile());
    std::shared_ptr<Mlt::Producer> clone = ProjectClip::cloneProducer(std::make_shared<Mlt::Producer>(original));
    // Delete all effects
    int ct = 0;
    Mlt::Filter *filter = clone->filter(ct);
    while (filter != nullptr) {
        QString ix = QString::fromLatin1(filter->get("kdenlive_id"));
        if (!ix.isEmpty()) {
            if (clone->detach(*filter) == 0) {
            } else {
                ct++;
            }
        } else {
            ct++;
        }
        delete filter;
        filter = clone->filter(ct);
    }
    trac.set_track(*original, 0);
    trac.set_track(*clone.get(), 1);
    clone.get()->attach(*m_splitEffect.get());
    t.set("always_active", 1);
    trac.plant_transition(t, 0, 1);
    delete original;
    m_splitProducer = std::make_shared<Mlt::Producer>(trac.get_producer());
    m_glMonitor->setProducer(m_splitProducer, isActive(), position());
    m_glMonitor->setRulerInfo(int(m_controller->frameDuration()), m_controller->getFilteredMarkerModel());
    loadQmlScene(MonitorSceneSplit);
}

QSize Monitor::profileSize() const
{
    return m_glMonitor->profileSize();
}

void Monitor::loadQmlScene(MonitorSceneType type, const QVariant &sceneData)
{
    if (type == m_qmlManager->sceneType() && sceneData.isNull()) {
        return;
    }
    bool sceneWithEdit = type == MonitorSceneGeometry || type == MonitorSceneCorners || type == MonitorSceneRoto;
    if (!m_monitorManager->getAction(QStringLiteral("monitor_editmode"))->isChecked() && sceneWithEdit) {
        // User doesn't want effect scenes
        pCore->displayMessage(i18n("Enable edit mode in monitor to edit effect"), InformationMessage, 500);
        type = MonitorSceneDefault;
    }
    m_qmlManager->setScene(m_id, type, pCore->getCurrentFrameSize(), pCore->getCurrentDar(), m_glMonitor->displayRect(), double(m_glMonitor->zoom()),
                           m_timePos->maximum());
    if (m_glMonitor->zoom() != 1.) {
        m_glMonitor->setZoom(m_glMonitor->zoom(), true);
    }
    QQuickItem *root = m_glMonitor->rootObject();
    switch (type) {
    case MonitorSceneSplit:
        QObject::connect(root, SIGNAL(qmlMoveSplit()), this, SLOT(slotAdjustEffectCompare()), Qt::UniqueConnection);
        break;
    case MonitorSceneTrimming:
    case MonitorSceneGeometry:
    case MonitorSceneCorners:
    case MonitorSceneRoto:
        break;
    case MonitorSceneDefault:
        QObject::connect(root, SIGNAL(editCurrentMarker()), this, SLOT(slotEditInlineMarker()), Qt::UniqueConnection);
        m_qmlManager->setProperty(QStringLiteral("timecode"), m_timePos->displayText());
        if (m_id == Kdenlive::ClipMonitor) {
            QObject::connect(root, SIGNAL(endDrag()), pCore->bin(), SIGNAL(processDragEnd()), Qt::UniqueConnection);
            updateQmlDisplay(KdenliveSettings::displayClipMonitorInfo());
        } else if (m_id == Kdenlive::ProjectMonitor) {
            updateQmlDisplay(KdenliveSettings::displayProjectMonitorInfo());
            QObject::connect(root, SIGNAL(startRecording()), pCore.get(), SLOT(startRecording()), Qt::UniqueConnection);
        }
        break;
    case MonitorSplitTrack:
        m_qmlManager->setProperty(QStringLiteral("tracks"), sceneData);
        break;
    default:
        break;
    }
    m_qmlManager->setProperty(QStringLiteral("fps"), QString::number(pCore->getCurrentFps(), 'f', 2));
}

void Monitor::setQmlProperty(const QString &name, const QVariant &value)
{
    m_qmlManager->setProperty(name, value);
}

void Monitor::slotAdjustEffectCompare()
{
    double percent = 0.5;
    if (m_qmlManager->sceneType() == MonitorSceneSplit) {
        // Adjust splitter pos
        QQuickItem *root = m_glMonitor->rootObject();
        percent = root->property("percentage").toDouble();
        // Store real frame percentage for resize events
        root->setProperty("realpercent", percent);
    }
    if (m_splitEffect) {
        m_splitEffect->set("0", 0.5 - (percent - 0.5) * .666);
    }
    m_glMonitor->refresh();
}

void Monitor::slotSwitchRec(bool enable)
{
    if (!m_recManager) {
        return;
    }
    if (enable) {
        m_toolbar->setVisible(false);
        m_recManager->toolbar()->setVisible(true);
    } else if (m_recManager->toolbar()->isVisible()) {
        m_recManager->stop();
        m_toolbar->setVisible(true);
        Q_EMIT refreshCurrentClip();
    }
}

void Monitor::slotSwitchTrimming(bool enable)
{
    if (!m_trimmingbar) {
        return;
    }
    if (enable) {
        loadQmlScene(MonitorSceneTrimming);
        m_toolbar->setVisible(false);
        m_trimmingbar->setVisible(true);
        if (pCore->activeTool() == ToolType::RippleTool) {
            m_oneLess->setVisible(false);
            m_oneMore->setVisible(false);
            m_fiveLess->setVisible(false);
            m_fiveMore->setVisible(false);
        } else {
            m_oneLess->setVisible(true);
            m_oneMore->setVisible(true);
            m_fiveLess->setVisible(true);
            m_fiveMore->setVisible(true);
        }
        m_glMonitor->switchRuler(false);
    } else if (m_trimmingbar->isVisible()) {
        loadQmlScene(MonitorSceneDefault);
        m_trimmingbar->setVisible(false);
        m_toolbar->setVisible(true);
        m_glMonitor->switchRuler(KdenliveSettings::displayClipMonitorInfo() & 0x01);
    }
}

void Monitor::doKeyPressEvent(QKeyEvent *ev)
{
    keyPressEvent(ev);
}

void Monitor::slotEditInlineMarker()
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (root) {
        std::shared_ptr<MarkerListModel> model;
        if (m_controller) {
            // We are editing a clip marker
            model = m_controller->getMarkerModel();
        } else {
            model = pCore->currentDoc()->getGuideModel(pCore->currentTimelineId());
        }
        QString newComment = root->property("markerText").toString();
        bool found = false;
        CommentedTime oldMarker = model->getMarker(m_timePos->getValue(), &found);
        if (!found || newComment == oldMarker.comment()) {
            // No change
            return;
        }
        oldMarker.setComment(newComment);
        model->addMarker(oldMarker.time(), oldMarker.comment(), oldMarker.markerType());
    }
}

void Monitor::prepareAudioThumb()
{
    if (m_controller) {
        m_glMonitor->getControllerProxy()->setAudioThumb();
        if (!m_controller->audioStreams().isEmpty() && m_controller->hasAudio()) {
            QList<int> streamIndexes = m_controller->activeStreams().keys();
            if (streamIndexes.count() == 1 && streamIndexes.at(0) == INT_MAX) {
                // Display all streams
                streamIndexes = m_controller->audioStreams().keys();
            }
            m_glMonitor->getControllerProxy()->setAudioThumb(streamIndexes, m_controller->activeStreamChannels());
        }
    }
}

void Monitor::slotSwitchAudioMonitor()
{
    if (!m_audioMeterWidget->isValid) {
        KdenliveSettings::setMonitoraudio(0x01);
        m_audioMeterWidget->setVisibility(false);
        return;
    }
    int currentOverlay = KdenliveSettings::monitoraudio();
    currentOverlay ^= m_id;
    KdenliveSettings::setMonitoraudio(currentOverlay);
    if ((KdenliveSettings::monitoraudio() & m_id) != 0) {
        // We want to enable this audio monitor, so make monitor active
        slotActivateMonitor();
    }
    displayAudioMonitor(isActive());
}

void Monitor::updateGuidesList()
{
    if (m_id == Kdenlive::ProjectMonitor) {
        if (pCore->currentDoc()) {
            const QUuid uuid = pCore->currentDoc()->activeUuid;
            if (!uuid.isNull()) {
                pCore->guidesList()->setModel(pCore->currentDoc()->getGuideModel(uuid), pCore->currentDoc()->getFilteredGuideModel(uuid));
            }
        }
    } else if (m_id == Kdenlive::ClipMonitor) {
        pCore->guidesList()->setClipMarkerModel(m_controller);
    }
}

void Monitor::displayAudioMonitor(bool isActive)
{
    bool enable = isActive && ((KdenliveSettings::monitoraudio() & m_id) != 0 || (m_id == Kdenlive::ProjectMonitor && pCore->audioMixerVisible));
    if (enable) {
        connect(m_monitorManager, &MonitorManager::frameDisplayed, m_audioMeterWidget, &ScopeWidget::onNewFrame, Qt::UniqueConnection);
    } else {
        disconnect(m_monitorManager, &MonitorManager::frameDisplayed, m_audioMeterWidget, &ScopeWidget::onNewFrame);
    }
    m_audioMeterWidget->setVisibility((KdenliveSettings::monitoraudio() & m_id) != 0);
    if (isActive && m_glWidget->isFullScreen()) {
        // If both monitors are fullscreen, this is necessary to do the switch
        m_glWidget->showFullScreen();
        pCore->window()->activateWindow();
        pCore->window()->setFocus();
    }
}

void Monitor::updateQmlDisplay(int currentOverlay)
{
    m_glMonitor->rootObject()->setVisible((currentOverlay & 0x01) != 0);
    m_glMonitor->rootObject()->setProperty("showMarkers", currentOverlay & 0x04);
    bool showDropped = currentOverlay & 0x20;
    m_glMonitor->rootObject()->setProperty("showFps", showDropped);
    m_glMonitor->rootObject()->setProperty("showTimecode", currentOverlay & 0x02);
    if (m_id == Kdenlive::ClipMonitor) {
        m_glMonitor->rootObject()->setProperty("showAudiothumb", currentOverlay & 0x10);
        m_glMonitor->rootObject()->setProperty("showClipJobs", currentOverlay & 0x40);
    }
    if (showDropped) {
        if (!m_droppedTimer.isActive() && m_playAction->isActive()) {
            m_glMonitor->resetDrops();
            m_droppedTimer.start();
        }
    } else {
        m_droppedTimer.stop();
    }
}

void Monitor::clearDisplay()
{
    m_glMonitor->clear();
}

void Monitor::panView(QPoint diff)
{
    // Only pan if scrollbars are visible
    if (m_horizontalScroll->isVisible()) {
        m_horizontalScroll->setValue(m_horizontalScroll->value() + diff.x());
    }
    if (m_verticalScroll->isVisible()) {
        m_verticalScroll->setValue(m_verticalScroll->value() + diff.y());
    }
}

void Monitor::processSeek(int pos, bool noAudioScrub)
{
    if (!slotActivateMonitor()) {
        return;
    }
    if (KdenliveSettings::pauseonseek()) {
        if (m_playAction->isActive()) {
            pause();
        } else {
            m_glMonitor->setVolume(KdenliveSettings::volume() / 100.);
        }
    }
    m_glMonitor->requestSeek(pos, noAudioScrub);
    Q_EMIT m_monitorManager->cleanMixer();
}

void Monitor::requestSeek(int pos)
{
    m_glMonitor->getControllerProxy()->setPosition(pos);
}

void Monitor::requestSeekIfVisible(int pos)
{
    if (monitorVisible()) {
        requestSeek(pos);
    }
}

void Monitor::setProducer(std::shared_ptr<Mlt::Producer> producer, int pos)
{
    if (locked) {
        if (pos > -1) {
            m_glMonitor->getControllerProxy()->setPositionAdvanced(pos, true);
        }
        return;
    }
    m_audioMeterWidget->audioChannels = pCore->audioChannels();
    if (producer) {
        m_markerModel = pCore->currentDoc()->getGuideModel(pCore->currentTimelineId());
    } else {
        m_markerModel.reset();
    }
    m_glMonitor->setProducer(std::move(producer), isActive(), pos);
}

void Monitor::reconfigure()
{
    m_glMonitor->reconfigure();
}

void Monitor::slotSeekPosition(int pos)
{
    Q_EMIT seekPosition(pos);
    m_timePos->setValue(pos);
    checkOverlay();
}

void Monitor::slotStart()
{
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->switchPlay(false);
    m_glMonitor->getControllerProxy()->setPosition(0);
}

void Monitor::slotTrimmingPos(int pos, int offset, int frames1, int frames2)
{
    if (m_glMonitor->producer() != pCore->window()->getCurrentTimeline()->model()->producer().get()) {
        processSeek(pos);
    }
    QString tc(pCore->timecode().getDisplayTimecodeFromFrames(offset, KdenliveSettings::frametimecode()));
    m_trimmingOffset->setText(tc);
    m_glMonitor->getControllerProxy()->setTrimmingTC1(frames1);
    m_glMonitor->getControllerProxy()->setTrimmingTC2(frames2);
}

void Monitor::slotTrimmingPos(int offset)
{
    offset = pCore->window()->getCurrentTimeline()->controller()->trimmingBoundOffset(offset);
    if (m_glMonitor->producer() != pCore->window()->getCurrentTimeline()->model()->producer().get()) {
        processSeek(m_glMonitor->producer()->position() + offset);
    }
    QString tc(pCore->timecode().getDisplayTimecodeFromFrames(offset, KdenliveSettings::frametimecode()));
    m_trimmingOffset->setText(tc);

    m_glMonitor->getControllerProxy()->setTrimmingTC1(offset, true);
    m_glMonitor->getControllerProxy()->setTrimmingTC2(offset, true);
}

void Monitor::slotEnd()
{
    if (!slotActivateMonitor()) {
        return;
    }
    m_glMonitor->switchPlay(false);
    if (m_id == Kdenlive::ClipMonitor) {
        m_glMonitor->getControllerProxy()->setPosition(m_glMonitor->duration() - 1);
    } else {
        m_glMonitor->getControllerProxy()->setPosition(pCore->projectDuration() - 1);
    }
}

void Monitor::addSnapPoint(int pos)
{
    m_snaps->addPoint(pos);
}

void Monitor::removeSnapPoint(int pos)
{
    m_snaps->removePoint(pos);
}

void Monitor::slotSetScreen(int screenIndex)
{
    Q_EMIT screenChanged(screenIndex);
}

void Monitor::slotZoomIn()
{
    m_glMonitor->slotZoom(true);
}

void Monitor::slotZoomOut()
{
    m_glMonitor->slotZoom(false);
}

void Monitor::setConsumerProperty(const QString &name, const QString &value)
{
    m_glMonitor->setConsumerProperty(name, value);
}

void Monitor::purgeCache()
{
    m_glMonitor->purgeCache();
}

void Monitor::updateBgColor()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_glMonitor->m_bgColor = KdenliveSettings::window_background();
#else
    m_glMonitor->setClearColor(KdenliveSettings::window_background());
#endif
}

MonitorProxy *Monitor::getControllerProxy()
{
    return m_glMonitor->getControllerProxy();
}

void Monitor::updateMultiTrackView(int tid)
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (root) {
        root->setProperty("activeTrack", tid);
    }
}

void Monitor::slotSwitchRecTimecode(bool enable)
{
    qDebug() << "=== SLOT SWITCH REC: " << enable;
    KdenliveSettings::setRectimecode(enable);
    if (!enable) {
        m_timePos->setOffset(0);
        return;
    }
    if (m_controller) {
        qDebug() << "=== GOT TIMECODE OFFSET: " << m_controller->getRecordTime();
        m_timePos->setOffset(m_controller->getRecordTime());
    }
}

void Monitor::focusTimecode()
{
    m_timePos->setFocus();
    m_timePos->selectAll();
}

void Monitor::startCountDown()
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (root) {
        QMetaObject::invokeMethod(root, "startCountdown");
    }
}

void Monitor::stopCountDown()
{
    QQuickItem *root = m_glMonitor->rootObject();
    if (root) {
        QMetaObject::invokeMethod(root, "stopCountdown");
    }
}

void Monitor::extractFrame(const QString &path)
{
    QStringList pathInfo = {QString(), path, QString()};
    m_glMonitor->getControllerProxy()->extractFrameToFile(m_glMonitor->getCurrentPos(), pathInfo, false, true);
}

const QStringList Monitor::getGPUInfo()
{
    return m_glMonitor->getGPUInfo();
}
