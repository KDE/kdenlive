/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "customruler.h"

#include "kdenlivesettings.h"

#include <QIcon>
#include <klocalizedstring.h>
#include <QFontDatabase>
#include <KColorScheme>

#include <QCursor>
#include <QApplication>
#include <QMouseEvent>
#include <QStylePainter>

// Height of the ruler display
static int MAX_HEIGHT;
// Height of the ruler display + timeline preview ruler
static int FULL_HEIGHT;
// Width of a frame in pixels
static int FRAME_SIZE;
// Height of the timecode text
static int LABEL_SIZE;
// Height of the timeline preview
static int PREVIEW_SIZE;
// Width of a letter, used for cursor width
static int FONT_WIDTH;

static int MIDDLE_MARK_X;
static int LITTLE_MARK_X;

static int littleMarkDistance;
static int mediumMarkDistance;
static int bigMarkDistance;

#define SEEK_INACTIVE (-1)

#include "definitions.h"

const int CustomRuler::comboScale[] = { 1, 2, 5, 10, 25, 50, 125, 250, 500, 750, 1500, 3000, 6000, 12000};

CustomRuler::CustomRuler(const Timecode &tc, const QList<QAction *> &rulerActions, CustomTrackView *parent) :
    QWidget(parent),
    m_timecode(tc),
    m_view(parent),
    m_duration(0),
    m_offset(0),
    m_hidePreview(true),
    m_headPosition(SEEK_INACTIVE),
    m_clickedGuide(-1),
    m_rate(-1),
    m_mouseMove(NO_MOVE)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QFontMetricsF fontMetrics(font());
    // Define size variables
    LABEL_SIZE = fontMetrics.ascent();
    FONT_WIDTH = fontMetrics.averageCharWidth();
    PREVIEW_SIZE = LABEL_SIZE / 3;
    setFixedHeight(LABEL_SIZE * 2);
    MAX_HEIGHT = height() - 1;
    FULL_HEIGHT = MAX_HEIGHT;
    int mark_length = MAX_HEIGHT - LABEL_SIZE - 1;
    MIDDLE_MARK_X = LABEL_SIZE + mark_length / 3;
    LITTLE_MARK_X = LABEL_SIZE + mark_length / 2;
    updateFrameSize();
    m_scale = 3;
    m_zoneStart = 0;
    m_zoneEnd = 100;
    m_contextMenu = new QMenu(this);
    m_contextMenu->addActions(rulerActions);
    QAction *addGuide = m_contextMenu->addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Add Guide"));
    connect(addGuide, &QAction::triggered, m_view, &CustomTrackView::slotAddGuide);
    m_editGuide = m_contextMenu->addAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18n("Edit Guide"));
    connect(m_editGuide, &QAction::triggered, this, &CustomRuler::slotEditGuide);
    m_deleteGuide = m_contextMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete Guide"));
    connect(m_deleteGuide, &QAction::triggered, this, &CustomRuler::slotDeleteGuide);
    QAction *delAllGuides = m_contextMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete All Guides"));
    connect(delAllGuides, &QAction::triggered, m_view, &CustomTrackView::slotDeleteAllGuides);
    m_goMenu = m_contextMenu->addMenu(i18n("Go To"));
    connect(m_goMenu, &QMenu::triggered, this, &CustomRuler::slotGoToGuide);
    setMouseTracking(true);
    m_zoneBG = palette().color(QPalette::Highlight);
    m_zoneBG.setAlpha(KdenliveSettings::useTimelineZoneToEdit() ? 180 : 60);
}

void CustomRuler::updateProjectFps(const Timecode &t)
{
    m_timecode = t;
    mediumMarkDistance = FRAME_SIZE * m_timecode.fps();
    bigMarkDistance = FRAME_SIZE * m_timecode.fps() * 60;
    setPixelPerMark(m_rate);
    update();
}

void CustomRuler::updateFrameSize()
{
    FRAME_SIZE = m_view->getFrameWidth();
    littleMarkDistance = FRAME_SIZE;
    mediumMarkDistance = FRAME_SIZE * m_timecode.fps();
    bigMarkDistance = FRAME_SIZE * m_timecode.fps() * 60;
    updateProjectFps(m_timecode);
    if (m_rate > 0) {
        setPixelPerMark(m_rate);
    }
}

void CustomRuler::slotEditGuide()
{
    m_view->slotEditGuide(m_clickedGuide);
}

