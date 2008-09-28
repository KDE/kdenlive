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

#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <KIcon>
#include <KCursor>
#include <KGlobalSettings>

#include "customruler.h"


static const int FIX_WIDTH = 24; /* widget width in pixel */
static const int LINE_END = (FIX_WIDTH - 3);
static const int END_MARK_LENGTH = (FIX_WIDTH - 8);
static const int BIG_MARK_LENGTH = (END_MARK_LENGTH * 3 / 4);
static const int BIG_MARK_X2 = LINE_END;
static const int BIG_MARK_X1 = (BIG_MARK_X2 - BIG_MARK_LENGTH);
static const int MIDDLE_MARK_LENGTH = (END_MARK_LENGTH / 2);
static const int MIDDLE_MARK_X2 = LINE_END;
static const int MIDDLE_MARK_X1 = (MIDDLE_MARK_X2 - MIDDLE_MARK_LENGTH);
static const int LITTLE_MARK_LENGTH = (MIDDLE_MARK_LENGTH / 2);
static const int LITTLE_MARK_X2 = LINE_END;
static const int LITTLE_MARK_X1 = (LITTLE_MARK_X2 - LITTLE_MARK_LENGTH);

static const int LABEL_SIZE = 9;
static const int END_LABEL_X = 4;
static const int END_LABEL_Y = (END_LABEL_X + LABEL_SIZE - 2);

static int littleMarkDistance;
static int mediumMarkDistance;
static int bigMarkDistance;

#include "definitions.h"

const int CustomRuler::comboScale[] = { 1, 2, 5, 10, 25, 50, 125, 250, 500, 725, 1500, 3000, 6000, 12000};

CustomRuler::CustomRuler(Timecode tc, CustomTrackView *parent)
        : QWidget(parent), m_timecode(tc), m_view(parent), m_duration(0), m_offset(0) {
    setFont(KGlobalSettings::toolBarFont());
    m_scale = 3;
    littleMarkDistance = FRAME_SIZE;
    mediumMarkDistance = FRAME_SIZE * m_timecode.fps();
    bigMarkDistance = FRAME_SIZE * m_timecode.fps() * 60;
    m_zoneStart = 2 * m_timecode.fps();
    m_zoneEnd = 10 * m_timecode.fps();
    m_contextMenu = new QMenu(this);
    QAction *addGuide = m_contextMenu->addAction(KIcon("document-new"), i18n("Add Guide"));
    connect(addGuide, SIGNAL(triggered()), m_view, SLOT(slotAddGuide()));
    QAction *editGuide = m_contextMenu->addAction(KIcon("document-properties"), i18n("Edit Guide"));
    connect(editGuide, SIGNAL(triggered()), m_view, SLOT(slotEditGuide()));
    QAction *delGuide = m_contextMenu->addAction(KIcon("edit-delete"), i18n("Delete Guide"));
    connect(delGuide, SIGNAL(triggered()), m_view, SLOT(slotDeleteGuide()));
    QAction *delAllGuides = m_contextMenu->addAction(KIcon("edit-delete"), i18n("Delete All Guides"));
    connect(delAllGuides, SIGNAL(triggered()), m_view, SLOT(slotDeleteAllGuides()));
    setMouseTracking(true);
    setMinimumHeight(20);
}

void CustomRuler::setZone(QPoint p) {
    int min = qMin(m_zoneStart, p.x());
    int max = qMax(m_zoneEnd, p.y());
    m_zoneStart = p.x();
    m_zoneEnd = p.y();
    update(min * m_factor - 2, 0, (max - min) * m_factor + 4, height());
}

// virtual
void CustomRuler::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::RightButton) {
        m_contextMenu->exec(event->globalPos());
        return;
    }
    m_view->activateMonitor();
    int pos = (int)((event->x() + offset()));
    m_moveCursor = RULER_CURSOR;
    if (event->y() > 10) {
        if (qAbs(pos - m_zoneStart * m_factor) < 4) m_moveCursor = RULER_START;
        else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * m_factor) < 4) m_moveCursor = RULER_MIDDLE;
        else if (qAbs(pos - m_zoneEnd * m_factor) < 4) m_moveCursor = RULER_END;
    }
    if (m_moveCursor == RULER_CURSOR)
        m_view->setCursorPos((int) pos / m_factor);
}

