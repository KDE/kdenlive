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
#include <kdebug.h>

#include <kruler.h>
#include <kstyle.h>
#include <krulersliderbase.h>

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

	void setType(KRulerSliderBase *type) {
		if(m_sliderType!=0) {
			m_sliderType->decrementRef();
			m_sliderType = 0;
		}
		if(type==0) {
			kdDebug() << "setType(type) passed nil as type" << endl;
		} else {		
			m_sliderType = type;
		}
	}

	void setValue(const int value) {
		m_value = value;
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

	/** draws the Slider, using current mode & type */
	void drawSlider(KRuler *ruler, QPainter &painter, const int x, const int height) {
		painter.setPen(ruler->palette().color(status(), QColorGroup::Foreground));
		painter.setBrush(ruler->palette().color(status(), QColorGroup::Button));

		m_sliderType->drawHorizontalSlider(painter, x, height);
	}

	KRulerSliderBase *newTypeInstance() const {
		return m_sliderType->newInstance();
	}
private:
	int m_id;
	KRulerSliderBase *m_sliderType;
	int m_value;
	QPalette::ColorGroup m_status;
};

class KRulerPrivate {
public:
	KRulerPrivate(KRuler *ruler) {
		m_ruler = ruler;
		m_activeID=-1;
		m_oldValue=-1;
	}

	int addSlider(const KRuler::KRulerSliderType type, const int value, const QPalette::ColorGroup status) {
		KRulerPrivateSlider s(id, type, value, status);
		if(status == QPalette::Active) {
			setActiveSlider(id);
		}		
		sliders.append(s);
		return id++;
	}

	void deleteSlider(const int id) {
		QValueList<KRulerPrivateSlider>::Iterator it;
		for(it = sliders.begin(); it != sliders.end(); it++) {
			if((*it).getID() == id) {
				sliders.remove(it);
				return;
			}
		}
		kdWarning() << "KRuler::deleteSlider(id) : id " << id << " does not exist, no deletion occured!" << endl;
	}

	void drawSliders(QPainter &paint, const int sx, const int ex, const int height) {
		int pixel;
				
		QValueList<KRulerPrivateSlider>::Iterator it;
		for(it = sliders.begin(); it != sliders.end(); it++) {
			pixel = (int)m_ruler->mapValueToLocal((*it).getValue());

			if((pixel >= sx) && (pixel<=ex)) {
				(*it).drawSlider(m_ruler, paint, pixel, height);
			}
		}
	}

	void activateSliderAt(int x, int y) {
		QValueList<KRulerPrivateSlider>::Iterator it;
		for(it = sliders.begin(); it != sliders.end(); it++) {
			if((*it).underMouse(x, y, (int)m_ruler->mapValueToLocal((*it).getValue()), m_ruler->height())) {
				if((*it).status() != QPalette::Disabled)
				setActiveSlider((*it).getID());
				return;
			}
		}
	}

	void setActiveSlider(int id) {
    QValueList<KRulerPrivateSlider>::Iterator it;
		for(it = sliders.begin(); it != sliders.end(); it++) {
			if((*it).status() != QPalette::Disabled) {
				(*it).setStatus(QPalette::Inactive);
				if((*it).getID() == id) {
					(*it).setStatus(QPalette::Active);
				}
			}			
		}
		m_activeID = id;
	}

	void setActiveSliderValue(int value) {
		setSliderValue(activeID(), value);
	}

	int getActiveSliderValue() {
		return getSliderValue(activeID());
	}

	void setSliderValue(int id, int value) {
		QValueList<KRulerPrivateSlider>::Iterator it;
		for(it = sliders.begin(); it != sliders.end(); it++) {
			if((*it).getID() == id) {
				(*it).setValue(value);
				return;
			}
		}
	}

	int getSliderValue(int id) {
		QValueList<KRulerPrivateSlider>::Iterator it;
		for(it = sliders.begin(); it != sliders.end(); it++) {
			if((*it).getID() == id) {
				return (*it).getValue();
			}
		}
		kdWarning() << "KRuler : attempt has been made to get the value of non-existant slider "<<id<<endl;
		return 0;
	}	

	int activeID() {
		return m_activeID;
	}

public:
	/** Holds a list of all sliders associated with this ruler */
	QValueList<KRulerPrivateSlider> sliders;
	int id;
	KRuler *m_ruler;
	int m_activeID;
	int m_oldValue;	
};

