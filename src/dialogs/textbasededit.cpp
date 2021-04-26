/***************************************************************************
 *   Copyright (C) 2020 by Jean-Baptiste Mardelle                          *
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

#include "textbasededit.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "bin/projectsubclip.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "timecodedisplay.h"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include <memory>
#include <profiles/profilemodel.hpp>

#include "klocalizedstring.h"

#include <QEvent>
#include <QKeyEvent>
#include <QToolButton>
#include <KMessageBox>
#include <KUrlRequesterDialog>

VideoTextEdit::VideoTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setMouseTracking(true);
    setReadOnly(true);
    //setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    lineNumberArea = new LineNumberArea(this);
    connect(this, &VideoTextEdit::cursorPositionChanged, [this]() {
        lineNumberArea->update();
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, [this]() {
        lineNumberArea->update();
    });
    QRect rect =  this->contentsRect();
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    bookmarkAction = new QAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add bookmark"), this);
    bookmarkAction->setEnabled(false);
    deleteAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete selection"), this);
    deleteAction->setEnabled(false);
}

void VideoTextEdit::repaintLines()
{
    lineNumberArea->update();
}

void VideoTextEdit::cleanup()
{
    speechZones.clear();
    cutZones.clear();
    m_hoveredBlock = -1;
    clear();
    document()->setDefaultStyleSheet(QString("body {font-size:%2px;}\na { text-decoration:none;color:%1;font-size:%2px;}").arg(palette().text().color().name()).arg(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize()));
}

const QString VideoTextEdit::selectionStartAnchor(QTextCursor &cursor, int start, int max)
{
    if (start == -1) {
        start = cursor.selectionStart();
    }
    if (max == -1) {
        max = cursor.selectionEnd();
    }
    cursor.setPosition(start);
    cursor.select(QTextCursor::WordUnderCursor);
    while (cursor.selectedText().isEmpty() && start < max) {
        start++;
        cursor.setPosition(start);
        cursor.select(QTextCursor::WordUnderCursor);
    }
    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    cursor.setPosition(selStart + (selEnd - selStart) / 2);
    return anchorAt(cursorRect(cursor).center());
}

const QString VideoTextEdit::selectionEndAnchor(QTextCursor &cursor, int end, int min)
{
    qDebug()<<"==== TESTING SELECTION END ANCHOR FROM: "<<end<<" , MIN: "<<min;
    if (end == -1) {
        end = cursor.selectionEnd();
    }
    if (min == -1) {
        min = cursor.selectionStart();
    }
    cursor.setPosition(end);
    cursor.select(QTextCursor::WordUnderCursor);
    while (cursor.selectedText().isEmpty() && end > min) {
        end--;
        cursor.setPosition(end);
        cursor.select(QTextCursor::WordUnderCursor);
    }
    qDebug()<<"==== TESTING SELECTION END ANCHOR FROM: "<<end<<" , WORD: "<<cursor.selectedText();
    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    cursor.setPosition(selStart + (selEnd - selStart) / 2);
    qDebug()<<"==== END POS SELECTION FOR: "<<cursor.selectedText()<<" = "<<anchorAt(cursorRect(cursor).center());
    QString anch = anchorAt(cursorRect(cursor).center());
    double endMs = anch.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
    qDebug()<<"==== GOT LAST FRAME: "<<GenTime(endMs).frames(25);
    return anchorAt(cursorRect(cursor).center());
}

void VideoTextEdit::processCutZones(QList <QPoint> loadZones)
{
    // Remove all outside load zones
    qDebug()<<"=== LOADING CUT ZONES: "<<loadZones<<"\n........................";
    QTextCursor curs = textCursor();
    curs.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    qDebug()<<"===== GOT DOCUMENT END: "<<curs.position();
    curs.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    double fps = pCore->getCurrentFps();
    while (!curs.atEnd()) {
        qDebug()<<"=== CURSOR POS: "<<curs.position();
        QString anchorStart = selectionStartAnchor(curs, curs.position(), document()->characterCount());
        int startPos = GenTime(anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble()).frames(fps);
        int endPos = GenTime(anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble()).frames(fps);
        bool isInZones = false;
        for (auto &p : loadZones) {
            if ((startPos >= p.x() && startPos <= p.y()) || (endPos >= p.x() && endPos <= p.y())) {
                isInZones = true;
                break;
            }
        }
        if (!isInZones) {
            // Delete current word
            qDebug()<<"=== DELETING WORD: "<<curs.selectedText();
            curs.select(QTextCursor::WordUnderCursor);
            curs.removeSelectedText();
            if (document()->characterAt(curs.position() - 1) == QLatin1Char(' ')) {
                // Remove trailing space
                curs.deleteChar();
            } else {
                if (!curs.movePosition(QTextCursor::NextWord, QTextCursor::MoveAnchor)) {
                    break;
                }
            }
        } else {
            curs.movePosition(QTextCursor::NextWord, QTextCursor::MoveAnchor);
            if (!curs.movePosition(QTextCursor::NextWord, QTextCursor::MoveAnchor)) {
                break;
            }
            qDebug()<<"=== WORD INSIDE, POS: "<<curs.position();
        }
        qDebug()<<"=== MOVED CURSOR POS: "<<curs.position();
    }
}

void VideoTextEdit::rebuildZones()
{
    speechZones.clear();
    m_selectedBlocks.clear();
    QTextCursor curs = textCursor();
    curs.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    for (int i = 0; i < document()->blockCount(); ++i) {
        int start = curs.position() + 1;
        QString anchorStart = selectionStartAnchor(curs, start, document()->characterCount());
        //qDebug()<<"=== START ANCHOR: "<<anchorStart<<" AT POS: "<<curs.position();
        curs.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        int end = curs.position() - 1;
        QString anchorEnd = selectionEndAnchor(curs, end, start);
        qDebug()<<"=== ANCHORAs FOR : "<<i<<", "<<anchorStart<<"-"<<anchorEnd<<" AT POS: "<<curs.position();
        if (!anchorStart.isEmpty() && !anchorEnd.isEmpty()) {
            double startMs = anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
            double endMs = anchorEnd.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
            speechZones << QPair<double, double>(startMs, endMs);
        }
        curs.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    }
    repaintLines();
}

int VideoTextEdit::lineNumberAreaWidth()
{
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * 11;
    return space;
}

QVector<QPoint> VideoTextEdit::processedZones(QVector<QPoint> sourceZones)
{
    QVector<QPoint> resultZones = sourceZones;
    for (auto &cut : cutZones) {
        QVector<QPoint> processingZones = resultZones;
        resultZones.clear();
        for (auto &zone : processingZones) {
            if (cut.x() > zone.x()) {
                if (cut.x() > zone.y()) {
                    // Cut is outside zone, keep it as is
                    resultZones << zone;
                    continue;
                }
                // Cut is inside zone
                if (cut.y() > zone.y()) {
                    // Only keep the start of this zone
                    resultZones << QPoint(zone.x(), cut.x());
                } else {
                    // Cut is in the middle of this zone
                    resultZones << QPoint(zone.x(), cut.x());
                    resultZones << QPoint(cut.y(), zone.y());
                }
            } else if (cut.y() < zone.y()) {
                // Only keep the end of this zone
                resultZones << QPoint(cut.y(), zone.y());
            }
        }
    }
    qDebug()<<"=== FINAL CUTS: "<<resultZones;
    return resultZones;
}

QVector<QPoint> VideoTextEdit::getInsertZones()
{
    if (m_selectedBlocks.isEmpty()) {
        // return text selection, not blocks
        QTextCursor cursor = textCursor();
        QString anchorStart;
        QString anchorEnd;
        if (!cursor.selectedText().isEmpty()) {
            qDebug()<<"=== EXPORTING SELECTION";
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd() - 1;
            anchorStart = selectionStartAnchor(cursor, start, end);
            anchorEnd = selectionEndAnchor(cursor, end, start);
        } else {
            // Return full text
            cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
            int end = cursor.position() - 1;
            cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
            int start = cursor.position();
            anchorStart = selectionStartAnchor(cursor, start, end);
            anchorEnd = selectionEndAnchor(cursor, end, start);
        }
        if (!anchorStart.isEmpty() && !anchorEnd.isEmpty()) {
            double startMs = anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
            double endMs = anchorEnd.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
            qDebug()<<"=== GOT EXPORT MAIN ZONE: "<<GenTime(startMs).frames(pCore->getCurrentFps())<<" - "<<GenTime(endMs).frames(pCore->getCurrentFps());
            QPoint originalZone(QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps())));
            return processedZones({originalZone});
        }
        return {};
    }
    QVector<QPoint> zones;
    int zoneStart = -1;
    int zoneEnd = -1;
    int currentEnd = -1;
    int currentStart = -1;
    qDebug()<<"=== FROM BLOCKS: "<<m_selectedBlocks;
    for (auto &bk : m_selectedBlocks) {
        QPair<double, double> z = speechZones.at(bk);
        currentStart = GenTime(z.first).frames(pCore->getCurrentFps());
        currentEnd = GenTime(z.second).frames(pCore->getCurrentFps());
        if (zoneStart < 0) {
            zoneStart = currentStart;
        } else if (currentStart - zoneEnd > 1) {
            // Insert last zone
            zones << QPoint(zoneStart, zoneEnd);
            zoneStart = currentStart;
        }
        zoneEnd = currentEnd;
    }
    qDebug()<<"=== INSERT LAST: "<<currentStart<<"-"<<currentEnd;
    zones << QPoint(currentStart, currentEnd);

    qDebug()<<"=== GOT RESULTING ZONES: "<<zones;
    return processedZones(zones);
}

void VideoTextEdit::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
}

void VideoTextEdit::resizeEvent(QResizeEvent *e)
{
    QTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void VideoTextEdit::keyPressEvent(QKeyEvent *e)
{
    QTextEdit::keyPressEvent(e);
}

void VideoTextEdit::checkHoverBlock(int yPos)
{
    QTextCursor curs = QTextCursor(this->document());
    curs.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    
    m_hoveredBlock = -1;
    for (int i = 0; i < this->document()->blockCount(); ++i) {
        QTextBlock block = curs.block();
        QRect r2 = this->document()->documentLayout()->blockBoundingRect(block).translated(
                0, 0 - (
                    this->verticalScrollBar()->sliderPosition()
                    ) ).toRect();
        if (yPos < r2.x()) {
            break;
        }
        if (yPos > r2.x() && yPos < r2.bottom()) {
            m_hoveredBlock = i;
            break;
        }
        curs.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    }
    setCursor(m_hoveredBlock == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
    lineNumberArea->update();
}

void VideoTextEdit::blockClicked(Qt::KeyboardModifiers modifiers, bool play)
{
    if (m_hoveredBlock > -1 && m_hoveredBlock < speechZones.count()) {
        if (m_selectedBlocks.contains(m_hoveredBlock)) {
            if (modifiers & Qt::ControlModifier) {
                // remove from selection on ctrl+click an already selected block
                m_selectedBlocks.removeAll(m_hoveredBlock);
            } else {
                m_selectedBlocks = {m_hoveredBlock};
                lineNumberArea->update();
            }
        } else {
            // Add to selection
            if (modifiers & Qt::ControlModifier) {
                m_selectedBlocks << m_hoveredBlock;
            } else if (modifiers & Qt::ShiftModifier) {
                if (m_lastClickedBlock > -1) {
                    for (int i = qMin(m_lastClickedBlock, m_hoveredBlock); i <= qMax(m_lastClickedBlock, m_hoveredBlock); i++) {
                        if (!m_selectedBlocks.contains(i)) {
                            m_selectedBlocks << i;
                        }
                    }
                } else {
                    m_selectedBlocks = {m_hoveredBlock};
                }
            } else {
                m_selectedBlocks = {m_hoveredBlock};
            }
        }
        if (m_hoveredBlock >= 0) {
            m_lastClickedBlock = m_hoveredBlock;
        }
        QPair<double, double> zone = speechZones.at(m_hoveredBlock);
        double startMs = zone.first;
        double endMs = zone.second;
        pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(GenTime(startMs).frames(pCore->getCurrentFps()));
        pCore->getMonitor(Kdenlive::ClipMonitor)->slotLoadClipZone(QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps())));
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, m_hoveredBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        setTextCursor(cursor);        
        if (play) {
            pCore->getMonitor(Kdenlive::ClipMonitor)->slotPlayZone();
        }
    }
}

int VideoTextEdit::getFirstVisibleBlockId()
{
// Detect the first block for which bounding rect - once
// translated in absolute coordinates - is contained
// by the editor's text area

// Costly way of doing but since 
// "blockBoundingGeometry(...)" doesn't exist 
// for "QTextEdit"...

    QTextCursor curs = QTextCursor(this->document());
    curs.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    for(int i=0; i < this->document()->blockCount(); ++i)
    {
        QTextBlock block = curs.block();

        QRect r1 = this->viewport()->geometry();
        QRect r2 = this->document()->documentLayout()->blockBoundingRect(block).translated(
                r1.x(), r1.y() - (
                    this->verticalScrollBar()->sliderPosition()
                    ) ).toRect();

        if (r1.contains(r2, true)) { return i; }

        curs.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    }
    return 0;
}

void VideoTextEdit::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    this->verticalScrollBar()->setSliderPosition(this->verticalScrollBar()->sliderPosition());

    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), palette().alternateBase().color());
    int blockNumber = this->getFirstVisibleBlockId();

    QTextBlock block = this->document()->findBlockByNumber(blockNumber);
    QTextBlock prev_block = (blockNumber > 0) ? this->document()->findBlockByNumber(blockNumber-1) : block;
    int translate_y = (blockNumber > 0) ? -this->verticalScrollBar()->sliderPosition() : 0;

    int top = this->viewport()->geometry().top();

    // Adjust text position according to the previous "non entirely visible" block 
    // if applicable. Also takes in consideration the document's margin offset.
    int additional_margin;
    if (blockNumber == 0)
        // Simply adjust to document's margin
        additional_margin = int(this->document()->documentMargin()) -1 - this->verticalScrollBar()->sliderPosition();
    else
        // Getting the height of the visible part of the previous "non entirely visible" block
        additional_margin = int(this->document()->documentLayout()->blockBoundingRect(prev_block)
                .translated(0, translate_y).intersected(this->viewport()->geometry()).height());

    // Shift the starting point
    top += additional_margin;

    int bottom = top + int(this->document()->documentLayout()->blockBoundingRect(block).height());

    QColor col_2 = palette().link().color();
    QColor col_1 = palette().highlightedText().color();
    QColor col_0 = palette().text().color();

    // Draw the numbers (displaying the current line number in green)
    while (block.isValid() && top <= event->rect().bottom()) {
        if (blockNumber >= speechZones.count()) {
            break;
        }
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (m_selectedBlocks.contains(blockNumber)) {
                painter.fillRect(QRect(0, top, lineNumberArea->width(), bottom - top), palette().highlight().color());
            }
            QString number = pCore->timecode().getDisplayTimecode(GenTime(speechZones[blockNumber].first), false);
            painter.setPen(QColor(120, 120, 120));
            painter.setPen((this->textCursor().blockNumber() == blockNumber) ? col_2 : m_selectedBlocks.contains(blockNumber) ? col_1 : col_0);
            painter.drawText(-5, top,
                             lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + int(this->document()->documentLayout()->blockBoundingRect(block).height());
        ++blockNumber;
    }

}

void VideoTextEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    menu->addAction(bookmarkAction);
    menu->addAction(deleteAction);
    menu->exec(event->globalPos());
    delete menu;
}

void VideoTextEdit::mousePressEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton) {
        QTextCursor current = textCursor();
        QTextCursor cursor = cursorForPosition(e->pos());
        int pos = cursor.position();
        qDebug()<<"=== CLICKED AT: "<<pos<<", SEL: "<<current.selectionStart()<<"-"<<current.selectionEnd();
        if (pos > current.selectionStart() && pos < current.selectionEnd()) {
            // Clicked in selection
            e->ignore();
            qDebug()<<"=== IGNORING MOUSE CLICK";
            return;
        } else {
            QTextEdit::mousePressEvent(e);
            const QString link = anchorAt(e->pos());
            if (!link.isEmpty()) {
                // Clicked on a word
                cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
                double startMs = link.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
                pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(GenTime(startMs).frames(pCore->getCurrentFps()));
            }
        }
        setTextCursor(cursor);
    } else {
        QTextEdit::mousePressEvent(e);
    }
}

void VideoTextEdit::mouseReleaseEvent(QMouseEvent *e)
{
    QTextEdit::mouseReleaseEvent(e);
    if (e->button() == Qt::LeftButton) {
        QTextCursor cursor = textCursor();
        if (!cursor.selectedText().isEmpty()) {
            // We have a selection, ensure full word is selected
            int pos = cursor.position();
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();
            if (document()->characterAt(end - 1) == QLatin1Char(' ')) {
                // Selection already ends with a space
                return;
            }
            QTextBlock 	bk = cursor.block();
            if (bk.text().simplified() == i18n("No speech")) {
                // This is a silence block, select all
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            } else {
                cursor.setPosition(start);
                cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
                cursor.setPosition(end, QTextCursor::KeepAnchor);
                cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
            }
            pos = cursor.position();
            if (!cursor.atBlockEnd() && document()->characterAt(pos - 1) != QLatin1Char(' ')) {
                // Remove trailing space
                cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
            }
            setTextCursor(cursor);
        }
        if (!m_selectedBlocks.isEmpty()) {
            m_selectedBlocks.clear();
            repaintLines();
        }
    } else {
        qDebug()<<"==== NO LEFT CLICK!";
    }
}

void VideoTextEdit::mouseMoveEvent(QMouseEvent *e)
{
    qDebug()<<"==== MOUSE MOVE EVENT!!!";
    QTextEdit::mouseMoveEvent(e);
    if (e->buttons() & Qt::LeftButton) {
        /*QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        setTextCursor(cursor);*/
    } else {
        const QString link = anchorAt(e->pos());
        viewport()->setCursor(link.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }
}

