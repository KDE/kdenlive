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

#include "rangelist.h"
#include "kruler.h"
#include "krulersliderbase.h"
#include "krulermodel.h"

namespace {
	uint g_scrollTimerDelay = 50;
	uint g_scrollThreshold = 50;
}

class KRulerPrivateSliderDiamond : public KRulerSliderBase {
public:
	KRulerPrivateSliderDiamond() {
	}
	
	~KRulerPrivateSliderDiamond() {
	}

  void drawHorizontalSlider(QPainter &painter, int x, int height) {
		QPointArray points(4);

		points.setPoint(0, x - (height/8) - 1, height/2);
		points.setPoint(1, x, 0);
		points.setPoint(2, x + (height/8) + 1, height/2);
		points.setPoint(3, x, height);
		painter.drawPolygon(points);  	
  }
    
  bool underMouse(int x, int y, int midx, int height) const {
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

  void drawHorizontalSlider(QPainter &painter, int x, int height) {
		QPointArray points(3);

		points.setPoint(0, x - (height/4) - 1, 0);
		points.setPoint(1, x + (height/4) + 1, 0);
		points.setPoint(2, x, height/2);
		painter.drawPolygon(points);		
  }

  bool underMouse(int x, int y, int midx, int height) const { 
		if(x < midx - (height/4)-1) return false;
		if(x > midx + (height/4)+1) return false;
    if(y > height/2) return false;
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

  void drawHorizontalSlider(QPainter &painter, int x, int height) {
		QPointArray points(3);

		points.setPoint(0, x, height/2);
		points.setPoint(1, x + (height/4) + 1, height);
		points.setPoint(2, x - (height/4) - 1, height);
		painter.drawPolygon(points);			
  }

  bool underMouse(int x, int y, int midx, int height) const {
		if(x < midx - (height/4)-1) return false;
		if(x > midx + (height/4)+1) return false;
    if(y < height / 2) return false;
		return true;
  }

  int leftBound(int x, int height) {
  	return x -(height/4) - 1;
  }

  int rightBound(int x, int height) {
  	return x + (height/4) + 1;
  }  
};

class KRulerPrivateSliderEndMark : public KRulerSliderBase {
public:
	KRulerPrivateSliderEndMark() {
	}

	~KRulerPrivateSliderEndMark() {
	}

  void drawHorizontalSlider(QPainter &painter, int x, int height) {
		QPointArray points(3);

		points.setPoint(0, x, height);
		points.setPoint(1, x, height/2);
		points.setPoint(2, x + (height/4) + 1, (height*3)/4);
		painter.drawPolygon(points);			
  }

  bool underMouse(int x, int y, int midx, int height) const {
		if(x < midx) return false;
		if(x > midx + (height/4) + 1) return false;
    if(y < height/2) return false;
		return true;  
  }

  int leftBound(int x, int height) {
  	return x;
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

	void drawHorizontalSlider(QPainter &painter, int x, int height) {
		QPointArray points(3);

		points.setPoint(0, x - (height/4) - 1, (height*3)/4);
		points.setPoint(1, x, height/2);
		points.setPoint(2, x, height);
		painter.drawPolygon(points);		
	}

  bool underMouse(int x, int y, int midx, int height) const {
		if(x < midx - (height/4) - 1) return false;
		if(x > midx) return false;
    if(y < height/2) return false;
		return true;
	}

	int leftBound(int x, int height) {
  	return x - (height/4) - 1;
  }

  int rightBound(int x, int height) {
  	return x;
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
	
	KRulerPrivateSlider(int id, const KRuler::KRulerSliderType type, int value, const QPalette::ColorGroup status) {
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

	bool setValue(int value) {
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

	bool underMouse(int x, int y, int midx, int height) const {
		return m_sliderType->underMouse(x, y, midx, height);
	}

	void drawSlider(KRuler *ruler, QPainter &painter, int x, int height) {
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
	RangeList<int> m_bufferDrawList;
};

 
KRuler::KRuler(int min, int max, double scale, KRulerModel *model, QWidget *parent, const char *name) :
						QWidget(parent, name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32),
						m_leftMostPixel(0),
						m_rulerModel(0),
						d(new KRulerPrivate()),
						m_minValue(min),
						m_maxValue(max),
						m_scrollTimer(this, "scroll timer")
{
	setRulerModel(model);
	
	setValueScale(scale);
	setStartPixel(0);
	setRange(min, max);

	setMinimumHeight(32);
	setMinimumWidth(32);
	setMaximumHeight(32);
	setMouseTracking(true);

	setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE));

	invalidateBackBuffer();

 // we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);

	connect(&m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotTimerScrollEvent()));
}

KRuler::KRuler(KRulerModel *model, QWidget *parent, const char *name ) :
						QWidget(parent,name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32),
						m_leftMostPixel(0),
						m_rulerModel(0),
						d(new KRulerPrivate()),
						m_minValue(0),
						m_maxValue(100),
						m_scrollTimer(this, "scroll timer")
{
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

	invalidateBackBuffer();
	connect(&m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotTimerScrollEvent()));
}

KRuler::KRuler(QWidget *parent, const char *name ) :
						QWidget(parent,name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32),
						m_leftMostPixel(0),
						m_rulerModel(0),
						d(new KRulerPrivate()),
						m_minValue(0),
						m_maxValue(0),
						m_scrollTimer(this, "scroll timer")
{	
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
	connect(&m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotTimerScrollEvent()));
}

KRuler::~KRuler()
{
	delete d;
	delete m_rulerModel;
}

void KRuler::paintEvent(QPaintEvent *event) {
	RangeListIterator<int> itt(d->m_bufferDrawList);
	
	while(!itt.finished()) {
		drawToBackBuffer(itt.start(), itt.end());
		++itt;
	}
	d->m_bufferDrawList.clear();
	
	QPainter painter(this);	
      	
	painter.drawPixmap(event->rect().x(), event->rect().y(),
										m_backBuffer,
										event->rect().x(), event->rect().y(),
										event->rect().width(), event->rect().height());
}

inline void KRuler::drawSmallTick(QPainter &painter, int pixel)
{
	painter.drawLine(pixel, 0, pixel, 4);
	painter.drawLine(pixel, 28, pixel, 32);
}

inline void KRuler::drawBigTick(QPainter &painter, int pixel)
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
	d->m_bufferDrawList.setFullRange(0, width());

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

int KRuler::addSlider(KRulerSliderType type, int value)
{
  KRulerPrivateSlider s(d->m_id, type, value, QPalette::Inactive);
  d->m_sliders.append(s);
	emit sliderValueChanged(d->m_id, value);
  return d->m_id++;	
}

void KRuler::deleteSlider(int id) {
	QValueList<KRulerPrivateSlider>::Iterator it;
	
	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).getID() == id) {
			d->m_sliders.remove(it);
			return;
		}
	}
	
	kdWarning() << "KRuler::deleteSlider(id) : id " << id << " does not exist, no deletion occured!" << endl;
}

