/***************************************************************************
                          KRuler.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qpainter.h>
#include <qcolor.h>
#include <qtextstream.h>
#include <qvaluelist.h>

#include <iostream>
#include <cmath>

#include <kdebug.h>
#include <kstyle.h>

#include "kruler.h"
#include "krulersliderbase.h"
#include "krulermodel.h"

class KRulerPrivateSliderDiamond : public KRulerSliderBase {
public:
	KRulerPrivateSliderDiamond() {
	}
	
	~KRulerPrivateSliderDiamond() {
	}

  void drawHorizontalSlider(QPainter &painter, const int x, const int height) {
		QPointArray points(4);

		points.setPoint(0, x - (height/8) - 1, height/2);
		points.setPoint(1, x, 0);
		points.setPoint(2, x + (height/8) + 1, height/2);
		points.setPoint(3, x, height);
		painter.drawPolygon(points);  	
  }
    
  bool underMouse(const int x, const int y, const int midx, const int height) const {
		if(x < midx - (height/8)-1) return false;
		if(x > midx + (height/8)+1) return false;
		return true;  
  }

  int leftBound(int x, int height) {
  	return x -(height/8) - 1;   
  }

  int rightBound(int x, int height) {
  	return x + (height/8) + 1;  
  }
};

class KRulerPrivateSliderTopMark : public KRulerSliderBase {
public:
	KRulerPrivateSliderTopMark() {
	}

	~KRulerPrivateSliderTopMark() {
	}

  void drawHorizontalSlider(QPainter &painter, const int x, const int height) {
		QPointArray points(3);

		points.setPoint(0, x - (height/4) - 1, 0);
		points.setPoint(1, x + (height/4) + 1, 0);
		points.setPoint(2, x, height/2);
		painter.drawPolygon(points);		
  }

  bool underMouse(const int x, const int y, const int midx, const int height) const { 
		if(x < midx - (height/4)-1) return false;
		if(x > midx + (height/4)+1) return false;
		return true;
  }

  int leftBound(int x, int height) {
  	return x -(height/4) - 1;
  }

  int rightBound(int x, int height) {
  	return x + (height/4) + 1;
  }  
};

class KRulerPrivateSliderBottomMark : public KRulerSliderBase {
public:
	KRulerPrivateSliderBottomMark() {
	}

	~KRulerPrivateSliderBottomMark() {
	}

  void drawHorizontalSlider(QPainter &painter, const int x, const int height) {
		QPointArray points(3);

		points.setPoint(0, x, height/2);
		points.setPoint(1, x + (height/4) + 1, height);
		points.setPoint(2, x - (height/4) - 1, height);
		painter.drawPolygon(points);			
  }

  bool underMouse(const int x, const int y, const int midx, const int height) const {
		if(x < midx - (height/4)-1) return false;
		if(x > midx + (height/4)+1) return false;
		return true;
  }

  int leftBound(int x, int height) {
  	return x -(height/4) - 1;
  }

  int rightBound(int x, int height) {
  	return x + (height/4) + 1;
  }  
};

class KRulerPrivateSliderStartMark : public KRulerSliderBase {
public:
	KRulerPrivateSliderStartMark() {
	}

	~KRulerPrivateSliderStartMark() {
	}

  void drawHorizontalSlider(QPainter &painter, const int x, const int height) {
		QPointArray points(3);

		points.setPoint(0, x - (height/4) - 1, height);
		points.setPoint(1, x - (height/4) - 1, 0);
		points.setPoint(2, x, height/2);
		painter.drawPolygon(points);			
  }

  bool underMouse(const int x, const int y, const int midx, const int height) const {
		if(x < midx - (height/4)-1) return false;
		if(x > midx) return false;
		return true;  
  }

  int leftBound(int x, int height) {
  	return x -(height/4) - 1;
  }

  int rightBound(int x, int height) {
  	return x;
  }  
};

class KRulerPrivateSliderEndMark : public KRulerSliderBase {
public:
	KRulerPrivateSliderEndMark() {
	}

	~KRulerPrivateSliderEndMark() {
	}

	void drawHorizontalSlider(QPainter &painter, const int x, const int height) {
		QPointArray points(3);

		points.setPoint(0, x, height/2);
		points.setPoint(1, x + (height/4) + 1, 0);
		points.setPoint(2, x + (height/4) + 1, height);
		painter.drawPolygon(points);		
	}

  bool underMouse(const int x, const int y, const int midx, const int height) const {
		if(x < midx) return false;
		if(x > midx + (height/4)+1) return false;
		return true;
	}

	int leftBound(int x, int height) {
  	return x;
  }

  int rightBound(int x, int height) {
  	return x + (height/4) + 1;
  }
};

class KRulerPrivateSlider {
public:
	KRulerPrivateSlider() {
		m_id = 0;
		m_sliderType = 0;
		setType(KRuler::Diamond);
		m_value = 0;
		m_status = QPalette::Inactive;
	}
	
	KRulerPrivateSlider(int id, const KRuler::KRulerSliderType type, const int value, const QPalette::ColorGroup status) {
		m_id = id;
		m_sliderType = 0;		
		setType(type);
		m_value = value;
		m_status = status;
	}

	KRulerPrivateSlider(const KRulerPrivateSlider &slider) {		
		m_id = slider.getID();
		m_sliderType=0;
		setType(slider.newTypeInstance());
		m_value = slider.getValue();
		m_status = slider.status();		
	}

	~KRulerPrivateSlider() {
		if(m_sliderType) {
			m_sliderType->decrementRef();
			m_sliderType = 0;
		}
	}

	int getID() const {
		return m_id;
	}

	int getValue() const {
		return m_value;
	}

	void setType(KRuler::KRulerSliderType type) {
		switch (type) {
			case KRuler::Diamond 		: setType(new KRulerPrivateSliderDiamond()); break;
			case KRuler::TopMark 		: setType(new KRulerPrivateSliderTopMark()); break;
			case KRuler::BottomMark : setType(new KRulerPrivateSliderBottomMark()); break;
			case KRuler::StartMark 	: setType(new KRulerPrivateSliderStartMark()); break;
			case KRuler::EndMark 		: setType(new KRulerPrivateSliderEndMark()); break;
			default									: setType(new KRulerPrivateSliderDiamond()); break;
		}		
	}
  /** Returns the left-most pixel that will be drawn by a horizontal slider. */
  int leftBound();

	void setType(KRulerSliderBase *type) {
		if(m_sliderType!=0) {
			m_sliderType->decrementRef();
			m_sliderType = 0;
		}

		m_sliderType = type;
	}

	bool setValue(const int value) {
		if(m_value != value) {
			m_value = value;
			return true;
		}
		return false;
	}

	void setStatus(const QPalette::ColorGroup status) {
		m_status = status;
	}

	QPalette::ColorGroup status() const {
		return m_status;
	}

	bool underMouse(const int x, const int y, const int midx, const int height) const {
		return m_sliderType->underMouse(x, y, midx, height);
	}

	void drawSlider(KRuler *ruler, QPainter &painter, const int x, const int height) {
		painter.setPen(ruler->palette().color(status(), QColorGroup::Foreground));
		painter.setBrush(ruler->palette().color(status(), QColorGroup::Button));

		m_sliderType->drawHorizontalSlider(painter, x, height);
	}

	KRulerSliderBase *newTypeInstance() const {
		return m_sliderType->newInstance();
	}

	/** Returns the left-most pixel that will be drawn by a horizontal slider. */
	int leftBound(int midx, int height)
	{
		return m_sliderType->leftBound(midx, height);
	}

	int rightBound(int midx, int height) {
		return m_sliderType->rightBound(midx, height);
	}
	
