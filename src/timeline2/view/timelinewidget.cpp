/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "timelinewidget.h"
#include "../model/builders/meltBuilder.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "capture/mediacapture.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "monitor/monitorproxy.h"
#include "qml/timelineitems.h"
#include "qmltypes/thumbnailprovider.h"
#include "timelinecontroller.h"
#include "utils/clipboardproxy.hpp"
#include "effects/effectsrepository.hpp"

#include <KDeclarative/KDeclarative>
// #include <QUrl>
#include <QAction>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QActionGroup>
#include <QUuid>
#include <QMenu>
#include <QFontDatabase>
#include <QSortFilterProxyModel>

const int TimelineWidget::comboScale[] = {1, 2, 4, 8, 15, 30, 50, 75, 100, 150, 200, 300, 500, 800, 1000, 1500, 2000, 3000, 6000, 15000, 30000};

TimelineWidget::TimelineWidget(QWidget *parent)
    : QQuickWidget(parent)
    , m_targetsGroup(nullptr)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
    kdeclarative.setupContext();
    setClearColor(palette().window().color());
    registerTimelineItems();
    m_sortModel = std::make_unique<QSortFilterProxyModel>(this);
    m_proxy = new TimelineController(this);
    connect(m_proxy, &TimelineController::zoneMoved, this, &TimelineWidget::zoneMoved);
    connect(m_proxy, &TimelineController::ungrabHack, this, &TimelineWidget::slotUngrabHack);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    engine()->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
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

void TimelineWidget::setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu, QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu)
{
    m_timelineClipMenu = clipMenu;
    m_timelineCompositionMenu = compositionMenu;
    m_timelineMenu = timelineMenu;
    m_timelineRulerMenu = timelineRulerMenu;
    m_guideMenu = guideMenu;
    m_headerMenu = headerMenu;
    m_thumbsMenu = thumbsMenu;
    m_headerMenu->addMenu(m_thumbsMenu);
    m_editGuideAcion = editGuideAction;
    updateEffectFavorites();
    updateTransitionFavorites();
    connect(m_favEffects, &QMenu::triggered, this, [&] (QAction *ac) {
        m_proxy->addEffectToClip(ac->data().toString());
    });
    connect(m_favCompositions, &QMenu::triggered, this, [&] (QAction *ac) {
        m_proxy->addCompositionToClip(ac->data().toString());
    });
    connect(m_guideMenu, &QMenu::triggered, this, [&] (QAction *ac) {
        m_proxy->setPosition(ac->data().toInt());
    });
    connect(m_thumbsMenu, &QMenu::triggered, this, [&] (QAction *ac) {
        m_proxy->setActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format"), ac->data().toString());
    });
    // Fix qml focus issue
    connect(m_headerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineCompositionMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineRulerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);

    m_timelineClipMenu->addMenu(m_favEffects);
    m_timelineClipMenu->addMenu(m_favCompositions);
    m_timelineMenu->addMenu(m_favCompositions);
}

void TimelineWidget::setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy)
{
    m_sortModel->setSourceModel(model.get());
    m_sortModel->setSortRole(TimelineItemModel::SortRole);
    m_sortModel->sort(0, Qt::DescendingOrder);
    m_proxy->setModel(model);
    rootContext()->setContextProperty("multitrack", m_sortModel.get());
    rootContext()->setContextProperty("controller", model.get());
    rootContext()->setContextProperty("timeline", m_proxy);
    rootContext()->setContextProperty("proxy", proxy);
    // Create a unique id for this timeline to prevent thumbnails 
    // leaking from one project to another because of qml's image caching
    rootContext()->setContextProperty("documentId", QUuid::createUuid());
    rootContext()->setContextProperty("audiorec", pCore->getAudioDevice());
    rootContext()->setContextProperty("guidesModel", pCore->projectManager()->current()->getGuideModel().get());
    rootContext()->setContextProperty("clipboard", new ClipboardProxy(this));
    QFont ft = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    ft.setPointSize(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont).pointSize());
    setFont(ft);
    rootContext()->setContextProperty("miniFont", font());
    const QStringList effs = sortedItems(KdenliveSettings::favorite_effects(), false).values();
    const QStringList trans = sortedItems(KdenliveSettings::favorite_transitions(), true).values();

    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));
    connect(rootObject(), SIGNAL(mousePosChanged(int)), pCore->window(), SLOT(slotUpdateMousePosition(int)));
    connect(rootObject(), SIGNAL(zoomIn(bool)), pCore->window(), SLOT(slotZoomIn(bool)));
    connect(rootObject(), SIGNAL(zoomOut(bool)), pCore->window(), SLOT(slotZoomOut(bool)));
    connect(rootObject(), SIGNAL(processingDrag(bool)), pCore->window(), SIGNAL(enableUndo(bool)));
    connect(m_proxy, &TimelineController::seeked, proxy, &MonitorProxy::setPosition);
    rootObject()->setProperty("dar", pCore->getCurrentDar());
    connect(rootObject(), SIGNAL(showClipMenu(int)), this, SLOT(showClipMenu(int)));
    connect(rootObject(), SIGNAL(showCompositionMenu()), this, SLOT(showCompositionMenu()));
    connect(rootObject(), SIGNAL(showTimelineMenu()), this, SLOT(showTimelineMenu()));
    connect(rootObject(), SIGNAL(showRulerMenu()), this, SLOT(showRulerMenu()));
    connect(rootObject(), SIGNAL(showHeaderMenu()), this, SLOT(showHeaderMenu()));
    connect(rootObject(), SIGNAL(showTargetMenu(int)), this, SLOT(showTargetMenu(int)));
    m_proxy->setRoot(rootObject());
    setVisible(true);
    loading = false;
    m_proxy->checkDuration();
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    emit focusProjectMonitor();
    m_clickPos = event->globalPos();
    QQuickWidget::mousePressEvent(event);
}

