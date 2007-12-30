
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>

#define LITTLE_MARK_X2 15
#define LITTLE_MARK_X1 13
#define MIDDLE_MARK_X1 9
#define MIDDLE_MARK_X2 15

#define LABEL_SIZE 8

#include "smallruler.h"


SmallRuler::SmallRuler(QWidget *parent)
    : KRuler(parent)
{
  setShowPointer(true);
  setShowBigMarks(false);
  setShowTinyMarks(false);
  slotNewOffset(0);
  setRulerMetricStyle(KRuler::Custom);
  setLengthFixed(true);
}

void SmallRuler::setPixelPerMark ( double rate )
{
  kDebug()<<" RULER SET RATE: "<<rate;
  if (rate > 0.5) {
    setLittleMarkDistance(25);
    setMediumMarkDistance(5 * 25);
  }
  else if (rate > 0.09) {
    setLittleMarkDistance(5 * 25);
    setMediumMarkDistance(30 * 25);
  }
  else {
    setLittleMarkDistance(30 * 25);
    setMediumMarkDistance(60 * 25);
  }
  
  KRuler::setPixelPerMark( rate );
}

// virtual 
void SmallRuler::mousePressEvent ( QMouseEvent * event )
{
  int pos = event->x();
  //slotNewValue( pos );
  emit seekRenderer(pos);
  kDebug()<<pos;
}

void SmallRuler::slotNewValue ( int _value )
{
  m_cursorPosition = _value / pixelPerMark();
  KRuler::slotNewValue(_value);
}

// virtual
void SmallRuler::paintEvent(QPaintEvent * /*e*/)
 {
   //  debug ("KRuler::drawContents, %s",(horizontal==dir)?"horizontal":"vertical");
 
   QStylePainter p(this);

 
   int value  = this->value(),
     minval = minimum(),
     maxval;
     maxval = maximum()
     + offset() - endOffset();

     //ioffsetval = value-offset;
     //    pixelpm = (int)ppm;
   //    left  = clip.left(),
   //    right = clip.right();
   double f, fend,
     offsetmin=(double)(minval-offset()),
     offsetmax=(double)(maxval-offset()),
     fontOffset = (((double)minval)>offsetmin)?(double)minval:offsetmin;
 
   // draw labels
   QFont font = p.font();
   font.setPointSize(LABEL_SIZE);
   p.setFont( font );

   if (showLittleMarks()) {
     // draw the little marks
     fend = pixelPerMark()*littleMarkDistance();
     if (fend > 2) for ( f=offsetmin; f<offsetmax; f+=fend ) {
         p.drawLine((int)f, LITTLE_MARK_X1, (int)f, LITTLE_MARK_X2);
     }
   }
   if (showMediumMarks()) {
     // draw medium marks
     fend = pixelPerMark()*mediumMarkDistance();
     if (fend > 2) for ( f=offsetmin; f<offsetmax; f+=fend ) {
         p.drawLine((int)f, MIDDLE_MARK_X1, (int)f, MIDDLE_MARK_X2);
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
   if (showPointer()) {
     QPolygon pa(4);
       pa.setPoints(3, value-6, 9, value+6, 9, value/*+0*/, 16);
     p.setBrush( QBrush(Qt::yellow) );
     p.drawPolygon( pa );
   }
 
 }

#include "smallruler.moc"
