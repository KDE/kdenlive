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
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "qml/timelineitems.h"
#include "qmltypes/thumbnailprovider.h"
#include "timelinecontroller.h"
#include "transitions/transitionlist/model/transitiontreemodel.hpp"
#include "utils/KoIconUtils.h"

#include <KDeclarative/KDeclarative>
// #include <QUrl>
#include <QAction>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSortFilterProxyModel>
#include <utility>

const int TimelineWidget::comboScale[] = {1, 2, 5, 10, 25, 50, 125, 250, 500, 750, 1500, 3000, 6000, 12000};

TimelineWidget::TimelineWidget(KActionCollection *actionCollection, QWidget *parent)
    : QQuickWidget(parent)
{
    registerTimelineItems();
    m_transitionModel = TransitionTreeModel::construct(true, this);
    m_transitionProxyModel.reset(new AssetFilter(this));
    m_transitionProxyModel->setSourceModel(m_transitionModel.get());
    m_transitionProxyModel->setSortRole(AssetTreeModel::NameRole);
    m_transitionProxyModel->sort(0, Qt::AscendingOrder);
    m_proxy = new TimelineController(actionCollection, this);
    connect(m_proxy, &TimelineController::zoneMoved, this, &TimelineWidget::zoneMoved);
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupBindings();
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_thumbnailer = new ThumbnailProvider;
    engine()->addImageProvider(QStringLiteral("thumbnail"), m_thumbnailer);
    setVisible(false);
    setFocusPolicy(Qt::StrongFocus);
    // connect(&*m_model, SIGNAL(seeked(int)), this, SLOT(onSeeked(int)));
}

TimelineWidget::~TimelineWidget()
{
    delete m_proxy;
}

void TimelineWidget::setModel(std::shared_ptr<TimelineItemModel> model)
{
    m_thumbnailer->resetProject();
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(model.get());
    sortModel->setSortRole(TimelineItemModel::SortRole);
    sortModel->sort(0, Qt::DescendingOrder);
    m_proxy->setModel(model);
    rootContext()->setContextProperty("multitrack", sortModel);
    rootContext()->setContextProperty("controller", model.get());
    rootContext()->setContextProperty("timeline", m_proxy);
    rootContext()->setContextProperty("transitionModel", m_transitionProxyModel.get());
    rootContext()->setContextProperty("guidesModel", pCore->projectManager()->current()->getGuideModel().get());
    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));
    connect(rootObject(), SIGNAL(mousePosChanged(int)), pCore->window(), SLOT(slotUpdateMousePosition(int)));
    m_proxy->setRoot(rootObject());
    setVisible(true);
    loading = false;
    m_proxy->setActiveTrack(model->getTrackIndexFromPosition(model->getTracksCount() - 1));
    m_proxy->checkDuration();
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    emit focusProjectMonitor();
    QQuickWidget::mousePressEvent(event);
}

void TimelineWidget::slotChangeZoom(int value, bool zoomOnMouse)
{
    m_proxy->setScaleFactorOnMouse(100.0 / comboScale[value], zoomOnMouse);
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0u) {
        if (event->delta() > 0) {
            emit zoomIn(true);
        } else {
            emit zoomOut(true);
        }
    } else {
        QQuickWidget::wheelEvent(event);
    }
}

Mlt::Tractor *TimelineWidget::tractor()
{
    return m_proxy->tractor();
}

TimelineController *TimelineWidget::controller()
{
    return m_proxy;
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
