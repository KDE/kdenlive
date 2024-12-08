/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "notesplugin.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/noteswidget.h"
#include "doc/kdenlivedoc.h"
#include "klocalizedstring.h"
#include "mainwindow.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"

#include <QStyle>
#include <QVBoxLayout>

NotesPlugin::NotesPlugin(QObject *parent)
    : QObject(parent)
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
    connect(m_widget, &NotesWidget::insertTextNote, this, &NotesPlugin::slotInsertText);
    connect(m_widget, &NotesWidget::reAssign, this, &NotesPlugin::slotReAssign);
    m_widget->setTabChangesFocus(true);
    m_widget->setPlaceholderText(i18n("Enter your project notes here …"));
    m_notesDock = pCore->window()->addDock(i18n("Project Notes"), QStringLiteral("notes_widget"), container);
    m_notesDock->close();
    connect(pCore->projectManager(), &ProjectManager::docOpened, this, &NotesPlugin::setProject);
}

void NotesPlugin::setProject(KdenliveDoc *document)
{
    connect(m_widget, SIGNAL(textChanged()), document, SLOT(setModified()));
    connect(m_widget, &NotesWidget::seekProject, pCore->projectManager(), &ProjectManager::seekTimeline, Qt::UniqueConnection);
    if (m_tb->actions().isEmpty()) {
        // initialize toolbar
        m_tb->addAction(pCore->window()->action("add_project_note"));
        QAction *a = new QAction(QIcon::fromTheme(QStringLiteral("edit-find-replace")), i18n("Reassign selected timecodes to current Bin clip"));
        connect(a, &QAction::triggered, m_widget, &NotesWidget::assignProjectNote);
        m_tb->addAction(a);
        a = new QAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Create markers from selected timecodes"));
        a->setWhatsThis(
            xi18nc("@info:whatsthis", "Creates markers in the timeline from the selected timecodes (doesn’t matter if other text is selected too)."));
        connect(a, &QAction::triggered, m_widget, &NotesWidget::createMarkers);
        m_tb->addAction(a);
        m_tb->addSeparator();
        a = new QAction(QIcon::fromTheme(QStringLiteral("format-text-bold")), i18n("Bold"));
        a->setWhatsThis(
            xi18nc("@info:whatsthis", "Creates markers in the timeline from the selected timecodes (doesn’t matter if other text is selected too)."));
        connect(a, &QAction::triggered, m_widget, &NotesWidget::switchBoldText);
        m_tb->addAction(a);

        QWidget *empty = new QWidget();
        empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_tb->addWidget(empty);
        m_markDownEdit = new QAction(QIcon::fromTheme(QStringLiteral("format-text-code")), i18n("Edit Markdown source code"));
        m_markDownEdit->setCheckable(true);
        m_markDownEdit->setWhatsThis(xi18nc("@info:whatsthis", "Enable or disable markdown editing."));
        connect(m_markDownEdit, &QAction::triggered, m_widget, &NotesWidget::switchMarkDownEditing);
        m_tb->addAction(m_markDownEdit);
    }
}

void NotesPlugin::showDock()
{
    m_notesDock->show();
    m_notesDock->raise();
}

void NotesPlugin::loadNotes(QString text)
{
    m_widget->clear();
    if (m_markDownEdit->isChecked()) {
        m_markDownEdit->trigger();
    }
    text.replace(QStringLiteral("\n\n\n"), QStringLiteral("\n<br />\n\n"));
    m_widget->setMarkdown(text);
}

void NotesPlugin::slotInsertTimecode()
{
    if (pCore->monitorManager()->isActive(Kdenlive::ClipMonitor)) {
        // Add a note on the current bin clip
        int frames = pCore->monitorManager()->clipMonitor()->position();
        QString position = pCore->timecode().getTimecodeFromFrames(frames);
        const QString binId = pCore->monitorManager()->clipMonitor()->activeClipId();
        if (binId.isEmpty()) {
            pCore->displayMessage(i18n("Cannot add note, no clip selected in project bin"), ErrorMessage);
            return;
        }
        const QString clipName = pCore->bin()->getBinClipName(binId);
        const QString uuid = pCore->projectItemModel()->getBinClipUuid(binId);
        m_widget->insertMarkDown(QStringLiteral("[%3:%4](%1#%2)&nbsp;").arg(uuid, QString::number(frames), clipName, position));
    } else {
        int frames = pCore->monitorManager()->projectMonitor()->position();
        QString position = pCore->timecode().getTimecodeFromFrames(frames);
        QPair<int, QString> currentTrackInfo = pCore->currentTrackInfo();
        const QUuid uuid = pCore->currentTimelineId();
        if (currentTrackInfo.first != -1) {
            // Insert timeline position with track reference
            m_widget->insertMarkDown(
                QStringLiteral("[%4 %5](%1!%2?%3)&nbsp;")
                    .arg(uuid.toString(), QString::number(frames), QString::number(currentTrackInfo.first), currentTrackInfo.second, position));
        } else {
            m_widget->insertMarkDown(QStringLiteral("[%3](%1!%2)&nbsp;").arg(uuid.toString(), QString::number(frames), position));
        }
    }
}

void NotesPlugin::slotReAssign(const QStringList &anchors, const QList<QPoint> &points)
{
    const QString binId = pCore->monitorManager()->clipMonitor()->activeClipId();
    int ix = 0;
    if (points.count() != anchors.count()) {
        // Something is wrong, abort
        pCore->displayMessage(i18n("Cannot perform assign"), ErrorMessage);
        return;
    }
    QString uuid;
    if (!binId.isEmpty()) {
        uuid = pCore->projectItemModel()->getBinClipUuid(binId);
    }
    for (const QString &a : anchors) {
        QPoint pt = points.at(ix);
        QString updatedLink = a;
        int position = 0;
        if (a.contains(QLatin1Char('#'))) {
            // Link was previously attached to another clip
            updatedLink = a.section(QLatin1Char('#'), 1);
            position = updatedLink.toInt();
            if (!uuid.isEmpty()) {
                updatedLink.prepend(QStringLiteral("%1#").arg(uuid));
            }
        } else {
            if (a.contains(QLatin1Char('!'))) {
                updatedLink = a.section(QLatin1Char('!'), 1);
            }
            if (a.contains(QLatin1Char('?'))) {
                updatedLink = updatedLink.section(QLatin1Char('?'), 0, 0);
            }
            position = updatedLink.toInt();
            if (!uuid.isEmpty()) {
                updatedLink.prepend(QStringLiteral("%1#").arg(uuid));
            }
        }
        QTextCursor cur(m_widget->textCursor());
        cur.setPosition(pt.x());
        cur.setPosition(pt.y(), QTextCursor::KeepAnchor);
        QString pos = pCore->timecode().getTimecodeFromFrames(position);
        if (!binId.isEmpty()) {
            QString clipName = pCore->bin()->getBinClipName(binId);
            cur.insertHtml(QStringLiteral("<a href=\"%1\">%2:%3</a> ").arg(updatedLink, clipName, pos));
        } else {
            // Timestamp relative to project timeline
            cur.insertHtml(QStringLiteral("<a href=\"%1\">%2</a> ").arg(updatedLink, pos));
        }
        ix++;
    }
}

void NotesPlugin::slotInsertText(const QString &text)
{
    m_widget->insertMarkDown(text);
}

NotesWidget *NotesPlugin::widget()
{
    return m_widget;
}

void NotesPlugin::clear()
{
    m_widget->clear();
}