TextBasedEdit::TextBasedEdit(QWidget *parent)
    : QWidget(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setFocusPolicy(Qt::StrongFocus);
    m_voskConfig = new QAction(i18n("Configure"), this);
    connect(m_voskConfig, &QAction::triggered, []() {
        pCore->window()->slotPreferences(8);
    });

    // Visual text editor
    auto *l = new QVBoxLayout;
    l->setContentsMargins(0, 0, 0, 0);
    m_visualEditor = new VideoTextEdit(this);
    m_visualEditor->installEventFilter(this);
    l->addWidget(m_visualEditor);
    text_frame->setLayout(l);
    m_visualEditor->setDocument(&m_document);
    connect(&m_document, &QTextDocument::blockCountChanged, [this](int ct) {
        m_visualEditor->repaintLines();
        qDebug()<<"++++++++++++++++++++\n\nGOT BLOCKS: "<<ct<<"\n\n+++++++++++++++++++++";
    });

    connect(m_visualEditor, &VideoTextEdit::selectionChanged, [this]() {
        bool hasSelection = m_visualEditor->textCursor().selectedText().simplified().isEmpty() == false;
        m_visualEditor->bookmarkAction->setEnabled(hasSelection);
        m_visualEditor->deleteAction->setEnabled(hasSelection);
        button_insert->setEnabled(hasSelection);
    });

    connect(button_start, &QPushButton::clicked, this, &TextBasedEdit::startRecognition);
    frame_progress->setVisible(false);
    button_abort->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    connect(button_abort, &QToolButton::clicked, [this]() {
        if (m_speechJob && m_speechJob->state() == QProcess::Running) {
            m_speechJob->kill();
        } else if (m_tCodeJob && m_tCodeJob->state() == QProcess::Running) {
            m_tCodeJob->kill();
        }
    });
    connect(pCore.get(), &Core::voskModelUpdate, [&](QStringList models) {
        language_box->clear();
        language_box->addItems(models);
        if (models.isEmpty()) {
            showMessage(i18n("Please install speech recognition models"), KMessageWidget::Information, m_voskConfig);
        } else {
            if (!KdenliveSettings::vosk_text_model().isEmpty() && models.contains(KdenliveSettings::vosk_text_model())) {
                int ix = language_box->findText(KdenliveSettings::vosk_text_model());
                if (ix > -1) {
                    language_box->setCurrentIndex(ix);
                }
            }
        }
    });
    connect(language_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this]() {
        KdenliveSettings::setVosk_text_model(language_box->currentText());
    });
    info_message->hide();

    m_logAction = new QAction(i18n("Show log"), this);
    connect(m_logAction, &QAction::triggered, [this]() {
        KMessageBox::sorry(this, m_errorString, i18n("Detailed log"));
    });

    speech_zone->setChecked(KdenliveSettings::speech_zone());
    connect(speech_zone, &QCheckBox::stateChanged, [](int state) {
        KdenliveSettings::setSpeech_zone(state == Qt::Checked);
    });
    button_delete->setDefaultAction(m_visualEditor->deleteAction);
    button_delete->setToolTip(i18n("Delete selected text"));
    connect(m_visualEditor->deleteAction, &QAction::triggered, this, &TextBasedEdit::deleteItem);
    
    button_add->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    button_add->setToolTip(i18n("Save edited text in a new playlist"));
    button_add->setEnabled(false);
    connect(button_add, &QToolButton::clicked, [this]() {
        previewPlaylist();
    });
    
    button_bookmark->setDefaultAction(m_visualEditor->bookmarkAction);
    button_bookmark->setToolTip(i18n("Add bookmark for current selection"));
    connect(m_visualEditor->bookmarkAction, &QAction::triggered, this, &TextBasedEdit::addBookmark);

    button_insert->setIcon(QIcon::fromTheme(QStringLiteral("timeline-insert")));
    button_insert->setToolTip(i18n("Insert selected blocks in timeline"));
    connect(button_insert, &QToolButton::clicked, this, &TextBasedEdit::insertToTimeline);
    button_insert->setEnabled(false);

    // Message Timer
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(5000);
    connect(&m_hideTimer, &QTimer::timeout, info_message, &KMessageWidget::animatedHide);

    // Search stuff
    search_frame->setVisible(false);
    button_search->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    search_prev->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    search_next->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    connect(button_search, &QToolButton::toggled, this, [&](bool toggled) {
        search_frame->setVisible(toggled);
        search_line->setFocus();
    });
    connect(search_line, &QLineEdit::textChanged, [this](const QString &searchText) {
        QPalette palette = this->palette();
        QColor col = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_visualEditor->find(searchText);
            if (found) {
                col.setGreen(qMin(255, static_cast<int>(col.green() * 1.5)));
                palette.setColor(QPalette::Base,col);
                QTextCursor cur = m_visualEditor->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_visualEditor->setTextCursor(cur);
            } else {
                // Loop over, abort
                col.setRed(qMin(255, static_cast<int>(col.red() * 1.5)));
                palette.setColor(QPalette::Base,col);
            }
        }
        search_line->setPalette(palette);   
    });
    connect(search_next, &QToolButton::clicked, [this]() {
        const QString searchText = search_line->text();
        QPalette palette = this->palette();
        QColor col = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_visualEditor->find(searchText);
            if (found) {
                col.setGreen(qMin(255, static_cast<int>(col.green() * 1.5)));
                palette.setColor(QPalette::Base,col);
                QTextCursor cur = m_visualEditor->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_visualEditor->setTextCursor(cur);
            } else {
                // Loop over, abort
                col.setRed(qMin(255, static_cast<int>(col.red() * 1.5)));
                palette.setColor(QPalette::Base,col);
            }
        }
        search_line->setPalette(palette);  
    });
    connect(search_prev, &QToolButton::clicked, [this]() {
        const QString searchText = search_line->text();
                QPalette palette = this->palette();
        QColor col = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_visualEditor->find(searchText, QTextDocument::FindBackward);
            if (found) {
                col.setGreen(qMin(255, static_cast<int>(col.green() * 1.5)));
                palette.setColor(QPalette::Base,col);
                QTextCursor cur = m_visualEditor->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_visualEditor->setTextCursor(cur);
            } else {
                // Loop over, abort
                col.setRed(qMin(255, static_cast<int>(col.red() * 1.5)));
                palette.setColor(QPalette::Base,col);
            }
        }
        search_line->setPalette(palette);  
    });
    parseVoskDictionaries();
}

