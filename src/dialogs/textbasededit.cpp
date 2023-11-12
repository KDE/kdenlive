/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "textbasededit.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "bin/projectsubclip.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "widgets/timecodedisplay.h"
#include <profiles/profilemodel.hpp>

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <KUrlRequesterDialog>
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QToolButton>

#include <memory>

VideoTextEdit::VideoTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setMouseTracking(true);
    setReadOnly(true);
    // setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    lineNumberArea = new LineNumberArea(this);
    connect(this, &VideoTextEdit::cursorPositionChanged, [this]() { lineNumberArea->update(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { lineNumberArea->update(); });
    QRect rect = this->contentsRect();
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    bookmarkAction = new QAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add marker"), this);
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
    document()->setDefaultStyleSheet(QString("a {text-decoration:none;color:%1}").arg(palette().text().color().name()));
    setCurrentFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
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
    qDebug() << "==== TESTING SELECTION END ANCHOR FROM: " << end << " , MIN: " << min;
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
    qDebug() << "==== TESTING SELECTION END ANCHOR FROM: " << end << " , WORD: " << cursor.selectedText();
    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    cursor.setPosition(selStart + (selEnd - selStart) / 2);
    qDebug() << "==== END POS SELECTION FOR: " << cursor.selectedText() << " = " << anchorAt(cursorRect(cursor).center());
    QString anch = anchorAt(cursorRect(cursor).center());
    double endMs = anch.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
    qDebug() << "==== GOT LAST FRAME: " << GenTime(endMs).frames(25);
    return anchorAt(cursorRect(cursor).center());
}

void VideoTextEdit::processCutZones(const QList<QPoint> &loadZones)
{
    // Remove all outside load zones
    qDebug() << "=== LOADING CUT ZONES: " << loadZones << "\n........................";
    QTextCursor curs = textCursor();
    curs.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    qDebug() << "===== GOT DOCUMENT END: " << curs.position();
    curs.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    double fps = pCore->getCurrentFps();
    while (!curs.atEnd()) {
        qDebug() << "=== CURSOR POS: " << curs.position();
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
            qDebug() << "=== DELETING WORD: " << curs.selectedText();
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
            qDebug() << "=== WORD INSIDE, POS: " << curs.position();
        }
        qDebug() << "=== MOVED CURSOR POS: " << curs.position();
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
        // qDebug()<<"=== START ANCHOR: "<<anchorStart<<" AT POS: "<<curs.position();
        curs.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        int end = curs.position() - 1;
        QString anchorEnd = selectionEndAnchor(curs, end, start);
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

QVector<QPoint> VideoTextEdit::processedZones(const QVector<QPoint> &sourceZones)
{
    if (cutZones.isEmpty()) {
        return sourceZones;
    }
    QVector<QPoint> resultZones;
    QVector<QPoint> processingZones = sourceZones;
    int ix = 0;
    for (auto &cut : cutZones) {
        for (auto &zone : processingZones) {
            if (cut.x() > zone.y() || cut.y() < zone.x()) {
                // Cut is outside zone, keep it as is
                resultZones << zone;
            } else if (cut.y() >= zone.y()) {
                // Only keep the start of this zone
                resultZones << QPoint(zone.x(), cut.x());
            } else if (cut.x() <= zone.x()) {
                // Only keep the end of this zone
                resultZones << QPoint(cut.y(), zone.y());
            } else {
                // Cut is in the middle of this zone
                resultZones << QPoint(zone.x(), cut.x());
                resultZones << QPoint(cut.y(), zone.y());
            }
        }
        processingZones = resultZones;
        ix++;
        resultZones.clear();
    }
    return processingZones;
}

QVector<QPoint> VideoTextEdit::getInsertZones()
{
    if (m_selectedBlocks.isEmpty()) {
        // return text selection, not blocks
        QTextCursor cursor = textCursor();
        QString anchorStart;
        QString anchorEnd;
        if (!cursor.selectedText().isEmpty()) {
            qDebug() << "=== EXPORTING SELECTION";
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
            qDebug() << "=== GOT EXPORT MAIN ZONE: " << GenTime(startMs).frames(pCore->getCurrentFps()) << " - "
                     << GenTime(endMs).frames(pCore->getCurrentFps());
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
    qDebug() << "=== FROM BLOCKS: " << m_selectedBlocks;
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
    qDebug() << "=== INSERT LAST: " << currentStart << "-" << currentEnd;
    zones << QPoint(currentStart, currentEnd);

    qDebug() << "=== GOT RESULTING ZONES: " << zones;
    return processedZones(zones);
}

QVector<QPoint> VideoTextEdit::fullExport()
{
    // Loop through all blocks
    QVector<QPoint> zones;
    int currentEnd = -1;
    int currentStart = -1;
    for (int i = 0; i < document()->blockCount(); ++i) {
        QTextBlock block = document()->findBlockByNumber(i);
        QTextCursor curs(block);
        int start = curs.position() + 1;
        QString anchorStart = selectionStartAnchor(curs, start, document()->characterCount());
        // qDebug()<<"=== START ANCHOR: "<<anchorStart<<" AT POS: "<<curs.position();
        curs.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        int end = curs.position() - 1;
        QString anchorEnd = selectionEndAnchor(curs, end, start);
        if (!anchorStart.isEmpty() && !anchorEnd.isEmpty()) {
            double startMs = anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
            double endMs = anchorEnd.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
            currentStart = GenTime(startMs).frames(pCore->getCurrentFps());
            currentEnd = GenTime(endMs).frames(pCore->getCurrentFps());
            zones << QPoint(currentStart, currentEnd);
        }
    }
    return processedZones(zones);
}

void VideoTextEdit::slotRemoveSilence()
{
    for (int i = 0; i < document()->blockCount(); ++i) {
        QTextBlock block = document()->findBlockByNumber(i);
        if (block.text() == i18n("No speech")) {
            QTextCursor curs(block);
            curs.select(QTextCursor::BlockUnderCursor);
            curs.removeSelectedText();
            curs.deleteChar();
            i--;
            continue;
        }
    }
    rebuildZones();
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
        QRect r2 = this->document()->documentLayout()->blockBoundingRect(block).translated(0, 0 - (this->verticalScrollBar()->sliderPosition())).toRect();
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

        // Find continuous block selection
        int startBlock = m_hoveredBlock;
        int endBlock = m_hoveredBlock;
        while (m_selectedBlocks.contains(startBlock)) {
            startBlock--;
        }
        if (!m_selectedBlocks.contains(startBlock)) {
            startBlock++;
        }
        while (m_selectedBlocks.contains(endBlock)) {
            endBlock++;
        }
        if (!m_selectedBlocks.contains(endBlock)) {
            endBlock--;
        }
        QPair<double, double> zone = {speechZones.at(startBlock).first, speechZones.at(endBlock).second};
        double startMs = zone.first;
        double endMs = zone.second;
        pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(GenTime(startMs).frames(pCore->getCurrentFps()));
        pCore->getMonitor(Kdenlive::ClipMonitor)
            ->slotLoadClipZone(QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps())));
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
    for (int i = 0; i < this->document()->blockCount(); ++i) {
        QTextBlock block = curs.block();

        QRect r1 = this->viewport()->geometry();
        QRect r2 =
            this->document()->documentLayout()->blockBoundingRect(block).translated(r1.x(), r1.y() - (this->verticalScrollBar()->sliderPosition())).toRect();

        if (r1.contains(r2, true)) {
            return i;
        }

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
    QTextBlock prev_block = (blockNumber > 0) ? this->document()->findBlockByNumber(blockNumber - 1) : block;
    int translate_y = (blockNumber > 0) ? -this->verticalScrollBar()->sliderPosition() : 0;

    int top = this->viewport()->geometry().top();

    // Adjust text position according to the previous "non entirely visible" block
    // if applicable. Also takes in consideration the document's margin offset.
    int additional_margin;
    if (blockNumber == 0)
        // Simply adjust to document's margin
        additional_margin = int(this->document()->documentMargin()) - 1 - this->verticalScrollBar()->sliderPosition();
    else
        // Getting the height of the visible part of the previous "non entirely visible" block
        additional_margin = int(
            this->document()->documentLayout()->blockBoundingRect(prev_block).translated(0, translate_y).intersected(this->viewport()->geometry()).height());

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
                painter.setPen(col_1);
            } else {
                painter.setPen((this->textCursor().blockNumber() == blockNumber) ? col_2 : col_0);
            }
            QString number = pCore->timecode().getDisplayTimecode(GenTime(speechZones[blockNumber].first), false);
            painter.drawText(-5, top, lineNumberArea->width(), fontMetrics().height(), Qt::AlignRight, number);
        }
        painter.setPen(palette().dark().color());
        painter.drawLine(0, bottom, width(), bottom);
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
        qDebug() << "=== CLICKED AT: " << pos << ", SEL: " << current.selectionStart() << "-" << current.selectionEnd();
        if (pos > current.selectionStart() && pos < current.selectionEnd()) {
            // Clicked in selection
            e->ignore();
            qDebug() << "=== IGNORING MOUSE CLICK";
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
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();
            if (document()->characterAt(end - 1) == QLatin1Char(' ')) {
                // Selection ends with a space
                end--;
            }
            QTextBlock bk = cursor.block();
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
            setTextCursor(cursor);
        }
        if (!m_selectedBlocks.isEmpty()) {
            m_selectedBlocks.clear();
            repaintLines();
        }
    } else {
        qDebug() << "==== NO LEFT CLICK!";
    }
}

