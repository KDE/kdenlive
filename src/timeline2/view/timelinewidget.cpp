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
#include "effects/effectlist/model/effectfilter.hpp"
#include "effects/effectlist/model/effecttreemodel.hpp"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "monitor/monitorproxy.h"
#include "qml/timelineitems.h"
#include "qmltypes/thumbnailprovider.h"
#include "timelinecontroller.h"
#include "transitions/transitionlist/model/transitionfilter.hpp"
#include "transitions/transitionlist/model/transitiontreemodel.hpp"
#include "utils/clipboardproxy.hpp"

#include <KDeclarative/KDeclarative>
// #include <QUrl>
#include <QAction>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUuid>
#include <QFontDatabase>
#include <QSortFilterProxyModel>

const int TimelineWidget::comboScale[] = {1, 2, 4, 8, 15, 30, 50, 75, 100, 150, 200, 300, 500, 800, 1000, 1500, 2000, 3000, 6000, 15000, 30000};

TimelineWidget::TimelineWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
    kdeclarative.setupContext();
    setClearColor(palette().window().color());
    registerTimelineItems();
    // Build transition model for context menu
    m_transitionModel = TransitionTreeModel::construct(true, this);
    m_transitionProxyModel = std::make_unique<TransitionFilter>(this);
    static_cast<TransitionFilter *>(m_transitionProxyModel.get())->setFilterType(true, TransitionType::Favorites);
    m_transitionProxyModel->setSourceModel(m_transitionModel.get());
    m_transitionProxyModel->setSortRole(AssetTreeModel::NameRole);
    m_transitionProxyModel->sort(0, Qt::AscendingOrder);

    // Build effects model for context menu
    m_effectsModel = EffectTreeModel::construct(QStringLiteral(), this);
    m_effectsProxyModel = std::make_unique<EffectFilter>(this);
    static_cast<EffectFilter *>(m_effectsProxyModel.get())->setFilterType(true, EffectType::Favorites);
    m_effectsProxyModel->setSourceModel(m_effectsModel.get());
    m_effectsProxyModel->setSortRole(AssetTreeModel::NameRole);
    m_effectsProxyModel->sort(0, Qt::AscendingOrder);
    m_proxy = new TimelineController(this);
    connect(m_proxy, &TimelineController::zoneMoved, this, &TimelineWidget::zoneMoved);
    connect(m_proxy, &TimelineController::ungrabHack, this, &TimelineWidget::slotUngrabHack);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_thumbnailer = new ThumbnailProvider;
    engine()->addImageProvider(QStringLiteral("thumbnail"), m_thumbnailer);
    setVisible(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setFocusPolicy(Qt::StrongFocus);
}

TimelineWidget::~TimelineWidget()
{
    delete m_proxy;
}

void TimelineWidget::updateEffectFavorites()
{
    rootContext()->setContextProperty("effectModel", sortedItems(KdenliveSettings::favorite_effects(), false));
}

void TimelineWidget::updateTransitionFavorites()
{
    rootContext()->setContextProperty("transitionModel", sortedItems(KdenliveSettings::favorite_transitions(), true));
}

const QStringList TimelineWidget::sortedItems(const QStringList &items, bool isTransition)
{
    QMap<QString, QString> sortedItems;
    for (const QString &effect : items) {
        sortedItems.insert(m_proxy->getAssetName(effect, isTransition), effect);
    }
    return sortedItems.values();
}

void TimelineWidget::setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy)
{
    m_sortModel = std::make_unique<QSortFilterProxyModel>(this);
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
    rootContext()->setContextProperty("transitionModel", sortedItems(KdenliveSettings::favorite_transitions(), true)); // m_transitionProxyModel.get());
    // rootContext()->setContextProperty("effectModel", m_effectsProxyModel.get());
    rootContext()->setContextProperty("effectModel", sortedItems(KdenliveSettings::favorite_effects(), false));
    rootContext()->setContextProperty("audiorec", pCore->getAudioDevice());
    rootContext()->setContextProperty("guidesModel", pCore->projectManager()->current()->getGuideModel().get());
    rootContext()->setContextProperty("clipboard", new ClipboardProxy(this));
    rootContext()->setContextProperty("smallFont", QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));
    connect(rootObject(), SIGNAL(mousePosChanged(int)), pCore->window(), SLOT(slotUpdateMousePosition(int)));
    connect(rootObject(), SIGNAL(zoomIn(bool)), pCore->window(), SLOT(slotZoomIn(bool)));
    connect(rootObject(), SIGNAL(zoomOut(bool)), pCore->window(), SLOT(slotZoomOut(bool)));
    connect(rootObject(), SIGNAL(processingDrag(bool)), pCore->window(), SIGNAL(enableUndo(bool)));
    connect(m_proxy, &TimelineController::seeked, proxy, &MonitorProxy::setPosition);
    m_proxy->setRoot(rootObject());
    setVisible(true);
    loading = false;
    m_proxy->checkDuration();
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    emit focusProjectMonitor();
    QQuickWidget::mousePressEvent(event);
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
    m_proxy->updateZoom(scale);
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
    m_proxy->setZone(zone);
}

void TimelineWidget::setTool(ProjectTool tool)
{
    rootObject()->setProperty("activeTool", (int)tool);
}

QPoint TimelineWidget::getTracksCount() const
{
    return m_proxy->getTracksCount();
}

void TimelineWidget::slotUngrabHack()
{
    // Workaround bug: https://bugreports.qt.io/browse/QTBUG-59044
    // https://phabricator.kde.org/D5515
    if (quickWindow() && quickWindow()->mouseGrabberItem()) {
        quickWindow()->mouseGrabberItem()->ungrabMouse();
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

