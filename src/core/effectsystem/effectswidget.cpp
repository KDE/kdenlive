/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "effectswidget.h"
#include "core.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <KLocale>
#include <KToolBar>
#include <KAction>
#include <KDebug>
#include <QStyle>
#include <QFrame>
#include <QScrollArea>
#include <QMenu>
#include <QToolButton>

#include "effectsystem/effectrepository.h"
#include "effectsystem/effectsystemitem.h"
#include "effectsystem/effectdescription.h"
#include "effectsystem/effectdevice.h"
#include "effectsystem/effect.h"
#include "effectsystem/commands/addeffectcommand.h"
#include "project/producerwrapper.h"
#include "project/timeline.h"
#include "bin/bin.h"

ToolPanel::ToolPanel(QWidget *parent) : 
    QWidget(parent)
{
}


EffectsWidget::EffectsWidget(KToolBar *toolbar, EffectRepository *repo, QWidget* parent) :
    ToolPanel(parent)
  , m_toolbar(toolbar)
  , m_repository(repo)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    setLayout(layout);
    m_frame = new QFrame(this);
    QVBoxLayout *l = new QVBoxLayout;
    m_frame->setLayout(l);
    layout->addWidget(m_frame);
    layout->addStretch(10);
    setStyleSheet(Bin::getStyleSheet());
    m_toolButton = new QToolButton(toolbar);
    m_toolButton->setIcon(KIcon("list-add"));
    m_toolButton->setPopupMode(QToolButton::InstantPopup);
    connect(m_toolButton, SIGNAL(triggered(QAction*)), this, SLOT(slotAddEffect(QAction*)));
    m_effectAction = m_toolbar->addWidget(m_toolButton);
}

EffectsWidget::~EffectsWidget()
{
}

void EffectsWidget::fillToolBar()
{
    m_toolbar->clear();
    m_toolbar->addAction(m_effectAction);
}


void EffectsWidget::setTimeline(Timeline *time)
{
    
    QMenu *menu = new QMenu();

    QMap <QString, QString> effects = m_repository->getEffectsList();
    
    QMap<QString, QString>::const_iterator i = effects.constBegin();
    QMap<QString, QString>::const_iterator end = effects.constEnd();
    while (i != end) {
        QAction *a = menu->addAction(i.key());
        a->setData(i.value());
        ++i;
    }
    m_service = time->producer()->get_service();
    m_toolButton->setMenu(menu);
    m_device = new EffectDevice(m_service, m_repository, m_frame);
    
    // Load current effects
    int ct = 0;
    Mlt::Filter *filter = m_service.filter(ct);
    while (filter) {
        // Load existing effects
        if (filter->is_valid()) {
            if (filter->get_int("_loader")) {
                // Discard normalisers
                ct++;
                filter = m_service.filter(ct);
                continue;
            }
            EffectDescription *desc = new EffectDescription(filter->get("mlt_service"), m_repository);
            if (desc) {
                Effect *e = desc->loadEffect(filter, m_device);
                e->checkPropertiesViewState();
            }
            
        }
        ct++;
        filter = m_service.filter(ct);
    }
    
    
    connect(m_device, SIGNAL(updateClip()), this, SIGNAL(refreshMonitor()));
}

void EffectsWidget::slotAddEffect(QAction *a)
{
    QString effectTag = a->data().toString();
    EffectDescription *desc = new EffectDescription(effectTag, m_repository);
    AddEffectCommand *command = new AddEffectCommand(desc, m_device, true);
    pCore->pushCommand(command);
}


#include "effectswidget.moc"

