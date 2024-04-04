/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KLocalizedContext>

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
#include "qml/timelineitems.h"
#include "qmltypes/thumbnailprovider.h"
#include "timelinecontroller.h"
#include "timelinewidget.h"
#include "utils/clipboardproxy.hpp"

#include <QAction>
#include <QActionGroup>
#include <QFontDatabase>
#include <QMenu>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSortFilterProxyModel>
#include <QUuid>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "kdeclarative_version.h"
#endif
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION > QT_VERSION_CHECK(5, 98, 0)
#include <KQuickIconProvider>
#else
#include <KDeclarative/KDeclarative>
#endif

const int TimelineWidget::comboScale[] = {1, 2, 4, 8, 15, 30, 50, 75, 100, 150, 200, 300, 500, 800, 1000, 1500, 2000, 3000, 6000, 15000, 30000};

TimelineWidget::TimelineWidget(const QUuid uuid, QWidget *parent)
    : QQuickWidget(parent)
    , m_uuid(uuid)
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION > QT_VERSION_CHECK(5, 98, 0)
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
#else
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
#endif
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));
    setClearColor(palette().window().color());
    setMouseTracking(true);
    registerTimelineItems();
    m_sortModel = std::make_unique<QSortFilterProxyModel>(this);
    m_proxy = new TimelineController(this);
    connect(m_proxy, &TimelineController::zoneMoved, this, &TimelineWidget::zoneMoved);
    connect(m_proxy, &TimelineController::ungrabHack, this, &TimelineWidget::slotUngrabHack);
    connect(m_proxy, &TimelineController::regainFocus, this, &TimelineWidget::regainFocus, Qt::DirectConnection);
    connect(m_proxy, &TimelineController::stopAudioRecord, this, &TimelineWidget::stopAudioRecord, Qt::DirectConnection);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setVisible(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setFocusPolicy(Qt::StrongFocus);
    m_favEffects = new QMenu(i18n("Insert an effect..."), this);
    m_favCompositions = new QMenu(i18n("Insert a composition..."), this);
    installEventFilter(this);
    m_targetsMenu = new QMenu(this);
}

TimelineWidget::~TimelineWidget()
{
    delete m_proxy;
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
        sortedItems.insert(m_proxy->getAssetName(effect, isTransition), effect);
    }
    return sortedItems;
}