TextBasedEdit::~TextBasedEdit()
{
    if (m_speechJob && m_speechJob->state() == QProcess::Running) {
        m_speechJob->kill();
        m_speechJob->waitForFinished();
    }
}

bool TextBasedEdit::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        qDebug()<<"==== FOT TXTEDIT EVENT FILTER: "<<static_cast <QKeyEvent*> (event)->key();
    }
    /*if(obj == m_visualEditor && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast <QKeyEvent*> (event);
        if (keyEvent->key() != Qt::Key_Left && keyEvent->key() != Qt::Key_Up && keyEvent->key() != Qt::Key_Right && keyEvent->key() != Qt::Key_Down) {
            parentWidget()->setFocus();
            return true;
        }
    }*/
    return QObject::eventFilter(obj, event);
}

void TextBasedEdit::startRecognition()
{
    if (m_speechJob && m_speechJob->state() != QProcess::NotRunning) {
        if (KMessageBox::questionYesNo(this, i18n("Another recognition job is running. Abort it ?")) !=  KMessageBox::Yes) {
            return;
        }
    }
    info_message->hide();
    m_errorString.clear();
    m_visualEditor->cleanup();
    //m_visualEditor->insertHtml(QStringLiteral("<body>"));
#ifdef Q_OS_WIN
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python"));
#else
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
#endif
    if (pyExec.isEmpty()) {
        showMessage(i18n("Cannot find python3, please install it on your system."), KMessageWidget::Warning);
        return;
    }

    if (!KdenliveSettings::vosk_found()) {
        showMessage(i18n("Please configure speech to text."), KMessageWidget::Warning, m_voskConfig);
        return;
    }
    // Start python script
    QString language = language_box->currentText();
    if (language.isEmpty()) {
        showMessage(i18n("Please install a language model."), KMessageWidget::Warning, m_voskConfig);
        return;
    }
    QString speechScript = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/speechtotext.py"));
    if (speechScript.isEmpty()) {
        showMessage(i18n("The speech script was not found, check your install."), KMessageWidget::Warning);
        return;
    }
    m_binId = pCore->getMonitor(Kdenlive::ClipMonitor)->activeClipId();
    std::shared_ptr<AbstractProjectItem> clip = pCore->projectItemModel()->getItemByBinId(m_binId);
    if (clip == nullptr) {
        showMessage(i18n("Select a clip with audio in Project Bin."), KMessageWidget::Information);
        return;
    }

    m_speechJob = std::make_unique<QProcess>(this);
    showMessage(i18n("Starting speech recognition"), KMessageWidget::Information);
    qApp->processEvents();
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    qDebug()<<"==== ANALYSIS SPEECH: "<<modelDirectory<<" - "<<language;
    
    m_sourceUrl.clear();
    QString clipName;
    m_clipOffset = 0;
    m_lastPosition = 0;
    double endPos = 0;
    bool hasAudio = false;
    if (clip->itemType() == AbstractProjectItem::ClipItem) {
        std::shared_ptr<ProjectClip> clipItem = std::static_pointer_cast<ProjectClip>(clip);
        if (clipItem) {
            m_sourceUrl = clipItem->url();
            clipName = clipItem->clipName();
            hasAudio = clipItem->hasAudio();
            if (speech_zone->isChecked()) {
                // Analyse clip zone only
                QPoint zone = clipItem->zone();
                m_lastPosition = zone.x();
                m_clipOffset = GenTime(zone.x(), pCore->getCurrentFps()).seconds();
                m_clipDuration = GenTime(zone.y() - zone.x(), pCore->getCurrentFps()).seconds();
                endPos = m_clipDuration;
            } else {
                m_clipDuration = clipItem->duration().seconds();
            }
        }
    } else if (clip->itemType() == AbstractProjectItem::SubClipItem) {
        std::shared_ptr<ProjectSubClip> clipItem = std::static_pointer_cast<ProjectSubClip>(clip);
        if (clipItem) {
            auto master = clipItem->getMasterClip();
            m_sourceUrl = master->url();
            hasAudio = master->hasAudio();
            clipName = master->clipName();
            QPoint zone = clipItem->zone();
            m_lastPosition = zone.x();
            m_clipOffset = GenTime(zone.x(), pCore->getCurrentFps()).seconds();
            m_clipDuration = GenTime(zone.y() - zone.x(), pCore->getCurrentFps()).seconds();
            endPos = m_clipDuration;
        }
    }
    if (m_sourceUrl.isEmpty() || !hasAudio) {
        showMessage(i18n("Select a clip with audio for speech recognition."), KMessageWidget::Information);
        return;
    }
    clipNameLabel->setText(clipName);
    if (clip->clipType() == ClipType::Playlist) {
        // We need to extract audio first
        m_playlistWav.remove();
        m_playlistWav.setFileTemplate(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.wav")));
        if (!m_playlistWav.open()) {
            showMessage(i18n("Cannot create temporary file."), KMessageWidget::Warning);
            return;
        }
        m_playlistWav.close();

        showMessage(i18n("Extracting audio for %1.", clipName), KMessageWidget::Information);
        qApp->processEvents();
        m_tCodeJob = std::make_unique<QProcess>(this);
        m_tCodeJob->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_tCodeJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                [this, language, pyExec, speechScript, clipName, modelDirectory, endPos](int code, QProcess::ExitStatus status) {
            Q_UNUSED(code)
            qDebug()<<"++++++++++++++++++++++ TCODE JOB FINISHED\n";
            if (status == QProcess::CrashExit) {
                showMessage(i18n("Audio extract failed."), KMessageWidget::Warning);
                speech_progress->setValue(0);
                frame_progress->setVisible(false);
                m_playlistWav.remove();
                return;
            }
            showMessage(i18n("Starting speech recognition on %1.", clipName), KMessageWidget::Information);
            qApp->processEvents();
            connect(m_speechJob.get(), &QProcess::readyReadStandardError, this, &TextBasedEdit::slotProcessSpeechError);
            connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &TextBasedEdit::slotProcessSpeech);
            connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [this](int code, QProcess::ExitStatus status) {
                m_playlistWav.remove();
                slotProcessSpeechStatus(code, status);
            });
            m_speechJob->start(pyExec, {speechScript, modelDirectory, language, m_playlistWav.fileName(), QString::number(m_clipOffset), QString::number(endPos)});
            speech_progress->setValue(0);
            frame_progress->setVisible(true);
        });
        connect(m_tCodeJob.get(), &QProcess::readyReadStandardOutput, [this]() {
            QString saveData = QString::fromUtf8(m_tCodeJob->readAllStandardOutput());
            qDebug()<<"+GOT OUTUT: "<<saveData;
            saveData = saveData.section(QStringLiteral("percentage:"), 1).simplified();
            int percent = saveData.section(QLatin1Char(' '), 0, 0).toInt();
            speech_progress->setValue(percent);
        });
        m_tCodeJob->start(KdenliveSettings::rendererpath(), {QStringLiteral("-progress"), m_sourceUrl, QStringLiteral("-consumer"), QString("avformat:%1").arg(m_playlistWav.fileName()), QStringLiteral("vn=1"), QStringLiteral("ar=16000")});
        speech_progress->setValue(0);
        frame_progress->setVisible(true);
    } else {
        showMessage(i18n("Starting speech recognition on %1.", clipName), KMessageWidget::Information);
        qApp->processEvents();
        connect(m_speechJob.get(), &QProcess::readyReadStandardError, this, &TextBasedEdit::slotProcessSpeechError);
        connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &TextBasedEdit::slotProcessSpeech);
        connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &TextBasedEdit::slotProcessSpeechStatus);
        qDebug()<<"=== STARTING RECO: "<<speechScript<<" / "<<modelDirectory<<" / "<<language<<" / "<<m_sourceUrl<<", START: "<<m_clipOffset<<", DUR: "<<endPos;
        button_add->setEnabled(false);
        m_speechJob->start(pyExec, {speechScript, modelDirectory, language, m_sourceUrl, QString::number(m_clipOffset), QString::number(endPos)});
        speech_progress->setValue(0);
        frame_progress->setVisible(true);
    }
}