private:
	int m_id;
	KRulerSliderBase *m_sliderType;
	int m_value;
	QPalette::ColorGroup m_status;
};

class KRulerPrivate {
public:
	KRulerPrivate() {
		m_id=0;
		m_activeID = -1;
		m_oldValue = -1;
	}
	
	/** Holds a list of all sliders associated with this ruler */
	QValueList<KRulerPrivateSlider> m_sliders;
	/** An id counter which is used to keep count of what ID number the next created slider should get. */
	int m_id;
	/** The id of the currently activated slider */		
	int m_activeID;
	/** The previous value of the slider */
	int m_oldValue;
	/** The draw list of invalidated rectangles; let's us know which bits of the back buffer need to be
	redrawn */
	QValueList<int> m_bufferDrawList;	
};

 
KRuler::KRuler(int min, int max, double scale, KRulerModel *model, QWidget *parent, const char *name) :
						QWidget(parent, name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32),
						d(new KRulerPrivate())
{
	m_rulerModel = 0;
	setRulerModel(model);
	
	setValueScale(scale);
	setStartPixel(0);
	setRange(min, max);

	setMinimumHeight(32);
	setMinimumWidth(32);
	setMaximumHeight(32);
	setMouseTracking(true);

	setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE));


 // we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
}

KRuler::KRuler(KRulerModel *model, QWidget *parent, const char *name ) :
						QWidget(parent,name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32),
						d(new KRulerPrivate())
{
	m_rulerModel=0;
	setRulerModel(model);
	
	setValueScale(1);
	setStartPixel(0);
	setRange(0, 100);

	setMinimumHeight(32);
	setMinimumWidth(32);
	setMaximumHeight(32);
	setMouseTracking(true);


	setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE));

 // we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
}