void TimelineWidget::setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu,
                                     QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu)
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
    updateEffectFavorites();
    updateTransitionFavorites();
    connect(m_favEffects, &QMenu::triggered, this, [&](QAction *ac) { m_proxy->addEffectToClip(ac->data().toString()); });
    connect(m_favCompositions, &QMenu::triggered, this, [&](QAction *ac) { m_proxy->addCompositionToClip(ac->data().toString()); });
    connect(m_guideMenu, &QMenu::triggered, this, [&](QAction *ac) { m_proxy->setPosition(ac->data().toInt()); });
    connect(m_thumbsMenu, &QMenu::triggered, this,
            [&](QAction *ac) { m_proxy->setActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format"), ac->data().toString()); });
    // Fix qml focus issue
    connect(m_headerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::triggered, this, &TimelineWidget::slotResetContextPos);
    connect(m_timelineCompositionMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineRulerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::triggered, this, &TimelineWidget::slotResetContextPos);
    connect(m_timelineSubtitleClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);

    m_timelineClipMenu->addMenu(m_favEffects);
    m_timelineClipMenu->addMenu(m_favCompositions);
    m_timelineMenu->addMenu(m_favCompositions);
}

void TimelineWidget::unsetModel()
{
    rootContext()->setContextProperty("controller", nullptr);
    rootContext()->setContextProperty("multitrack", nullptr);
    rootContext()->setContextProperty("timeline", nullptr);
    rootContext()->setContextProperty("guidesModel", nullptr);
    rootContext()->setContextProperty("subtitleModel", nullptr);
    m_sortModel.reset(new QSortFilterProxyModel(this));
    m_proxy->prepareClose();
}

const QUuid &TimelineWidget::getUuid() const
{
    return m_uuid;
}

void TimelineWidget::setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy)
{
    loading = true;
    m_sortModel->setSourceModel(model.get());
    m_sortModel->setSortRole(TimelineItemModel::SortRole);
    m_sortModel->sort(0, Qt::DescendingOrder);
    m_proxy->setModel(model);
    rootContext()->setContextProperty("multitrack", m_sortModel.get());
    rootContext()->setContextProperty("controller", model.get());
    rootContext()->setContextProperty("timeline", m_proxy);
    rootContext()->setContextProperty("guidesModel", model->getFilteredGuideModel().get());
    // Create a unique id for this timeline to prevent thumbnails
    // leaking from one project to another because of qml's image caching
    rootContext()->setContextProperty("documentId", model->uuid());
    rootContext()->setContextProperty("audiorec", pCore->getAudioDevice());
    rootContext()->setContextProperty("clipboard", new ClipboardProxy(this));
    rootContext()->setContextProperty("miniFont", QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    rootContext()->setContextProperty("subtitleModel", model->getSubtitleModel().get());
    const QStringList effs = sortedItems(KdenliveSettings::favorite_effects(), false).values();
    const QStringList trans = sortedItems(KdenliveSettings::favorite_transitions(), true).values();

    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));
    engine()->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    connect(rootObject(), SIGNAL(mousePosChanged(int)), this, SLOT(emitMousePos(int)));
    connect(rootObject(), SIGNAL(zoomIn(bool)), pCore->window(), SLOT(slotZoomIn(bool)));
    connect(rootObject(), SIGNAL(zoomOut(bool)), pCore->window(), SLOT(slotZoomOut(bool)));
    connect(rootObject(), SIGNAL(processingDrag(bool)), pCore->window(), SIGNAL(enableUndo(bool)));
    connect(m_proxy, &TimelineController::seeked, proxy, &MonitorProxy::setPosition);
    rootObject()->setProperty("dar", pCore->getCurrentDar());
    connect(rootObject(), SIGNAL(showClipMenu(int)), this, SLOT(showClipMenu(int)));
    connect(rootObject(), SIGNAL(showMixMenu(int)), this, SLOT(showMixMenu(int)));
    connect(rootObject(), SIGNAL(showCompositionMenu()), this, SLOT(showCompositionMenu()));
    connect(rootObject(), SIGNAL(showTimelineMenu()), this, SLOT(showTimelineMenu()));
    connect(rootObject(), SIGNAL(showRulerMenu()), this, SLOT(showRulerMenu()));
    connect(rootObject(), SIGNAL(showHeaderMenu()), this, SLOT(showHeaderMenu()));
    connect(rootObject(), SIGNAL(showTargetMenu(int)), this, SLOT(showTargetMenu(int)));
    connect(rootObject(), SIGNAL(showSubtitleClipMenu()), this, SLOT(showSubtitleClipMenu()));
    m_proxy->setRoot(rootObject());
    setVisible(true);
    loading = false;
    m_proxy->checkDuration();
}

void TimelineWidget::emitMousePos(int offset)
{
    pCore->window()->slotUpdateMousePosition(int((offset + mapFromGlobal(QCursor::pos()).x()) / m_proxy->scaleFactor()));
}

