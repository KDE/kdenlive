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

#include <klocale.h>

#include <iostream>
#include <cmath>

#include <kdebug.h>
#include <kstyle.h>
#include <kiconloader.h>

#include "rangelist.h"
#include "kruler.h"
#include "krulersliderbase.h"
#include "krulermodel.h"

namespace {
    uint g_scrollTimerDelay = 50;
    uint g_scrollThreshold = 50;
}

namespace Gui {

    class KRulerPrivateSliderDiamond:public KRulerSliderBase {
      public:
	KRulerPrivateSliderDiamond() {
	} 
        
        ~KRulerPrivateSliderDiamond() {} 
        
        void drawHorizontalSlider(QPainter & painter, int x, int height) {
	    QPointArray points(4);

	    points.setPoint(0, x - (height / 8) - 1, height / 2);
	    points.setPoint(1, x, 0);
	    points.setPoint(2, x + (height / 8) + 1, height / 2);
	    points.setPoint(3, x, height);
	    painter.drawPolygon(points);
	}

	bool underMouse(int x, int y, int midx, int height) const {
	    if (x < midx - (height / 8) - 1)
		return false;
	    if (x > midx + (height / 8) + 1)
		return false;
	    return true;
	} 
        
        int leftBound(int x, int height) {
	    return x - (height / 8) - 2;
	}

	int rightBound(int x, int height) {
	    return x + (height / 8) + 2;
	}
    };

    class KRulerPrivateSliderTopMark:public KRulerSliderBase {
      public:
	KRulerPrivateSliderTopMark() {
	} ~KRulerPrivateSliderTopMark() {
	}

	void drawHorizontalSlider(QPainter & painter, int x, int height) {
	    QPointArray points(3);

	    points.setPoint(0, x - (height / 4) - 1, 0);
	    points.setPoint(1, x + (height / 4) + 1, 0);
	    points.setPoint(2, x, height / 2);
	    painter.drawPolygon(points);
	}

	bool underMouse(int x, int y, int midx, int height) const {
	    if (x < midx - (height / 4) - 1)
		return false;
	    if (x > midx + (height / 4) + 1)
		return false;
	    if (y > height / 2)
		return false;
	    return true;
	} int leftBound(int x, int height) {
	    return x - (height / 4) - 2;
	}

	int rightBound(int x, int height) {
	    return x + (height / 4) + 2;
	}
    };

    class KRulerPrivateSliderBottomMark:public KRulerSliderBase {
      public:
	KRulerPrivateSliderBottomMark() {} 
        ~KRulerPrivateSliderBottomMark() {}

	void drawHorizontalSlider(QPainter & painter, int x, int height) {
	    QPointArray points(3);

	    points.setPoint(0, x, height / 2);
	    points.setPoint(1, x + (height / 4) + 1, height);
	    points.setPoint(2, x - (height / 4) - 1, height);
	    painter.drawPolygon(points);
	}

	bool underMouse(int x, int y, int midx, int height) const {
	    if (x < midx - (height / 4) - 1)
		return false;
	    if (x > midx + (height / 4) + 1)
		return false;
	    if (y < height / 2)
		return false;
	    return true;
	} 
        
        int leftBound(int x, int height) {
	    return x - (height / 4) - 2;
	}

	int rightBound(int x, int height) {
	    return x + (height / 4) + 2;
	}
    };

    class KRulerPrivateSliderEndMark:public KRulerSliderBase {
      public:
	KRulerPrivateSliderEndMark() {
	} ~KRulerPrivateSliderEndMark() {
	}

	void drawHorizontalSlider(QPainter & painter, int x, int height) {
	    QPointArray points(3);

	    points.setPoint(0, x, height);
	    points.setPoint(1, x, height / 2);
	    points.setPoint(2, x + (height / 4) + 1, (height * 3) / 4);
	    painter.drawPolygon(points);
	}

	bool underMouse(int x, int y, int midx, int height) const {
	    if (x < midx)
		return false;
	    if (x > midx + (height / 4) + 1)
		return false;
	    if (y < height / 2)
		return false;
	    return true;
	} 
        
        int leftBound(int x, int height) {
	    return x;
	}

	int rightBound(int x, int height) {
	    return x + (height / 4) + 2;
	}
    };

    class KRulerPrivateSliderStartMark:public KRulerSliderBase {
      public:
	KRulerPrivateSliderStartMark() {
	} ~KRulerPrivateSliderStartMark() {
	}

	void drawHorizontalSlider(QPainter & painter, int x, int height) {
	    QPointArray points(3);

	    points.setPoint(0, x - (height / 4) - 1, (height * 3) / 4);
	    points.setPoint(1, x, height / 2);
	    points.setPoint(2, x, height);
	    painter.drawPolygon(points);
	}