void CustomRuler::slotDeleteGuide()
{
    m_view->slotDeleteGuide(m_clickedGuide);
}

void CustomRuler::slotGoToGuide(QAction *act)
{
    m_view->seekCursorPos(act->data().toInt());
    m_view->initCursorPos(act->data().toInt());
}

void CustomRuler::setZone(const QPoint &p)
{
    m_zoneStart = p.x();
    m_zoneEnd = p.y();
    update();
}

void CustomRuler::mouseReleaseEvent(QMouseEvent *event)
{
    event->setAccepted(true);
    if (m_moveCursor == RULER_START || m_moveCursor == RULER_END || m_moveCursor == RULER_MIDDLE) {
        emit zoneMoved(m_zoneStart, m_zoneEnd);
        m_view->setDocumentModified();
    }
    m_mouseMove = NO_MOVE;

}

// virtual
void CustomRuler::mousePressEvent(QMouseEvent *event)
{
    event->setAccepted(true);
    int pos = (int)((event->x() + offset()));
    if (event->button() == Qt::RightButton) {
        m_clickedGuide = m_view->hasGuide(pos, false);
        m_editGuide->setEnabled(m_clickedGuide > 0);
        m_deleteGuide->setEnabled(m_clickedGuide > 0);
        m_view->buildGuidesMenu(m_goMenu);
        m_contextMenu->exec(event->globalPos());
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_view->activateMonitor();
    m_moveCursor = RULER_CURSOR;
    if (event->y() > LABEL_SIZE) {
        if (qAbs(pos - m_zoneStart * m_factor) < 4) {
            m_moveCursor = RULER_START;
        } else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2.0) * m_factor) < 4) {
            m_moveCursor = RULER_MIDDLE;
        } else if (qAbs(pos - (m_zoneEnd + 1)* m_factor) < 4) {
            m_moveCursor = RULER_END;
        }
        m_view->updateSnapPoints(nullptr);
    }
    if (m_moveCursor == RULER_CURSOR) {
        m_view->seekCursorPos((int) pos / m_factor);
        m_clickPoint = event->pos();
        m_startRate = m_rate;
    }
}

// virtual
void CustomRuler::mouseMoveEvent(QMouseEvent *event)
{
    event->setAccepted(true);
    int mappedXPos = (int)((event->x() + offset()) / m_factor);
    emit mousePosition(mappedXPos);
    if (event->buttons() == Qt::LeftButton) {
        int pos;
        if (m_moveCursor == RULER_START || m_moveCursor == RULER_END) {
            pos = m_view->getSnapPointForPos(mappedXPos);
        } else {
            pos = mappedXPos;
        }
        int zoneStart = m_zoneStart;
        int zoneEnd = m_zoneEnd;
        if (pos < 0) {
            pos = 0;
        }
        if (m_moveCursor == RULER_CURSOR) {
            QPoint diff = event->pos() - m_clickPoint;
            if (m_mouseMove == NO_MOVE) {
                if (qAbs(diff.x()) >= QApplication::startDragDistance()) {
                    m_mouseMove = HORIZONTAL_MOVE;
                } else if (KdenliveSettings::verticalzoom() && qAbs(diff.y()) >= QApplication::startDragDistance()) {
                    m_mouseMove = VERTICAL_MOVE;
                } else {
                    return;
                }
            }
            if (m_mouseMove == HORIZONTAL_MOVE) {
                if (pos != m_headPosition && pos != m_view->cursorPos()) {
                    int x = m_headPosition == SEEK_INACTIVE ? pos : m_headPosition;
                    m_headPosition = pos;
                    int min = qMin(x, m_headPosition);
                    int max = qMax(x, m_headPosition);
                    update(min * m_factor - offset() - 3, LABEL_SIZE, (max - min) * m_factor + 6, FULL_HEIGHT - LABEL_SIZE);
                    emit seekCursorPos(pos);
                    m_view->slotCheckPositionScrolling();
                }
            } else {
                int verticalDiff = m_startRate - (diff.y()) / 7;
                if (verticalDiff != m_rate) {
                    emit adjustZoom(verticalDiff);
                }
            }
            return;
        } else if (m_moveCursor == RULER_START) {
            m_zoneStart = qMin(pos, m_zoneEnd);
        } else if (m_moveCursor == RULER_END) {
            m_zoneEnd = qMax(pos, m_zoneStart);
        } else if (m_moveCursor == RULER_MIDDLE) {
            int move = pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2);
            if (move + m_zoneStart < 0) {
                move = - m_zoneStart;
            }
            m_zoneStart += move;
            m_zoneEnd += move;
        }
        int min = qMin(m_zoneStart, zoneStart);
        int max = qMax(m_zoneEnd, zoneEnd);
        update(min * m_factor - m_offset - 2, LABEL_SIZE, (max - min + 1) * m_factor + 4, FULL_HEIGHT - LABEL_SIZE);

    } else {
        int pos = (int)((event->x() + m_offset));
        if (event->y() <= LABEL_SIZE) {
            setCursor(Qt::ArrowCursor);
        } else if (qAbs(pos - m_zoneStart * m_factor) < 4) {
            setCursor(QCursor(Qt::SizeHorCursor));
            if (KdenliveSettings::frametimecode()) {
                setToolTip(i18n("Zone start: %1", m_zoneStart));
            } else {
                setToolTip(i18n("Zone start: %1", m_timecode.getTimecodeFromFrames(m_zoneStart)));
            }
        } else if (qAbs(pos - (m_zoneEnd + 1) * m_factor) < 4) {
            setCursor(QCursor(Qt::SizeHorCursor));
            if (KdenliveSettings::frametimecode()) {
                setToolTip(i18n("Zone end: %1", m_zoneEnd));
            } else {
                setToolTip(i18n("Zone end: %1", m_timecode.getTimecodeFromFrames(m_zoneEnd)));
            }
        } else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2.0) * m_factor) < 4) {
            setCursor(Qt::SizeHorCursor);
            if (KdenliveSettings::frametimecode()) {
                setToolTip(i18n("Zone duration: %1", m_zoneEnd - m_zoneStart));
            } else {
                setToolTip(i18n("Zone duration: %1", m_timecode.getTimecodeFromFrames(m_zoneEnd - m_zoneStart)));
            }
        } else {
            setCursor(Qt::ArrowCursor);
            if (KdenliveSettings::frametimecode()) {
                setToolTip(i18n("Position: %1", (int)(pos / m_factor)));
            } else {
                setToolTip(i18n("Position: %1", m_timecode.getTimecodeFromFrames(pos / m_factor)));
            }
        }
    }
}