void VideoTextEdit::mouseMoveEvent(QMouseEvent *e)
{
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

void VideoTextEdit::wheelEvent(QWheelEvent *e)
{
    QTextEdit::wheelEvent(e);
}

TextBasedEdit::TextBasedEdit(QWidget *parent)
    : QWidget(parent)
    , m_stt(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setFocusPolicy(Qt::StrongFocus);
    connect(pCore.get(), &Core::speechEngineChanged, this, &TextBasedEdit::updateEngine);

    // Settings menu
    QMenu *menu = new QMenu(this);
    m_translateAction = new QAction(i18n("Translate to English"), this);
    m_translateAction->setCheckable(true);
    menu->addAction(m_translateAction);
    QAction *configAction = new QAction(i18n("Configure Speech Recognition"), this);
    menu->addAction(configAction);
    button_config->setMenu(menu);
    button_config->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    connect(m_translateAction, &QAction::triggered, [this](bool enabled) { KdenliveSettings::setWhisperTranslate(enabled); });
    connect(configAction, &QAction::triggered, [this]() { pCore->window()->slotShowPreferencePage(Kdenlive::PageSpeech); });
    connect(menu, &QMenu::aboutToShow, [this]() {
        m_translateAction->setChecked(KdenliveSettings::whisperTranslate());
        m_translateAction->setEnabled(KdenliveSettings::speechEngine() == QLatin1String("whisper"));
    });

    m_voskConfig = new QAction(i18n("Configure"), this);
    connect(m_voskConfig, &QAction::triggered, []() { pCore->window()->slotShowPreferencePage(Kdenlive::PageSpeech); });

    // Visual text editor
    auto *l = new QVBoxLayout;
    l->setContentsMargins(0, 0, 0, 0);
    m_visualEditor = new VideoTextEdit(this);
    m_visualEditor->installEventFilter(this);
    l->addWidget(m_visualEditor);
    text_frame->setLayout(l);
    m_document.setDefaultFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    // m_document = m_visualEditor->document();
    // m_document.setDefaultFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_visualEditor->setDocument(&m_document);
    connect(&m_document, &QTextDocument::blockCountChanged, this, [this](int ct) {
        m_visualEditor->repaintLines();
        qDebug() << "++++++++++++++++++++\n\nGOT BLOCKS: " << ct << "\n\n+++++++++++++++++++++";
    });

    QMenu *insertMenu = new QMenu(this);
    QAction *createSequence = new QAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Create new sequence with edit"), this);
    QAction *insertSelection = new QAction(QIcon::fromTheme(QStringLiteral("timeline-insert")), i18n("Insert selection in timeline"), this);
    QAction *saveAsPlaylist = new QAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Save edited text in a playlist file"), this);
    insertMenu->addAction(createSequence);
    insertMenu->addAction(insertSelection);
    insertMenu->addSeparator();
    insertMenu->addAction(saveAsPlaylist);
    button_insert->setMenu(insertMenu);
    button_insert->setDefaultAction(createSequence);
    button_insert->setToolTip(i18n("Create new sequence with text edit "));

    connect(createSequence, &QAction::triggered, this, &TextBasedEdit::createSequence);
    connect(insertSelection, &QAction::triggered, this, &TextBasedEdit::insertToTimeline);
    connect(saveAsPlaylist, &QAction::triggered, this, [&]() { previewPlaylist(true); });
    insertSelection->setEnabled(false);

    connect(m_visualEditor, &VideoTextEdit::selectionChanged, this, [this, insertSelection]() {
        bool hasSelection = m_visualEditor->textCursor().selectedText().simplified().isEmpty() == false;
        m_visualEditor->bookmarkAction->setEnabled(hasSelection);
        m_visualEditor->deleteAction->setEnabled(hasSelection);
        insertSelection->setEnabled(hasSelection);
    });

    button_start->setEnabled(false);
    connect(button_start, &QPushButton::clicked, this, &TextBasedEdit::startRecognition);
    frame_progress->setVisible(false);
    connect(button_abort, &QToolButton::clicked, this, [this]() {
        if (m_speechJob && m_speechJob->state() == QProcess::Running) {
            m_speechJob->kill();
        } else if (m_tCodeJob && m_tCodeJob->state() == QProcess::Running) {
            m_tCodeJob->kill();
        }
    });
    language_box->setToolTip(i18n("Speech model"));
    speech_language->setToolTip(i18n("Speech language"));
    connect(pCore.get(), &Core::voskModelUpdate, this, [&](const QStringList &models) {
        if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
            return;
        }
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
    connect(language_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this]() {
        if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
            const QString modelName = language_box->currentData().toString();
            speech_language->setEnabled(!modelName.endsWith(QLatin1String(".en")));
            KdenliveSettings::setWhisperModel(modelName);
        } else {
            KdenliveSettings::setVosk_text_model(language_box->currentText());
        }
    });
    info_message->hide();
    updateEngine();

    m_logAction = new QAction(i18n("Show log"), this);
    connect(m_logAction, &QAction::triggered, this, [this]() { KMessageBox::error(this, m_errorString, i18n("Detailed log")); });

    speech_zone->setChecked(KdenliveSettings::speech_zone());
    connect(speech_zone, &QCheckBox::stateChanged, [](int state) { KdenliveSettings::setSpeech_zone(state == Qt::Checked); });
    button_delete->setDefaultAction(m_visualEditor->deleteAction);
    button_delete->setToolTip(i18n("Delete selected text"));
    connect(m_visualEditor->deleteAction, &QAction::triggered, this, &TextBasedEdit::deleteItem);

    button_bookmark->setDefaultAction(m_visualEditor->bookmarkAction);
    button_bookmark->setToolTip(i18n("Add marker for current selection"));
    connect(m_visualEditor->bookmarkAction, &QAction::triggered, this, &TextBasedEdit::addBookmark);

    // Zoom
    QAction *zoomIn = new QAction(QIcon::fromTheme(QStringLiteral("zoom-in")), i18n("Zoom In"), this);
    connect(zoomIn, &QAction::triggered, this, &TextBasedEdit::slotZoomIn);
    QAction *zoomOut = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")), i18n("Zoom Out"), this);
    connect(zoomOut, &QAction::triggered, this, &TextBasedEdit::slotZoomOut);
    QAction *removeSilence = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Remove non speech zones"), this);
    connect(removeSilence, &QAction::triggered, m_visualEditor, &VideoTextEdit::slotRemoveSilence);
    // Build menu
    QMenu *extraMenu = new QMenu(this);
    extraMenu->addAction(zoomIn);
    extraMenu->addAction(zoomOut);
    extraMenu->addSeparator();
    extraMenu->addAction(removeSilence);
    subMenu->setMenu(extraMenu);

    // Message Timer
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(5000);
    connect(&m_hideTimer, &QTimer::timeout, info_message, &KMessageWidget::animatedHide);

    // Search stuff
    search_frame->setVisible(false);
    connect(button_search, &QToolButton::toggled, this, [&](bool toggled) {
        search_frame->setVisible(toggled);
        search_line->setFocus();
    });
    connect(search_line, &QLineEdit::textChanged, this, [this](const QString &searchText) {
        QPalette palette = this->palette();
        QColor col = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_visualEditor->find(searchText);
            if (found) {
                col.setGreen(qMin(255, static_cast<int>(col.green() * 1.5)));
                palette.setColor(QPalette::Base, col);
                QTextCursor cur = m_visualEditor->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_visualEditor->setTextCursor(cur);
            } else {
                // Loop over, abort
                col.setRed(qMin(255, static_cast<int>(col.red() * 1.5)));
                palette.setColor(QPalette::Base, col);
            }
        }
        search_line->setPalette(palette);
    });
    connect(search_next, &QToolButton::clicked, this, [this]() {
        const QString searchText = search_line->text();
        QPalette palette = this->palette();
        QColor col = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_visualEditor->find(searchText);
            if (found) {
                col.setGreen(qMin(255, static_cast<int>(col.green() * 1.5)));
                palette.setColor(QPalette::Base, col);
                QTextCursor cur = m_visualEditor->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_visualEditor->setTextCursor(cur);
            } else {
                // Loop over, abort
                col.setRed(qMin(255, static_cast<int>(col.red() * 1.5)));
                palette.setColor(QPalette::Base, col);
            }
        }
        search_line->setPalette(palette);
    });
    connect(search_prev, &QToolButton::clicked, this, [this]() {
        const QString searchText = search_line->text();
        QPalette palette = this->palette();
        QColor col = palette.color(QPalette::Base);
        if (searchText.length() > 2) {
            bool found = m_visualEditor->find(searchText, QTextDocument::FindBackward);
            if (found) {
                col.setGreen(qMin(255, static_cast<int>(col.green() * 1.5)));
                palette.setColor(QPalette::Base, col);
                QTextCursor cur = m_visualEditor->textCursor();
                cur.select(QTextCursor::WordUnderCursor);
                m_visualEditor->setTextCursor(cur);
            } else {
                // Loop over, abort
                col.setRed(qMin(255, static_cast<int>(col.red() * 1.5)));
                palette.setColor(QPalette::Base, col);
            }
        }
        search_line->setPalette(palette);
    });
}