void TextBasedEdit::slotProcessSpeechStatus(int, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        showMessage(i18n("Speech recognition aborted."), KMessageWidget::Warning, m_errorString.isEmpty() ? nullptr : m_logAction);
    } else if (m_visualEditor->toPlainText().isEmpty()) {
        if (m_errorString.contains(QStringLiteral("ModuleNotFoundError"))) {
            showMessage(i18n("Error, please check the speech to text configuration."), KMessageWidget::Warning, m_voskConfig);
        } else {
            showMessage(i18n("No speech detected."), KMessageWidget::Information, m_errorString.isEmpty() ? nullptr : m_logAction);
        }
    } else {
        button_add->setEnabled(true);
        showMessage(i18n("Speech recognition finished."), KMessageWidget::Positive);
        // Store speech analysis in clip properties
        std::shared_ptr<AbstractProjectItem> clip = pCore->projectItemModel()->getItemByBinId(m_binId);
        if (clip) {
            std::shared_ptr<ProjectClip> clipItem = std::static_pointer_cast<ProjectClip>(clip);
            QString oldSpeech;
            if (clipItem) {
                oldSpeech = clipItem->getProducerProperty(QStringLiteral("kdenlive:speech"));
            }
            QMap<QString, QString> oldProperties;
            oldProperties.insert(QStringLiteral("kdenlive:speech"), oldSpeech);
            QMap<QString, QString> properties;
            properties.insert(QStringLiteral("kdenlive:speech"), m_visualEditor->toHtml());
            pCore->bin()->slotEditClipCommand(m_binId, oldProperties, properties);
        }
    }
    QTextCursor cur = m_visualEditor->textCursor();
    cur.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    m_visualEditor->setTextCursor(cur);
    frame_progress->setVisible(false);
}