// virtual
void CustomRuler::wheelEvent(QWheelEvent *e)
{
    int delta = 1;
    m_view->activateMonitor();
    if (e->modifiers() == Qt::ControlModifier) {
        delta = m_timecode.fps();
    }
    if (e->delta() < 0) {
        delta = 0 - delta;
    }
    m_view->moveCursorPos(delta);
}

int CustomRuler::inPoint() const
{
    return m_zoneStart;
}

int CustomRuler::outPoint() const
{
    return m_zoneEnd;
}

void CustomRuler::slotMoveRuler(int newPos)
{
    if (m_offset != newPos) {
        m_offset = newPos;
        update();
    }
}

int CustomRuler::offset() const
{
    return m_offset;
}

void CustomRuler::slotCursorMoved(int oldpos, int newpos)
{
    int min = qMin(oldpos, newpos);
    int max = qMax(oldpos, newpos);
    m_headPosition = newpos;
    update(min * m_factor - m_offset - FONT_WIDTH, LABEL_SIZE, (max - min) * m_factor + FONT_WIDTH * 2 + 2, FULL_HEIGHT - LABEL_SIZE);
}

void CustomRuler::updateRuler(int pos)
{
    int x = m_headPosition;
    m_headPosition = pos;
    if (x == SEEK_INACTIVE) {
        x = pos;
    }
    int min = qMin(x, m_headPosition);
    int max = qMax(x, m_headPosition);
    update(min * m_factor - offset() - 3, LABEL_SIZE, (max - min) * m_factor + 6, FULL_HEIGHT - LABEL_SIZE);
}

