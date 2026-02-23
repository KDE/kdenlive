/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "noteswidget.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlive_debug.h"

#include <KLocalizedString>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QTextDocumentFragment>
#include <QToolTip>
#include <QUuid>

NotesWidget::NotesWidget(QWidget *parent)
    : QTextEdit(parent)
{
    setMouseTracking(true);
}

NotesWidget::~NotesWidget() = default;

void NotesWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    if (menu) {
        QAction *a = new QAction(i18n("Insert current timecode"), menu);
        connect(a, &QAction::triggered, this, &NotesWidget::insertNotesTimecode);
        menu->insertAction(menu->actions().at(0), a);
        QPair<QStringList, QList<QPoint>> result = getSelectedAnchors();
        QStringList anchors = result.first;
        QList<QPoint> anchorPoints = result.second;
        if (anchors.isEmpty()) {
            const QString anchor = anchorAt(event->pos());
            if (!anchor.isEmpty()) {
                anchors << anchor;
            }
        }
        if (!anchors.isEmpty()) {
            a = new QAction(i18np("Create marker", "create markers", anchors.count()), menu);
            connect(a, &QAction::triggered, this, [this, anchors]() { createMarker(anchors); });
            menu->insertAction(menu->actions().at(1), a);
            if (!anchorPoints.isEmpty()) {
                a = new QAction(i18n("Assign timestamps to current Bin Clip"), menu);
                connect(a, &QAction::triggered, this, [this, anchors, anchorPoints]() { Q_EMIT reAssign(anchors, anchorPoints); });
                menu->insertAction(menu->actions().at(2), a);
            }
        }
        menu->exec(event->globalPos());
        delete menu;
    }
}

void NotesWidget::createMarker(const QStringList &anchors, const QList<QPoint> &points)
{
    QMap<QString, QMap<int, QString>> clipMarkers;
    QMap<QUuid, QMap<int, QString>> guides;
    int ix = 0;
    for (const QString &anchor : anchors) {
        // Find out marker text
        int startPos = -1;
        int endPos = -1;
        QString markerText;
        if (points.size() > ix) {
            startPos = points.at(ix).y();
            QTextCursor cur = textCursor();
            cur.setPosition(startPos);
            cur.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            endPos = cur.position();
            // Check if we have another anchor before line end
            bool modified = false;
            for (auto &p : points) {
                if (p.x() > startPos && p.x() < endPos) {
                    endPos = p.x() - 1;
                    modified = true;
                }
            }
            if (modified) {
                cur.setPosition(startPos);
                cur.setPosition(endPos, QTextCursor::KeepAnchor);
            }
            markerText = cur.selectedText().simplified();
            markerText.truncate(40);
        }
        if (anchor.contains(QLatin1Char('#'))) {
            // That's a Bin Clip reference.
            const QString uuid = anchor.section(QLatin1Char('#'), 0, 0);
            const QString binId = pCore->projectItemModel()->getBinClipIdByUuid(uuid);
            QMap<int, QString> timecodes;
            if (clipMarkers.contains(binId)) {
                timecodes = clipMarkers.value(binId);
            }
            timecodes.insert(anchor.section(QLatin1Char('#'), 1).toInt(), markerText);
            clipMarkers.insert(binId, timecodes);
        } else {
            // That is a guide
            QUuid uuid;
            QString anchorLink = anchor;
            if (anchorLink.contains(QLatin1Char('!'))) {
                // Linked to a timeline sequence
                uuid = QUuid(anchorLink.section(QLatin1Char('!'), 0, 0));
                anchorLink = anchorLink.section(QLatin1Char('!'), 1);
            }
            if (anchorLink.contains(QLatin1Char('?'))) {
                anchorLink = anchorLink.section(QLatin1Char('?'), 0, 0);
            }
            QMap<int, QString> guidesToAdd;
            if (guides.contains(uuid)) {
                guidesToAdd = guides.value(uuid);
            }
            guidesToAdd.insert(anchorLink.toInt(), markerText);
            guides.insert(uuid, guidesToAdd);
        }
        ix++;
    }
    QMapIterator<QString, QMap<int, QString>> i(clipMarkers);
    while (i.hasNext()) {
        i.next();
        // That's a Bin Clip reference.
        pCore->bin()->addClipMarker(i.key(), i.value());
    }
    if (!clipMarkers.isEmpty()) {
        const QString &binId = clipMarkers.firstKey();
        pCore->selectBinClip(binId, true, clipMarkers.value(binId).firstKey(), QPoint());
    }
    if (!guides.isEmpty()) {
        pCore->addGuides(guides);
    }
}

void NotesWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QString anchor = anchorAt(e->pos());
    if (anchor.isEmpty()) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        viewport()->setCursor(Qt::PointingHandCursor);
    }
    QTextEdit::mouseMoveEvent(e);
}

void NotesWidget::mousePressEvent(QMouseEvent *e)
{
    QString anchor = anchorAt(e->pos());
    if (anchor.isEmpty() || e->button() != Qt::LeftButton) {
        QTextEdit::mousePressEvent(e);
        return;
    }
    if (anchor.contains(QLatin1Char('#'))) {
        // That's a Bin Clip reference.
        const QString uuid = anchor.section(QLatin1Char('#'), 0, 0);
        const QString binId = pCore->projectItemModel()->getBinClipIdByUuid(uuid);
        if (!binId.isEmpty()) {
            pCore->selectBinClip(binId, true, anchor.section(QLatin1Char('#'), 1).toInt(), QPoint());
        } else {
            pCore->displayMessage(i18n("Cannot find project clip"), ErrorMessage);
        }
    } else {
        Q_EMIT seekProject(anchor);
    }
    e->setAccepted(true);
}

bool NotesWidget::selectionHasAnchors() const
{
    int startPos = textCursor().selectionStart();
    int endPos = textCursor().selectionEnd();
    QTextCursor cur = textCursor();
    for (int p = startPos; p <= endPos; ++p) {
        cur.setPosition(p, QTextCursor::MoveAnchor);
        if (!anchorAt(cursorRect(cur).center()).isEmpty()) {
            return true;
        }
    }
    return false;
}

QPair<QStringList, QList<QPoint>> NotesWidget::getAnchors(int startPos, int endPos)
{
    QStringList anchors;
    QList<QPoint> anchorPoints;
    if (endPos > startPos) {
        textCursor().clearSelection();
        QTextCursor cur(textCursor());
        // Ensure we are at the start of current selection
        if (!cur.atBlockStart()) {
            cur.setPosition(startPos, QTextCursor::MoveAnchor);
            int pos = startPos;
            const QString an = anchorAt(cursorRect(cur).center());
            while (!cur.atBlockStart()) {
                pos--;
                cur.setPosition(pos, QTextCursor::MoveAnchor);
                if (anchorAt(cursorRect(cur).center()) == an) {
                    startPos = pos;
                } else {
                    break;
                }
            }
        }
        bool isInAnchor = false;
        QPoint anchorPoint;
        for (int p = startPos; p <= endPos; ++p) {
            cur.setPosition(p, QTextCursor::MoveAnchor);
            const QString anchor = anchorAt(cursorRect(cur).center());
            if (isInAnchor && !anchor.isEmpty() && p == endPos) {
                endPos++;
            }
            if (isInAnchor && (anchor.isEmpty() || !anchors.contains(anchor) || cur.atEnd())) {
                // End of current anchor
                anchorPoint.setY(p);
                anchorPoints.prepend(anchorPoint);
                isInAnchor = false;
            }
            if (!anchor.isEmpty() && !anchors.contains(anchor)) {
                anchors.prepend(anchor);
                if (!isInAnchor) {
                    isInAnchor = true;
                    anchorPoint.setX(p);
                }
            }
        }
    }
    return {anchors, anchorPoints};
}

QPair<QStringList, QList<QPoint>> NotesWidget::getSelectedAnchors()
{
    int startPos = textCursor().selectionStart();
    int endPos = textCursor().selectionEnd();
    return getAnchors(startPos, endPos);
}

QPair<QStringList, QList<QPoint>> NotesWidget::getAllAnchors()
{
    int startPos = 0;
    QTextCursor cur = textCursor();
    cur.movePosition(QTextCursor::End);
    int endPos = cur.position();
    return getAnchors(startPos, endPos);
}

void NotesWidget::assignProjectNoteToTimelineClip()
{
    QPair<QStringList, QList<QPoint>> result = getSelectedAnchors();
    QStringList anchors = result.first;
    QList<QPoint> anchorPoints = result.second;
    if (anchors.isEmpty()) {
        pCore->displayMessage(i18n("Select some timecodes to reassign"), ErrorMessage);
        return;
    }
    auto clipAndOffset = pCore->getSelectedClipAndOffset();
    if (clipAndOffset.first.isEmpty()) {
        pCore->displayMessage(i18n("No clip selected in timeline"), ErrorMessage);
        return;
    }
    Q_EMIT reAssign(anchors, anchorPoints, clipAndOffset.first, clipAndOffset.second);
}