void TextBasedEdit::slotProcessSpeechError()
{
    m_errorString.append(QString::fromUtf8(m_speechJob->readAllStandardError()));
}

void TextBasedEdit::slotProcessSpeech()
{
    QString saveData = QString::fromUtf8(m_speechJob->readAllStandardOutput());
    qDebug()<<"=== GOT DATA:\n"<<saveData;
    QJsonParseError error;
    auto loadDoc = QJsonDocument::fromJson(saveData.toUtf8(), &error);
    qDebug()<<"===JSON ERROR: "<<error.errorString();
    QTextCursor cursor = m_visualEditor->textCursor();
    if (loadDoc.isObject()) {
        QJsonObject obj = loadDoc.object();
        if (!obj.isEmpty()) {
            //QString itemText = obj["text"].toString();
            QString htmlLine;
            QPair <double, double>sentenceZone;
            if (obj["result"].isArray()) {
                QJsonArray obj2 = obj["result"].toArray();
                // Store words with their start/end time
                foreach (const QJsonValue & v, obj2) {
                    htmlLine.append(QString("<a href=\"%1#%2:%3\">%4</a> ").arg(m_binId).arg(v.toObject().value("start").toDouble() + m_clipOffset).arg(v.toObject().value("end").toDouble() + m_clipOffset).arg(v.toObject().value("word").toString()));
                }
                // Get start time for first word
                QJsonValue val = obj2.first();
                if (val.isObject() && val.toObject().keys().contains("start")) {
                    double ms = val.toObject().value("start").toDouble() + m_clipOffset;
                    GenTime startPos(ms);
                    sentenceZone.first = ms;
                    if (startPos.frames(pCore->getCurrentFps()) > m_lastPosition + 1) {
                        // Insert space
                        GenTime silenceStart(m_lastPosition, pCore->getCurrentFps());
                        m_visualEditor->moveCursor(QTextCursor::End);
                        QString htmlSpace = QString("<a href=\"#%1:%2\">%3</a>").arg(silenceStart.seconds()).arg(GenTime(startPos.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds()).arg(i18n("No speech"));
                        m_visualEditor->insertHtml(htmlSpace);
                        m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
                        m_visualEditor->speechZones << QPair<double, double>(silenceStart.seconds(), GenTime(startPos.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds());
                    }
                    val = obj2.last();
                    if (val.isObject() && val.toObject().keys().contains("end")) {
                        double ms = val.toObject().value("end").toDouble() + m_clipOffset;
                        sentenceZone.second = ms;
                        m_lastPosition = GenTime(ms).frames(pCore->getCurrentFps());
                        if (m_clipDuration > 0.) {
                            speech_progress->setValue(static_cast<int>(100 * ms / ( + m_clipOffset + m_clipDuration)));
                        }
                    }
                }
            } else {
                // Last empty object - no speech detected
                GenTime silenceStart(m_lastPosition + 1, pCore->getCurrentFps());
                m_visualEditor->moveCursor(QTextCursor::End);
                QString htmlSpace = QString("<a href=\"#%1:%2\">%3</a>").arg(silenceStart.seconds()).arg(GenTime(m_clipDuration + m_clipOffset).seconds()).arg(i18n("No speech"));
                m_visualEditor->insertHtml(htmlSpace);
                m_visualEditor->speechZones << QPair<double, double>(silenceStart.seconds(), GenTime(m_clipDuration + m_clipOffset).seconds());
            }
            if (!htmlLine.isEmpty()) {
                m_visualEditor->insertHtml(htmlLine.simplified());
                if (sentenceZone.second < m_clipOffset + m_clipDuration) {
                    m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
                }
                m_visualEditor->speechZones << sentenceZone;
            }
        }
    } else if (loadDoc.isEmpty()) {
        qDebug()<<"==== EMPTY OBJEC DOC";
    }
    qDebug()<<"==== GOT BLOCKS: "<<m_document.blockCount();
    qDebug()<<"=== LINES: "<<m_document.firstBlock().lineCount();
    m_visualEditor->repaintLines();
}

void TextBasedEdit::parseVoskDictionaries()
{
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    QDir dir;
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dir = QDir(modelDirectory);
        if (!dir.cd(QStringLiteral("speechmodels"))) {
            qDebug()<<"=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
            pCore->voskModelUpdate({});
            return;
        }
    } else {
        dir = QDir(modelDirectory);
    }
    QStringList dicts = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList final;
    for (auto &d : dicts) {
        QDir sub(dir.absoluteFilePath(d));
        if (sub.exists(QStringLiteral("mfcc.conf")) || (sub.exists(QStringLiteral("conf/mfcc.conf")))) {
            final << d;
        }
    }
    pCore->voskModelUpdate(final);
}

void TextBasedEdit::deleteItem()
{
    QTextCursor cursor = m_visualEditor->textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    qDebug()<<"=== CUTTONG: "<<start<<" - "<<end;
    if (end > start) {
        QString anchorStart = m_visualEditor->selectionStartAnchor(cursor, start, end);
        cursor.setPosition(end);
        bool blockEnd = cursor.atBlockEnd();
        cursor = m_visualEditor->textCursor();
        QString anchorEnd = m_visualEditor->selectionEndAnchor(cursor, end, start);
        qDebug()<<"=== FINAL END CUT: "<<end;
        qDebug()<<"=== GOT END ANCHOR: "<<cursor.selectedText()<<" = "<<anchorEnd;
        if (!anchorEnd.isEmpty() && !anchorEnd.isEmpty()) {
            double startMs = anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
            double endMs = anchorEnd.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
            if (startMs < endMs) {
                qDebug()<<"=== GOT CUT ZONE: "<<GenTime(startMs).frames(pCore->getCurrentFps())<<" - "<<GenTime(endMs).frames(pCore->getCurrentFps());
                m_visualEditor->cutZones << QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps()));
                cursor = m_visualEditor->textCursor();
                cursor.removeSelectedText();
                if (blockEnd) {
                    cursor.deleteChar();
                }
            }
        }
    } else {
        QTextCursor curs = m_visualEditor->textCursor();
        curs.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        for (int i = 0; i < m_document.blockCount(); ++i) {
            int blockStart = curs.position();
            curs.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
            int blockEnd = curs.position();
            if (blockStart == blockEnd) {
                // Empty block, delete
                curs.select(QTextCursor::BlockUnderCursor);
                curs.removeSelectedText();
                curs.deleteChar();
            }
            curs.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        }
    }
    // Reset selection and rebuild line numbers
    m_visualEditor->rebuildZones();
    previewPlaylist(false);
}