void TextBasedEdit::slotZoomIn()
{
    QTextCursor cursor = m_visualEditor->textCursor();
    m_visualEditor->selectAll();
    qreal fontSize = QFontInfo(m_visualEditor->currentFont()).pointSizeF() * 1.2;
    KdenliveSettings::setSubtitleEditFontSize(fontSize);
    m_visualEditor->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
    m_visualEditor->setTextCursor(cursor);
}

void TextBasedEdit::slotZoomOut()
{
    QTextCursor cursor = m_visualEditor->textCursor();
    m_visualEditor->selectAll();
    qreal fontSize = QFontInfo(m_visualEditor->currentFont()).pointSizeF() / 1.2;
    fontSize = qMax(fontSize, QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pointSizeF());
    KdenliveSettings::setSubtitleEditFontSize(fontSize);
    m_visualEditor->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
    m_visualEditor->setTextCursor(cursor);
}

void TextBasedEdit::applyFontSize()
{
    if (KdenliveSettings::subtitleEditFontSize() > 0) {
        QTextCursor cursor = m_visualEditor->textCursor();
        m_visualEditor->selectAll();
        m_visualEditor->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
        m_visualEditor->setTextCursor(cursor);
    }
}

void TextBasedEdit::updateEngine()
{
    delete m_stt;
    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        m_stt = new SpeechToText(SpeechToText::EngineType::EngineWhisper, this);
        language_box->clear();
        QList<std::pair<QString, QString>> whisperModels = m_stt->whisperModels();
        for (auto &w : whisperModels) {
            language_box->addItem(w.first, w.second);
        }
        int ix = language_box->findData(KdenliveSettings::whisperModel());
        if (ix > -1) {
            language_box->setCurrentIndex(ix);
        }
        if (speech_language->count() == 0) {
            // Fill whisper languages
            QMap<QString, QString> languages = m_stt->whisperLanguages();
            QMapIterator<QString, QString> j(languages);
            while (j.hasNext()) {
                j.next();
                speech_language->addItem(j.key(), j.value());
            }
            int ix = speech_language->findData(KdenliveSettings::whisperLanguage());
            if (ix > -1) {
                speech_language->setCurrentIndex(ix);
            }
        }
        speech_language->setEnabled(!KdenliveSettings::whisperModel().endsWith(QLatin1String(".en")));
        speech_language->setVisible(true);
    } else {
        // VOSK
        speech_language->setVisible(false);
        m_stt = new SpeechToText(SpeechToText::EngineType::EngineVosk, this);
        language_box->clear();
        m_stt->parseVoskDictionaries();
    }
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
        qDebug() << "==== FOT TXTEDIT EVENT FILTER: " << static_cast<QKeyEvent *>(event)->key();
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
        if (KMessageBox::questionTwoActions(
                this, i18n("Another recognition job is already running. It will be aborted in favor of the new job. Do you want to proceed?"), {},
                KStandardGuiItem::cont(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
            return;
        }
    }
    info_message->hide();
    m_errorString.clear();
    m_visualEditor->cleanup();
    // m_visualEditor->insertHtml(QStringLiteral("<body>"));
    m_stt->checkDependencies(false);
    QString modelDirectory;
    QString language;
    QString modelName;
    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        // Whisper engine
        if (!m_stt->checkSetup() || !m_stt->missingDependencies({QStringLiteral("openai-whisper")}).isEmpty()) {
            showMessage(i18n("Please configure speech to text."), KMessageWidget::Warning, m_voskConfig);
            return;
        }
        modelName = language_box->currentData().toString();
        language = speech_language->isEnabled() && !speech_language->currentData().isNull()
                       ? QStringLiteral("language=%1").arg(speech_language->currentData().toString())
                       : QString();
        if (KdenliveSettings::whisperDisableFP16()) {
            language.append(QStringLiteral(" fp16=False"));
        }
    } else {
        // VOSK engine
        if (!m_stt->checkSetup() || !m_stt->missingDependencies({QStringLiteral("vosk")}).isEmpty()) {
            showMessage(i18n("Please configure speech to text."), KMessageWidget::Warning, m_voskConfig);
            return;
        }
        // Start python script
        modelName = language_box->currentText();
        if (modelName.isEmpty()) {
            showMessage(i18n("Please install a language model."), KMessageWidget::Warning, m_voskConfig);
            return;
        }
        modelDirectory = m_stt->voskModelPath();
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
        connect(m_tCodeJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
                [this, modelName, language, clipName, modelDirectory, endPos](int code, QProcess::ExitStatus status) {
                    Q_UNUSED(code)
                    qDebug() << "++++++++++++++++++++++ TCODE JOB FINISHED\n";
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
                    connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
                            [this](int code, QProcess::ExitStatus status) {
                                m_playlistWav.remove();
                                slotProcessSpeechStatus(code, status);
                            });
                    qDebug() << "::: STARTING SPEECH: " << modelDirectory << " / " << modelName << " / " << language;
                    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
                        // Whisper
                        connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &TextBasedEdit::slotProcessWhisperSpeech);
                        if (speech_zone->isChecked()) {
                            m_tmpCutWav.setFileTemplate(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.wav")));
                            if (!m_tmpCutWav.open()) {
                                showMessage(i18n("Cannot create temporary file."), KMessageWidget::Warning);
                                return;
                            }
                            m_tmpCutWav.close();
                            m_speechJob->start(m_stt->pythonExec(),
                                               {m_stt->speechScript(), m_playlistWav.fileName(), modelName, KdenliveSettings::whisperDevice(),
                                                KdenliveSettings::whisperTranslate() ? QStringLiteral("translate") : QStringLiteral("transcribe"), language,
                                                QString::number(m_clipOffset), QString::number(endPos), m_tmpCutWav.fileName()});
                        } else {
                            m_speechJob->start(m_stt->pythonExec(),
                                               {m_stt->speechScript(), m_playlistWav.fileName(), modelName, KdenliveSettings::whisperDevice(),
                                                KdenliveSettings::whisperTranslate() ? QStringLiteral("translate") : QStringLiteral("transcribe"), language});
                        }
                    } else {
                        m_speechJob->start(m_stt->pythonExec(), {m_stt->speechScript(), modelDirectory, modelName, m_playlistWav.fileName(),
                                                                 QString::number(m_clipOffset), QString::number(endPos)});
                    }
                    speech_progress->setValue(0);
                    frame_progress->setVisible(true);
                });
        connect(m_tCodeJob.get(), &QProcess::readyReadStandardOutput, this, [this]() {
            QString saveData = QString::fromUtf8(m_tCodeJob->readAllStandardOutput());
            qDebug() << "+GOT OUTPUT: " << saveData;
            saveData = saveData.section(QStringLiteral("percentage:"), 1).simplified();
            int percent = saveData.section(QLatin1Char(' '), 0, 0).toInt();
            speech_progress->setValue(percent);
        });
        m_tCodeJob->start(KdenliveSettings::meltpath(),
                          {QStringLiteral("-progress"), m_sourceUrl, QStringLiteral("-consumer"), QString("avformat:%1").arg(m_playlistWav.fileName()),
                           QStringLiteral("vn=1"), QStringLiteral("ar=16000")});
        speech_progress->setValue(0);
        frame_progress->setVisible(true);
    } else {
        showMessage(i18n("Starting speech recognition on %1.", clipName), KMessageWidget::Information);
        qApp->processEvents();
        connect(m_speechJob.get(), &QProcess::readyReadStandardError, this, &TextBasedEdit::slotProcessSpeechError);
        connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
                &TextBasedEdit::slotProcessSpeechStatus);
        button_insert->setEnabled(false);
        if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
            // Whisper
            qDebug() << "=== STARTING Whisper reco: " << m_stt->speechScript() << " / " << language_box->currentData() << " / "
                     << KdenliveSettings::whisperDevice() << " / "
                     << (KdenliveSettings::whisperTranslate() ? QStringLiteral("translate") : QStringLiteral("transcribe")) << " / " << m_sourceUrl
                     << ", START: " << m_clipOffset << ", DUR: " << endPos << " / " << language;
            connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &TextBasedEdit::slotProcessWhisperSpeech);
            if (speech_zone->isChecked()) {
                m_tmpCutWav.setFileTemplate(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.wav")));
                if (!m_tmpCutWav.open()) {
                    showMessage(i18n("Cannot create temporary file."), KMessageWidget::Warning);
                    return;
                }
                m_tmpCutWav.close();
                m_speechJob->start(m_stt->pythonExec(), {m_stt->speechScript(), m_sourceUrl, modelName, KdenliveSettings::whisperDevice(),
                                                         KdenliveSettings::whisperTranslate() ? QStringLiteral("translate") : QStringLiteral("transcribe"),
                                                         language, QString::number(m_clipOffset), QString::number(endPos), m_tmpCutWav.fileName()});
            } else {
                m_speechJob->start(m_stt->pythonExec(),
                                   {m_stt->speechScript(), m_sourceUrl, modelName, KdenliveSettings::whisperDevice(),
                                    KdenliveSettings::whisperTranslate() ? QStringLiteral("translate") : QStringLiteral("transcribe"), language});
            }
        } else {
            // VOSK
            qDebug() << "=== STARTING RECO: " << m_stt->speechScript() << " / " << modelDirectory << " / " << modelName << " / " << m_sourceUrl
                     << ", START: " << m_clipOffset << ", DUR: " << endPos;
            connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &TextBasedEdit::slotProcessSpeech);
            m_speechJob->start(m_stt->pythonExec(),
                               {m_stt->speechScript(), modelDirectory, modelName, m_sourceUrl, QString::number(m_clipOffset), QString::number(endPos)});
        }
        speech_progress->setValue(0);
        frame_progress->setVisible(true);
    }
}