void NotesWidget::assignProjectNote()
{
    QPair<QStringList, QList<QPoint>> result = getSelectedAnchors();
    QStringList anchors = result.first;
    QList<QPoint> anchorPoints = result.second;
    if (anchors.isEmpty()) {
        pCore->displayMessage(i18n("Select some timecodes to reassign"), ErrorMessage);
        return;
    }
    Q_EMIT reAssign(anchors, anchorPoints);
}

void NotesWidget::createMarkers()
{
    QPair<QStringList, QList<QPoint>> result = getSelectedAnchors();
    QStringList anchors = result.first;
    if (!anchors.isEmpty()) {
        createMarker(anchors, result.second);
    } else {
        pCore->displayMessage(i18n("Select some timecodes to create markers"), ErrorMessage);
    }
}

void NotesWidget::addProjectNote()
{
    if (!textCursor().atBlockStart()) {
        QTextCursor cur = textCursor();
        cur.movePosition(QTextCursor::EndOfBlock);
        setTextCursor(cur);
        insertPlainText(QStringLiteral("\n"));
    }
    Q_EMIT insertNotesTimecode();
}

void NotesWidget::addTextNote(const QString &text)
{
    if (!textCursor().atEnd()) {
        QTextCursor cur = textCursor();
        cur.movePosition(QTextCursor::End);
        setTextCursor(cur);
    }
    Q_EMIT insertTextNote(text);
}

void NotesWidget::insertFromMimeData(const QMimeData *source)
{
    bool enforceHtml = false;
    // Check for timecodes
    const QStringList sentences = source->text().split(QLatin1Char('\n'));
    for (auto &s : sentences) {
        QString pastedText = s;
        const QStringList words = s.split(QLatin1Char(' '));
        for (const QString &w : words) {
            if (w.size() > 4 && w.size() < 13 && w.count(QLatin1Char(':')) > 1) {
                // This is probably a timecode
                int frames = pCore->timecode().getFrameCount(w);
                if (frames > 0) {
                    pastedText.replace(w, QStringLiteral("<a href=\"") + QString::number(frames) + QStringLiteral("\">") + w + QStringLiteral("</a> "));
                    enforceHtml = true;
                }
            }
        }
        if (enforceHtml || Qt::mightBeRichText(pastedText)) {
            if (s != sentences.last()) {
                pastedText.append(QStringLiteral("<br/>"));
            }
            insertHtml(pastedText);
        } else {
            if (s != sentences.last()) {
                pastedText.append(QLatin1Char('\n'));
            }
            insertPlainText(pastedText);
        }
    }
}

bool NotesWidget::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
        const auto key = keyEvent->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            // Check if current line contains a timecode
            QTextCursor cur = textCursor();
            if (!cur.hasSelection()) {
                cur.select(QTextCursor::LineUnderCursor);
                int startPos = cur.selectionStart();
                QTextDocumentFragment selectedText = cur.selection();
                QString pastedText = selectedText.toPlainText();
                const QStringList words = pastedText.split(QLatin1Char(' '));
                bool enforceHtml = false;
                for (const QString &w : words) {
                    if (w.size() > 4 && w.size() < 13 && w.count(QLatin1Char(':')) > 1) {
                        // This is probably a timecode
                        // Check if it already has a link
                        int wordPosition = pastedText.indexOf(w) + 2;
                        QTextCursor cur2 = cur;
                        cur2.setPosition(startPos + wordPosition);
                        if (anchorAt(cursorRect(cur2).center()).isEmpty()) {
                            int frames = pCore->timecode().getFrameCount(w);
                            if (frames > 0) {
                                pastedText.replace(w, QStringLiteral("<a href=\"") + QString::number(frames) + QStringLiteral("\">") + w +
                                                          QStringLiteral("</a> "));
                                enforceHtml = true;
                            }
                        }
                    }
                }
                if (enforceHtml) {
                    // Replace line
                    cur.removeSelectedText();
                    cur.insertHtml(pastedText);
                }
            }
        }
    } else if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        const QString anchor = anchorAt(helpEvent->pos());
        if (!anchor.isEmpty()) {
            QString sequenceName;
            if (anchor.contains(QLatin1Char('!'))) {
                // We have a sequence reference
                const QString binId = pCore->projectItemModel()->getSequenceId(QUuid(anchor.section(QLatin1Char('!'), 0, 0)));
                if (!binId.isEmpty()) {
                    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(binId);
                    if (clip) {
                        sequenceName = clip->clipName();
                    }
                }
            }
            if (!sequenceName.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos(), sequenceName);
            } else {
                QToolTip::hideText();
            }
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QTextEdit::event(event);
}