void TextBasedEdit::insertToTimeline()
{
    QVector<QPoint> zones = m_visualEditor->getInsertZones();
    if (zones.isEmpty()) {
        return;
    }
    for (auto &zone : zones) {
        pCore->window()->getMainTimeline()->controller()->insertZone(m_binId, zone, false);
    }
}

void TextBasedEdit::previewPlaylist(bool createNew)
{
    QVector<QPoint> zones = m_visualEditor->getInsertZones();
    if (zones.isEmpty()) {
        showMessage(i18n("No text to export"), KMessageWidget::Information);
        return;
    }
    std::shared_ptr<AbstractProjectItem> clip = pCore->projectItemModel()->getItemByBinId(m_binId);
    std::shared_ptr<ProjectClip> clipItem = std::static_pointer_cast<ProjectClip>(clip);
    QString sourcePath = clipItem->url();
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("kdenlive:baseid"), m_binId);
    QStringList playZones;
    for (const auto&p : qAsConst(zones)) {
        playZones << QString("%1:%2").arg(p.x()).arg(p.y());
    }
    properties.insert(QStringLiteral("kdenlive:cutzones"), playZones.join(QLatin1Char(';')));
    int ix = 1;
    if (createNew) {
        m_playlist = QString("%1-cut%2.kdenlive").arg(sourcePath).arg(ix);
        while (QFile::exists(m_playlist)) {
            ix++;
            m_playlist = QString("%1-cut%2.kdenlive").arg(sourcePath).arg(ix);
        }
        QUrl url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(m_playlist), this, i18n("Enter new playlist path"));
        if (url.isEmpty()) {
            return;
        }
        m_playlist = url.toLocalFile();
    }
    if (!m_playlist.isEmpty()) {
        pCore->bin()->savePlaylist(m_binId, m_playlist, zones, properties, createNew);
        clipNameLabel->setText(QFileInfo(m_playlist).fileName());
    }
}