	bool underMouse(int x, int y, int midx, int height) const {
	    if (x < midx - (height / 4) - 1)
		return false;
	    if (x > midx)
		return false;
	    if (y < height / 2)
		return false;
	    return true;
	} 
        
        int leftBound(int x, int height) {
	    return x - (height / 4) - 1;
	}

	int rightBound(int x, int height) {
	    return x + 1;
	}
    };

    class KRulerPrivateSliderHorizontalMark:public KRulerSliderBase {
      public:
	KRulerPrivateSliderHorizontalMark() {
	} ~KRulerPrivateSliderHorizontalMark() {
	}

	void drawHorizontalSlider(QPainter & painter, int x, int height) {
	    QPointArray points(4);

	    points.setPoint(0, x - 4, height - 3);
	    points.setPoint(1, x - 4, (height / 2) + 3);
	    points.setPoint(2, x + 4, (height / 2) + 3);
	    points.setPoint(3, x + 4, height - 3);
	    painter.drawPolygon(points);
	    painter.drawLine(QPoint(x, height - 3), QPoint(x,
		    (height / 2) + 6));
	}

	bool underMouse(int x, int y, int midx, int height) const {
	    if (x < midx - 4)
		return false;
	    if (x > midx + 4)
		return false;
	    if (y > height - 3)
		return false;
	    if (y < (height / 2) + 3)
		return false;
	    return true;
	} 
        
        int leftBound(int x, int height) {
	    return x - 5;
	}

	int rightBound(int x, int height) {
	    return x + 5;
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
        
        KRulerPrivateSlider(int id, const KRuler::KRulerSliderType type,
	    int value, const QPalette::ColorGroup status) {
	    m_id = id;
	    m_sliderType = 0;
	    setType(type);
	    m_value = value;
	    m_status = status;
	}

	KRulerPrivateSlider(const KRulerPrivateSlider & slider) {
	    m_id = slider.getID();
	    m_sliderType = 0;
	    setType(slider.newTypeInstance());
	    m_value = slider.getValue();
	    m_status = slider.status();
	}

	~KRulerPrivateSlider() {
	    if (m_sliderType) {
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
	    case KRuler::Diamond:
		setType(new KRulerPrivateSliderDiamond());
		break;
	    case KRuler::TopMark:
		setType(new KRulerPrivateSliderTopMark());
		break;
	    case KRuler::BottomMark:
		setType(new KRulerPrivateSliderBottomMark());
		break;
	    case KRuler::StartMark:
		setType(new KRulerPrivateSliderStartMark());
		break;
	    case KRuler::EndMark:
		setType(new KRulerPrivateSliderEndMark());
		break;
	    case KRuler::HorizontalMark:
		setType(new KRulerPrivateSliderHorizontalMark());
		break;
	    default:
		setType(new KRulerPrivateSliderDiamond());
		break;
	    }
	}
  /** Returns the left-most pixel that will be drawn by a horizontal slider. */
	int leftBound();

	void setType(KRulerSliderBase * type) {
	    if (m_sliderType != 0) {
		m_sliderType->decrementRef();
		m_sliderType = 0;
	    }

	    m_sliderType = type;
	}

	bool setValue(int value) {
	    if (m_value != value) {
		m_value = value;
		return true;
	    }
	    return false;
	}

	void setStatus(const QPalette::ColorGroup status) {
	    m_status = status;
	}

	QPalette::ColorGroup status()const {
	    return m_status;
	} 
        
        bool underMouse(int x, int y, int midx, int height) const {
	    return m_sliderType->underMouse(x, y, midx, height);
	} 
        
        void drawSlider(KRuler * ruler, QPainter & painter, int x,
	    int height) {
	    painter.setPen(ruler->palette().color(status(),
		    QColorGroup::Foreground));
	    painter.setBrush(ruler->palette().color(status(),
		    QColorGroup::Button));

	    m_sliderType->drawHorizontalSlider(painter, x, height);
	}

	KRulerSliderBase *newTypeInstance() const {
	    return m_sliderType->newInstance();
	}
        
	/** Returns the left-most pixel that will be drawn by a horizontal slider. */
	    int leftBound(int midx, int height) {
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
	    m_id = 0;
	    m_activeID = -1;
	    m_oldValue = -1;
	}
	/** Holds a list of all sliders associated with this ruler */
	    QValueList < KRulerPrivateSlider > m_sliders;
	/** An id counter which is used to keep count of what ID number the next created slider should get. */
	int m_id;
	/** The id of the currently activated slider */
	int m_activeID;
	/** The previous value of the slider */
	int m_oldValue;
	/** The draw list of invalidated rectangles; let's us know which bits of the back buffer need to be
	redrawn */
	RangeList < int >m_bufferDrawList;
    };