KRuler::KRuler(QWidget *parent, const char *name ) :
						QWidget(parent,name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32),
						d(new KRulerPrivate())
{	
	m_rulerModel = 0;
	setRulerModel(0);

	setValueScale(1);
	setStartPixel(0);
	setRange(0, 100);

	setMinimumHeight(32);
	setMinimumWidth(32);
	setMaximumHeight(32);
	setMouseTracking(true);

	setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE));
	
 // we draw everything ourselves, no need to draw background. 
	setBackgroundMode(Qt::NoBackground);

	m_autoClickSlider = -1;
}

KRuler::~KRuler()
{
	delete d;
	delete m_rulerModel;
}

void KRuler::paintEvent(QPaintEvent *event) {
	while(!d->m_bufferDrawList.isEmpty()) {
		drawToBackBuffer(d->m_bufferDrawList[0], d->m_bufferDrawList[1]);
		d->m_bufferDrawList.pop_front();
		d->m_bufferDrawList.pop_front();		
	}
	QPainter painter(this);	
      	
	painter.drawPixmap(event->rect().x(), event->rect().y(),
										m_backBuffer,
										event->rect().x(), event->rect().y(),
										event->rect().width(), event->rect().height());
}

inline void KRuler::drawSmallTick(QPainter &painter, const int pixel)
{
	painter.drawLine(pixel, 0, pixel, 4);
	painter.drawLine(pixel, 28, pixel, 32);
}

inline void KRuler::drawBigTick(QPainter &painter, const int pixel)
{
	painter.drawLine(pixel, 0, pixel, 8);
	painter.drawLine(pixel, 24, pixel, 32);
}

QSize KRuler::sizeHint() {
	return m_sizeHint;
}

void KRuler::setStartPixel(int value) {
	if(m_leftMostPixel != value) {
		m_leftMostPixel = value;
		invalidateBackBuffer(0, width());
	}
}