// virtual
void CustomRuler::mouseMoveEvent(QMouseEvent * event) {
    if (event->buttons() == Qt::LeftButton) {
        int pos = (int)((event->x() + offset()) / m_factor);
        int zoneStart = m_zoneStart;
        int zoneEnd = m_zoneEnd;
        if (pos < 0) pos = 0;
        if (m_moveCursor == RULER_CURSOR) {
            m_view->setCursorPos(pos);
            return;
        } else if (m_moveCursor == RULER_START) m_zoneStart = pos;
        else if (m_moveCursor == RULER_END) m_zoneEnd = pos;
        else if (m_moveCursor == RULER_MIDDLE) {
            int move = pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2);
            m_zoneStart += move;
            m_zoneEnd += move;
        }
        emit zoneMoved(m_zoneStart, m_zoneEnd);
        m_view->setDocumentModified();

        int min = qMin(m_zoneStart, zoneStart);
        int max = qMax(m_zoneEnd, zoneEnd);
        update(min * m_factor - m_offset - 2, 0, (max - min) * m_factor + 4, height());

    } else {
        int pos = (int)((event->x() + offset()));
        if (event->y() <= 10) setCursor(Qt::ArrowCursor);
        else if (qAbs(pos - m_zoneStart * m_factor) < 4) setCursor(KCursor("left_side", Qt::SizeHorCursor));
        else if (qAbs(pos - m_zoneEnd * m_factor) < 4) setCursor(KCursor("right_side", Qt::SizeHorCursor));
        else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * m_factor) < 4) setCursor(Qt::SizeHorCursor);
        else setCursor(Qt::ArrowCursor);
    }
}


// virtual
void CustomRuler::wheelEvent(QWheelEvent * e) {
    int delta = 1;
    if (e->modifiers() == Qt::ControlModifier) delta = m_timecode.fps();
    if (e->delta() < 0) delta = 0 - delta;
    m_view->moveCursorPos(delta);
}

int CustomRuler::inPoint() const {
    return m_zoneStart;
}

int CustomRuler::outPoint() const {
    return m_zoneEnd;
}

void CustomRuler::slotMoveRuler(int newPos) {
    m_offset = newPos;
    update();
}

int CustomRuler::offset() const {
    return m_offset;
}

void CustomRuler::slotCursorMoved(int oldpos, int newpos) {
    if (qAbs(oldpos - newpos) * m_factor > 40) {
        update(oldpos * m_factor - offset() - 6, 7, 17, 16);
        update(newpos * m_factor - offset() - 6, 7, 17, 16);
    } else update(qMin(oldpos, newpos) * m_factor - offset() - 6, 7, qAbs(oldpos - newpos) * m_factor + 17, 16);
}

void CustomRuler::setPixelPerMark(double rate) {
    int scale = comboScale[(int) rate];
    m_factor = 1.0 / (double) scale * FRAME_SIZE;
    m_scale = 1.0 / (double) scale;
    double fend = m_scale * littleMarkDistance;
    switch ((int) rate) {
    case 0:
        m_textSpacing = fend;
        break;
    case 1:
        m_textSpacing = fend * 5;
        break;
    case 2:
    case 3:
    case 4:
        m_textSpacing = fend * m_timecode.fps();
        break;
    case 5:
        m_textSpacing = fend * m_timecode.fps() * 5;
        break;
    case 6:
        m_textSpacing = fend * m_timecode.fps() * 10;
        break;
    case 7:
        m_textSpacing = fend * m_timecode.fps() * 30;
        break;
    case 8:
    case 9:
    case 10:
        m_textSpacing = fend * m_timecode.fps() * 60;
        break;
    case 11:
    case 12:
        m_textSpacing = fend * m_timecode.fps() * 300;
        break;
    case 13:
        m_textSpacing = fend * m_timecode.fps() * 600;
        break;
    }
    update();
}