    KRuler::KRuler(int min, int max, double scale, KRulerModel * model,
	QWidget * parent, const char *name):QWidget(parent, name),
	m_sizeHint(500, 32), m_backBuffer(500, 32), m_leftMostPixel(0),
	m_rulerModel(0), d(new KRulerPrivate()), m_minValue(min),
	m_maxValue(max), m_scrollTimer(this, "scroll timer") {
	setRulerModel(model);
	setValueScale(scale);
	setRange(min, max);

	doCommonCtor();
    }

    KRuler::KRuler(KRulerModel * model, QWidget * parent,
	const char *name):QWidget(parent, name), m_sizeHint(500, 32),
	m_backBuffer(500, 32), m_leftMostPixel(0), m_rulerModel(0),
	d(new KRulerPrivate()), m_minValue(0), m_maxValue(100),
	m_scrollTimer(this, "scroll timer") {
	setRulerModel(model);
	setValueScale(1);
	setRange(0, 100);

	doCommonCtor();
    }

    KRuler::KRuler(QWidget * parent, const char *name):QWidget(parent,
	name), m_sizeHint(500, 32), m_backBuffer(500, 32),
	m_leftMostPixel(0), m_rulerModel(0), d(new KRulerPrivate()),
	m_minValue(0), m_maxValue(0), m_scrollTimer(this, "scroll timer") {
	setRulerModel(0);
	setValueScale(1);
	setRange(0, 100);

	doCommonCtor();
    }

    void KRuler::doCommonCtor() {
	setStartPixel(0);
	m_markerPixmap = KGlobal::iconLoader()->loadIcon("kdenlive_guide", KIcon::Small, 15);
	setMinimumHeight(16);
	setMinimumWidth(32);
	setMaximumHeight(32);
	setMouseTracking(true);

	setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
		QSizePolicy::Preferred, FALSE));

	invalidateBackBuffer();

	// we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);