void KRuler::setValueScale(double size){
	m_xValueSize = size;
	int tick;

	for(tick=1; tick*m_xValueSize < m_rulerModel->minimumTextSeparation(); tick *=2);
	m_textEvery = m_rulerModel->getTickDisplayInterval(tick);

	for(tick=1; tick*m_xValueSize < m_rulerModel->minimumLargeTickSeparation(); tick *=2);	
	m_bigTickEvery = m_rulerModel->getTickDisplayInterval(tick);

	for(tick=1; tick*m_xValueSize < m_rulerModel->minimumSmallTickSeparation(); tick *=2);
		
	m_smallTickEvery = m_rulerModel->getTickDisplayInterval(tick);

	while(m_textEvery	% m_bigTickEvery != 0) {
		m_textEvery = m_rulerModel->getTickDisplayInterval(m_textEvery+1);		
	}

	while(m_bigTickEvery % m_smallTickEvery != 0) {
		m_smallTickEvery = m_rulerModel->getTickDisplayInterval(m_smallTickEvery+1);
	}

	if(m_smallTickEvery == m_bigTickEvery) {
		m_bigTickEvery = m_rulerModel->getTickDisplayInterval(m_smallTickEvery+1);
	}

	emit scaleChanged(size);

	invalidateBackBuffer(0, width());
}

void KRuler::resizeEvent(QResizeEvent *event)
{
	m_backBuffer.resize(width(), height());
	invalidateBackBuffer(0, width());
		
	emit resized();
}

double KRuler::mapValueToLocal(double value) const
{
	return ((value * m_xValueSize) - m_leftMostPixel);
}

double KRuler::mapLocalToValue(double x) const
{
	return (x + m_leftMostPixel) / m_xValueSize;
}

int KRuler::addSlider(const KRulerSliderType type, const int value)
{
  KRulerPrivateSlider s(d->m_id, type, value, QPalette::Inactive);
  d->m_sliders.append(s);
	emit sliderValueChanged(d->m_id, value);
  return d->m_id++;	
}

void KRuler::deleteSlider(const int id) {
	QValueList<KRulerPrivateSlider>::Iterator it;
	
	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).getID() == id) {
			d->m_sliders.remove(it);
			return;
		}
	}
	
	kdWarning() << "KRuler::deleteSlider(id) : id " << id << " does not exist, no deletion occured!" << endl;
}

void KRuler::setSliderValue(const int id, const int value)
{
	int actValue = (value <minValue()) ? minValue() : value;
	actValue = (actValue > maxValue()) ? maxValue() : actValue;
	
	QValueList<KRulerPrivateSlider>::Iterator it;

	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).getID() == id) {
			invalidateBackBuffer((*it).leftBound((int)mapValueToLocal((*it).getValue()), height()),
														 (*it).rightBound((int)mapValueToLocal((*it).getValue()), height()) + 1);
			
			if((*it).setValue(actValue)) {		
				invalidateBackBuffer((*it).leftBound((int)mapValueToLocal((*it).getValue()), height()),
															 (*it).rightBound((int)mapValueToLocal((*it).getValue()), height()) + 1);				
				
				emit sliderValueChanged(id, actValue);				
				break;
			}
		}
	}
}                     

void KRuler::activateSliderUnderCoordinate(int x, int y) {
	QValueList<KRulerPrivateSlider>::Iterator it;
	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).underMouse(x, y, (int)mapValueToLocal((*it).getValue()), height())) {
			if((*it).status() != QPalette::Disabled)
			activateSlider((*it).getID());
			return;
		}
	}
	activateSlider(m_autoClickSlider);
}

void KRuler::activateSlider(int id) {
	QValueList<KRulerPrivateSlider>::Iterator it;
	
	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).status() != QPalette::Disabled) {
			(*it).setStatus(QPalette::Inactive);
			if((*it).getID() == id) {
				(*it).setStatus(QPalette::Active);
			}
		}
	}
	d->m_activeID = id;
}

int KRuler::getSliderValue(int id) {
	QValueList<KRulerPrivateSlider>::Iterator it;
	
	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).getID() == id) {
			return (*it).getValue();
		}
	}
	
	kdWarning() << "KRuler : getSliderValue(id) attempt has been made to get the value of non-existant slider "<<id<<endl;
	return 0;
}

int KRuler::activeSliderID()
{
	return d->m_activeID;
}

void KRuler::setMinValue(const int value)
{
	m_minValue = value;
}

void KRuler::setMaxValue(const int value)
{
	m_maxValue = (value > m_minValue) ? value : m_minValue + 1;
}

