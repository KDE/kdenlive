/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "monitormanager.h"
#include "monitormodel.h"
#include "mltcontroller.h"
#include "monitorview.h"
#include <qtextstream.h>
#include "core.h"
#include "mainwindow.h"
#include "project/project.h"
#include "effectsystem/mltcore.h"
#include "project/binmodel.h"
#include "project/timeline.h"
#include "project/projectmanager.h"
#include "project/producerwrapper.h"
#include <KLocale>
#include <QSignalMapper>
#include <QRunnable>
#include <QThreadPool>
#include <KDebug>


class UpdateThumbnailTask : public QRunnable
{
    MltController* m_controller;
    ProducerWrapper *m_producer;
    QString m_id;
    QList <int> m_frames;
public:
    UpdateThumbnailTask(MltController* controller, ProducerWrapper *producer, const QString &id, QList<int> frames)
        : QRunnable()
        , m_controller(controller)
	, m_producer(producer)
	, m_id(id)
        , m_frames(frames)
    {}
    void run() {
        m_controller->renderImage(m_id, m_producer, m_frames);
    }
};

MonitorManager::MonitorManager(QObject* parent) :
    QObject(parent)
{
    //m_modelSignalMapper = new QSignalMapper(this);
    qRegisterMetaType<Mlt::QFrame>("Mlt::QFrame");

    //TODO: make it configurable
    DISPLAYMODE mode;
    if (isSupported(MLTOPENGL)) mode = MLTOPENGL;
    else mode = MLTSDL;

    //TODO: User setting to select between 1 and 2 monitors mode
    
    if (mode == MLTSDL) {
	// SDL only supports one display
	MonitorView *autoView = new MonitorView(mode, new Mlt::Profile(), AutoMonitor, ClipMonitor, pCore->window());
	pCore->window()->addDock(i18n("Monitor"), "auto_monitor", autoView);
	m_monitors.insert(autoView->id(), autoView);
	connect(autoView, SIGNAL(controllerChanged(MONITORID, MltController *)), this, SLOT(updateController(MONITORID, MltController *)));
    }
    else {
	MonitorView *autoView = new MonitorView(mode, new Mlt::Profile(), ClipMonitor, ClipMonitor, pCore->window());
	pCore->window()->addDock(i18n("Clip Monitor"), "clip_monitor", autoView);
	m_monitors.insert(autoView->id(), autoView);
	connect(autoView, SIGNAL(controllerChanged(MONITORID, MltController *)), this, SLOT(updateController(MONITORID, MltController *)));

	MonitorView *autoView2 = new MonitorView(mode, new Mlt::Profile(), ProjectMonitor, ProjectMonitor, pCore->window());
	pCore->window()->addDock(i18n("Project Monitor"), "project_monitor", autoView2);
	m_monitors.insert(autoView2->id(), autoView2);
	connect(autoView2, SIGNAL(controllerChanged(MONITORID, MltController *)), this, SLOT(updateController(MONITORID, MltController *)));
    }
    
    //connect(m_modelSignalMapper, SIGNAL(mapped(const QString &)), this, SLOT(onModelActivated(const QString &)));
    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}


QList <DISPLAYMODE> MonitorManager::supportedDisplayModes()
{
    return pCore->mltCore()->availableDisplayModes();
}

bool MonitorManager::isSupported(DISPLAYMODE mode) const
{
    QList <DISPLAYMODE> modes = pCore->mltCore()->availableDisplayModes();
    return modes.contains(mode);
}

const QString MonitorManager::getDisplayName(DISPLAYMODE mode) const
{
    return pCore->mltCore()->getDisplayName(mode);
}

bool MonitorManager::isAvailable(DISPLAYMODE mode)
{
    bool singleMonitorOnly = false;
    switch (mode) {
      case MLTGLSL:
      case MLTSDL:
	  singleMonitorOnly = true;
	  break;
      default:
	  singleMonitorOnly = false;
	  break;
    }
    if (!singleMonitorOnly) return true;
    // We can only have one monitorof that type
    QHashIterator<MONITORID, MonitorView*> i(m_monitors);
    while (i.hasNext()) {
	i.next();
	if (i.value()->displayType() == mode) {
	    return false;
	}
    }
    return true;
}

void MonitorManager::requestActivation(MONITORID id)
{
    QHashIterator<MONITORID, MonitorView*> i(m_monitors);
    while (i.hasNext()) {
	i.next();
	if (i.key() != id && i.value()->isActive()) {
	    kDebug()<<"CLOSING monitor: "<<i.key() <<" NOT: "<<id;
	    i.value()->close();
	}
    }
    if (!m_monitors.value(id)->isActive()) m_monitors.value(id)->activate();
}

/*void MonitorManager::registerModel(const QString &id, MonitorModel* model, bool needsOwnView)
{
    return;
    if (m_models.contains(id) || model == NULL) return;
    m_models.insert(id, model);
    if (m_controllers.contains(id)) {
        m_controllers.value(id)->setModel(model);
    } else {
        if (needsOwnView) {
            MonitorView *view = new MonitorView(new Mlt::Profile(), pCore->window());
            view->setModel(model);
            m_controllers.insert(id, view);
            pCore->window()->addDock(model->name(), id + "_monitor", view);
        }
    }
    connect(model, SIGNAL(activated()), m_modelSignalMapper, SLOT(map()));
    m_modelSignalMapper->setMapping(model, id);
}*/