void CustomRuler::setPixelPerMark(int rate, bool force)
{
    if (rate < 0 || (rate == m_rate && !force)) {
        return;
    }
    int scale = comboScale[rate];
    m_rate = rate;
    m_factor = 1.0 / (double) scale * FRAME_SIZE;
    m_scale = 1.0 / (double) scale;
    double fend = m_scale * littleMarkDistance;
    int textFactor = 1;
    int timeLabelSize = QWidget::fontMetrics().boundingRect(QStringLiteral("00:00:00:000")).width();
    if (timeLabelSize > littleMarkDistance) {
        textFactor = timeLabelSize / littleMarkDistance + 1;
    }
    if (rate > 8) {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 60;
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 300;
    } else if (rate > 6) {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 10;
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 30;
    } else if (rate > 3) {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps();
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 5;
    } else {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps();
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 60;
    }

    m_textSpacing = fend * textFactor;

    if (m_textSpacing < timeLabelSize) {
        int roundedFps = (int)(m_timecode.fps() + 0.5);
        int factor = timeLabelSize / m_textSpacing;
        if (factor < 2) {
            m_textSpacing *= 2;
        } else if (factor < 5) {
            m_textSpacing *= 5;
        } else if (factor < 10) {
            m_textSpacing *= 10;
        } else if (factor < roundedFps) {
            m_textSpacing *= roundedFps;
        } else if (factor < 2 * roundedFps) {
            m_textSpacing *= 2 * roundedFps;
        } else if (factor < 5 * roundedFps) {
            m_textSpacing *= 5 * roundedFps;
        } else if (factor < 10 * roundedFps) {
            m_textSpacing *= 10 * roundedFps;
        } else if (factor < 20 * roundedFps) {
            m_textSpacing *= 20 * roundedFps;
        } else if (factor < 60 * roundedFps) {
            m_textSpacing *= 60 * roundedFps;
        } else if (factor < 120 * roundedFps) {
            m_textSpacing *= 120 * roundedFps;
        } else if (factor < 150 * roundedFps) {
            m_textSpacing *= 150 * roundedFps;
        } else if (factor < 300 * roundedFps) {
            m_textSpacing *= 300 * roundedFps;
        } else {
            factor /= (300 * roundedFps);
            m_textSpacing *= (factor + 1) * (300 * roundedFps);
        }
    }
    update();
}

void CustomRuler::setDuration(int d)
{
    int oldduration = m_duration;
    m_duration = d;
    update(qMin(oldduration, m_duration) * m_factor - 1 - offset(), 0, qAbs(oldduration - m_duration) * m_factor + 2, FULL_HEIGHT);
}