void KRuler::setRange(const int min, const int max)
{
	int oldvalue;
	
	if(min != minValue()) {
		oldvalue = minValue();
		setMinValue(min);
		invalidateBackBuffer((int)mapValueToLocal(min), (int)mapValueToLocal(oldvalue));
	}
	if(max != maxValue()) {
		oldvalue = maxValue();
		setMaxValue(max);
		invalidateBackBuffer((int)mapValueToLocal(oldvalue), (int)mapValueToLocal(max));		
	}
}

int KRuler::minValue() const
{
	return m_minValue;
}

int KRuler::maxValue() const
{
	return m_maxValue;	
}

void KRuler::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == QMouseEvent::RightButton) {
		if(d->m_oldValue!=-1) {
			setSliderValue(activeSliderID(), d->m_oldValue);
			d->m_oldValue=-1;
		}
	} else if(event->button() == QMouseEvent::LeftButton) {
		if(d->m_oldValue==-1) {
			activateSliderUnderCoordinate(event->x(), event->y());
			d->m_oldValue = getSliderValue(activeSliderID());
		}
	}
}

void KRuler::mouseReleaseEvent(QMouseEvent *event)
{
	if(event->button() == QMouseEvent::LeftButton) {
		if(d->m_oldValue!=-1) {
			d->m_oldValue=-1;
		}
	}
}

void KRuler::mouseMoveEvent(QMouseEvent *event)
{
	if(event->state() & (QMouseEvent::LeftButton | QMouseEvent::RightButton | QMouseEvent::MidButton)) {
		if(d->m_oldValue != -1) {
			setSliderValue(activeSliderID(), (int)floor(mapLocalToValue((int)event->x())+0.5));
		}
	}
}

void KRuler::setRulerModel(KRulerModel *model)
{
	if(m_rulerModel != 0) {
		delete m_rulerModel;
		m_rulerModel = 0;
	}
	
	if(model==0) {
		model = new KRulerModel();
	}
	m_rulerModel = model;
}

double KRuler::valueScale()
{
	return m_xValueSize;
}

void KRuler::setAutoClickSlider(int ID)
{
	m_autoClickSlider = ID;
}

void KRuler::drawToBackBuffer(int start, int end)
{
	QPainter painter(&m_backBuffer);
	int value;
	double pixel;
	double pixelIncrement;
	int sx, ex;

	sx = start;
	ex = end;

	// draw background, adding different colors before the start, and after the end values.

	int startRuler = (int)mapValueToLocal(minValue());
	int endRuler = (int)mapValueToLocal(maxValue());

	if(startRuler > sx) {
		painter.fillRect(sx, 0, startRuler-sx, height(), palette().active().background());
	} else {
		startRuler = sx;
	}
	if(endRuler > ex) {
		endRuler = ex;
	}
	painter.fillRect(startRuler, 0, endRuler-startRuler, height(), palette().active().base());
	if(endRuler < ex) {
		painter.fillRect(endRuler, 0, ex-endRuler, height(), palette().active().background());
	}

	painter.setPen(palette().active().foreground());
	painter.setBrush(palette().active().background());

  //
  // Special case draw for when the big ticks are always coincidental with small ticks.
  // Nicely stops rounding errors from creeping in, and should be slightly faster.
  //
  if(!(m_bigTickEvery % m_smallTickEvery)) {
  	value = (int)((m_leftMostPixel+sx)/m_xValueSize);
   	value -= value % m_smallTickEvery;
    pixel = (value * m_xValueSize) - m_leftMostPixel;
    pixelIncrement = m_xValueSize * m_smallTickEvery;
    // big tick every so many small ticks.
    int bigTick2SmallTick = m_bigTickEvery / m_smallTickEvery;
    value = (value % m_bigTickEvery) / m_smallTickEvery;

    while(pixel < ex) {
			if(value) {
				drawSmallTick(painter, (int)pixel);
			} else {
				drawBigTick(painter, (int)pixel);
			}
			value = (value+1)%bigTick2SmallTick;
			pixel += pixelIncrement;
    }
  } else {
		//
		// Draw small ticks.
		//

		value = (int)((m_leftMostPixel+sx) / m_xValueSize);
		value -= value % m_smallTickEvery;
		pixel = (value * m_xValueSize) - m_leftMostPixel;
		pixelIncrement = m_xValueSize * m_smallTickEvery;

		while(pixel < ex) {
			drawSmallTick(painter, (int)pixel);
			pixel += pixelIncrement;
		}

		//
		// Draw Big ticks
		//
		value = (int)((m_leftMostPixel+sx) / m_xValueSize);
		value -= value % m_bigTickEvery;
		pixel = (value * m_xValueSize) - m_leftMostPixel;
		pixelIncrement = m_xValueSize * m_bigTickEvery;

		while(pixel < ex) {
			drawBigTick(painter, (int)pixel);
			pixel += pixelIncrement;
		}
	}

	//
	// Draw displayed text
	//

	value = (int)((m_leftMostPixel + sx) / m_xValueSize);
	value -= value % m_textEvery;
	pixel = (value * m_xValueSize) - m_leftMostPixel;
	pixelIncrement = m_xValueSize * m_textEvery;

	while(value <= ((m_leftMostPixel + ex) / m_xValueSize) + m_textEvery) {
		painter.drawText((int)pixel-50, 0, 100, 32, AlignCenter, m_rulerModel->mapValueToText(value));
		value += m_textEvery;
		pixel += pixelIncrement;
	}

	//
	// draw sliders
	//

	QValueList<KRulerPrivateSlider>::Iterator it;

	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		value = (int)mapValueToLocal((*it).getValue());

		if((value >= sx) && (value<=ex)) {
			(*it).drawSlider(this, painter, value, height());
		}
	}

	update();
}