void TextBasedEdit::showMessage(const QString &text, KMessageWidget::MessageType type, QAction *action)
{
    if (m_currentMessageAction != nullptr && (action == nullptr || action != m_currentMessageAction)) {
        info_message->removeAction(m_currentMessageAction);
        m_currentMessageAction = action;
        if (m_currentMessageAction) {
            info_message->addAction(m_currentMessageAction);
        }
    } else if (action) {
        m_currentMessageAction = action;
        info_message->addAction(m_currentMessageAction);
    }

    if (info_message->isVisible()) {
        m_hideTimer.stop();
    }
    info_message->setMessageType(type);
    info_message->setText(text);
    info_message->animatedShow();
    if (type != KMessageWidget::Error && m_currentMessageAction == nullptr) {
        m_hideTimer.start();
    }
}

void TextBasedEdit::openClip(std::shared_ptr<ProjectClip> clip)
{
    if (m_speechJob && m_speechJob->state() == QProcess::Running) {
        // TODO: ask for job cancelation
        return;
    }
    if (clip) {
        QString refId = clip->getProducerProperty(QStringLiteral("kdenlive:baseid"));
        if (!refId.isEmpty() && refId == m_refId) {
            // We opened a resulting playlist, do not clear text edit
            return;
        }
        m_visualEditor->cleanup();
        QString speech;
        QList<QPoint> cutZones;
        m_binId = refId.isEmpty() ? clip->binId() : refId;
        if (!refId.isEmpty()) {
            // this is a clip  playlist with a bin reference, fetch it
            m_refId = refId;
            std::shared_ptr<ProjectClip> refClip = pCore->bin()->getBinClip(refId);
            if (refClip) {
                speech = refClip->getProducerProperty(QStringLiteral("kdenlive:speech"));
                clipNameLabel->setText(refClip->clipName());
            }
            QStringList zones = clip->getProducerProperty("kdenlive:cutzones").split(QLatin1Char(';'));
            for (const QString &z : qAsConst(zones)) {
                cutZones << QPoint(z.section(QLatin1Char(':'), 0, 0).toInt(), z.section(QLatin1Char(':'), 1, 1).toInt());
            }
        } else {
            m_refId.clear();
            speech = clip->getProducerProperty(QStringLiteral("kdenlive:speech"));
            clipNameLabel->setText(clip->clipName());
        }
        m_visualEditor->insertHtml(speech);
        if (!cutZones.isEmpty()) {
            m_visualEditor->processCutZones(cutZones);
        }
        m_visualEditor->rebuildZones();
        button_add->setEnabled(!speech.isEmpty());
    }
}