// virtual
void CustomRuler::paintEvent(QPaintEvent *e)
{
    QStylePainter p(this);
    const QRect &paintRect = e->rect();
    p.setClipRect(paintRect);
    p.fillRect(paintRect, palette().midlight().color());

    // Draw zone background
    const int zoneStart = (int)(m_zoneStart * m_factor);
    const int zoneEnd = (int)((m_zoneEnd + 1) * m_factor);
    int zoneHeight = LABEL_SIZE * 0.8;
    p.fillRect(zoneStart - m_offset, MAX_HEIGHT - zoneHeight + 1, zoneEnd - zoneStart, zoneHeight - 1, m_zoneBG);

    double f, fend;
    const int offsetmax = ((paintRect.right() + m_offset) / FRAME_SIZE + 1) * FRAME_SIZE;
    int offsetmin;

    p.setPen(palette().text().color());
    // draw time labels
    if (paintRect.y() < LABEL_SIZE) {
        offsetmin = (paintRect.left() + m_offset) / m_textSpacing;
        offsetmin = offsetmin * m_textSpacing;
        for (f = offsetmin; f < offsetmax; f += m_textSpacing) {
            QString lab;
            if (KdenliveSettings::frametimecode()) {
                lab = QString::number((int)(f / m_factor + 0.5));
            } else {
                lab = m_timecode.getTimecodeFromFrames((int)(f / m_factor + 0.5));
            }
            p.drawText(f - m_offset + 2, LABEL_SIZE, lab);
        }
    }
    p.setPen(palette().dark().color());
    offsetmin = (paintRect.left() + m_offset) / littleMarkDistance;
    offsetmin = offsetmin * littleMarkDistance;
    // draw the little marks
    fend = m_scale * littleMarkDistance;
    if (fend > 5) {
        QLineF l(offsetmin - m_offset, LITTLE_MARK_X, offsetmin - m_offset, MAX_HEIGHT);
        for (f = offsetmin; f < offsetmax; f += fend) {
            l.translate(fend, 0);
            p.drawLine(l);
        }
    }

    offsetmin = (paintRect.left() + m_offset) / mediumMarkDistance;
    offsetmin = offsetmin * mediumMarkDistance;
    // draw medium marks
    fend = m_scale * mediumMarkDistance;
    if (fend > 5) {
        QLineF l(offsetmin - m_offset - fend, MIDDLE_MARK_X, offsetmin - m_offset - fend, MAX_HEIGHT);
        for (f = offsetmin - fend; f < offsetmax + fend; f += fend) {
            l.translate(fend, 0);
            p.drawLine(l);
        }
    }

    offsetmin = (paintRect.left() + m_offset) / bigMarkDistance;
    offsetmin = offsetmin * bigMarkDistance;
    // draw big marks
    fend = m_scale * bigMarkDistance;
    if (fend > 5) {
        QLineF l(offsetmin - m_offset, LABEL_SIZE, offsetmin - m_offset, MAX_HEIGHT);
        for (f = offsetmin; f < offsetmax; f += fend) {
            l.translate(fend, 0);
            p.drawLine(l);
        }
    }
    // draw zone handles
    if (zoneStart > 0) {
        QPolygon pa(4);
        pa.setPoints(4, zoneStart - m_offset + FONT_WIDTH / 2, MAX_HEIGHT - zoneHeight, zoneStart - m_offset, MAX_HEIGHT - zoneHeight, zoneStart - m_offset, MAX_HEIGHT, zoneStart - m_offset + FONT_WIDTH / 2, MAX_HEIGHT);
        p.drawPolyline(pa);
    }

    if (zoneEnd > 0) {
        QColor center(Qt::white);
        center.setAlpha(150);
        QRect rec(zoneStart - m_offset + (zoneEnd - zoneStart - zoneHeight) / 2 + 2, MAX_HEIGHT - zoneHeight + 2, zoneHeight - 4, zoneHeight - 4);
        p.fillRect(rec, center);
        p.drawRect(rec);

        QPolygon pa(4);
        pa.setPoints(4, zoneEnd - m_offset - FONT_WIDTH / 2, MAX_HEIGHT - zoneHeight, zoneEnd - m_offset, MAX_HEIGHT - zoneHeight, zoneEnd - m_offset, MAX_HEIGHT, zoneEnd - m_offset - FONT_WIDTH / 2, MAX_HEIGHT);
        p.drawPolyline(pa);
    }

    // draw Rendering preview zones
    if (!m_hidePreview) {
        p.setPen(palette().dark().color());
        p.drawLine(paintRect.left(), MAX_HEIGHT, paintRect.right(), MAX_HEIGHT);
        p.fillRect(paintRect.left(), MAX_HEIGHT + 1, paintRect.width(), PREVIEW_SIZE - 1, palette().mid().color());
        QColor preview(Qt::green);
        preview.setAlpha(120);
        double chunkWidth = KdenliveSettings::timelinechunks() * m_factor;
        foreach (int frame, m_renderingPreviews) {
            double xPos = frame * m_factor  - m_offset;
            if (xPos + chunkWidth < paintRect.x() || xPos > paintRect.right()) {
                continue;
            }
            QRectF rec(xPos, MAX_HEIGHT + 1, chunkWidth, PREVIEW_SIZE - 1);
            p.fillRect(rec, preview);
        }
        preview = QColor(200, 0, 0);
        preview.setAlpha(120);
        foreach (int frame, m_dirtyRenderingPreviews) {
            double xPos = frame * m_factor  - m_offset;
            if (xPos + chunkWidth < paintRect.x() || xPos > paintRect.right()) {
                continue;
            }
            QRectF rec(xPos, MAX_HEIGHT + 1, chunkWidth, PREVIEW_SIZE - 1);
            p.fillRect(rec, preview);
        }
        preview = palette().dark().color();
        preview.setAlpha(70);
        p.fillRect(paintRect.left(), MAX_HEIGHT + 1, paintRect.width(), 2, preview);
        p.drawLine(paintRect.left(), MAX_HEIGHT + PREVIEW_SIZE, paintRect.right(), MAX_HEIGHT + PREVIEW_SIZE);
    }

    if (m_headPosition == m_view->cursorPos()) {
        m_headPosition = SEEK_INACTIVE;
    }
    if (m_headPosition != SEEK_INACTIVE) {
        p.fillRect(m_headPosition * m_factor - m_offset - 1, LABEL_SIZE + 1, 3, FULL_HEIGHT - LABEL_SIZE - 1, palette().linkVisited());
    }

    // draw pointer
    const int value = m_view->cursorPos() * m_factor - m_offset;
    QPolygon pa(3);
    pa.setPoints(3, value - FONT_WIDTH, FULL_HEIGHT - FONT_WIDTH, value + FONT_WIDTH, FULL_HEIGHT - FONT_WIDTH, value, FULL_HEIGHT);
    p.setBrush(palette().brush(QPalette::Text));
    p.setPen(Qt::NoPen);
    p.drawPolygon(pa);
}