void KRuler::invalidateBackBuffer(int start, int end)
{
	if(start == end) return;
	if(start > end) {
		int temp = start;
		start = end;
		end = temp;		
	}

	if(start >= width()) return;
	if(end <= 0) return;
		
	if(start < 0) start = 0;
	if(end > width()) end = width();

	int cs, ce, count;
	int ns, ne;

	if(d->m_bufferDrawList.isEmpty()) {
		d->m_bufferDrawList.push_back(start);
		d->m_bufferDrawList.push_back(end);
		update();
		return;
	}
	
	// search for the correct place in the list, or a pair that overlaps.	
	for(count=0; count<d->m_bufferDrawList.count(); count+=2) {
		cs = d->m_bufferDrawList[count];
		ce = d->m_bufferDrawList[count+1];

		if(end <= cs) {
			d->m_bufferDrawList.insert(d->m_bufferDrawList.at(count), end);
			d->m_bufferDrawList.insert(d->m_bufferDrawList.at(count), start);
			break;
		}
		if((start <= ce) && (end >= cs)) {
			d->m_bufferDrawList[count] = (start < cs) ? start : cs;
			d->m_bufferDrawList[count+1] = (end > ce) ? end : ce;
			break;
		}
	}

	if(count == d->m_bufferDrawList.count()) {
		d->m_bufferDrawList.push_back(start);
		d->m_bufferDrawList.push_back(end);
	} else {
		// Check the remaining paris don't overlap.

		while(count+2 < d->m_bufferDrawList.count()) {
			cs = d->m_bufferDrawList[count];
			ce = d->m_bufferDrawList[count+1];		
			ns = d->m_bufferDrawList[count+2];
			ne = d->m_bufferDrawList[count+3];			

			if((cs <=ne ) && (ns <= ce)) {
				d->m_bufferDrawList[count] = (cs < ns) ? cs : ns;
				d->m_bufferDrawList[count+1] = (ce > ne) ? ce : ne;
				d->m_bufferDrawList.remove(d->m_bufferDrawList.at(count+3));
				d->m_bufferDrawList.remove(d->m_bufferDrawList.at(count+2));
			} else {
				break;
			}
		}	
	}
	
	update();
}
