/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "notesplugin.h"
#include "core.h"
#include "dialogs/noteswidget.h"
#include "mainwindow.h"
#include "doc/kdenlivedoc.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include "klocalizedstring.h"

#include <QStyle>

NotesPlugin::NotesPlugin(ProjectManager *projectManager)
    : QObject(projectManager)
{
    QWidget *container = new QWidget();
    auto *lay = new QVBoxLayout();
    lay->setSpacing(0);
    m_tb = new QToolBar();
    m_tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    int size = container->style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);
    m_tb->setIconSize(iconSize);
    lay->addWidget(m_tb);
    m_widget = new NotesWidget();
    lay->addWidget(m_widget);
    container->setLayout(lay);
    connect(m_widget, &NotesWidget::insertNotesTimecode, this, &NotesPlugin::slotInsertTimecode);
    connect(m_widget, &NotesWidget::reAssign, this, &NotesPlugin::slotReAssign);
    m_widget->setTabChangesFocus(true);
    m_widget->setPlaceholderText(i18n("Enter your project notes here ..."));
    m_notesDock = pCore->window()->addDock(i18n("Project Notes"), QStringLiteral("notes_widget"), container);
    m_notesDock->close();
    connect(projectManager, &ProjectManager::docOpened, this, &NotesPlugin::setProject);
}

void NotesPlugin::setProject(KdenliveDoc *document)
{
    connect(m_widget, &NotesWidget::seekProject, pCore->monitorManager()->projectMonitor(), &Monitor::requestSeek);
    connect(m_widget, SIGNAL(textChanged()), document, SLOT(setModified()));
    if (m_tb->actions().isEmpty()) {
        // initialize toolbar
        m_tb->addAction(pCore->window()->action("add_project_note"));
        QAction *a = new QAction(QIcon::fromTheme(QStringLiteral("edit-find-replace")), i18n("Reassign selected timecodes to current Bin clip"));
        connect(a, &QAction::triggered, m_widget, &NotesWidget::assignProjectNote);
        m_tb->addAction(a);
        a = new QAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Create markers from selected timecodes"));
        connect(a, &QAction::triggered, m_widget, &NotesWidget::createMarkers);
        m_tb->addAction(a);
    }
}

void NotesPlugin::showDock()
{
    m_notesDock->show();
}

void NotesPlugin::slotInsertTimecode()
{
    if (pCore->monitorManager()->isActive(Kdenlive::ClipMonitor)) {
        // Add a note on the current bin clip
        int frames = pCore->monitorManager()->clipMonitor()->position();
        QString position = pCore->timecode().getTimecodeFromFrames(frames);
        const QString binId = pCore->monitorManager()->clipMonitor()->activeClipId();
        if (binId.isEmpty()) {
            pCore->displayMessage(i18n("Cannot add note, no clip selected in project bin"), InformationMessage);
            return;
        }
        QString clipName = pCore->bin()->getBinClipName(binId);
        m_widget->insertHtml(QString("<a href=\"%1#%2\">%3:%4</a> ").arg(binId, QString::number(frames)).arg(clipName, position));
    } else {
        int frames = pCore->monitorManager()->projectMonitor()->position();
        QString position = pCore->timecode().getTimecodeFromFrames(frames);
        m_widget->insertHtml(QString("<a href=\"%1\">%2</a> ").arg(QString::number(frames), position));
    }
}

void NotesPlugin::slotReAssign(QStringList anchors, QList <QPoint> points)
{
    const QString binId = pCore->monitorManager()->clipMonitor()->activeClipId();
    int ix = 0;
    if (points.count() != anchors.count()) {
        // Something is wrong, abort
        pCore->displayMessage(i18n("Cannot perform assign"), InformationMessage);
        return;
    }
    for (const QString & a : anchors) {
        QPoint pt = points.at(ix);
        QString updatedLink = a;
        int position = 0;
        if (a.contains(QLatin1Char('#'))) {
            // Link was previously attached to another clip
            updatedLink = a.section(QLatin1Char('#'), 1);
            position = updatedLink.toInt();
            if (!binId.isEmpty()) {
                updatedLink.prepend(QString("%1#").arg(binId));
            }
        } else {
            updatedLink = a;
            position = a.toInt();
            if (!binId.isEmpty()) {
                updatedLink.prepend(QString("%1#").arg(binId));
            }
        }
        QTextCursor cur(m_widget->textCursor());
        cur.setPosition(pt.x());
        cur.setPosition(pt.y(), QTextCursor::KeepAnchor);
        QString pos = pCore->timecode().getTimecodeFromFrames(position);
        if (!binId.isEmpty()) {
            QString clipName = pCore->bin()->getBinClipName(binId);
            cur.insertHtml(QString("<a href=\"%1\">%2:%3</a> ").arg(updatedLink).arg(clipName, pos));
        } else {
            // Timestamp relative to project timeline
            cur.insertHtml(QString("<a href=\"%1\">%2</a> ").arg(updatedLink, pos));
        }
        ix++;
    }
}

NotesWidget *NotesPlugin::widget()
{
    return m_widget;
}

void NotesPlugin::clear()
{
    m_widget->clear();
}