void CustomRuler::activateZone()
{
    m_zoneBG.setAlpha(KdenliveSettings::useTimelineZoneToEdit() ? 180 : 60);
    update();
}

bool CustomRuler::isUnderPreview(int start, int end)
{
    QList<int> allPreviews;
    allPreviews << m_renderingPreviews << m_dirtyRenderingPreviews;
    qSort(allPreviews);
    foreach (int ix, allPreviews) {
        if (ix >= start && ix <= end) {
            return true;
        }
    }
    return false;
}

bool CustomRuler::updatePreview(int frame, bool rendered, bool refresh)
{
    bool result = false;
    if (rendered) {
        m_renderingPreviews << frame;
        m_dirtyRenderingPreviews.removeAll(frame);
    } else {
        if (m_renderingPreviews.removeAll(frame) > 0) {
            m_dirtyRenderingPreviews << frame;
            result = true;
        }
    }
    std::sort(m_renderingPreviews.begin(), m_renderingPreviews.end());
    std::sort(m_dirtyRenderingPreviews.begin(), m_dirtyRenderingPreviews.end());
    if (refresh && !m_hidePreview) {
        update(frame * m_factor - offset(), MAX_HEIGHT, KdenliveSettings::timelinechunks() * m_factor + 1, PREVIEW_SIZE);
    }
    return result;
}

void CustomRuler::hidePreview(bool hide)
{
    m_hidePreview = hide;
    if (hide) {
        emit resizeRuler(MAX_HEIGHT + 1);
        setFixedHeight(MAX_HEIGHT + 1);
        //MAX_HEIGHT = height();
    } else {
        emit resizeRuler(MAX_HEIGHT + PREVIEW_SIZE + 1);
        setFixedHeight(MAX_HEIGHT + PREVIEW_SIZE + 1);
        //MAX_HEIGHT = height() - LABEL_SIZE / 3;
    }
    update();
}

void CustomRuler::updatePreviewDisplay(int start, int end)
{
    if (!m_hidePreview) {
        update(start * m_factor - offset(), MAX_HEIGHT, (end - start) * KdenliveSettings::timelinechunks() * m_factor + 1, PREVIEW_SIZE);
    }
}

const QPair <QStringList, QStringList> CustomRuler::previewChunks() const
{
    QStringList clean;
    QStringList dirty;
    foreach (int frame, m_renderingPreviews) {
        clean << QString::number(frame);
    }
    foreach (int frame, m_dirtyRenderingPreviews) {
        dirty << QString::number(frame);
    }
    QPair <QStringList, QStringList> resultChunks;
    resultChunks.first = clean;
    resultChunks.second = dirty;
    return resultChunks;
}

const QList<int> CustomRuler::getProcessedChunks() const
{
    return m_renderingPreviews;
}

const QList<int> CustomRuler::getDirtyChunks() const
{
    return m_dirtyRenderingPreviews;
}

bool CustomRuler::hasPreviewRange() const
{
    return (!m_dirtyRenderingPreviews.isEmpty() || !m_renderingPreviews.isEmpty());
}

void CustomRuler::clearChunks()
{
    m_renderingPreviews.clear();
    m_dirtyRenderingPreviews.clear();
    update();
}

QList<int> CustomRuler::addChunks(QList<int> chunks, bool add)
{
    qSort(chunks);
    QList<int> toProcess;
    if (add) {
        foreach (int frame, chunks) {
            if (m_renderingPreviews.contains(frame)) {
                // already rendered, ignore
                continue;
            }
            if (m_dirtyRenderingPreviews.contains(frame)) {
                continue;
            }
            m_dirtyRenderingPreviews << frame;
            // This is a new dirty chunk
            toProcess << frame;
        }
    } else {
        foreach (int frame, chunks) {
            if (m_renderingPreviews.removeAll(frame) > 0) {
                // A preview file existed for this chunk, ask deletion
                toProcess << frame;
            }
            m_dirtyRenderingPreviews.removeAll(frame);
        }
    }
    std::sort(m_renderingPreviews.begin(), m_renderingPreviews.end());
    std::sort(m_dirtyRenderingPreviews.begin(), m_dirtyRenderingPreviews.end());
    if (!m_hidePreview) {
        update(chunks.first() * m_factor - offset(), MAX_HEIGHT, (chunks.last() - chunks.first()) * KdenliveSettings::timelinechunks() * m_factor + 1, PREVIEW_SIZE);
    }
    return toProcess;
}

