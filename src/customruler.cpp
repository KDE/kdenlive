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

#include "definitions.h"

const int CustomRuler::comboScale[] = { 1, 2, 5, 10, 25, 50, 125, 250, 500, 725, 1500, 3000, 6000, 12000};

CustomRuler::CustomRuler(Timecode tc, CustomTrackView *parent)
        : KRuler(parent), m_timecode(tc), m_view(parent), m_duration(0) {
    setFont(KGlobalSettings::toolBarFont());
    slotNewOffset(0);
    setRulerMetricStyle(KRuler::Pixel);
    setLength(1024);
    setMaximum(1024);
    setPixelPerMark(3);
    setLittleMarkDistance(FRAME_SIZE);
    setMediumMarkDistance(FRAME_SIZE * m_timecode.fps());
    setBigMarkDistance(FRAME_SIZE * m_timecode.fps() * 60);
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
}

void CustomRuler::setZone(QPoint p) {
    m_zoneStart = p.x();
    m_zoneEnd = p.y();
    update();
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
        update();
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
    KRuler::slotNewOffset(newPos);
}

void CustomRuler::slotCursorMoved(int oldpos, int newpos) {
    update(oldpos * m_factor - offset() - 6, 2, 17, 16);
    update(newpos * m_factor - offset() - 6, 2, 17, 16);
}

void CustomRuler::setPixelPerMark(double rate) {
    int scale = comboScale[(int) rate];
    m_factor = 1.0 / (double) scale * FRAME_SIZE;
    KRuler::setPixelPerMark(1.0 / scale);
    double fend = pixelPerMark() * littleMarkDistance();
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
}

void CustomRuler::setDuration(int d) {
    m_duration = d;
    update();
}

// virtual
void CustomRuler::paintEvent(QPaintEvent *e) {
    QStylePainter p(this);
    p.setClipRect(e->rect());

    const int projectEnd = (int)(m_duration * m_factor);
    p.fillRect(QRect(0, 0, projectEnd - offset(), height()), QBrush(QColor(245, 245, 245)));

    const int zoneStart = (int)(m_zoneStart * m_factor);
    const int zoneEnd = (int)(m_zoneEnd * m_factor);

    p.fillRect(QRect(zoneStart - offset(), height() / 2, zoneEnd - zoneStart, height() / 2), QBrush(QColor(133, 255, 143)));

    const int value  = m_view->cursorPos() * m_factor - offset();
    const int minval = minimum();
    const int maxval = maximum() + offset() - endOffset();

    double f, fend,
    offsetmin = (double)(minval - offset()),
                offsetmax = (double)(maxval - offset()),
                            fontOffset = (((double)minval) > offsetmin) ? (double)minval : offsetmin;
    QRect bg = QRect((int)offsetmin, 0, (int)offsetmax, height());

    QPalette palette;
    //p.fillRect(bg, palette.light());
    // draw labels
    p.setPen(palette.dark().color());
    // draw littlemarklabel

    // draw mediummarklabel

    // draw bigmarklabel

    // draw endlabel
    /*if (d->showEndL) {
      if (d->dir == Qt::Horizontal) {
        p.translate( fontOffset, 0 );
        p.drawText( END_LABEL_X, END_LABEL_Y, d->endlabel );
      }*/

    // draw the tiny marks
    //if (showTinyMarks())
    /*{
      fend =   pixelPerMark()*tinyMarkDistance();
      if (fend > 5) for ( f=offsetmin; f<offsetmax; f+=fend ) {
          p.drawLine((int)f, BASE_MARK_X1, (int)f, BASE_MARK_X2);
      }
    }*/

    for (f = offsetmin; f < offsetmax; f += m_textSpacing) {
        QString lab = m_timecode.getTimecodeFromFrames((int)((f - offsetmin) / m_factor + 0.5));
        p.drawText((int)f + 2, LABEL_SIZE, lab);
    }

    if (showLittleMarks()) {
        // draw the little marks
        fend = pixelPerMark() * littleMarkDistance();
        if (fend > 5) for (f = offsetmin; f < offsetmax; f += fend)
                p.drawLine((int)f, LITTLE_MARK_X1, (int)f, LITTLE_MARK_X2);
    }
    if (showMediumMarks()) {
        // draw medium marks
        fend = pixelPerMark() * mediumMarkDistance();
        if (fend > 5) for (f = offsetmin; f < offsetmax; f += fend)
                p.drawLine((int)f, MIDDLE_MARK_X1, (int)f, MIDDLE_MARK_X2);
    }
    if (showBigMarks()) {
        // draw big marks
        fend = pixelPerMark() * bigMarkDistance();
        if (fend > 5) for (f = offsetmin; f < offsetmax; f += fend)
                p.drawLine((int)f, BIG_MARK_X1, (int)f, BIG_MARK_X2);
    }
    /*   if (d->showem) {
         // draw end marks
         if (d->dir == Qt::Horizontal) {
           p.drawLine(minval-d->offset, END_MARK_X1, minval-d->offset, END_MARK_X2);
           p.drawLine(maxval-d->offset, END_MARK_X1, maxval-d->offset, END_MARK_X2);
         }
         else {
           p.drawLine(END_MARK_X1, minval-d->offset, END_MARK_X2, minval-d->offset);
           p.drawLine(END_MARK_X1, maxval-d->offset, END_MARK_X2, maxval-d->offset);
         }
       }*/


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
