/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QtVersionChecks>
#include <ki18n_version.h>
#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <KLocalizedQmlContext>
#else
#include <KLocalizedContext>
#endif

#include "../model/builders/meltBuilder.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markersortmodel.h"
#include "capture/mediacapture.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "project/dialogs/guideslist.h"
#include "qmltypes/thumbnailprovider.h"
#include "timelinewidget.h"

#include <QAction>
#include <QActionGroup>
#include <QFontDatabase>
#include <QMenu>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUuid>

const int TimelineWidget::comboScale[] = {1, 2, 4, 8, 15, 30, 50, 75, 100, 150, 200, 300, 500, 800, 1000, 1500, 2000, 3000, 6000, 15000, 30000};

TimelineWidget::TimelineWidget(const QUuid uuid, QWidget *parent)
    : QQuickWidget(parent)
    , timelineController(this)
    , m_uuid(uuid)
{
#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    KLocalization::setupLocalizedContext(engine());
#else
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));
#endif
    setClearColor(palette().window().color());
    m_sortModel = std::make_unique<QSortFilterProxyModel>(this);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setVisible(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setFocusPolicy(Qt::StrongFocus);
    m_favEffects = new QMenu(i18n("Insert an effect..."), this);
    m_favCompositions = new QMenu(i18n("Insert a composition..."), this);
    installEventFilter(this);
    connect(&timelineController, &TimelineController::zoneMoved, this, &TimelineWidget::zoneMoved);
    connect(&timelineController, &TimelineController::ungrabHack, this, &TimelineWidget::slotUngrabHack);
    connect(&timelineController, &TimelineController::regainFocus, this, &TimelineWidget::regainFocus, Qt::DirectConnection);
    connect(&timelineController, &TimelineController::stopAudioRecord, this, &TimelineWidget::stopAudioRecord, Qt::DirectConnection);
    m_targetsMenu = new QMenu(this);
}

TimelineWidget::~TimelineWidget()
{
    rootObject()->blockSignals(true);
    timelineController.prepareClose();
    QList<QQmlContext::PropertyPair> propertyList = {{QStringLiteral("multitrack"), QVariant()},  {QStringLiteral("timeline"), QVariant()},
                                                     {QStringLiteral("guidesModel"), QVariant()}, {QStringLiteral("subtitleModel"), QVariant()},
                                                     {QStringLiteral("controller"), QVariant()},  {QStringLiteral("audiorec"), QVariant()}};
    rootContext()->setContextProperties(propertyList);
    setSource(QUrl());
}

void TimelineWidget::updateEffectFavorites()
{
    const QMap<QString, QString> effects = sortedItems(KdenliveSettings::favorite_effects(), false);
    QMapIterator<QString, QString> i(effects);
    m_favEffects->clear();
    while (i.hasNext()) {
        i.next();
        QAction *ac = m_favEffects->addAction(i.key());
        ac->setData(i.value());
    }
}

void TimelineWidget::updateTransitionFavorites()
{
    const QMap<QString, QString> effects = sortedItems(KdenliveSettings::favorite_transitions(), true);
    QMapIterator<QString, QString> i(effects);
    m_favCompositions->clear();
    while (i.hasNext()) {
        i.next();
        QAction *ac = m_favCompositions->addAction(i.key());
        ac->setData(i.value());
    }
}

const QMap<QString, QString> TimelineWidget::sortedItems(const QStringList &items, bool isTransition)
{
    QMap<QString, QString> sortedItems;
    for (const QString &effect : items) {
        sortedItems.insert(timelineController.getAssetName(effect, isTransition), effect);
    }
    return sortedItems;
}