	connect(&m_scrollTimer, SIGNAL(timeout()), this,
	    SLOT(slotTimerScrollEvent()));
    }

    KRuler::~KRuler() {
	delete d;
	delete m_rulerModel;
    }

    void KRuler::tip(const QPoint &pos, QRect &rect, QString &tipText) {

        QValueList < KTimelineGuide >::Iterator itt = m_guides.begin();
	uint ct = 0;
        for ( itt = m_guides.begin(); itt != m_guides.end(); ++itt ) {
	    int value = (int) mapValueToLocal((*itt).guidePosition());
	    if (abs (pos.x() - value) < 7 ) { 
		rect.setRect( value -7, y(), 15, height());
		tipText = (*itt).guideComment();
		break;
	    }
	    ct++;
	}
    }

    void KRuler::paintEvent(QPaintEvent * event) {
	RangeListIterator < int >itt(d->m_bufferDrawList);
        QPainter painter(this);
        
	while (!itt.finished()) {
	    drawToBackBuffer(itt.start(), itt.end());
	    ++itt;
	}
	d->m_bufferDrawList.clear();

	painter.drawPixmap(event->rect().x(), event->rect().y(), m_backBuffer, 
        event->rect().x(), event->rect().y(), event->rect().width(), event->rect().height());
    }

    inline void KRuler::drawSmallTick(QPainter & painter, int pixel) {
	int lineHeight = height() / 8;
	if (lineHeight == 0)
	    lineHeight = 1;
	painter.drawLine(pixel, 0, pixel, lineHeight);
	painter.drawLine(pixel, height() - lineHeight, pixel, height());
    }

    inline void KRuler::drawBigTick(QPainter & painter, int pixel) {
	int lineHeight = height() / 4;
	if (lineHeight < 2)
	    lineHeight = 2;

	painter.drawLine(pixel, 0, pixel, lineHeight);
	painter.drawLine(pixel, height() - lineHeight, pixel, height());
    }

    QSize KRuler::sizeHint() {
	return m_sizeHint;
    }

    void KRuler::setStartPixel(int value) {
	if (m_leftMostPixel != value) {
	    m_leftMostPixel = value;
	    invalidateBackBuffer(0, width());
	}
    }

    void KRuler::setValueScale(double size) {
	m_xValueSize = size;
	int tick;

	if (m_xValueSize == 0) {
	    kdError() <<
		"KRuler::setValueScale : m_xValueSize is 0, cannot set ticks!!!"
		<< endl;
	}

	for (tick = 1;
	    tick * m_xValueSize < m_rulerModel->minimumTextSeparation();
	    tick *= 2);
	m_textEvery = m_rulerModel->getTickDisplayInterval(tick);

	for (tick = 1;
	    tick * m_xValueSize <
	    m_rulerModel->minimumLargeTickSeparation(); tick *= 2);
	m_bigTickEvery = m_rulerModel->getTickDisplayInterval(tick);

	for (tick = 1;
	    tick * m_xValueSize <
	    m_rulerModel->minimumSmallTickSeparation(); tick *= 2);

	m_smallTickEvery = m_rulerModel->getTickDisplayInterval(tick);

	while (m_textEvery % m_bigTickEvery != 0) {
	    m_textEvery =
		m_rulerModel->getTickDisplayInterval(m_textEvery + 1);
	}

	while (m_bigTickEvery % m_smallTickEvery != 0) {
	    m_smallTickEvery =
		m_rulerModel->getTickDisplayInterval(m_smallTickEvery + 1);
	}

	if (m_smallTickEvery == m_bigTickEvery) {
	    m_bigTickEvery =
		m_rulerModel->getTickDisplayInterval(m_smallTickEvery + 1);
	}

	emit scaleChanged(size);

	invalidateBackBuffer(0, width());
    }

    void KRuler::resizeEvent(QResizeEvent * event) {
	m_backBuffer.resize(width(), height());
	d->m_bufferDrawList.setFullRange(0, width());

	invalidateBackBuffer(0, width());

	emit resized();
    }

    double KRuler::mapValueToLocal(double value) const {
	return ((value * m_xValueSize) - m_leftMostPixel);
    } 
    
    double KRuler::mapLocalToValue(double x) const {
	return (x + m_leftMostPixel) / m_xValueSize;
    } 
    
    int KRuler::addSlider(KRulerSliderType type, int value) {
	KRulerPrivateSlider s(d->m_id, type, value, QPalette::Inactive);
	d->m_sliders.append(s);
	emit sliderValueChanged(d->m_id, value);
	return d->m_id++;
    }

    void KRuler::deleteSlider(int id) {
	QValueList < KRulerPrivateSlider >::Iterator it;

	for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
	    if ((*it).getID() == id) {
		d->m_sliders.remove(it);
		return;
	    }
	}

	kdWarning() << "KRuler::deleteSlider(id) : id " << id <<
	    " does not exist, no deletion occured!" << endl;
    }

    void KRuler::setSliderValue(int id, int value) {
	int actValue = (value < minValue())? minValue() : value;
	actValue = (actValue > maxValue())? maxValue() : actValue;
        int oldValue = 0;
	QValueList < KRulerPrivateSlider >::Iterator it;

	if (id == 1 || id == 2) { // snap to guides
	    QValueList < KTimelineGuide >::Iterator itt = m_guides.begin();
            for ( itt = m_guides.begin(); itt != m_guides.end(); ++itt ) {
	        int pos = (int) mapValueToLocal((*itt).guidePosition());
		int sliderpos = (int) mapValueToLocal(actValue);
	        if (abs(pos - sliderpos) < 10) {
		    actValue = (*itt).guidePosition();
		    break;
		}
	    }
	}

	for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
	    if ((*it).getID() == id) {
                oldValue = (*it).getValue();
                int oldStart = (*it).leftBound((int) mapValueToLocal((*it).getValue()), height());
                int oldEnd = (*it).rightBound((int) mapValueToLocal((*it).getValue()), height()+1);

		if ((*it).setValue(actValue)) {
                    if ((*it).leftBound((int) mapValueToLocal((*it).
                          getValue()), height()) < oldStart) 
                        invalidateBackBuffer((*it).leftBound((int) mapValueToLocal((*it).
                                getValue()), height()), oldEnd);
                    
                    else invalidateBackBuffer(oldStart, (*it).rightBound((int) mapValueToLocal((*it).
                                getValue()), height()) + 1);
		    emit sliderValueChanged(id, actValue);
                    if (id == 0) emit sliderValueMoved(oldValue, actValue);
		    break;
		}
	    }
	}
    }

    void KRuler::activateSliderUnderCoordinate(int x, int y) {
	QValueList < KRulerPrivateSlider >::Iterator it;
	for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
	    if ((*it).underMouse(x, y,
		    (int) mapValueToLocal((*it).getValue()), height())) {
		if ((*it).status() != QPalette::Disabled)
		    activateSlider((*it).getID());
		return;
	    }
	}
	activateSlider(m_autoClickSlider);
    }

    void KRuler::activateSlider(int id) {
	QValueList < KRulerPrivateSlider >::Iterator it;

	for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
	    if ((*it).status() != QPalette::Disabled) {
		(*it).setStatus(QPalette::Inactive);
		if ((*it).getID() == id) {
		    (*it).setStatus(QPalette::Active);
		}
	    }
	}
	d->m_activeID = id;
    }

    int KRuler::getSliderValue(int id) {
	QValueList < KRulerPrivateSlider >::Iterator it;

	for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
	    if ((*it).getID() == id) {
		return (*it).getValue();
	    }
	}

	kdWarning() <<
	    "KRuler : getSliderValue(id) attempt has been made to get the value of non-existant slider "
	    << id << endl;
	return 0;
    }

    int KRuler::activeSliderID() {
	return d->m_activeID;
    }

    void KRuler::setMinValue(int value) {
	m_minValue = value;
	setSlidersToRange();
    }

    void KRuler::setMaxValue(int value) {
	m_maxValue = (value > m_minValue) ? value : m_minValue;
	setSlidersToRange();
    }

    void KRuler::setRange(int min, int max) {
	int oldvalue = 0;

	if (min != minValue()) {
	    oldvalue = minValue();
	    setMinValue(min);
	    invalidateBackBuffer((int) mapValueToLocal(min),
		(int) mapValueToLocal(oldvalue));
	}
	if (max != maxValue()) {
	    oldvalue = maxValue();
	    setMaxValue(max);
	    invalidateBackBuffer((int) mapValueToLocal(oldvalue),
		(int) mapValueToLocal(max));
	}
	setSlidersToRange();
    }

    int KRuler::minValue() const {
	return m_minValue;
    } 
    
    int KRuler::maxValue() const {
	return m_maxValue;
    } 
    
    void KRuler::mousePressEvent(QMouseEvent * event) {
	if (event->button() == QMouseEvent::RightButton) {
		//setSliderValue(0,  (int) floor(mapLocalToValue((int) event->x()) + 0.5));
		d->m_oldValue = -1;
		emit rightButtonPressed();
	} else if (event->button() == QMouseEvent::LeftButton) {
	    if (d->m_oldValue == -1) {
		activateSliderUnderCoordinate(event->x(), event->y());
		d->m_oldValue = getSliderValue(activeSliderID());
	    }
	}
    }

    void KRuler::mouseDoubleClickEvent(QMouseEvent * event) {
		activateSlider(3);
		setSliderValue(3, (int) floor(mapLocalToValue((int) event->x()) + 0.5));
    }

    void KRuler::mouseReleaseEvent(QMouseEvent * event) {
	if (event->button() == QMouseEvent::LeftButton) {
	    if (d->m_oldValue != -1) {
		d->m_oldValue = -1;
		setSliderValue(activeSliderID(),
		    (int) floor(mapLocalToValue((int) event->x()) + 0.5));
	    }
	    m_scrollTimer.stop();
	}
    }

    void KRuler::mouseMoveEvent(QMouseEvent * event) {
	if (event->state() & (QMouseEvent::LeftButton | QMouseEvent::RightButton | QMouseEvent::MidButton)) {
	    if (d->m_oldValue != -1) {
		setSliderValue(activeSliderID(), (int) (mapLocalToValue((int) event->x())));

    if (event->x() < (int) g_scrollThreshold) {
		    m_scrollRight = false;
		    if (!m_scrollTimer.isActive()) {
			m_scrollTimer.start(g_scrollTimerDelay, false);
		    }
    } else if (width() - event->x() < (int) g_scrollThreshold) {
		    m_scrollRight = true;
		    if (!m_scrollTimer.isActive()) {
			m_scrollTimer.start(g_scrollTimerDelay, false);
		    }
		} else {
		    m_scrollTimer.stop();
		}
	    }
	}
    }
    
    void KRuler::wheelEvent ( QWheelEvent * e ) {
        if (( e->state() & ControlButton) == ControlButton) { // If Ctrl is pressed, move faster
            if (e->delta() > 0) emit moveBackward(true);
            else emit moveForward(true);
            e->accept();
        }
        else {
            if (e->delta() > 0) emit moveBackward(false);
            else emit moveForward(false);
            e->accept();
        }
    }

    void KRuler::setRulerModel(KRulerModel * model) {
	if (m_rulerModel != 0) {
	    delete m_rulerModel;
	    m_rulerModel = 0;
	}

	if (model == 0) {
	    model = new KRulerModel();
	}
	m_rulerModel = model;
    }

    double KRuler::valueScale() {
	return m_xValueSize;
    }

    void KRuler::setAutoClickSlider(int ID) {
	m_autoClickSlider = ID;
    }

    void KRuler::drawToBackBuffer(int start, int end) {
	QPainter painter(&m_backBuffer);
	painter.setClipRect(start, 0, end - start, height());
	int value;
	double pixel;
	double pixelIncrement;
	int sx, ex;

	sx = start;
	ex = end;

	// draw background, adding different colors before the start, and after the end values.

	int startRuler = (int) mapValueToLocal(minValue());
	int endRuler =
	    (int) mapValueToLocal((maxValue() >
		minValue())? maxValue() : maxValue() + 1);

	if (startRuler > sx) {
	    painter.fillRect(sx, 0, startRuler - sx, height(),
		palette().active().background());
	} else {
	    startRuler = sx;
	}
	if (endRuler > ex) {
	    endRuler = ex;
	}
	painter.fillRect(startRuler, 0, endRuler - startRuler, height(),
	    palette().active().base());
	if (endRuler < ex) {
	    painter.fillRect(endRuler, 0, ex - endRuler, height(),
		palette().active().background());
	}

        // Red background for virtual zones
	QValueList < QPoint >::Iterator vit;
	QBrush zoneBrush(QColor(253,85,83));
	zoneBrush.setStyle(Qt::Dense4Pattern);
        for ( vit = m_vzones.begin(); vit != m_vzones.end(); ++vit ) {
	    int vstart = (int) mapValueToLocal((*vit).x());
	    int vend = (int) mapValueToLocal((*vit).y()) - vstart;
	    painter.fillRect(vstart, 1, vend, (int) height()/2, zoneBrush);
	}
        
        int selectedStart = 0;
        int selectedEnd = 0;
        
        // Yellow background for the selected area
        QValueList < KRulerPrivateSlider >::Iterator it;
        for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
            if ((*it).getID() == 1) {
                selectedStart = (int) mapValueToLocal((*it).getValue());
                if (sx >= selectedStart) selectedStart = sx;
            }
            else if ((*it).getID() == 2) {
                selectedEnd = (int) mapValueToLocal((*it).getValue());
                if (ex <= selectedEnd) selectedEnd = ex;
            }
        }
        
        if (selectedStart < selectedEnd)
            painter.fillRect(selectedStart, (int) height()/2, selectedEnd - selectedStart, (int) height()/2, QBrush(QColor(253,255,143)));

	painter.setPen(palette().active().foreground());
	painter.setBrush(palette().active().background());

	//
	// Special case draw for when the big ticks are always coincidental with small ticks.
	// Nicely stops rounding errors from creeping in, and should be slightly faster.
	//
	if (!(m_bigTickEvery % m_smallTickEvery)) {
	    value = (int) ((m_leftMostPixel + sx) / m_xValueSize);
	    value -= value % m_smallTickEvery;
	    pixel = (value * m_xValueSize) - m_leftMostPixel;
	    pixelIncrement = m_xValueSize * m_smallTickEvery;
	    // big tick every so many small ticks.
	    int bigTick2SmallTick = m_bigTickEvery / m_smallTickEvery;
	    value = (value % m_bigTickEvery) / m_smallTickEvery;

	    while (pixel < ex) {
		if (value) {
		    drawSmallTick(painter, (int) pixel);
		} else {
		    drawBigTick(painter, (int) pixel);
		}
		value = (value + 1) % bigTick2SmallTick;
		pixel += pixelIncrement;
	    }
	} else {
	    //
	    // Draw small ticks.
	    //

	    value = (int) ((m_leftMostPixel + sx) / m_xValueSize);
	    value -= value % m_smallTickEvery;
	    pixel = (value * m_xValueSize) - m_leftMostPixel;
	    pixelIncrement = m_xValueSize * m_smallTickEvery;

	    while (pixel < ex) {
		drawSmallTick(painter, (int) pixel);
		pixel += pixelIncrement;
	    }

	    //
	    // Draw Big ticks
	    //
	    value = (int) ((m_leftMostPixel + sx) / m_xValueSize);
	    value -= value % m_bigTickEvery;
	    pixel = (value * m_xValueSize) - m_leftMostPixel;
	    pixelIncrement = m_xValueSize * m_bigTickEvery;

	    while (pixel < ex) {
		drawBigTick(painter, (int) pixel);
		pixel += pixelIncrement;
	    }
	}

	//
	// Draw displayed text
	//

	value = (int) ((m_leftMostPixel + sx) / m_xValueSize);
	value -= value % m_textEvery;
	pixel = (value * m_xValueSize) - m_leftMostPixel;
	pixelIncrement = m_xValueSize * m_textEvery;

	while (value <=
	    ((m_leftMostPixel + ex) / m_xValueSize) + m_textEvery) {
	    painter.drawText((int) pixel - 50, 0, 100, height(),
		AlignCenter, m_rulerModel->mapValueToText(value));
	    value += m_textEvery;
	    pixel += pixelIncrement;
	}

	//
	// draw guide markers
	//
	QFont orig = painter.font();
	QFont ft = orig;
	ft.setPixelSize(11);
	painter.setFont(ft);

        QValueList < KTimelineGuide >::Iterator itt;
        for ( itt = m_guides.begin(); itt != m_guides.end(); ++itt ) {
	    value = (int) mapValueToLocal((*itt).guidePosition());
	    int chap = (*itt).chapterNum();
	    int offset = 7;
	    if (chap > -1) offset = 20;
	    if (value + offset >= sx && value - offset <= ex) {
		if ((*itt).chapterNum() == -1)
		    painter.drawPixmap(value - 7, height() - 20, m_markerPixmap);
		else {
		    QString txt;
		    if (chap == 1000) txt = ">";
		    else txt = QString::number(chap);
	    	    QRect textBound = painter.boundingRect(value - 20, height() -16, 40, 16, AlignCenter | AlignVCenter, txt);

		    painter.setBrush(QColor(253,120,40));
		    painter.drawRect(textBound.x() - 2, textBound.y(), textBound.width() + 4, textBound.height());

		    painter.setPen(Qt::white);
		    
		    painter.drawText(value - 20, height() -16, 40, 16, AlignCenter | AlignVCenter, txt);
		    painter.setPen(Qt::black);
		}
	    }
	}
	painter.setFont(orig);
	//
	// draw sliders
	//

	for (it = d->m_sliders.begin(); it != d->m_sliders.end(); it++) {
	    value = (int) mapValueToLocal((*it).getValue());

	    if ((ex >= (*it).leftBound(value, height()))
		&& (sx <= (*it).rightBound(value, height()))) {
		(*it).drawSlider(this, painter, value, height());
	    }
	}
	//update();
	painter.setClipping(false);
    }

    void KRuler::invalidateBackBuffer() {	
        invalidateBackBuffer(0, width());
    }

    void KRuler::invalidateBackBuffer(int start, int end) {
	d->m_bufferDrawList.addRange(start, end);
        // Optimise painting and redraw only around the moving cursor
        update(start, -1, end - start, height() + 1);
    }

    void KRuler::setSlidersToRange() {
	for (uint count = 0; count < d->m_sliders.size(); ++count) {
	    if (getSliderValue(count) < minValue())
		setSliderValue(count, minValue());
	    if (getSliderValue(count) > maxValue())
		setSliderValue(count, maxValue());
	}
    }

    void KRuler::slotTimerScrollEvent() {
	if (m_scrollRight) {
	    emit requestScrollRight();
	} else {
	    emit requestScrollLeft();
	}
    }

   QValueList < int > KRuler::timelineGuides() {
	QValueList < int > guideTimes;
 	QValueList < KTimelineGuide >::Iterator itt = m_guides.begin();
        for ( itt = m_guides.begin(); itt != m_guides.end(); ++itt ) {
		guideTimes.append((*itt).guidePosition());
	}
	return guideTimes;
    }

   void KRuler::slotAddGuide(int time, QString comment, int chapterNum) {

	int chapterCount = 0;
        QValueList < KTimelineGuide >::Iterator it = m_guides.begin();
        for ( it = m_guides.begin(); it != m_guides.end(); ++it ) {
	    if ((*it).guidePosition() >= time)
	    	break;
	    else 
	    if ((*it).chapterNum() > -1 && (*it).chapterNum() < 1000) chapterCount++;
        }

    	if ((it != m_guides.end()) && ((*it).guidePosition() == time)) {
	kdError() <<
	    "trying to add Guide that already exists, this will cause inconsistancies with undo/redo"
	    << endl;
    	} else {
	    if (chapterNum == -1 || chapterNum == 1000) chapterCount = chapterNum;
	    m_guides.insert(it, KTimelineGuide(time, comment, chapterCount));
	    if (chapterNum == 0) {
	        while (it != m_guides.end()) {
		    int cn = (*it).chapterNum();
	            if (cn > -1 && cn < 1000) (*it).setChapterNum(cn+1);
		    it++;
	    	}
		invalidateBackBuffer();
	    }
	    else invalidateBackBuffer(mapValueToLocal(time) - 20, mapValueToLocal(time) + 20);
	    
	}
    }

    void KRuler::slotDeleteGuide() {
	int localTime = (int) mapValueToLocal(getSliderValue(0));
	uint ct = 0;
	bool isChapter = false;
        QValueList < KTimelineGuide >::Iterator it = m_guides.begin();
        for ( it = m_guides.begin(); it != m_guides.end(); ++it ) {
	    if (abs(mapValueToLocal((*it).guidePosition()) - localTime) < 10)
	    	break;
	    ct++;
        }
    	if (it != m_guides.end()) {
	    if ((*it).chapterNum() != -1 && (*it).chapterNum() != 1000) isChapter = true;
	    int pos = (*it).guidePosition();
	    QValueList < KTimelineGuide >::Iterator toBeRemoved = it;
	    
	    // *it = m_guides[ct];
	    if (isChapter) {
		while (it != m_guides.end()) {
		    int cn = (*it).chapterNum();
	            if (cn > -1 && cn < 1000) (*it).setChapterNum(cn-1);
		    it++;
	    	}
		m_guides.erase(toBeRemoved);
		invalidateBackBuffer();
	    }
	    else {
		m_guides.erase(toBeRemoved);
		invalidateBackBuffer(mapValueToLocal(pos) - 20, mapValueToLocal(pos) + 20);
	    }
    	} else {
	    kdError()<<"CANNOT find guide to delete"<<endl;
	}
    }

    void KRuler::clearGuides() {
        m_guides.clear();
	invalidateBackBuffer();
    }

    void KRuler::slotEditGuide(QString comment, int chapter) {
	int localTime = (int) mapValueToLocal(getSliderValue(0));
	uint ct = 0;
        QValueList < KTimelineGuide >::Iterator it = m_guides.begin();
        for ( it = m_guides.begin(); it != m_guides.end(); ++it ) {
	    if (abs(mapValueToLocal((*it).guidePosition()) - localTime) < 10)
	    	break;
	    ct++;
        }
    	if (it != m_guides.end()) {
	    (*it).setComment(comment);
	    (*it).setChapterNum(chapter);
	    int pos = (*it).guidePosition();
	    invalidateBackBuffer(mapValueToLocal(pos) - 20, mapValueToLocal(pos) + 20);
	}
    }

    void KRuler::slotSetVZone(QValueList < QPoint > zones) {
	m_vzones = zones;
    }

    QStringList KRuler::timelineRulerComments() {
	QStringList list;
	QValueList < KTimelineGuide >::Iterator it;
	int ix = 0;
        for ( it = m_guides.begin(); it != m_guides.end(); ++it ) {
	    if ((*it).chapterNum() == -1) {
		QString cmt = (*it).guideComment();
		if (!cmt.isEmpty()) list<<cmt;
		else list<<i18n("Guide %1").arg(ix);
		ix++;
	    }
	    else {
		if ((*it).guideComment().isEmpty()) {
		    if ((*it).chapterNum() == 1000) list<<i18n("Chapter End");
	    	    else list<<i18n("Chapter %1").arg((*it).chapterNum());
	        }
	        else list<<(*it).guideComment();
	    }
        }
	return list;
    }

    QDomDocument KRuler::xmlGuides() {
        QDomDocument doc;
	QDomElement list = doc.createElement("guides");
	QValueList < KTimelineGuide >::Iterator it = m_guides.begin();
        for ( it = m_guides.begin(); it != m_guides.end(); ++it ) {
	    QDomElement guide = doc.createElement("guide");
	    guide.setAttribute("position", (*it).guidePosition());
	    guide.setAttribute("comment", (*it).guideComment());
	    guide.setAttribute("chapter", QString::number((*it).chapterNum()));
	    list.appendChild(guide);
        }
	doc.appendChild(list);
	return doc;
    }

    void KRuler::guidesFromXml(QDomElement xml) {
	QDomNode node = xml.firstChild();
	while (!node.isNull()) {
	    QDomElement element = node.toElement();
	    if (!element.isNull()) {
	    	if (element.tagName() == "guide") {
		slotAddGuide(element.attribute("position").toInt(), element.attribute("comment"), element.attribute("chapter").toInt());
	        }
	    }
	node = node.nextSibling();
	}
    }

    int KRuler::currentGuideIndex() {
	int localTime = (int) mapValueToLocal(getSliderValue(0));
	uint ct = 0;
        QValueList < KTimelineGuide >::Iterator it = m_guides.begin();
        for ( it = m_guides.begin(); it != m_guides.end(); ++it ) {
	    if (abs(mapValueToLocal((*it).guidePosition()) - localTime) < 10)
	    	break;
	    ct++;
        }
	if (it != m_guides.end()) return ct;
	return -1;
    }

    QString KRuler::guideComment(int ix) {
	if (ix != -1) return m_guides[ix].guideComment();
	return QString::null;
    }

    int KRuler::guidePosition(int ix) {
	if (ix != -1) return m_guides[ix].guidePosition();
	return 0;
    }

    int KRuler::guideChapter(int ix) {
	if (ix != -1) return m_guides[ix].chapterNum();
	return 0;
    }

}				// namespace Gui