void TextBasedEdit::slotProcessSpeechStatus(int, QProcess::ExitStatus status)
{
    m_tmpCutWav.remove();
    if (status == QProcess::CrashExit) {
        showMessage(i18n("Speech recognition aborted."), KMessageWidget::Warning, m_errorString.isEmpty() ? nullptr : m_logAction);
    } else if (m_visualEditor->toPlainText().isEmpty()) {
        if (m_errorString.contains(QStringLiteral("ModuleNotFoundError"))) {
            showMessage(i18n("Error, please check the speech to text configuration."), KMessageWidget::Warning, m_voskConfig);
        } else {
            showMessage(i18n("No speech detected."), KMessageWidget::Information, m_errorString.isEmpty() ? nullptr : m_logAction);
        }
    } else {
        // Last empty object - no speech detected
        if (KdenliveSettings::speechEngine() != QLatin1String("whisper")) {
            // VOSK
            GenTime silenceStart(m_lastPosition + 1, pCore->getCurrentFps());
            if (silenceStart.seconds() < m_clipDuration + m_clipOffset) {
                m_visualEditor->moveCursor(QTextCursor::End);
                QTextCursor cursor = m_visualEditor->textCursor();
                QTextCharFormat fmt = cursor.charFormat();
                fmt.setAnchorHref(QString("%1#%2:%3").arg(m_binId).arg(silenceStart.seconds()).arg(GenTime(m_clipDuration + m_clipOffset).seconds()));
                fmt.setAnchor(true);
                cursor.insertText(i18n("No speech"), fmt);
                m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
                m_visualEditor->speechZones << QPair<double, double>(silenceStart.seconds(), GenTime(m_clipDuration + m_clipOffset).seconds());
                m_visualEditor->repaintLines();
            }
        }

        button_insert->setEnabled(true);
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
    applyFontSize();
}

void TextBasedEdit::slotProcessSpeechError()
{
    const QString log = QString::fromUtf8(m_speechJob->readAllStandardError());
    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        if (log.contains(QStringLiteral("%|"))) {
            int prog = log.section(QLatin1Char('%'), 0, 0).toInt();
            speech_progress->setValue(prog);
        }
    }
    m_errorString.append(log);
}

