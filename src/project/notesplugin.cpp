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

#include <QFrame>
#include <QLineEdit>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <KColorScheme>
#include <kddockwidgets/DockWidget.h>

NotesPlugin::NotesPlugin(KDDockWidgets::QtWidgets::DockWidget *tabbedDock, QObject *parent)
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
    // Search line
    m_searchFrame = new QFrame(container);
    QHBoxLayout *l = new QHBoxLayout(m_searchFrame);
    m_searchLine = new QLineEdit(container);
    m_button_next = new QToolButton(container);
    m_button_next->setIcon(QIcon::fromTheme("go-down"));
    m_button_next->setAutoRaise(true);
    m_button_prev = new QToolButton(container);
    m_button_prev->setIcon(QIcon::fromTheme("go-up"));
    m_button_prev->setAutoRaise(true);
    l->addWidget(m_searchLine);
    l->addWidget(m_button_prev);
    l->addWidget(m_button_next);
    lay->addWidget(m_searchFrame);
    m_searchFrame->setVisible(false);

    m_findAction = KStandardAction::find(this, &NotesPlugin::find, container);
    container->addAction(m_findAction);
    m_findAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_widget = new NotesWidget();
    lay->addWidget(m_widget);
    container->setLayout(lay);

    connect(m_widget, &NotesWidget::insertNotesTimecode, this, &NotesPlugin::slotInsertTimecode);
    connect(m_widget, &NotesWidget::insertTextNote, this, &NotesPlugin::slotInsertText);
    connect(m_widget, &NotesWidget::reAssign, this, &NotesPlugin::slotReAssign);
    m_widget->setTabChangesFocus(true);
    m_widget->setPlaceholderText(i18n("Enter your project notes here …"));
    m_notesDock = pCore->window()->addDock(i18n("Project Notes"), QStringLiteral("notes_widget"), container, KDDockWidgets::Location_None, tabbedDock);
    m_notesDock->close();
    connect(pCore->projectManager(), &ProjectManager::docOpened, this, &NotesPlugin::setProject);
    connect(m_searchLine, &QLineEdit::textChanged, this, [this](const QString &searchText) {
        QPalette palette = m_searchLine->palette();
        QColor bgColor = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_widget->find(searchText);
            KColorScheme scheme(palette.currentColorGroup(), KColorScheme::Window);
            if (found) {
                bgColor = scheme.background(KColorScheme::PositiveBackground).color();
                QTextCursor cur = m_widget->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_widget->setTextCursor(cur);
            } else {
                // Loop over, abort
                bgColor = scheme.background(KColorScheme::NegativeBackground).color();
            }
        }
        palette.setColor(QPalette::Base, bgColor);
        m_searchLine->setPalette(palette);
    });
    QAction *next = KStandardAction::findNext(this, &NotesPlugin::findNext, container);
    next->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    QAction *previous = KStandardAction::findPrev(this, &NotesPlugin::findPrevious, container);
    previous->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_button_next->setDefaultAction(next);
    m_button_prev->setDefaultAction(previous);
    container->addAction(next);
    container->addAction(previous);
    connect(pCore->projectItemModel().get(), &ProjectItemModel::clipRenamed, this, [this](const QString &uuid, const QString &clipName) {
        QPair<QStringList, QList<QPoint>> anchors = m_widget->getAllAnchors();
        clipRenamed(uuid, clipName, anchors);
    });
}

void NotesPlugin::findNext()
{
    const QString searchText = m_searchLine->text();
    QPalette palette = m_searchLine->palette();
    QColor bgColor = palette.color(QPalette::Base);
    if (searchText.length() > 2) {
        bool found = m_widget->find(searchText);
        KColorScheme scheme(palette.currentColorGroup(), KColorScheme::Window);
        if (found) {
            bgColor = scheme.background(KColorScheme::PositiveBackground).color();
            QTextCursor cur = m_widget->textCursor();
            cur.select(QTextCursor::WordUnderCursor);
            m_widget->setTextCursor(cur);
        } else {
            // Loop over, abort
            bgColor = scheme.background(KColorScheme::NegativeBackground).color();
        }
    }
    palette.setColor(QPalette::Base, bgColor);
    m_searchLine->setPalette(palette);
}

void NotesPlugin::find()
{
    m_showSearch->toggle();
}

void NotesPlugin::findPrevious()
{
    const QString searchText = m_searchLine->text();
    QPalette palette = m_searchLine->palette();
    QColor bgColor = palette.color(QPalette::Base);
    if (searchText.length() > 2) {
        bool found = m_widget->find(searchText, QTextDocument::FindBackward);
        KColorScheme scheme(palette.currentColorGroup(), KColorScheme::Window);
        if (found) {
            bgColor = scheme.background(KColorScheme::PositiveBackground).color();
            QTextCursor cur = m_widget->textCursor();
            cur.select(QTextCursor::WordUnderCursor);
            m_widget->setTextCursor(cur);
        } else {
            // Loop over, abort
            bgColor = scheme.background(KColorScheme::NegativeBackground).color();
        }
    }
    palette.setColor(QPalette::Base, bgColor);
    m_searchLine->setPalette(palette);
}

