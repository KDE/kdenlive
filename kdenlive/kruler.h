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

#ifndef KMMRULER_H
#define KMMRULER_H

#include <qwidget.h>
#include <qpixmap.h>
#include <krulermodel.h>

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
	enum KRulerSliderType {
		Diamond,
		TopMark,
		BottomMark,
		StartMark,
		EndMark,
	};
	/** Creates a default ruler, set to ManualScale mode, based at 0 and with a scaleFactor of
 	1 */
	KRuler(KRulerModel *model, QWidget *parent=0, const char *name=0);
	KRuler(QWidget *parent=0, const char *name=0);	

	virtual ~KRuler();
  void paintEvent(QPaintEvent *event);
  QSize sizeHint();
  /** No descriptions */
  void resizeEvent(QResizeEvent *event);
  /** Returns the value which is currently displayed at x in the ruler widget's local coordinate system. Takes and returns a double for accuracy. */
  double mapLocalToValue(double x) const;
  /** returns the x-value coordinate in the rulers widget's local coordinate space which maps to the value requested. This funtion takes and returns doubles, as does it's counterpart - mapLocalToValue() */
  double mapValueToLocal(double value) const;
  /** Adds a new slider to the ruler. The style in which the slider is drawn is determined by type, and the slider is initially set to value. The value returned is the id that should be used to move this slider. */
  int addSlider(const KRulerSliderType type, const int value);
  /** Deletes the slider with the given id. If no such slider exists, a warning will be issued for debugging purposes. */
  void deleteSlider(const int id);
  /** Draws the ruler to a back buffered QImage, but does not display it. This image can then be blitted straight to the screen for speedy drawing. */
  void drawToBackBuffer();
protected slots: // Public slots
	/** Sets the leftmost pixel which is displayed on the widget. To understand why this
	is in pixels rather than setting the value directly, remember that the timeline is
	essential a "view" into the full timeline. The timeline can be scaled, so that
	sometimes 1 pixel will be the equivalent of 1 second, sometimes 100 pixels will be.
	To perform smooth scaling, we can move the viewport based on pixels rather than
  value. */
  void setStartPixel(int value);
  /** Sets the size of each value on the timeline - i.e. it specifies how zoomed in the
  widget it. */
  void setValueScale(double size);
signals: // Signals
  /** This signal is emitted when the ruler is resized. */
  void resized();
private:
	QSize m_sizeHint;
  /** This is the back buffer image, to which the ruler is drawn before being displayed on screen. This helps to optimise drawing, and reduces flicker. */
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

	inline void drawSmallTick(QPainter &painter, const int pixel);
	inline void drawBigTick(QPainter &painter, const int pixel);
	KRulerPrivate *d;
protected: // Protected methods
  /** Handles mouse movement. This includes tracking which slider currently has focus, but not dragging
		of sliders around. */
  void mouseMoveEvent(QMouseEvent *event);
};

#endif