void TextBasedEdit::slotProcessWhisperSpeech()
{
    const QString saveData = QString::fromUtf8(m_speechJob->readAllStandardOutput());
    QStringList sentences = saveData.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString sentenceTimings = sentences.takeFirst();
    if (!sentenceTimings.startsWith(QLatin1Char('['))) {
        // This is not a timing output
        if (sentenceTimings.startsWith(QStringLiteral("Detected "))) {
            showMessage(sentenceTimings, KMessageWidget::Information);
        }
        return;
    }
    QPair<double, double> sentenceZone;
    sentenceZone.first = sentenceTimings.section(QLatin1Char('['), 1).section(QLatin1Char('>'), 0, 0).toDouble() + m_clipOffset;
    sentenceZone.second = sentenceTimings.section(QLatin1Char('>'), 1).section(QLatin1Char(']'), 0, 0).toDouble() + m_clipOffset;
    QTextCursor cursor = m_visualEditor->textCursor();
    QTextCharFormat fmt = cursor.charFormat();
    QPair<double, double> wordZone;
    GenTime sentenceStart(sentenceZone.first);
    if (sentenceStart.frames(pCore->getCurrentFps()) > m_lastPosition + 1) {
        // Insert space
        GenTime silenceStart(m_lastPosition, pCore->getCurrentFps());
        m_visualEditor->moveCursor(QTextCursor::End);
        fmt.setAnchorHref(QString("%1#%2:%3")
                              .arg(m_binId)
                              .arg(silenceStart.seconds())
                              .arg(GenTime(sentenceStart.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds()));
        fmt.setAnchor(true);
        cursor.insertText(i18n("No speech"), fmt);
        fmt.setAnchor(false);
        m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
        m_visualEditor->speechZones << QPair<double, double>(silenceStart.seconds(),
                                                             GenTime(sentenceStart.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds());
    }
    for (auto &s : sentences) {
        wordZone.first = s.section(QLatin1Char('['), 1).section(QLatin1Char('>'), 0, 0).toDouble() + m_clipOffset;
        wordZone.second = s.section(QLatin1Char('>'), 1).section(QLatin1Char(']'), 0, 0).toDouble() + m_clipOffset;
        const QString text = s.section(QLatin1Char(']'), 1);
        if (text.isEmpty()) {
            // new section, insert
            m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
            m_visualEditor->speechZones << sentenceZone;
            GenTime lastSentence(sentenceZone.second);
            GenTime nextSentence(wordZone.first);
            if (nextSentence.frames(pCore->getCurrentFps()) > lastSentence.frames(pCore->getCurrentFps()) + 1) {
                // Insert space
                m_visualEditor->moveCursor(QTextCursor::End);
                fmt.setAnchorHref(QString("%1#%2:%3")
                                      .arg(m_binId)
                                      .arg(lastSentence.seconds())
                                      .arg(GenTime(nextSentence.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds()));
                fmt.setAnchor(true);
                cursor.insertText(i18n("No speech"), fmt);
                fmt.setAnchor(false);
                m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
                m_visualEditor->speechZones << QPair<double, double>(
                    lastSentence.seconds(), GenTime(nextSentence.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds());
            }
            sentenceZone = wordZone;
            continue;
        }
        fmt.setAnchor(true);
        fmt.setAnchorHref(QString("%1#%2:%3").arg(m_binId).arg(wordZone.first).arg(wordZone.second));
        cursor.insertText(text, fmt);
        fmt.setAnchor(false);
        cursor.insertText(QStringLiteral(" "), fmt);
    }
    m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
    m_visualEditor->speechZones << sentenceZone;
    if (sentenceZone.second < m_clipOffset + m_clipDuration) {
    }
    m_visualEditor->repaintLines();
    qDebug() << ":::  " << saveData;
}

void TextBasedEdit::slotProcessSpeech()
{
    QString saveData = QString::fromUtf8(m_speechJob->readAllStandardOutput());
    qDebug() << "=== GOT DATA:\n" << saveData;
    QJsonParseError error;
    auto loadDoc = QJsonDocument::fromJson(saveData.toUtf8(), &error);
    qDebug() << "===JSON ERROR: " << error.errorString();
    QTextCursor cursor = m_visualEditor->textCursor();
    QTextCharFormat fmt = cursor.charFormat();
    // fmt.setForeground(palette().text().color());
    if (loadDoc.isObject()) {
        QJsonObject obj = loadDoc.object();
        if (!obj.isEmpty()) {
            // QString itemText = obj["text"].toString();
            bool textFound = false;
            QPair<double, double> sentenceZone;
            if (obj["result"].isArray()) {
                QJsonArray obj2 = obj["result"].toArray();

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
                        fmt.setAnchorHref(QString("%1#%2:%3")
                                              .arg(m_binId)
                                              .arg(silenceStart.seconds())
                                              .arg(GenTime(startPos.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds()));
                        fmt.setAnchor(true);
                        cursor.insertText(i18n("No speech"), fmt);
                        m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
                        m_visualEditor->speechZones << QPair<double, double>(
                            silenceStart.seconds(), GenTime(startPos.frames(pCore->getCurrentFps()) - 1, pCore->getCurrentFps()).seconds());
                    }
                    val = obj2.last();
                    if (val.isObject() && val.toObject().keys().contains("end")) {
                        ms = val.toObject().value("end").toDouble() + m_clipOffset;
                        sentenceZone.second = ms;
                        m_lastPosition = GenTime(ms).frames(pCore->getCurrentFps());
                        if (m_clipDuration > 0.) {
                            speech_progress->setValue(static_cast<int>(100 * ms / (+m_clipOffset + m_clipDuration)));
                        }
                    }
                }
                // Store words with their start/end time
                for (const QJsonValue &v : obj2) {
                    textFound = true;
                    fmt.setAnchor(true);
                    fmt.setAnchorHref(QString("%1#%2:%3")
                                          .arg(m_binId)
                                          .arg(v.toObject().value("start").toDouble() + m_clipOffset)
                                          .arg(v.toObject().value("end").toDouble() + m_clipOffset));
                    cursor.insertText(v.toObject().value("word").toString(), fmt);
                    fmt.setAnchor(false);
                    cursor.insertText(QStringLiteral(" "), fmt);
                }
            } else {
                // Last empty object - no speech detected
            }
            if (textFound) {
                if (sentenceZone.second < m_clipOffset + m_clipDuration) {
                    m_visualEditor->textCursor().insertBlock(cursor.blockFormat());
                }
                m_visualEditor->speechZones << sentenceZone;
            }
        }
    } else if (loadDoc.isEmpty()) {
        qDebug() << "==== EMPTY OBJECT DOC";
    }
    qDebug() << "==== GOT BLOCKS: " << m_document.blockCount();
    qDebug() << "=== LINES: " << m_document.firstBlock().lineCount();
    m_visualEditor->repaintLines();
}

void TextBasedEdit::deleteItem()
{
    QTextCursor cursor = m_visualEditor->textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    if (end > start) {
        QTextDocumentFragment fragment = cursor.selection();
        QString anchorStart = m_visualEditor->selectionStartAnchor(cursor, start, end);
        cursor.setPosition(end);
        bool blockEnd = cursor.atBlockEnd();
        cursor = m_visualEditor->textCursor();
        QString anchorEnd = m_visualEditor->selectionEndAnchor(cursor, end, start);
        qDebug() << "=== FINAL END CUT: " << end;
        qDebug() << "=== GOT END ANCHOR: " << cursor.selectedText() << " = " << anchorEnd;
        if (!anchorStart.isEmpty() && !anchorEnd.isEmpty()) {
            double startMs = anchorStart.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 0, 0).toDouble();
            double endMs = anchorEnd.section(QLatin1Char('#'), 1).section(QLatin1Char(':'), 1, 1).toDouble();
            if (startMs < endMs) {
                Fun redo = [this, start, end, startMs, endMs, blockEnd]() {
                    QTextCursor tCursor = m_visualEditor->textCursor();
                    tCursor.setPosition(start);
                    tCursor.setPosition(end, QTextCursor::KeepAnchor);
                    m_visualEditor->cutZones << QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps()));
                    tCursor.removeSelectedText();
                    if (blockEnd) {
                        tCursor.deleteChar();
                    }
                    // Reset selection and rebuild line numbers
                    m_visualEditor->rebuildZones();
                    previewPlaylist(false);
                    return true;
                };
                Fun undo = [this, start, fr = fragment, startMs, endMs]() {
                    qDebug() << "::: PASTING FRAGMENT: " << fr.toPlainText();
                    QTextCursor tCursor = m_visualEditor->textCursor();
                    QPoint p(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps()));
                    int ix = m_visualEditor->cutZones.indexOf(p);
                    if (ix > -1) {
                        m_visualEditor->cutZones.remove(ix);
                    }
                    tCursor.setPosition(start);
                    tCursor.insertFragment(fr);
                    // Reset selection and rebuild line numbers
                    m_visualEditor->rebuildZones();
                    previewPlaylist(false);
                    return true;
                };
                redo();
                pCore->pushUndo(undo, redo, i18nc("@action", "Edit clip text"));
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
}

void TextBasedEdit::insertToTimeline()
{
    QVector<QPoint> zones = m_visualEditor->getInsertZones();
    if (zones.isEmpty()) {
        return;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (auto &zone : zones) {
        pCore->window()->getCurrentTimeline()->controller()->insertZone(m_binId, zone, false, undo, redo);
    }
    pCore->pushUndo(undo, redo, i18nc("@action", "Create sequence clip"));
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
    for (const auto &p : qAsConst(zones)) {
        playZones << QString("%1:%2").arg(p.x()).arg(p.y());
    }
    properties.insert(QStringLiteral("kdenlive:cutzones"), playZones.join(QLatin1Char(';')));
    if (createNew) {
        int ix = 1;
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

void TextBasedEdit::createSequence()
{
    QVector<QPoint> zones = m_visualEditor->fullExport();
    if (zones.isEmpty()) {
        showMessage(i18n("No text to export"), KMessageWidget::Information);
        return;
    }
    QVector<QPoint> mergedZones;
    int max = zones.count();
    int current = 0;
    int referenceStart = zones.at(current).x();
    int currentEnd = zones.at(current).y();
    int nextStart = 0;
    current++;
    while (current < max) {
        nextStart = zones.at(current).x();
        if (nextStart == currentEnd || nextStart == currentEnd + 1) {
            // Contiguous zones
            currentEnd = zones.at(current).y();
            current++;
            continue;
        }
        // Insert zone
        mergedZones << QPoint(referenceStart, currentEnd);
        referenceStart = zones.at(current).x();
        currentEnd = zones.at(current).y();
        current++;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Create new timeline sequence
    std::shared_ptr<AbstractProjectItem> clip = pCore->projectItemModel()->getItemByBinId(m_binId);
    std::shared_ptr<ProjectClip> clipItem = std::static_pointer_cast<ProjectClip>(clip);
    const QString sequenceId = pCore->bin()->buildSequenceClipWithUndo(undo, redo, -1, -1, clipItem->clipName());
    if (sequenceId == QLatin1String("-1")) {
        // Aborting
        return;
    }
    for (const auto &p : qAsConst(zones)) {
        if (p.y() > p.x()) {
            pCore->window()->getCurrentTimeline()->controller()->insertZone(m_binId, p, false, undo, redo);
        }
    }
    pCore->pushUndo(undo, redo, i18nc("@action", "Create sequence clip"));
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
    if (clip && clip->isValid() && clip->hasAudio()) {
        qDebug() << "====== OPENING CLIP: " << clip->clipName();
        QString refId = clip->getProducerProperty(QStringLiteral("kdenlive:baseid"));
        if (!refId.isEmpty() && refId == m_refId) {
            // We opened a resulting playlist, do not clear text edit
            // TODO: this is broken. We should try reading the kdenlive:speech data from the sequence xml
            return;
        }
        if (!m_visualEditor->toPlainText().isEmpty()) {
            m_visualEditor->cleanup();
        }
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
        if (speech.isEmpty()) {
            // Nothing else to do
            button_insert->setEnabled(false);
            button_start->setEnabled(true);
            return;
        }
        m_visualEditor->insertHtml(speech);
        if (!cutZones.isEmpty()) {
            m_visualEditor->processCutZones(cutZones);
        }
        m_visualEditor->rebuildZones();
        button_insert->setEnabled(true);
        button_start->setEnabled(true);
    } else {
        button_start->setEnabled(false);
        clipNameLabel->clear();
        m_visualEditor->cleanup();
    }
    applyFontSize();
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
        qDebug() << "==== GOT MARKER: " << txt << ", FOR POS: " << startPos << "-" << endPos << ", MON: " << monitorPos;
        if (monitorPos > startPos && monitorPos < endPos) {
            // Monitor seek is on the selection, use the current frame
            pCore->bin()->addClipMarker(m_binId, {monitorPos}, {txt});
        } else {
            pCore->bin()->addClipMarker(m_binId, {startPos}, {txt});
        }
    } else {
        qDebug() << "==== NO CLIP FOR " << m_binId;
    }
}