void NotesPlugin::setProject(KdenliveDoc *document)
{
    connect(m_widget, SIGNAL(textChanged()), document, SLOT(setModified()));
    connect(m_widget, &NotesWidget::seekProject, pCore->projectManager(), &ProjectManager::seekTimeline, Qt::UniqueConnection);
    if (m_tb->actions().isEmpty()) {
        // initialize toolbar
        m_tb->addAction(pCore->window()->action("add_project_note"));
        m_reassingToBin = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18n("Reassign selected timecodes to current Bin clip"));
        connect(m_reassingToBin, &QAction::triggered, m_widget, &NotesWidget::assignProjectNote);
        m_tb->addAction(m_reassingToBin);
        m_reassingToBin->setEnabled(false);
        m_reassingToTimeline = new QAction(QIcon::fromTheme(QStringLiteral("link")), i18n("Reassign selected timecodes to current timeline clip"));
        connect(m_reassingToTimeline, &QAction::triggered, m_widget, &NotesWidget::assignProjectNoteToTimelineClip);
        m_tb->addAction(m_reassingToTimeline);
        m_reassingToTimeline->setEnabled(false);
        m_createFromSelection = new QAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Create markers from selected timecodes"));
        m_createFromSelection->setWhatsThis(
            xi18nc("@info:whatsthis", "Creates markers in the timeline from the selected timecodes (doesn’t matter if other text is selected too)."));
        connect(m_createFromSelection, &QAction::triggered, m_widget, &NotesWidget::createMarkers);
        m_tb->addAction(m_createFromSelection);
        m_createFromSelection->setEnabled(false);
        connect(m_widget, &QTextEdit::selectionChanged, this, &NotesPlugin::checkSelection);

        m_showSearch = new QToolButton(m_widget);
        m_showSearch->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
        m_showSearch->setCheckable(true);
        m_showSearch->setAutoRaise(true);
        m_showSearch->setToolTip(i18n("Search…"));
        m_tb->addWidget(m_showSearch);
        connect(m_showSearch, &QToolButton::toggled, this, [this](bool checked) {
            m_searchFrame->setVisible(checked);
            if (checked) {
                m_searchLine->setFocus();
            }
        });
    }
}

void NotesPlugin::showDock()
{
    m_notesDock->show();
    m_notesDock->raise();
}

void NotesPlugin::checkSelection()
{
    bool hasTimeCode = m_widget->selectionHasAnchors();
    m_reassingToBin->setEnabled(hasTimeCode);
    m_reassingToTimeline->setEnabled(hasTimeCode);
    m_createFromSelection->setEnabled(hasTimeCode);
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
        m_widget->insertHtml(QStringLiteral("<a href=\"%1#%2\">%3 %4</a> ").arg(uuid, QString::number(frames), clipName, position));
    } else {
        int frames = pCore->monitorManager()->projectMonitor()->position();
        QString position = pCore->timecode().getTimecodeFromFrames(frames);
        QPair<int, QString> currentTrackInfo = pCore->currentTrackInfo();
        const QUuid uuid = pCore->currentTimelineId();
        if (currentTrackInfo.first != -1) {
            // Insert timeline position with track reference
            m_widget->insertHtml(
                QStringLiteral("<a href=\"%1!%2?%3\">%4 %5</a> ")
                    .arg(uuid.toString(), QString::number(frames), QString::number(currentTrackInfo.first), currentTrackInfo.second, position));
        } else {
            m_widget->insertHtml(QStringLiteral("<a href=\"%1!%2\">%3</a> ").arg(uuid.toString(), QString::number(frames), position));
        }
    }
}

void NotesPlugin::slotReAssign(const QStringList &anchors, const QList<QPoint> &points, QString binId, int offset)
{
    if (binId.isEmpty()) {
        binId = pCore->monitorManager()->clipMonitor()->activeClipId();
    }
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
            position = updatedLink.toInt() - offset;
            updatedLink = QString::number(position);
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
            position = updatedLink.toInt() - offset;
            updatedLink = QString::number(position);
            if (!uuid.isEmpty()) {
                updatedLink.prepend(QStringLiteral("%1#").arg(uuid));
            }
        }
        if (updatedLink == a) {
            // No change requested
            continue;
        }
        QTextCursor cur(m_widget->textCursor());
        cur.setPosition(pt.x());
        cur.setPosition(pt.y(), QTextCursor::KeepAnchor);
        const QString pos = pCore->timecode().getTimecodeFromFrames(position);
        if (!binId.isEmpty()) {
            QString clipName = pCore->bin()->getBinClipName(binId);
            cur.insertHtml(QStringLiteral("<a href=\"%1\">%2 %3</a> ").arg(updatedLink, clipName, pos));
        } else {
            // Timestamp relative to project timeline
            cur.insertHtml(QStringLiteral("<a href=\"%1\">%2</a> ").arg(updatedLink, pos));
        }
        ix++;
    }
}

void NotesPlugin::clipRenamed(const QString &uuid, const QString &newName, QPair<QStringList, QList<QPoint>> anchors)
{
    int ix = 0;
    for (const QString &a : anchors.first) {
        QPoint pt = anchors.second.at(ix);
        const QString updatedLink = a;
        if (a.contains(QLatin1Char('#'))) {
            // Link was previously attached to another clip
            const QString linkUuid = a.section(QLatin1Char('#'), 0, 0);
            if (linkUuid == uuid) {
                // Match
                int position = a.section(QLatin1Char('#'), 1).toInt();
                QTextCursor cur(m_widget->textCursor());
                cur.setPosition(pt.x());
                cur.setPosition(pt.y(), QTextCursor::KeepAnchor);
                const QString pos = pCore->timecode().getTimecodeFromFrames(position);
                cur.insertHtml(QStringLiteral("<a href=\"%1\">%2 %3</a> ").arg(updatedLink, newName, pos));
            }
        }
        ix++;
    }
}

void NotesPlugin::slotInsertText(const QString &text)
{
    m_widget->insertHtml(text);
}

NotesWidget *NotesPlugin::widget()
{
    return m_widget;
}

void NotesPlugin::clear()
{
    m_widget->clear();
}