void TimelineWidget::setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu,
                                     QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu, QMenu *addClipMenu)
{
    m_timelineClipMenu = new QMenu(this);
    QList<QAction *> cActions = clipMenu->actions();
    for (auto &a : cActions) {
        m_timelineClipMenu->addAction(a);
    }
    m_timelineCompositionMenu = new QMenu(this);
    cActions = compositionMenu->actions();
    for (auto &a : cActions) {
        m_timelineCompositionMenu->addAction(a);
    }
    m_timelineMixMenu = new QMenu(this);
    QAction *deleteAction = pCore->window()->actionCollection()->action(QLatin1String("delete_timeline_clip"));
    m_timelineMixMenu->addAction(deleteAction);

    m_timelineMenu = new QMenu(this);
    cActions = timelineMenu->actions();
    for (auto &a : cActions) {
        m_timelineMenu->addAction(a);
    }
    m_timelineRulerMenu = new QMenu(this);
    cActions = timelineRulerMenu->actions();
    for (auto &a : cActions) {
        m_timelineRulerMenu->addAction(a);
    }
    m_guideMenu = guideMenu;
    m_headerMenu = headerMenu;
    m_thumbsMenu = thumbsMenu;
    m_headerMenu->addMenu(m_thumbsMenu);
    m_timelineSubtitleClipMenu = subtitleClipMenu;
    m_editGuideAcion = editGuideAction;
    m_addClipMenu = addClipMenu;
    updateEffectFavorites();
    updateTransitionFavorites();
    connect(m_favEffects, &QMenu::triggered, this, [&](QAction *ac) { timelineController.addEffectToClip(ac->data().toString()); });
    connect(m_favCompositions, &QMenu::triggered, this, [&](QAction *ac) { timelineController.addCompositionToClip(ac->data().toString()); });
    connect(m_guideMenu, &QMenu::triggered, this, [&](QAction *ac) { timelineController.setPosition(ac->data().toInt()); });
    connect(m_thumbsMenu, &QMenu::triggered, this,
            [&](QAction *ac) { timelineController.setActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format"), ac->data().toString()); });
    // Fix qml focus issue
    connect(m_headerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::triggered, this, &TimelineWidget::slotResetContextPos);
    connect(m_timelineCompositionMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineRulerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::triggered, this, &TimelineWidget::slotResetContextPos);
    connect(m_timelineMenu, &QMenu::aboutToShow, this, &TimelineWidget::updateAddClipMenuStatus);
    connect(m_timelineMenu, &QMenu::triggered, this, &TimelineWidget::updateAddClipMenuStatus);
    connect(m_timelineSubtitleClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);

    m_timelineClipMenu->addMenu(m_favEffects);
    m_timelineClipMenu->addMenu(m_favCompositions);
    m_timelineMenu->addMenu(m_favCompositions);
    m_timelineMenu->addMenu(m_addClipMenu);
}

const QUuid &TimelineWidget::getUuid() const
{
    return m_uuid;
}

void TimelineWidget::setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy)
{
    loading = true;
    Q_ASSERT(model != nullptr);
    connect(&timelineController, &TimelineController::timelineMouseOffsetChanged, this, &TimelineWidget::emitMousePos, Qt::QueuedConnection);
    m_sortModel->setSourceModel(model.get());
    m_sortModel->setSortRole(TimelineItemModel::SortRole);
    m_sortModel->sort(0, Qt::DescendingOrder);
    timelineController.setModel(model);
    connect(pCore->bin(), &Bin::requestAddClipReset, this, [&]() { m_shouldAddClip = false; });
    m_audioRec = pCore->getAudioDevice();
    QList<QQmlContext::PropertyPair> propertyList = {{"controller", QVariant::fromValue(model.get())},
                                                     {"multitrack", QVariant::fromValue(m_sortModel.get())},
                                                     {"timeline", QVariant::fromValue(&timelineController)},
                                                     {"guidesModel", QVariant::fromValue(model->getFilteredGuideModel().get())},
                                                     {"documentId", QVariant::fromValue(model->uuid())},
                                                     {"audiorec", QVariant::fromValue(m_audioRec.get())},
                                                     {"miniFontSize", QVariant::fromValue(QFontInfo(font()).pixelSize())},
                                                     {"proxy", QVariant()}};
    if (model->getSubtitleModel()) {
        propertyList.append({"subtitleModel", QVariant::fromValue(model->getSubtitleModel().get())});
    } else {
        propertyList.append({"subtitleModel", QVariant()});
    }
    rootContext()->setContextProperties(propertyList);
    setSource(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/kdenlive/Timeline.qml")));

    engine()->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    connect(rootObject(), SIGNAL(zoomIn(bool)), pCore->window(), SLOT(slotZoomIn(bool)));
    connect(rootObject(), SIGNAL(zoomOut(bool)), pCore->window(), SLOT(slotZoomOut(bool)));
    connect(rootObject(), SIGNAL(processingDrag(bool)), pCore->window(), SIGNAL(enableUndo(bool)));
    connect(&timelineController, &TimelineController::seeked, proxy, &MonitorProxy::setPosition);
    rootObject()->setProperty("dar", pCore->getCurrentDar());
    connect(rootObject(), SIGNAL(showClipMenu(int)), this, SLOT(showClipMenu(int)));
    connect(rootObject(), SIGNAL(showMixMenu(int)), this, SLOT(showMixMenu(int)));
    connect(rootObject(), SIGNAL(showCompositionMenu()), this, SLOT(showCompositionMenu()));
    connect(rootObject(), SIGNAL(showTimelineMenu()), this, SLOT(showTimelineMenu()));
    connect(rootObject(), SIGNAL(showRulerMenu()), this, SLOT(showRulerMenu()));
    connect(rootObject(), SIGNAL(showHeaderMenu()), this, SLOT(showHeaderMenu()));
    connect(rootObject(), SIGNAL(showTargetMenu(int)), this, SLOT(showTargetMenu(int)));
    connect(rootObject(), SIGNAL(showSubtitleClipMenu()), this, SLOT(showSubtitleClipMenu()));
    connect(rootObject(), SIGNAL(markerActivated(int)), pCore->guidesList(), SLOT(markerActivated(int)));
    connect(rootObject(), SIGNAL(updateTimelineMousePos(int, int)), pCore->window(), SLOT(slotUpdateMousePosition(int, int)));
    timelineController.setRoot(rootObject());
    setVisible(true);
    loading = false;
    timelineController.checkDuration();
}

void TimelineWidget::emitMousePos(int offset)
{
    pCore->window()->slotUpdateMousePosition(int((offset + mapFromGlobal(QCursor::pos()).x()) / timelineController.scaleFactor()),
                                             timelineController.duration());
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    Q_EMIT focusProjectMonitor();
    m_clickPos = event->globalPosition().toPoint();
    QQuickWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isEnabled()) {
        emitMousePos(timelineController.timelineMouseOffset());
    }
    QQuickWidget::mouseMoveEvent(event);
}

void TimelineWidget::showClipMenu(int cid)
{
    // Hide not applicable effects
    QList<QAction *> effects = m_favEffects->actions();
    int tid = model()->getClipTrackId(cid);
    bool isAudioTrack = false;
    if (tid > -1) {
        isAudioTrack = model()->isAudioTrack(tid);
    }
    m_favCompositions->setEnabled(!isAudioTrack);
    for (auto ac : std::as_const(effects)) {
        const QString &id = ac->data().toString();
        if (EffectsRepository::get()->isAudioEffect(id) != isAudioTrack) {
            ac->setVisible(false);
        } else {
            ac->setVisible(true);
        }
    }
    m_timelineClipMenu->popup(m_clickPos);
}

void TimelineWidget::showMixMenu(int /*cid*/)
{
    // Show mix menu
    m_timelineMixMenu->popup(m_clickPos);
}

void TimelineWidget::showCompositionMenu()
{
    m_timelineCompositionMenu->popup(m_clickPos);
}

void TimelineWidget::showHeaderMenu()
{
    bool isAudio = timelineController.isActiveTrackAudio();
    QList<QAction *> menuActions = m_headerMenu->actions();
    QList<QAction *> audioActions;
    QStringList allowedActions = {QLatin1String("show_track_record"), QLatin1String("separate_channels"), QLatin1String("normalize_channels")};
    for (QAction *ac : std::as_const(menuActions)) {
        if (allowedActions.contains(ac->data().toString())) {
            if (ac->data().toString() == QLatin1String("separate_channels")) {
                ac->setChecked(KdenliveSettings::displayallchannels());
            }
            audioActions << ac;
        }
    }
    if (!isAudio) {
        // Video track
        int currentThumbs = timelineController.getActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format")).toInt();
        QList<QAction *> actions = m_thumbsMenu->actions();
        for (QAction *ac : std::as_const(actions)) {
            if (ac->data().toInt() == currentThumbs) {
                ac->setChecked(true);
                break;
            }
        }
        m_thumbsMenu->menuAction()->setVisible(true);
        for (auto ac : std::as_const(audioActions)) {
            ac->setVisible(false);
        }
    } else {
        // Audio track
        m_thumbsMenu->menuAction()->setVisible(false);
        for (auto ac : std::as_const(audioActions)) {
            ac->setVisible(true);
            if (ac->data().toString() == QLatin1String("show_track_record")) {
                ac->setChecked(timelineController.getActiveTrackProperty(QStringLiteral("kdenlive:audio_rec")).toInt() == 1);
            }
        }
    }
    m_headerMenu->popup(m_clickPos);
}

void TimelineWidget::showTargetMenu(int tid)
{
    int currentTargetStream;
    if (tid == -1) {
        // Called through shortcut
        tid = timelineController.activeTrack();
        if (tid == -1) {
            return;
        }
        if (timelineController.clipTargets() < 2 || !model()->isAudioTrack(tid)) {
            pCore->displayMessage(i18n("No available stream"), MessageType::ErrorMessage);
            return;
        }
        QVariant returnedValue;
        QMetaObject::invokeMethod(rootObject(), "getActiveTrackStreamPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
        m_clickPos = mapToGlobal(QPoint(5, y())) + QPoint(0, returnedValue.toInt());
    }
    QMap<int, QString> possibleTargets = timelineController.getCurrentTargets(tid, currentTargetStream);
    m_targetsMenu->clear();
    if (m_targetsGroup) {
        delete m_targetsGroup;
    }
    m_targetsGroup = new QActionGroup(this);
    QMapIterator<int, QString> i(possibleTargets);
    while (i.hasNext()) {
        i.next();
        QAction *ac = m_targetsMenu->addAction(i.value());
        ac->setData(i.key());
        m_targetsGroup->addAction(ac);
        ac->setCheckable(true);
        if (i.key() == currentTargetStream) {
            ac->setChecked(true);
        }
    }
    connect(m_targetsGroup, &QActionGroup::triggered, this, [this, tid](QAction *action) {
        int targetStream = action->data().toInt();
        timelineController.assignAudioTarget(tid, targetStream);
    });
    if (m_targetsMenu->isEmpty() || possibleTargets.isEmpty()) {
        m_headerMenu->popup(m_clickPos);
    } else {
        m_targetsMenu->popup(m_clickPos);
    }
}

void TimelineWidget::showRulerMenu()
{
    m_guideMenu->clear();
    const QList<CommentedTime> guides = pCore->currentDoc()->getGuideModel(m_uuid)->getAllMarkers();
    m_editGuideAcion->setEnabled(false);
    double fps = pCore->getCurrentFps();
    int currentPos = rootObject()->property("consumerPosition").toInt();
    for (const auto &guide : guides) {
        auto *ac = new QAction(guide.comment(), this);
        int frame = guide.time().frames(fps);
        ac->setData(frame);
        if (frame == currentPos) {
            m_editGuideAcion->setEnabled(true);
        }
        m_guideMenu->addAction(ac);
    }
    m_timelineRulerMenu->popup(m_clickPos);
}

void TimelineWidget::showTimelineMenu()
{
    m_guideMenu->clear();
    const QList<CommentedTime> guides = pCore->currentDoc()->getGuideModel(m_uuid)->getAllMarkers();
    m_editGuideAcion->setEnabled(false);
    double fps = pCore->getCurrentFps();
    int currentPos = rootObject()->property("consumerPosition").toInt();
    for (const auto &guide : guides) {
        auto ac = new QAction(guide.comment(), this);
        int frame = guide.time().frames(fps);
        ac->setData(frame);
        if (frame == currentPos) {
            m_editGuideAcion->setEnabled(true);
        }
        m_guideMenu->addAction(ac);
    }
    m_addClipPos = m_clickPos;
    m_shouldAddClip = true;

    QPoint posInWidget = mapFromGlobal(m_clickPos);
    m_addClipFrame = timelineController.getMousePos(posInWidget);
    m_addClipTrack = timelineController.getMouseTrack(posInWidget);

    // Calculate maximum available space on this track
    int maxSpace = timelineController.getFreeSpace(m_addClipTrack, m_addClipFrame);
    pCore->bin()->setSuggestedDuration(maxSpace);

    qDebug() << "ADDING CLIP AT TRACK:" << m_addClipTrack << "FRAME:" << m_addClipFrame << "MAX SPACE:" << maxSpace;

    pCore->bin()->setReadyCallBack([this](const QString &clipId) {
        qDebug() << "CALLBACK TRIGGERED FOR CLIP:" << clipId;
        // Process with insertion
        timelineController.insertClips(m_addClipTrack, m_addClipFrame, QStringList(clipId), true, true);
    });
    m_timelineMenu->popup(m_clickPos);
}

void TimelineWidget::showSubtitleClipMenu()
{
    m_timelineSubtitleClipMenu->popup(m_clickPos);
}

void TimelineWidget::updateShouldAddClip(bool shouldAddClip)
{
    m_shouldAddClip = shouldAddClip;
}

void TimelineWidget::updateAddClipMenuStatus()
{
    int tid = timelineController.getMouseTrack();
    if (tid == -2 || tid == -1 || !model()->isTrack(tid) || model()->isAudioTrack(tid)) {
        m_addClipMenu->setEnabled(false);
    } else {
        m_addClipMenu->setEnabled(true);
    }
}

void TimelineWidget::slotChangeZoom(int value, bool zoomOnMouse)
{
    double pixelScale = QFontMetrics(font()).maxWidth() * 2;
    timelineController.setScaleFactorOnMouse(pixelScale / comboScale[value], zoomOnMouse);
}

void TimelineWidget::slotCenterView()
{
    QMetaObject::invokeMethod(rootObject(), "centerViewOnCursor");
}

void TimelineWidget::slotFitZoom()
{
    QVariant returnedValue;
    double prevScale = timelineController.scaleFactor();
    QMetaObject::invokeMethod(rootObject(), "fitZoom", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    double scale = returnedValue.toDouble();
    QMetaObject::invokeMethod(rootObject(), "scrollPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    int scrollPos = returnedValue.toInt();
    if (qFuzzyCompare(prevScale, scale) && scrollPos == 0) {
        scale = m_prevScale;
        scrollPos = m_scrollPos;
    } else {
        m_prevScale = prevScale;
        m_scrollPos = scrollPos;
        scrollPos = 0;
    }
    timelineController.setScaleFactorOnMouse(scale, false);
    // Update zoom slider
    Q_EMIT timelineController.updateZoom(scale);
    QMetaObject::invokeMethod(rootObject(), "goToStart", Q_ARG(QVariant, scrollPos));
}

Mlt::Tractor *TimelineWidget::tractor()
{
    return timelineController.tractor();
}

TimelineController *TimelineWidget::controller()
{
    return &timelineController;
}

std::shared_ptr<TimelineItemModel> TimelineWidget::model()
{
    return timelineController.getModel();
}

void TimelineWidget::zoneUpdated(const QPoint &zone)
{
    timelineController.setZone(zone, false);
}

void TimelineWidget::zoneUpdatedWithUndo(const QPoint &oldZone, const QPoint &newZone)
{
    timelineController.updateZone(oldZone, newZone);
}

void TimelineWidget::setTool(ToolType::ProjectTool tool)
{
    rootObject()->setProperty("activeTool", int(tool));
}

ToolType::ProjectTool TimelineWidget::activeTool()
{
    return ToolType::ProjectTool(rootObject()->property("activeTool").toInt());
}

QPair<int, int> TimelineWidget::getAvTracksCount() const
{
    return timelineController.getAvTracksCount();
}

void TimelineWidget::slotUngrabHack()
{
    // Workaround bug: https://bugreports.qt.io/browse/QTBUG-59044
    // https://phabricator.kde.org/D5515
    QTimer::singleShot(250, this, [this]() {
        // Reset menu position, necessary if user closes the menu without selecting any action
        rootObject()->setProperty("clickFrame", -1);
    });
    if (quickWindow()) {
        if (quickWindow()->mouseGrabberItem()) {
            quickWindow()->mouseGrabberItem()->ungrabMouse();
            QPoint mousePos = mapFromGlobal(QCursor::pos());
            QMetaObject::invokeMethod(rootObject(), "regainFocus", Qt::DirectConnection, Q_ARG(QVariant, mousePos));
        } else {
            QMetaObject::invokeMethod(rootObject(), "endDrag", Qt::DirectConnection);
        }
    }
}

void TimelineWidget::slotResetContextPos(QAction *)
{
    rootObject()->setProperty("clickFrame", -1);
    m_clickPos = QPoint();
}

int TimelineWidget::zoomForScale(double value) const
{
    int scale = int(100 / value);
    int ix = 13;
    while (comboScale[ix] > scale && ix > 0) {
        ix--;
    }
    return ix;
}

void TimelineWidget::focusTimeline()
{
    setFocus();
    if (rootObject()) {
        rootObject()->setFocus(true);
    }
}

void TimelineWidget::endDrag()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "endBinDrag");
    }
}

void TimelineWidget::startAudioRecord(int tid)
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "startAudioRecord", Qt::DirectConnection, Q_ARG(QVariant, tid));
    }
}

void TimelineWidget::stopAudioRecord()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "stopAudioRecord", Qt::DirectConnection);
    }
}

void TimelineWidget::regainFocus()
{
    if (underMouse() && rootObject()) {
        QPoint mousePos = mapFromGlobal(QCursor::pos());
        QMetaObject::invokeMethod(rootObject(), "regainFocus", Qt::DirectConnection, Q_ARG(QVariant, mousePos));
    }
}

bool TimelineWidget::hasSubtitles() const
{
    return timelineController.getModel()->hasSubtitleModel();
}

void TimelineWidget::connectSubtitleModel(bool firstConnect)
{
    qDebug() << "root context get sub model new function";
    if (!model()->hasSubtitleModel()) {
        return;
    }

    rootObject()->setProperty("showSubtitles", KdenliveSettings::showSubtitles());
    if (firstConnect) {
        rootContext()->setContextProperty("subtitleModel", model()->getSubtitleModel().get());
        QQmlEngine::setObjectOwnership(model()->getSubtitleModel().get(), QQmlEngine::CppOwnership);
    }
}
