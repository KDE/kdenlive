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

#define FIX_WIDTH 20 /* widget width in pixel */
#define LINE_END (FIX_WIDTH - 3)
#define END_MARK_LENGTH (FIX_WIDTH - 6)
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

#define LABEL_SIZE 8
#define END_LABEL_X 4
#define END_LABEL_Y (END_LABEL_X + LABEL_SIZE - 2)

#include "definitions.h"

const int CustomRuler::comboScale[] =
	{ 1, 2, 5, 10, 25, 50, 125, 250, 500, 725, 1500, 3000, 6000,
	    12000 };

CustomRuler::CustomRuler(Timecode tc, QWidget *parent)
    : KRuler(parent), m_timecode(tc)
{
  slotNewOffset(0);
  setRulerMetricStyle(KRuler::Pixel);
  setLength(1024);
  setMaximum(1024);
  setPixelPerMark(3);
  setLittleMarkDistance (FRAME_SIZE);
  setMediumMarkDistance (FRAME_SIZE * 25);
  setBigMarkDistance (FRAME_SIZE * 25 * 60);
}

// virtual 
void CustomRuler::mousePressEvent ( QMouseEvent * event )
{
  int pos = event->x();
  slotMoveCursor( pos, true );
}

// virtual
void CustomRuler::mouseMoveEvent ( QMouseEvent * event )
{
  int pos = event->x();
  slotMoveCursor( pos, true );
}

void CustomRuler::slotMoveRuler(int newPos)
{
  int diff = offset() - newPos;
  KRuler::slotNewOffset(newPos);
  KRuler::slotNewValue(value() + diff);
}

void CustomRuler::slotMoveCursor( int _value, bool emitSignal )
{
  KRuler::slotNewValue(_value );
  m_cursorPosition= (_value + offset()) / pixelPerMark();
  if (emitSignal) emit cursorMoved(m_cursorPosition / FRAME_SIZE);
}


void CustomRuler::slotNewValue ( int _value, bool emitSignal )
{
  m_cursorPosition= _value / pixelPerMark();
  if (emitSignal) emit cursorMoved(m_cursorPosition / FRAME_SIZE);
  KRuler::slotNewValue(_value * pixelPerMark() - offset());
}

void CustomRuler::setPixelPerMark (double rate)
{
    int scale = comboScale[(int) rate];
    int newPos = m_cursorPosition * (1.0 / scale); 
    KRuler::setPixelPerMark(1.0 / scale);
    KRuler::slotNewValue( newPos );
}

// virtual 
void CustomRuler::paintEvent(QPaintEvent * /*e*/)
 {
   //  debug ("KRuler::drawContents, %s",(horizontal==dir)?"horizontal":"vertical");
 
   QStylePainter p(this);

 
   int value  = this->value();
   int minval = minimum();
   int maxval = maximum() + offset() - endOffset();

     //ioffsetval = value-offset;
     //    pixelpm = (int)ppm;
   //    left  = clip.left(),
   //    right = clip.right();
   double f, fend,
     offsetmin=(double)(minval-offset()),
     offsetmax=(double)(maxval-offset()),
     fontOffset = (((double)minval)>offsetmin)?(double)minval:offsetmin;
   QRect bg = QRect(offsetmin, 0, offsetmax, height());

   QPalette palette;
   //p.fillRect(bg, palette.light());
   // draw labels
   QFont font = p.font();
   font.setPointSize(LABEL_SIZE);
   p.setFont( font );
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
   if (showLittleMarks()) {
     // draw the little marks
     fend = pixelPerMark()*littleMarkDistance();
     if (fend > 5) for ( f=offsetmin; f<offsetmax; f+=fend ) {
         p.drawLine((int)f, LITTLE_MARK_X1, (int)f, LITTLE_MARK_X2);
	 if (fend > 60) {
	  QString lab = m_timecode.getTimecodeFromFrames((int) ((f - offsetmin) / pixelPerMark() / FRAME_SIZE + 0.5));
	  p.drawText( (int)f + 2, LABEL_SIZE, lab );
	 }
     }
   }
   if (showMediumMarks()) {
     // draw medium marks
     fend = pixelPerMark()*mediumMarkDistance();
     if (fend > 5) for ( f=offsetmin; f<offsetmax; f+=fend ) {
         p.drawLine((int)f, MIDDLE_MARK_X1, (int)f, MIDDLE_MARK_X2);
	 if (fend > 60) {
	  QString lab = m_timecode.getTimecodeFromFrames((int) ((f - offsetmin) / pixelPerMark() / FRAME_SIZE + 0.5) );
	  p.drawText( (int)f + 2, LABEL_SIZE, lab );
	 }
     }
   }
   if (showBigMarks()) {
     // draw big marks
     fend = pixelPerMark()*bigMarkDistance();
     if (fend > 5) for ( f=offsetmin; f<offsetmax; f+=fend ) {
         p.drawLine((int)f, BIG_MARK_X1, (int)f, BIG_MARK_X2);
	 if (fend > 60) {
	  QString lab = m_timecode.getTimecodeFromFrames((int) ((f - offsetmin) / pixelPerMark() / FRAME_SIZE + 0.5) );
	  p.drawText( (int)f + 2, LABEL_SIZE, lab );
	 }
	 else if (((int) (f - offsetmin)) % ((int)(fend * 5)) == 0) {
	    QString lab = m_timecode.getTimecodeFromFrames((int) ((f - offsetmin) / pixelPerMark() / FRAME_SIZE + 0.5) );
	  p.drawText( (int)f + 2, LABEL_SIZE, lab );
	 }
     }
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
 
   // draw pointer
   if (showPointer() && value > 0) {
     QPolygon pa(4);
       pa.setPoints(3, value-6, 9, value+6, 9, value/*+0*/, 16);
     p.setBrush( QBrush(Qt::yellow) );
     p.drawPolygon( pa );
   }
 
 }

#include "customruler.moc"