void TimelineWidget::showClipMenu(int cid)
{
    // Hide not applicable effects
    QList <QAction *> effects = m_favEffects->actions();
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

void TimelineWidget::showCompositionMenu()
{
    m_timelineCompositionMenu->popup(m_clickPos);
}

void TimelineWidget::showHeaderMenu()
{
    bool isAudio = m_proxy->isActiveTrackAudio();
    QList <QAction *> menuActions = m_headerMenu->actions();
    QList <QAction *> audioActions;
    for (QAction *ac : qAsConst(menuActions)) {
        if (ac->data().toString() == QLatin1String("show_track_record") || ac->data().toString() == QLatin1String("separate_channels")) {
            audioActions << ac;
        }
    }
    if (!isAudio) {
        // Video track
        int currentThumbs = m_proxy->getActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format")).toInt();
        QList <QAction *> actions = m_thumbsMenu->actions();
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
            pCore->displayMessage(i18n("No available stream"), MessageType::InformationMessage);
            return;
        }
        QVariant returnedValue;
        QMetaObject::invokeMethod(rootObject(), "getActiveTrackStreamPos", Q_RETURN_ARG(QVariant, returnedValue));
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
    connect(m_targetsGroup, &QActionGroup::triggered, this, [this, tid] (QAction *action) {
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
    const QList<CommentedTime> guides = pCore->projectManager()->current()->getGuideModel()->getAllMarkers();
    QAction *ac;
    m_editGuideAcion->setEnabled(false);
    double fps = pCore->getCurrentFps();
    int currentPos = rootObject()->property("consumerPosition").toInt();
    for (const auto &guide : guides) {
        ac = new QAction(guide.comment(), this);
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
    const QList<CommentedTime> guides = pCore->projectManager()->current()->getGuideModel()->getAllMarkers();
    QAction *ac;
    m_editGuideAcion->setEnabled(false);
    double fps = pCore->getCurrentFps();
    int currentPos = rootObject()->property("consumerPosition").toInt();
    for (const auto &guide : guides) {
        ac = new QAction(guide.comment(), this);
        int frame = guide.time().frames(fps);
        ac->setData(frame);
        if (frame == currentPos) {
            m_editGuideAcion->setEnabled(true);
        }
        m_guideMenu->addAction(ac);
    }
    m_timelineMenu->popup(m_clickPos);
}

void TimelineWidget::slotChangeZoom(int value, bool zoomOnMouse)
{
    double pixelScale = QFontMetrics(font()).maxWidth() * 2;
    m_proxy->setScaleFactorOnMouse(pixelScale / comboScale[value], zoomOnMouse);
}

void TimelineWidget::slotFitZoom()
{
    QVariant returnedValue;
    double prevScale = m_proxy->scaleFactor();
    QMetaObject::invokeMethod(rootObject(), "fitZoom", Q_RETURN_ARG(QVariant, returnedValue));
    double scale = returnedValue.toDouble();
    QMetaObject::invokeMethod(rootObject(), "scrollPos", Q_RETURN_ARG(QVariant, returnedValue));
    int scrollPos = returnedValue.toInt();
    if (qFuzzyCompare(prevScale, scale)) {
        scale = m_prevScale;
        scrollPos = m_scrollPos;
    } else {
        m_prevScale = prevScale;
        m_scrollPos = scrollPos;
        scrollPos = 0;
    }
    m_proxy->setScaleFactorOnMouse(scale, false);
    // Update zoom slider
    emit m_proxy->updateZoom(scale);
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

void TimelineWidget::setTool(ProjectTool tool)
{
    rootObject()->setProperty("activeTool", (int)tool);
}

QPair<int, int> TimelineWidget::getTracksCount() const
{
    return m_proxy->getTracksCount();
}

void TimelineWidget::slotUngrabHack()
{
    // Workaround bug: https://bugreports.qt.io/browse/QTBUG-59044
    // https://phabricator.kde.org/D5515
    if (quickWindow() && quickWindow()->mouseGrabberItem()) {
        quickWindow()->mouseGrabberItem()->ungrabMouse();
        // Reset menu position
        QTimer::singleShot(200, this, [this]() {
            rootObject()->setProperty("mainFrame", -1);
        });
    }
}

int TimelineWidget::zoomForScale(double value) const
{
    int scale = 100.0 / value;
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


bool TimelineWidget::eventFilter(QObject *object, QEvent *event)
{
    switch(event->type()) {
        case QEvent::Enter:
            if (!hasFocus()) {
                emit pCore->window()->focusTimeline(true, true);
            }
            break;
        case QEvent::Leave:
            if (!hasFocus()) {
                emit pCore->window()->focusTimeline(false, true);
            }
            break;
        case QEvent::FocusOut:
            emit pCore->window()->focusTimeline(false, false);
            break;
        case QEvent::FocusIn:
            emit pCore->window()->focusTimeline(true, false);
            break;
        default:
            break;
    }

    return QQuickWidget::eventFilter(object, event);
}
