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
#include <KGlobalSettings>

#include "customruler.h"


#define INIT_VALUE 0
#define INIT_MIN_VALUE 0
#define INIT_MAX_VALUE 100
#define INIT_TINY_MARK_DISTANCE 1
#define INIT_LITTLE_MARK_DISTANCE 5
#define INIT_MIDDLE_MARK_DISTANCE (INIT_LITTLE_MARK_DISTANCE * 2)
#define INIT_BIG_MARK_DISTANCE (INIT_LITTLE_MARK_DISTANCE * 10)
#define INIT_SHOW_TINY_MARK false
#define INIT_SHOW_LITTLE_MARK true
#define INIT_SHOW_MEDIUM_MARK true
#define INIT_SHOW_BIG_MARK true
#define INIT_SHOW_END_MARK true
#define INIT_SHOW_POINTER true
#define INIT_SHOW_END_LABEL true

#define INIT_PIXEL_PER_MARK (double)10.0 /* distance between 2 base marks in pixel */
#define INIT_OFFSET (-20)
#define INIT_LENGTH_FIX true
#define INIT_END_OFFSET 0

#define FIX_WIDTH 24 /* widget width in pixel */
#define LINE_END (FIX_WIDTH - 3)
#define END_MARK_LENGTH (FIX_WIDTH - 8)
#define END_MARK_X2 LINE_END
#define END_MARK_X1 (END_MARK_X2 - END_MARK_LENGTH)
#define BIG_MARK_LENGTH (END_MARK_LENGTH*3/4)
#define BIG_MARK_X2 LINE_END
#define BIG_MARK_X1 (BIG_MARK_X2 - BIG_MARK_LENGTH)
#define MIDDLE_MARK_LENGTH (END_MARK_LENGTH/2)
#define MIDDLE_MARK_X2 LINE_END
#define MIDDLE_MARK_X1 (MIDDLE_MARK_X2 - MIDDLE_MARK_LENGTH)
#define LITTLE_MARK_LENGTH (MIDDLE_MARK_LENGTH/2)
#define LITTLE_MARK_X2 LINE_END
#define LITTLE_MARK_X1 (LITTLE_MARK_X2 - LITTLE_MARK_LENGTH)
#define BASE_MARK_LENGTH (LITTLE_MARK_LENGTH/2)
#define BASE_MARK_X2 LINE_END
#define BASE_MARK_X1 (BASE_MARK_X2 - 3) //BASE_MARK_LENGTH

#define LABEL_SIZE 9
#define END_LABEL_X 4
#define END_LABEL_Y (END_LABEL_X + LABEL_SIZE - 2)

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
    connect(addGuide, SIGNAL(triggered()), this, SLOT(slotAddGuide()));
}

// virtual
void CustomRuler::mousePressEvent(QMouseEvent * event) {
	if (event->button() == Qt::RightButton) {
		
		return;
	}
    m_view->activateMonitor();
    int pos = (int)((event->x() + offset()));
    m_moveCursor = RULER_CURSOR;
    if (event->y() > 10) {
        if (qAbs(pos - m_zoneStart * pixelPerMark() * FRAME_SIZE) < 4) m_moveCursor = RULER_START;
        else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * pixelPerMark() * FRAME_SIZE) < 4) m_moveCursor = RULER_MIDDLE;
        else if (qAbs(pos - m_zoneEnd * pixelPerMark() * FRAME_SIZE) < 4) m_moveCursor = RULER_END;
    }
    if (m_moveCursor == RULER_CURSOR)
        m_view->setCursorPos((int) pos / pixelPerMark() / FRAME_SIZE);
}

// virtual
void CustomRuler::mouseMoveEvent(QMouseEvent * event) {
    int pos = (int)((event->x() + offset()) / pixelPerMark() / FRAME_SIZE);
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
    update();
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
    update(oldpos - offset() -6, 2, 17, 16);
    update(newpos - offset() - 6, 2, 17, 16);
}

void CustomRuler::slotAddGuide() {

}

void CustomRuler::setPixelPerMark(double rate) {
    int scale = comboScale[(int) rate];
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
    //  debug ("KRuler::drawContents, %s",(horizontal==dir)?"horizontal":"vertical");

    QStylePainter p(this);
    p.setClipRect(e->rect());

    //p.fillRect(e->rect(), QBrush(QColor(200, 200, 200)));
    //kDebug()<<"RULER ZONE: "<<m_zoneStart<<", OFF: "<<offset()<<", END: "<<m_zoneEnd<<", FACTOR: "<<pixelPerMark() * FRAME_SIZE;
    int projectEnd = (int)(m_duration * pixelPerMark() * FRAME_SIZE);
    p.fillRect(QRect(- offset(), e->rect().y(), projectEnd, e->rect().height()), QBrush(QColor(245, 245, 245)));


    int zoneStart = (int)(m_zoneStart * pixelPerMark() * FRAME_SIZE);
    int zoneEnd = (int)(m_zoneEnd * pixelPerMark() * FRAME_SIZE);

    p.fillRect(QRect(zoneStart - offset(), e->rect().y() + e->rect().height() / 2, zoneEnd - zoneStart, e->rect().height() / 2), QBrush(QColor(133, 255, 143)));

    int value  = m_view->cursorPos() - offset() + 4;
    int minval = minimum();
    int maxval = maximum() + offset() - endOffset();

    //ioffsetval = value-offset;
    //    pixelpm = (int)ppm;
    //    left  = clip.left(),
    //    right = clip.right();
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
        QString lab = m_timecode.getTimecodeFromFrames((int)((f - offsetmin) / pixelPerMark() / FRAME_SIZE + 0.5));
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
    if (showPointer() && value > 0) {
        QPolygon pa(3);
        pa.setPoints(3, value - 6, 7, value + 6, 7, value, 16);
        p.setBrush(QBrush(Qt::yellow));
        p.drawPolygon(pa);
    }



}

#include "customruler.moc"