KRuler::KRuler(KRulerModel *model, QWidget *parent, const char *name ) :
						QWidget(parent,name),
						m_sizeHint(500, 32),
						m_backBuffer(500, 32)
{
	d = new KRulerPrivate(this);
	if(model==0) {
		model = new KRulerModel();
	}

	d->addSlider(KRuler::Diamond, 30, QPalette::Inactive);
	d->addSlider(KRuler::TopMark, 60, QPalette::Inactive);
	d->addSlider(KRuler::BottomMark, 90, QPalette::Inactive);
	d->addSlider(KRuler::StartMark, 120, QPalette::Inactive);
	d->addSlider(KRuler::EndMark, 150, QPalette::Inactive);

	m_rulerModel = model;
	
	setValueScale(1);
	m_leftMostPixel = 0;

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
						m_backBuffer(500, 32)
{
	d = new KRulerPrivate(this);
	
	m_rulerModel = new KRulerModel();

	d->addSlider(KRuler::Diamond, 30, QPalette::Inactive);		

	setValueScale(1);
	m_leftMostPixel = 0;

	setMinimumHeight(32);
	setMinimumWidth(32);
	setMaximumHeight(32);
	setMouseTracking(true);

	setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE));

 // we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
}

KRuler::~KRuler()
{
	delete d;
	delete m_rulerModel;
}
/** No descriptions */

void KRuler::paintEvent(QPaintEvent *event) {
	QPainter painter(this);

	drawToBackBuffer();
      	
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
	m_leftMostPixel = value;
	update();
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

	update();
}

/** No descriptions */
void KRuler::resizeEvent(QResizeEvent *event)
{
	m_backBuffer.resize(width(), height());
	drawToBackBuffer();
	emit resized();
}

/** returns the x-value coordinate in the rulers widget's local coordinate space which maps to the value requested. This funtion takes and returns doubles, as does it's counterpart - mapLocalToValue() */
double KRuler::mapValueToLocal(double value) const
{
	return (value * m_xValueSize) - m_leftMostPixel;	
}
/** Returns the value which is currently displayed at x in the ruler widget's local coordinate system. Takes and returns a double for accuracy. */
double KRuler::mapLocalToValue(double x) const
{
	return (x + m_leftMostPixel) / m_xValueSize;
}

/** Adds a new slider to the ruler. The style in which the slider is drawn is determined by type, and the slider is initially set to value. The value returned is the id that should be used to move this slider. */
int KRuler::addSlider(const KRulerSliderType type, const int value)
{
	return d->addSlider(type, value, QPalette::Inactive);	
}

void KRuler::deleteSlider(const int id) {
	d->deleteSlider(id);
}

void KRuler::mouseMoveEvent(QMouseEvent *event)
{
	if(event->state() & (QMouseEvent::LeftButton | QMouseEvent::RightButton | QMouseEvent::MidButton)) {
		if(event->state() & QMouseEvent::RightButton) {
			if(d->m_oldValue!=-1) {
				d->setActiveSliderValue(d->m_oldValue);
				d->m_oldValue=-1;
				update();
			}
		}	else {
			if(d->m_oldValue==-1) {
				d->activateSliderAt(event->x(), event->y());
				d->m_oldValue = d->getActiveSliderValue();
				update();
			} else {
				d->setActiveSliderValue((int)mapLocalToValue((int)event->x()));
				update();
			}
		}
	} else {
		if(d->m_oldValue!=-1) {
			d->m_oldValue=-1;
		}
	}
}

/** Draws the ruler to a back buffered QImage, but does not display it. This image can then be blitted straight to the screen for speedy drawing. */
void KRuler::drawToBackBuffer()
{
	QPainter painter(&m_backBuffer);
	int value;
	double pixel;
	double pixelIncrement;
	int sx; // = event->rect().x();
	int ex; // = sx + event->rect().width();

	sx = 0;
	ex = width();

	painter.fillRect(sx, 0, ex, height(), palette().active().background());
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
  ex += m_textEvery - (ex % m_textEvery);

	while(pixel < ex+m_textEvery) {
		painter.drawText((int)pixel-50, 0, 100, 32, AlignCenter, m_rulerModel->mapValueToText(value));
		value += m_textEvery;
		pixel += pixelIncrement;
	}

	//
	// draw sliders
	//

	d->drawSliders(painter, sx, ex, height());
}
