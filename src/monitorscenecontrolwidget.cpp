/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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

#include "monitorscenecontrolwidget.h"
#include "monitorscene.h"
#include "kdenlivesettings.h"


MonitorSceneControlWidget::MonitorSceneControlWidget(MonitorScene* scene, QWidget* parent) :
        QWidget(parent),
        m_scene(scene)
{
    m_ui.setupUi(this);

    m_buttonConfig = new QToolButton();
    m_buttonConfig->setCheckable(true);
    m_buttonConfig->setAutoRaise(true);
    m_buttonConfig->setIcon(KIcon("system-run"));
    m_buttonConfig->setToolTip(i18n("Show/Hide Settings"));
    this->setHidden(true);

    m_ui.buttonShowScene->setIcon(KIcon("video-display"));
    m_ui.buttonShowScene->setToolTip(i18n("Show monitor scene"));
    m_ui.buttonDirectUpdate->setIcon(KIcon("transform-scale"));
    m_ui.buttonDirectUpdate->setToolTip(i18n("Update parameters while monitor scene changes"));
    m_ui.buttonDirectUpdate->setChecked(KdenliveSettings::monitorscene_directupdate());

    m_ui.buttonZoomFit->setIcon(KIcon("zoom-fit-best"));
    m_ui.buttonZoomFit->setToolTip(i18n("Fit zoom to monitor size"));
    m_ui.buttonZoomOriginal->setIcon(KIcon("zoom-original"));
    m_ui.buttonZoomOriginal->setToolTip(i18n("Original size"));
    m_ui.buttonZoomIn->setIcon(KIcon("zoom-in"));
    m_ui.buttonZoomIn->setToolTip(i18n("Zoom in"));
    m_ui.buttonZoomOut->setIcon(KIcon("zoom-out"));
    m_ui.buttonZoomOut->setToolTip(i18n("Zoom out"));

    connect(m_buttonConfig, SIGNAL(toggled(bool)), this, SLOT(setVisible(bool)));

    connect(m_ui.sliderZoom, SIGNAL(valueChanged(int)), m_scene, SLOT(slotZoom(int)));
    connect(m_scene, SIGNAL(zoomChanged(int)), m_ui.sliderZoom, SLOT(setValue(int)));
    connect(m_ui.buttonZoomFit,      SIGNAL(clicked()), m_scene, SLOT(slotZoomFit()));
    connect(m_ui.buttonZoomOriginal, SIGNAL(clicked()), m_scene, SLOT(slotZoomOriginal()));
    connect(m_ui.buttonZoomIn,       SIGNAL(clicked()), m_scene, SLOT(slotZoomIn()));
    connect(m_ui.buttonZoomOut,      SIGNAL(clicked()), m_scene, SLOT(slotZoomOut()));
    m_scene->slotZoomFit();

    connect(m_ui.buttonShowScene, SIGNAL(toggled(bool)), this, SIGNAL(showScene(bool)));
    connect(m_ui.buttonDirectUpdate, SIGNAL(toggled(bool)), m_scene, SLOT(slotSetDirectUpdate(bool)));
}

MonitorSceneControlWidget::~MonitorSceneControlWidget()
{
    delete m_buttonConfig;
}

QToolButton *MonitorSceneControlWidget::getShowHideButton()
{
    return m_buttonConfig;
}

#include "monitorscenecontrolwidget.moc"