void CustomRuler::setDuration(int d) {
    int oldduration = m_duration;
    m_duration = d;
    update(qMin(oldduration, m_duration) * m_factor - 1, 0, qAbs(oldduration - m_duration) * m_factor + 2, height());
}

// virtual
void CustomRuler::paintEvent(QPaintEvent *e) {
    QStylePainter p(this);
    p.setClipRect(e->rect());

    const int projectEnd = (int)(m_duration * m_factor);
    p.fillRect(QRect(0, 0, projectEnd - m_offset, height()), QBrush(QColor(245, 245, 245)));

    const int zoneStart = (int)(m_zoneStart * m_factor);
    const int zoneEnd = (int)(m_zoneEnd * m_factor);

    p.fillRect(QRect(zoneStart - offset(), height() / 2, zoneEnd - zoneStart, height() / 2), QBrush(QColor(133, 255, 143)));

    const int value  = m_view->cursorPos() * m_factor - offset();
    int minval = (e->rect().left() + m_offset) / FRAME_SIZE - 1;
    const int maxval = (e->rect().right() + m_offset) / FRAME_SIZE + 1;
    if (minval < 0) minval = 0;

    double f, fend;
    const int offsetmax = maxval * FRAME_SIZE;

    QPalette palette;
    p.setPen(palette.dark().color());
    int offsetmin = (e->rect().left() + m_offset) / m_textSpacing;
    offsetmin = offsetmin * m_textSpacing;
    for (f = offsetmin; f < offsetmax; f += m_textSpacing) {
        QString lab = m_timecode.getTimecodeFromFrames((int)((f) / m_factor + 0.5));
        p.drawText((int)f - m_offset + 2, LABEL_SIZE, lab);
    }

    if (true) {
        offsetmin = (e->rect().left() + m_offset) / littleMarkDistance;
        offsetmin = offsetmin * littleMarkDistance;
        // draw the little marks
        fend = m_scale * littleMarkDistance;
        if (fend > 5) for (f = offsetmin - m_offset; f < offsetmax - m_offset; f += fend)
                p.drawLine((int)f, LITTLE_MARK_X1, (int)f, LITTLE_MARK_X2);
    }
    if (true) {
        offsetmin = (e->rect().left() + m_offset) / mediumMarkDistance;
        offsetmin = offsetmin * mediumMarkDistance;
        // draw medium marks
        fend = m_scale * mediumMarkDistance;
        if (fend > 5) for (f = offsetmin - m_offset - fend; f < offsetmax - m_offset + fend; f += fend)
                p.drawLine((int)f, MIDDLE_MARK_X1, (int)f, MIDDLE_MARK_X2);
    }
    if (true) {
        offsetmin = (e->rect().left() + m_offset) / bigMarkDistance;
        offsetmin = offsetmin * bigMarkDistance;
        // draw big marks
        fend = m_scale * bigMarkDistance;
        if (fend > 5) for (f = offsetmin - m_offset; f < offsetmax - m_offset; f += fend)
                p.drawLine((int)f, BIG_MARK_X1, (int)f, BIG_MARK_X2);
    }

    // draw zone cursors
    int off = offset();
    if (zoneStart > 0) {
        QPolygon pa(4);
        pa.setPoints(4, zoneStart - off + 3, 9, zoneStart - off, 9, zoneStart - off, 18, zoneStart - off + 3, 18);
        p.drawPolyline(pa);
    }

    if (zoneEnd > 0) {
        QRect rec(zoneStart - off + (zoneEnd - zoneStart) / 2 - 4, 9, 8, 9);
        p.fillRect(rec, QColor(255, 255, 255, 150));
        p.drawRect(rec);

        QPolygon pa(4);
        pa.setPoints(4, zoneEnd - off - 3, 9, zoneEnd - off, 9, zoneEnd - off, 18, zoneEnd - off - 3, 18);
        p.drawPolyline(pa);
    }

    // draw pointer
    QPolygon pa(3);
    pa.setPoints(3, value - 6, 7, value + 6, 7, value, 16);
    p.setBrush(QBrush(Qt::yellow));
    p.drawPolygon(pa);
}

#include "customruler.moc"