void TextBasedEdit::addBookmark()
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(m_binId);
    if (clip) {
        QString txt = m_visualEditor->textCursor().selectedText();
        QTextCursor cursor = m_visualEditor->textCursor();
        QString startAnchor = m_visualEditor->selectionStartAnchor(cursor, -1, -1);
        cursor = m_visualEditor->textCursor();
        QString endAnchor = m_visualEditor->selectionEndAnchor(cursor, -1, -1);
        if (startAnchor.isEmpty()) {
            showMessage(i18n("No timecode found in selection"), KMessageWidget::Information);
            return;
        }
        double ms = startAnchor.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
        int startPos = GenTime(ms).frames(pCore->getCurrentFps());
        ms = endAnchor.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
        int endPos = GenTime(ms).frames(pCore->getCurrentFps());
        int monitorPos = pCore->getMonitor(Kdenlive::ClipMonitor)->position();
        qDebug()<<"==== GOT MARKER: "<<txt<<", FOR POS: "<<startPos<<"-"<<endPos<<", MON: "<<monitorPos;
        if (monitorPos > startPos && monitorPos < endPos) {
            // Monitor seek is on the selection, use the current frame
            pCore->bin()->addClipMarker(m_binId, {monitorPos}, {txt});
        } else {
            pCore->bin()->addClipMarker(m_binId, {startPos}, {txt});
        }
    } else {
        qDebug()<<"==== NO CLIP FOR "<<m_binId;
    }
}