void KRuler::setSliderValue(int id, int value)
{
	int actValue = (value <minValue()) ? minValue() : value;
	actValue = (actValue > maxValue()) ? maxValue() : actValue;
	
	QValueList<KRulerPrivateSlider>::Iterator it;

	for(it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
		if((*it).getID() == id) {
			invalidateBackBuffer(
				(*it).leftBound((int)mapValueToLocal((*it).getValue()), height()),
				(*it).rightBound((int)mapValueToLocal((*it).getValue()), height()) + 1);
			
			if((*it).setValue(actValue)) {		
				invalidateBackBuffer(
				   (*it).leftBound((int)mapValueToLocal((*it).getValue()), height()),
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

void KRuler::setMinValue(int value)
{
	m_minValue = value;
	setSlidersToRange();
}

void KRuler::setMaxValue(int value)
{
	m_maxValue = (value > m_minValue) ? value : m_minValue;
	setSlidersToRange();  
}

void KRuler::setRange(int min, int max)
{
	int oldvalue = 0;
	
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
	setSlidersToRange();  
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
			setSliderValue(activeSliderID(), (int)floor(mapLocalToValue((int)event->x())+0.5));      
		}
		m_scrollTimer.stop();
	}
}

void KRuler::mouseMoveEvent(QMouseEvent *event)
{
	if(event->state() & (QMouseEvent::LeftButton | QMouseEvent::RightButton | QMouseEvent::MidButton)) {
		if(d->m_oldValue != -1) {
			setSliderValue(activeSliderID(), (int)floor(mapLocalToValue((int)event->x())+0.5));

			if(event->x() < g_scrollThreshold) {
				m_scrollRight = false;
				if(!m_scrollTimer.isActive()) {
					m_scrollTimer.start(g_scrollTimerDelay, false);
				}
			} else if(width() - event->x() < g_scrollThreshold) {
				m_scrollRight = true;
				if(!m_scrollTimer.isActive()) {
					m_scrollTimer.start(g_scrollTimerDelay, false);
				}
			} else {
				m_scrollTimer.stop();
			}
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
	painter.setClipRect(start, 0, end-start, height());
	int value;
	double pixel;
	double pixelIncrement;
	int sx, ex;

	sx = start;
	ex = end;

	// draw background, adding different colors before the start, and after the end values.

	int startRuler = (int)mapValueToLocal(minValue());
	int endRuler = (int)mapValueToLocal((maxValue() > minValue()) ? maxValue() : maxValue()+1);

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

		if((ex >= (*it).leftBound(value, height())) && (sx <= (*it).rightBound(value, height()))) {
			(*it).drawSlider(this, painter, value, height());
		}
	}

	update();
}

void KRuler::invalidateBackBuffer()
{
	invalidateBackBuffer(0, width());
}

void KRuler::invalidateBackBuffer(int start, int end)
{
	d->m_bufferDrawList.addRange(start, end);
	update();
}

void KRuler::setSlidersToRange()
{
  for(uint count=0; count<d->m_sliders.size(); ++count)
  {
    if(getSliderValue(count) < minValue()) setSliderValue(count, minValue());
    if(getSliderValue(count) > maxValue()) setSliderValue(count, maxValue());
  }
}

void KRuler::slotTimerScrollEvent()
{
	if(m_scrollRight) {
		emit requestScrollRight();
	} else {
		emit requestScrollLeft();
	}
}
