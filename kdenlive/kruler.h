/***************************************************************************
                          kmmruler.h  -  description
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

#ifndef KRULER_H
#define KRULER_H

#include <qwidget.h>
#include <qpixmap.h>

#include "krulermodel.h"

/** The multimedia ruler used in the timeline. The ruler handles time in seconds,
  * and displays it in Hours:Minutes:Seconds, but this behaviour can be changed by
  * extending the class and overriding QString getTimeText(int), QString
  * as well as
	* the various markers that are required.
  *@author Jason Wood
  */

class KRulerPrivate;
  
class KRuler : public QWidget {
   Q_OBJECT
public:
/** The KRulerSliderType enumeration lists the standard slider types which can be used within the ruler.
For other types to be used, a custom slider class can be created. The names given are there to indicate
what kind of shape to expect the slider to be. A diamond should produce a roughly diamond like shape which
covers the timeline from top to bottom. The Top Mark and Bottom Marks should point from the top of the ruler
down, adn from the bottom up respectively. The start and end marks should point towards opposite ends of the
ruler, and could be used to indicate the start and end of a repeated section, for instance.*/
	enum KRulerSliderType {
		Diamond,
		TopMark,
		BottomMark,
		StartMark,
		EndMark,
	};
	/** Creates a default ruler, with the given model. Sets min/max range and the scale factor.*/
	KRuler(int min, int max, double scale=1.0, KRulerModel *model=0, QWidget *parent=0, const char *name=0);	 
	KRuler(KRulerModel *model, QWidget *parent=0, const char *name=0);
	KRuler(QWidget *parent=0, const char *name=0);	

	virtual ~KRuler();
	
  void paintEvent(QPaintEvent *event);
  QSize sizeHint();
  /** Returns the value which is currently displayed at x in the ruler widget's local coordinate
  system. Takes and returns a double for accuracy. */
  double mapLocalToValue(double x) const;
  /** returns the x-value coordinate in the rulers widget's local coordinate space which maps to the
   value requested. This funtion takes and returns doubles, as does it's counterpart - mapLocalToValue() */
  double mapValueToLocal(double value) const;
  /** Adds a new slider to the ruler. The style in which the slider is drawn is determined by type,
   and the slider is initially set to value. The value returned is the id that should be used to move
   this slider. */
  int addSlider(const KRulerSliderType type, const int value);
  /** Deletes the slider with the given id. If no such slider exists, a warning will be issued for
   debugging purposes. */
  void deleteSlider(const int id);
  /** Draws the ruler to a back buffered QImage, but does not display it. This image can then be
   blitted straight to the screen for speedy drawing. */
  void drawToBackBuffer();
  /** Get the value of the slider with the given id. */
  int getSliderValue(int id);
  /** Returns the id of the currently activated slider, or -1 if there isn't one. */
  int activeSliderID();
  /** Retursn the maximum value a slider can be set to on this ruler. */
  int maxValue() const;
  /** Returns the minimum value that a slider can be set to on this ruler. */
  int minValue() const;
  /** Changes the ruler model for this ruler. The old ruler model is deleted if it exists. If null is passed to this method, a default model is created. */
  void setRulerModel(KRulerModel *model);
public slots: // public slots
  /** Sets the slider with the given id to the given value. The display will be updated.  */
  void setSliderValue(const int id, const int value);

signals: // Signals
  /** This signal is emitted when the ruler is resized. */
  void resized();
  /** This signal is emitted whenever a sliders value changes. */
  void sliderValueChanged(int, int);    
  
protected slots: // Protected slots
	/** Sets the leftmost pixel which is displayed on the widget. To understand why this
	is in pixels rather than setting the value directly, remember that the timeline is
	essential a "view" into the full timeline. The timeline can be scaled, so that
	sometimes 1 pixel will be the equivalent of 1 second, sometimes 100 pixels will be.
	To perform smooth scaling, we can move the viewport based on pixels rather than
  value. */
  void setStartPixel(int value);
  /** Sets the size of each value on the timeline - i.e. it specifies how zoomed in the
  widget it. Note : this function has a bit of hackery to make sure that smal/large/display
  tick values are multiples of each other. This may cause unexpected results, but I haven't
  tested it enough yet to understand the implications.*/
  void setValueScale(double size);

private: // private variables
	QSize m_sizeHint;
  /** This is the back buffer image, to which the ruler is drawn before being displayed on
   screen. This helps to optimise drawing, and reduces flicker. */
  QPixmap m_backBuffer;	
  /** how many value increments should be between each displayed value.*/
	int m_textEvery;
  /** how many value increments should be between each big tick.*/
	int m_smallTickEvery;
  /** how many value increments should be between each small tick.*/
	int m_bigTickEvery;
  /** Size of each value on the screen. Measured in pixels.*/
	double m_xValueSize;
  /** the leftmost value that the timeline should draw.*/
	int m_leftMostPixel;
  /** The model used by this ruler. */
  KRulerModel *m_rulerModel;
	/** D pointer for private variables */
	KRulerPrivate *d;
  /** The minimum value range for this ruler */
  int m_minValue;
  /** The maximum value range for this ruler. */
  int m_maxValue;  

private: // private methods  
  /** Sets the slider under the specified coordinate to be active, and setting other sliders
  as inactive. This does not include disabled sliders. */
  void activateSliderUnderCoordinate(int x, int y);

  /** Sets the slider with the given id to be active. Sets all other sliders to be inactive. This
  does not include disabled sliders*/
  void activateSlider(int id);

	inline void drawSmallTick(QPainter &painter, const int pixel);
	inline void drawBigTick(QPainter &painter, const int pixel);
protected: // Protected methods
  /** Handles mouse movement. This includes tracking which slider currently has focus, but not dragging
		of sliders around. */
  void mouseMoveEvent(QMouseEvent *event);
  /** Called when the mouse has been released. */
  void mouseReleaseEvent(QMouseEvent *event);
  /** This method is called when the mouse is pressed */
  void mousePressEvent(QMouseEvent *event);  
  /** Handles window resize events. */
  void resizeEvent(QResizeEvent *event);  
  /** Sets the minimum value allowed by the ruler. If a slider is below this value at any point, it will be incremented so that it is at this value. If the part of the ruler which displays values less than this value is visible, it will be displayed in a different color to show that it is out of range. */
  void setMinValue(const int value);
  /** Sets the range of values used in the ruler. Sliders will always have a value between these two ranges, and any visible area of the ruler outside of this range will be shown in a different color to respect this. */
  void setRange(const int min, const int max);
  /** Sets the maximum value for this ruler. If a slider is ever set beyond this value, it will be reset to this value. If part of the ruler is visible which extends beyond this value, it will be drawn if a different colour to show that it is outside of the valid range of values. */
  void setMaxValue(const int value);
};

#endif