void MonitorManager::setProject(Project* project)
{
    
    /*QString id = "bin";
    MonitorView *view = new MonitorView(project->profile(), pCore->window());
    //view->setModel(model);
    m_controllers.insert(id, view);
    pCore->window()->addDock(id, id + "_monitor", view);*/

    if (m_monitors.contains(ClipMonitor)) {
	project->bin()->setMonitor(m_monitors.value(ClipMonitor));
	connect(m_monitors.value(ClipMonitor)->controller()->videoWidget(), SIGNAL(imageRendered(QString,int,QImage)), project->bin(), SLOT(slotGotImage(QString,int,QImage)));
	m_monitors.value(ClipMonitor)->setProfile(project->profile());
	m_monitors.value(ClipMonitor)->activate();
    }
    else {
	project->bin()->setMonitor(m_monitors.value(AutoMonitor));
	connect(m_monitors.value(AutoMonitor)->controller()->videoWidget(), SIGNAL(imageRendered(QString,int,QImage)), project->bin(), SLOT(slotGotImage(QString,int,QImage)));
	m_monitors.value(AutoMonitor)->setProfile(project->profile());
	m_monitors.value(AutoMonitor)->activate();
    }
    /*id = "timeline";
    view = new MonitorView(project->profile(), pCore->window());
    //view->setModel(model);
    m_controllers.insert(id, view);
    pCore->window()->addDock(id, id + "_monitor", view);*/
    if (m_monitors.contains(ProjectMonitor)) {
	project->timeline()->setMonitor(m_monitors.value(ProjectMonitor));
	m_monitors.value(ProjectMonitor)->setProfile(project->profile());
    }
    else {
	project->timeline()->setMonitor(m_monitors.value(AutoMonitor));
    }
    //registerModel("bin", project->binMonitor());
    //registerModel("timeline", project->timelineMonitor());
}

void MonitorManager::updateController(MONITORID id, MltController *controller)
{
    Project *proj = pCore->projectManager()->current();
    if (!proj) return;
    if (id == ClipMonitor) {
	//proj->bin()->setMonitor(controller);
	kDebug()<<"/ // / /UPDATING CLIP MON CTRL";
    }
    else if (id == ProjectMonitor) {
	//proj->timeline()->setMonitor(controller);
	kDebug()<<"/ // / /UPDATING PROJECT MON CTRL";
    }
    else if (id == AutoMonitor) {
	kDebug()<<"/ // / /UPDATING AUTO MON CTRL";
	//proj->bin()->setMonitor(controller);
	//proj->timeline()->setMonitor(controller);
    }
    
    //disconnect(m_monitors.value(id)->controller()->videoWidget(), SIGNAL(imageRendered(QString,int,QImage)), proj->bin(), SLOT(slotGotImage(QString,int,QImage)));
    //m_monitors.insert(id, controller);
    connect(controller->videoWidget(), SIGNAL(imageRendered(QString,int,QImage)), proj->bin(), SLOT(slotGotImage(QString,int,QImage)));
}

void MonitorManager::requestThumbnails(const QString &id, QList <int> positions)
{
    if (m_thumbRequests.contains(id)) {
	QList <int> existingPositions = m_thumbRequests.value(id);
	positions << existingPositions;
	qSort(existingPositions);
    }
    m_thumbRequests.insert(id, positions);
    QMap<QString, QList<int> >::const_iterator i = m_thumbRequests.constBegin();
    MltController *thumbcontroller;
    if (m_monitors.contains(AutoMonitor)) thumbcontroller = m_monitors.value(AutoMonitor)->controller();
    else thumbcontroller = m_monitors.value(ClipMonitor)->controller();
    while (i != m_thumbRequests.constEnd()) {
	QString firstId = i.key();
	QList <int>pos = i.value();
	ProducerWrapper *producer = pCore->projectManager()->current()->bin()->clipProducer(firstId);
	if (producer && producer->is_valid()) {
	    // clear current request and process it
	    m_thumbRequests.insert(firstId, QList <int>());
	    QThreadPool::globalInstance()->start(
                    new UpdateThumbnailTask(thumbcontroller, producer, firstId, pos));
	}
	++i;
    }
    // Cleanup
    QMap<QString, QList<int> > toDo;
    QMap<QString, QList<int> >::const_iterator j = m_thumbRequests.constBegin();
    while (j != m_thumbRequests.constEnd()) {
	if (!j.value().isEmpty()) toDo.insert(j.key(), j.value());
	j++;
    }
    m_thumbRequests.clear();
    if (!toDo.isEmpty()) {
	m_thumbRequests = toDo;
	kDebug()<<" STILL NEED THUMBS: "<<m_thumbRequests.count();
    }
    else kDebug()<<" FINISHED THUMBS!!!!!!!!!";
}

/*void MonitorManager::onModelActivated(MONITORID id)
{
    MonitorModel *model = m_models.value(id);

    if (model == m_active) {
        return;
    }

    m_active = model;

    MonitorView *view;
    if (m_controllers.contains(id)) {
        view = m_controllers.value(id);
    } else {
        view = m_controllers.value("auto");
        view->setModel(model);
    }

    view->parentWidget()->raise();
}*/

#include "monitormanager.moc"