void TimelineWidget::loadMarkerModel()
{
    if (m_proxy) {
        rootContext()->setContextProperty("guidesModel", m_proxy->getModel()->getFilteredGuideModel().get());
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    Q_EMIT focusProjectMonitor();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_clickPos = event->globalPos();
#else
    m_clickPos = event->globalPosition().toPoint();
#endif
    QQuickWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getMouseOffset", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    emitMousePos(returnedValue.toInt());
    QQuickWidget::mousePressEvent(event);
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
    for (auto ac : qAsConst(effects)) {
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
    bool isAudio = m_proxy->isActiveTrackAudio();
    QList<QAction *> menuActions = m_headerMenu->actions();
    QList<QAction *> audioActions;
    QStringList allowedActions = {QLatin1String("show_track_record"), QLatin1String("separate_channels"), QLatin1String("normalize_channels")};
    for (QAction *ac : qAsConst(menuActions)) {
        if (allowedActions.contains(ac->data().toString())) {
            audioActions << ac;
        }
    }
    if (!isAudio) {
        // Video track
        int currentThumbs = m_proxy->getActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format")).toInt();
        QList<QAction *> actions = m_thumbsMenu->actions();
        for (QAction *ac : qAsConst(actions)) {
            if (ac->data().toInt() == currentThumbs) {
                ac->setChecked(true);
                break;
            }
        }
        m_thumbsMenu->menuAction()->setVisible(true);
        for (auto ac : qAsConst(audioActions)) {
            ac->setVisible(false);
        }
    } else {
        // Audio track
        m_thumbsMenu->menuAction()->setVisible(false);
        for (auto ac : qAsConst(audioActions)) {
            ac->setVisible(true);
            if (ac->data().toString() == QLatin1String("show_track_record")) {
                ac->setChecked(m_proxy->getActiveTrackProperty(QStringLiteral("kdenlive:audio_rec")).toInt() == 1);
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
        tid = m_proxy->activeTrack();
        if (tid == -1) {
            return;
        }
        if (m_proxy->clipTargets() < 2 || !model()->isAudioTrack(tid)) {
            pCore->displayMessage(i18n("No available stream"), MessageType::ErrorMessage);
            return;
        }
        QVariant returnedValue;
        QMetaObject::invokeMethod(rootObject(), "getActiveTrackStreamPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
        m_clickPos = mapToGlobal(QPoint(5, y())) + QPoint(0, returnedValue.toInt());
    }
    QMap<int, QString> possibleTargets = m_proxy->getCurrentTargets(tid, currentTargetStream);
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
        m_proxy->assignAudioTarget(tid, targetStream);
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
    m_timelineMenu->popup(m_clickPos);
}

void TimelineWidget::showSubtitleClipMenu()
{
    m_timelineSubtitleClipMenu->popup(m_clickPos);
}

void TimelineWidget::slotChangeZoom(int value, bool zoomOnMouse)
{
    double pixelScale = QFontMetrics(font()).maxWidth() * 2;
    m_proxy->setScaleFactorOnMouse(pixelScale / comboScale[value], zoomOnMouse);
}

void TimelineWidget::slotCenterView()
{
    QMetaObject::invokeMethod(rootObject(), "centerViewOnCursor");
}

void TimelineWidget::slotFitZoom()
{
    QVariant returnedValue;
    double prevScale = m_proxy->scaleFactor();
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
    m_proxy->setScaleFactorOnMouse(scale, false);
    // Update zoom slider
    Q_EMIT m_proxy->updateZoom(scale);
    QMetaObject::invokeMethod(rootObject(), "goToStart", Q_ARG(QVariant, scrollPos));
}

Mlt::Tractor *TimelineWidget::tractor()
{
    return m_proxy->tractor();
}

TimelineController *TimelineWidget::controller()
{
    return m_proxy;
}

std::shared_ptr<TimelineItemModel> TimelineWidget::model()
{
    return m_proxy->getModel();
}

void TimelineWidget::zoneUpdated(const QPoint &zone)
{
    m_proxy->setZone(zone, false);
}

void TimelineWidget::zoneUpdatedWithUndo(const QPoint &oldZone, const QPoint &newZone)
{
    m_proxy->updateZone(oldZone, newZone);
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
    return m_proxy->getAvTracksCount();
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

void TimelineWidget::focusInEvent(QFocusEvent *event)
{
    QQuickWidget::focusInEvent(event);
    if (!m_proxy->grabIsActive()) {
        QTimer::singleShot(250, rootObject(), SLOT(forceActiveFocus()));
    }
}

bool TimelineWidget::eventFilter(QObject *object, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter:
        if (!hasFocus()) {
            Q_EMIT pCore->window()->showTimelineFocus(true, true);
        }
        break;
    case QEvent::Leave:
        if (!hasFocus()) {
            Q_EMIT pCore->window()->showTimelineFocus(false, true);
        }
        break;
    case QEvent::FocusOut:
        Q_EMIT pCore->window()->showTimelineFocus(false, false);
        break;
    case QEvent::FocusIn:
        Q_EMIT pCore->window()->showTimelineFocus(true, false);
        break;
    default:
        break;
    }
    return QQuickWidget::eventFilter(object, event);
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
    return m_proxy->getModel()->hasSubtitleModel();
}

void TimelineWidget::connectSubtitleModel(bool firstConnect)
{
    qDebug() << "root context get sub model new function";
    if (!model()->hasSubtitleModel()) {
        // qDebug()<<"null ptr here at root context";
        return;
    } else {
        // qDebug()<<"null ptr NOT here at root context";
        rootObject()->setProperty("showSubtitles", KdenliveSettings::showSubtitles());
        if (firstConnect) {
            rootContext()->setContextProperty("subtitleModel", model()->getSubtitleModel().get());
        }
    }
}
