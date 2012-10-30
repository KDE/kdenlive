/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/


#include "monitoreditwidget.h"
#include "renderer.h"
#include "monitorscene.h"
#include "kdenlivesettings.h"

#include "onmonitoritems/onmonitorrectitem.h"
#include "onmonitoritems/onmonitorpathitem.h"

#include <QGraphicsView>
#include <QVBoxLayout>
#include <QAction>
#include <QToolButton>
#include <QMouseEvent>

#include <KIcon>


MonitorEditWidget::MonitorEditWidget(Render* renderer, QWidget* parent) :
        QWidget(parent)
{
    setAutoFillBackground(true);
    setAttribute(Qt::WA_PaintOnScreen, false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_ui.setupUi(this);
    m_scene = new MonitorScene(renderer);
    m_view = new QGraphicsView(m_scene, m_ui.frameVideo);
    m_view->setFrameShape(QFrame::NoFrame);
    //m_view->setRenderHints(QFlags<QPainter::RenderHint>());
    m_view->scale(((double) renderer->renderWidth()) / renderer->frameRenderWidth(), 1.0);
    m_view->setMouseTracking(true);
    m_scene->setUp();

    ((QVBoxLayout*)m_ui.frameVideo->layout())->addWidget(m_view);

    m_customControlsLayout = new QVBoxLayout(m_ui.frameCustomControls);
    m_customControlsLayout->setContentsMargins(0, 4, 0, 4);
    m_customControlsLayout->setSpacing(0);

    m_visibilityAction = new QAction(KIcon("video-display"), i18n("Show/Hide edit mode"), this);
    m_visibilityAction->setCheckable(true);
    m_visibilityAction->setChecked(KdenliveSettings::showOnMonitorScene());
    m_visibilityAction->setVisible(false);

    m_ui.buttonDirectUpdate->setIcon(KIcon("transform-scale"));
    m_ui.buttonDirectUpdate->setToolTip(i18n("Update parameters while monitor scene changes"));
    m_ui.buttonDirectUpdate->setChecked(KdenliveSettings::monitorscene_directupdate());

    m_ui.buttonZoomFit->setIcon(KIcon("zoom-fit-best"));
    m_ui.buttonZoomFit->setToolTip(i18n("Fit zoom to monitor size"));
    m_ui.buttonZoomOriginal->setIcon(KIcon("zoom-original"));
    m_ui.buttonZoomOriginal->setToolTip(i18n("Original size"));

    connect(m_ui.sliderZoom, SIGNAL(valueChanged(int)), m_scene, SLOT(slotZoom(int)));
    connect(m_scene, SIGNAL(zoomChanged(int)), this, SLOT(slotZoom(int)));
    connect(m_ui.buttonZoomFit,      SIGNAL(clicked()), m_scene, SLOT(slotZoomFit()));
    connect(m_ui.buttonZoomOriginal, SIGNAL(clicked()), m_scene, SLOT(slotZoomOriginal()));
    m_scene->slotZoomFit();

    connect(m_visibilityAction, SIGNAL(triggered(bool)), this, SIGNAL(showEdit(bool)));
    connect(m_ui.buttonDirectUpdate, SIGNAL(toggled(bool)), this, SLOT(slotSetDirectUpdate(bool)));
}

MonitorEditWidget::~MonitorEditWidget()
{
    KdenliveSettings::setShowOnMonitorScene(m_visibilityAction->isChecked());
    delete m_view;
    delete m_scene;
    delete m_visibilityAction;
}

void MonitorEditWidget::slotZoom(int value)
{
    m_ui.sliderZoom->blockSignals(true);
    m_ui.sliderZoom->setValue(value);
    m_ui.sliderZoom->blockSignals(false);
}

void MonitorEditWidget::resetProfile(Render* renderer)
{
    m_view->scale(((double) renderer->renderWidth()) / renderer->frameRenderWidth(), 1.0);
    m_scene->resetProfile();
}

MonitorScene* MonitorEditWidget::getScene()
{
    return m_scene;
}

QAction* MonitorEditWidget::getVisibilityAction()
{
    return m_visibilityAction;
}

void MonitorEditWidget::showVisibilityButton(bool show)
{
    m_visibilityAction->setVisible(show);
}

void MonitorEditWidget::addCustomControl(QWidget* widget)
{
    m_customControlsLayout->addWidget(widget);
}

void MonitorEditWidget::addCustomButton(const QIcon& icon, const QString& text, const QObject* receiver, const char* member, bool checkable, bool checked)
{
    QToolButton *button = new QToolButton(m_ui.frameCustomControls);
    button->setIcon(icon);
    button->setToolTip(text);
    button->setCheckable(checkable);
    button->setChecked(checked);
    button->setAutoRaise(true);
    if (checkable)
        connect(button, SIGNAL(toggled(bool)), receiver, member);
    else
        connect(button, SIGNAL(clicked()), receiver, member);
    m_customControlsLayout->addWidget(button);
}

void MonitorEditWidget::removeCustomControls()
{
    QLayoutItem *child;
    while ((child = m_customControlsLayout->takeAt(0)) != 0) {
        if (child->widget()) delete child->widget();
        delete child;
    }
}

void MonitorEditWidget::slotSetDirectUpdate(bool directUpdate)
{
    KdenliveSettings::setMonitorscene_directupdate(directUpdate);
}

#include "monitoreditwidget.moc"
