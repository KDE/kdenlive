/***************************************************************************
                          transitionPipWidget  -  description
                             -------------------
    begin                : F� 2005
    copyright            : (C) 2005 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qlayout.h>
#include <qfont.h>
#include <qpoint.h>
#include <qcolor.h>
#include <qwmatrix.h>
#include <qtooltip.h>
#include <qspinbox.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <qimage.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qtoolbutton.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qcursor.h>

#include <kpushbutton.h>
#include <kfontcombo.h>
#include <kcolorcombo.h>
#include <kinputdialog.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <ktempfile.h>
#include <klocale.h>

#include "transitionpipwidget.h"
#include "kdenlive.h"

#define CursorMode 1
#define TextMode 2
#define RectMode 3
#define ResizeMode 4

//TODO don't hardcode image size
#define imageWidth 240
#define imageHeight 192
// safety margin for text
#define frameWidth 80
#define frameHeight 64


namespace Gui {

ScreenPreview::ScreenPreview(QCanvas& c, QWidget* parent, const char* name, WFlags f) :
                QCanvasView(&c,parent,name,f), m_silent(false)
{
        selection=0;
        numItems=0;
        drawingRect=0;
        selectedItem=0;

        //TODO make background color configurable
        canvas()->setBackgroundColor(black);

        // Draw the monitor rectangle
        QCanvasRectangle *marginRect = new QCanvasRectangle(QRect(imageWidth/2-frameWidth/2,imageHeight/2-frameHeight/2,frameWidth,frameHeight),canvas());
        marginRect->setZ(-100);
        marginRect->setPen(QPen(QColor(255,255,255)));
        marginRect->show();

        operationMode=CursorMode;
        setCursor(arrowCursor);

}


ScreenPreview::~ScreenPreview()
{
        // Delete temp file when it is not needed anymore
        //tmp->unlink();
}

void ScreenPreview::setSilent(bool silent) {
    m_silent = silent;
}

void ScreenPreview::initRectangle(int x, int y, int w, int h)
{
    // add screen rect
    QCanvasRectangle* i = new QCanvasRectangle(QRect(x,y,w,h),canvas());
    i->setZ(1);
    numItems++;
    i->setPen(QPen(QColor(200,0,0)));
    i->show();
    moving = i;
}


void ScreenPreview::resizeEvent ( QResizeEvent * e)
{
        //TODO make canvas keep a fixed ratio when resizing
        QWMatrix wm;
        wm.scale(((double) width()-10)/((double) imageWidth),((double) height()-10)/((double) imageHeight));
        setWorldMatrix (wm);
}



void ScreenPreview::contentsMouseReleaseEvent(QMouseEvent* e)
{
        // If user was resizing replace rect with the new one
        if (operationMode == ResizeMode) {
			  int pos=(int)selectedItem->z();
                delete selectedItem;
                delete drawingRect;
                drawingRect=0;
                QPoint p = inverseWorldMatrix().map(e->pos());
                emit addRect(QRect(draw_start,p),pos);
                return;
        }
        // If user was drawing a new rectangle, create it
        if (operationMode == RectMode) {
                delete drawingRect;
                drawingRect=0;
                QPoint p = inverseWorldMatrix().map(e->pos());
                if ((p-draw_start).manhattanLength()>20)
                        emit addRect(QRect(draw_start,p),-1);
                // If new rectangle is too tiny, don't create it, was probably a user mistake
                else {
                        operationMode=CursorMode;
                        setCursor(arrowCursor);
                        canvas()->update();
                        emit adjustButtons();
                }
        }
        // If user was moving an item, end the move
		  int x = (int)((moving->x() + frameWidth/2 - imageWidth/2) * 100.0 /(frameWidth));
		  int y = (int)((moving->y() + frameHeight/2 - imageHeight/2) * 100.0 /(frameHeight));
        emit positionRect(x,y);

}


void ScreenPreview::contentsMousePressEvent(QMouseEvent* e)
{
        QPoint p = inverseWorldMatrix().map(e->pos());


        // Deselect item if user clicks in an empty area
        QCanvasItemList l=canvas()->collisions(p);
        if (l.isEmpty() || (l.first()->z()<0)) {
                //canvas()->update();
                return;
        }

	QCanvasItemList::Iterator it=l.begin();
        moving_start = p;
        //canvas()->update();
}


void ScreenPreview::clear()
{
        // clear all canvas
        QCanvasItemList list = canvas()->allItems();
        QCanvasItemList::Iterator it = list.begin();
        for (; it != list.end(); ++it) {
                if ( *it )
                        delete *it;
        }
}


void ScreenPreview::moveX(int x)
{
    if ( moving ) {
        setCursor(QCursor(Qt::SizeAllCursor));
        moving->setX(imageWidth/2 + frameWidth * x/100.0 - frameWidth/2);
        if (!m_silent) canvas()->update();
    }
}

void ScreenPreview::moveY(int y)
{
    if ( moving ) {
        setCursor(QCursor(Qt::SizeAllCursor));
        moving->setY(imageHeight/2 + frameHeight * y/100.0 - frameHeight/2);
        if (!m_silent) canvas()->update();
    }
}

void ScreenPreview::adjustSize(int x)
{
    if ( moving ) {
        setCursor(QCursor(Qt::SizeAllCursor));
		  ((QCanvasRectangle*)(moving))->setSize((int)(x/100.0 * frameWidth), (int)(x/100.0 * frameHeight));
        if (!m_silent) canvas()->update();
    }
    
}

void ScreenPreview::contentsMouseMoveEvent(QMouseEvent* e)
{
        QPoint p = inverseWorldMatrix().map(e->pos());

        // move item
        if ( moving ) {
                setCursor(QCursor(Qt::SizeAllCursor));
                moving->moveBy(p.x() - moving_start.x(), p.y() - moving_start.y());
                moving_start = p;
                if (operationMode!=CursorMode) {
                        operationMode=CursorMode;
                        setCursor(arrowCursor);
                        emit adjustButtons();
                }
        }
        canvas()->setAllChanged ();
        canvas()->update();
}



transitionPipWidget::transitionPipWidget(KdenliveApp * app, int width, int height, QWidget* parent, const char* name, WFlags fl ):
        transitionPip_UI(parent,name), m_silent(false), m_app(app)
{
        /*frame_preview->setMinimumWidth(width);
	frame_preview->setMaximumWidth(width);
        frame_preview->setMinimumHeight(height);
	frame_preview->setMaximumHeight(height);*/
        canvas=new QCanvas(imageWidth,imageHeight);
        canview = new ScreenPreview(*canvas,frame_preview);
        canview->initRectangle(imageWidth/2-frameWidth/2,imageHeight/2-frameHeight/2,frameWidth,frameHeight);
	fixed_trans->setChecked(false);

        QHBoxLayout* flayout = new QHBoxLayout( frame_preview, 1, 1, "flayout");
        flayout->addWidget( canview, 1 );

	spin_size->setMaxValue(500);
	slider_size->setMaxValue(500);

        connect(slider_transparency, SIGNAL(valueChanged(int)), spin_transparency, SLOT(setValue(int)));
        connect(spin_transparency, SIGNAL(valueChanged(int)), slider_transparency, SLOT(setValue(int)));
        connect(slider_size, SIGNAL(valueChanged(int)), spin_size, SLOT(setValue(int)));
        connect(spin_size, SIGNAL(valueChanged(int)), slider_size, SLOT(setValue(int)));
        connect(slider_x, SIGNAL(valueChanged(int)), spin_x, SLOT(setValue(int)));
        connect(spin_x, SIGNAL(valueChanged(int)), slider_x, SLOT(setValue(int)));
        connect(slider_y, SIGNAL(valueChanged(int)), spin_y, SLOT(setValue(int)));
        connect(spin_y, SIGNAL(valueChanged(int)), slider_y, SLOT(setValue(int)));
        
        connect(spin_x, SIGNAL(valueChanged(int)), this, SLOT(moveX(int)));
        connect(spin_y, SIGNAL(valueChanged(int)), this, SLOT(moveY(int)));
        connect(canview, SIGNAL(positionRect(int, int)), this, SLOT(adjustSliders(int, int)));
        connect(spin_size, SIGNAL(valueChanged(int)), this, SLOT(adjustSize(int)));
        connect(spin_transparency, SIGNAL(valueChanged(int)), this, SLOT(adjustTransparency(int)));

        connect(slider_soft, SIGNAL(valueChanged(int)), spin_soft, SLOT(setValue(int)));
        connect(spin_soft, SIGNAL(valueChanged(int)), slider_soft, SLOT(setValue(int)));
        connect(spin_soft, SIGNAL(valueChanged(int)), this, SIGNAL(transitionChanged()));
	connect(luma_file, SIGNAL(activated(int)), this, SIGNAL(transitionChanged()));

        
        connect(radio_start, SIGNAL(stateChanged(int)), this, SLOT(changeKeyFrame(int)));
        connect(radio_start, SIGNAL(pressed()), this, SLOT(focusInOut()));
        connect(radio_end, SIGNAL(pressed()), this, SLOT(focusInOut()));

        connect(fixed_trans, SIGNAL(toggled(bool)), this, SLOT(duplicateKeyFrame(bool)));

        m_transitionParameters[0]="0:0:100:0";
        m_transitionParameters[1]="0:0:100:0";
        changeKeyFrame(radio_start->isChecked());
}


transitionPipWidget::~transitionPipWidget()
{
    if (canview) delete canview;
    if (canvas) delete canvas;
}

void transitionPipWidget::focusInOut()
{

    // Hack: if a spinbox has focus, we need to focus it out before changing keyframe so that a signal is emitted
    // or the value will be assigned to wrong keyframe
    if (spin_size->hasFocus()) slider_size->setFocus();
    if (spin_transparency->hasFocus()) slider_transparency->setFocus();
    if (spin_x->hasFocus()) slider_x->setFocus();
    if (spin_y->hasFocus()) slider_y->setFocus();

}

void transitionPipWidget::duplicateKeyFrame(bool isOn)
{
    if (!isOn) {
	radio_end->setEnabled(true);
	return;
    }
    radio_end->setEnabled(false);
    int current, toChange;
    if (radio_start->isChecked()) {
	current = 0;
	toChange = 1;
    }
    else {
    	current = 1;
	toChange = 0;
    }

    // size
    QString s1 = m_transitionParameters[current].section(":",0,1);
    QString s2 = m_transitionParameters[current].section(":",3);
    m_transitionParameters[toChange] = s1+":"+ QString::number(spin_size->value())+":"+s2;

    // transparency
    QString s = m_transitionParameters[current].section(":",0,2);
    m_transitionParameters[toChange] = s1+":"+ QString::number(spin_transparency->value());

    // x pos
    s = m_transitionParameters[current].section(":",1);
    m_transitionParameters[toChange] = QString::number(spin_x->value())+":"+s;

    // y pos
    s1 = m_transitionParameters[current].section(":",0,0);
    s2 = m_transitionParameters[current].section(":",2);
    m_transitionParameters[toChange] = s1+":"+ QString::number(spin_y->value())+":"+s2;
    if (current == 1) radio_start->setChecked(true);
    emit transitionChanged();

}

void transitionPipWidget::changeKeyFrame(int isOn)
{
    int x, y, size, transp, ix;
    if (isOn) ix = 0;
    else ix = 1;
    x = m_transitionParameters[ix].section(":",0,0).toInt();
    y = m_transitionParameters[ix].section(":",1,1).toInt();
    size = m_transitionParameters[ix].section(":",2,2).toInt();
    transp = m_transitionParameters[ix].section(":",3,3).toInt();
    m_silent = true;
    canview->setSilent(true);
    slider_x->setValue(x);
    slider_y->setValue(y);
    slider_size->setValue(size);
    slider_transparency->setValue(transp);
    m_silent = false;
    canview->setSilent(false);
    emit transitionChanged();
    canview->canvas()->update();
}

void transitionPipWidget::adjustSize(int x)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s1 = m_transitionParameters[ix].section(":",0,1);
    QString s2 = m_transitionParameters[ix].section(":",3);
    m_transitionParameters[ix] = s1+":"+ QString::number(x)+":"+s2;

    if (fixed_trans->isChecked()) {
	m_transitionParameters[1] = m_transitionParameters[0];
    }
    canview->adjustSize(x);
    if (!m_silent) {
	emit transitionChanged();
	m_app->focusTimelineWidget();
    }
}

void transitionPipWidget::adjustTransparency(int x)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s1 = m_transitionParameters[ix].section(":",0,2);
    m_transitionParameters[ix] = s1+":"+ QString::number(x);

    if (fixed_trans->isChecked()) {
	m_transitionParameters[1] = m_transitionParameters[0];
    }
    if (!m_silent) {
	emit transitionChanged();
	m_app->focusTimelineWidget();
    }
}

void transitionPipWidget::moveX(int x)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s = m_transitionParameters[ix].section(":",1);
    m_transitionParameters[ix] = QString::number(x)+":"+s;

    if (fixed_trans->isChecked()) {
	m_transitionParameters[1] = m_transitionParameters[0];
    }
    canview->moveX(x);
    if (!m_silent) {
	emit transitionChanged();
	m_app->focusTimelineWidget();
    }
}

void transitionPipWidget::moveY(int y)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s1 = m_transitionParameters[ix].section(":",0,0);
    QString s2 = m_transitionParameters[ix].section(":",2);
    m_transitionParameters[ix] = s1+":"+ QString::number(y)+":"+s2;

    if (fixed_trans->isChecked()) {
	m_transitionParameters[1] = m_transitionParameters[0];
    }
    canview->moveY(y);
    if (!m_silent) {
	emit transitionChanged();
	m_app->focusTimelineWidget();
    }
}

void transitionPipWidget::adjustSliders(int x, int y)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s = m_transitionParameters[ix].section(":",2);
    m_transitionParameters[ix] = QString::number(x)+":"+QString::number(y)+":"+s;
    spin_x->setValue(x);
    spin_y->setValue(y);
}


void transitionPipWidget::setParameters(QString params)
{
    if (params.isEmpty()) params = "0=0%,0%:100%x100%:100;-1=0%,0%:100%x100%:100";
    QString param1 = params.section(";",0,0);
    QString transp1 = QString::number(100-param1.section(":",-1).toInt());
    QString size1 = param1.section("x",1,1).section("%",0,0);
    QString x1 = param1.section("=",1,1).section("%",0,0);
    QString y1 = param1.section(",",1,1).section("%",0,0);
    
    QString param2 = params.section(";",1,1);
    QString transp2 = QString::number(100 - param2.section(":",-1).toInt());
    QString size2 = param2.section("x",1,1).section("%",0,0);
    QString x2 = param2.section("=",1,1).section("%",0,0);
    QString y2 = param2.section(",",1,1).section("%",0,0);
    
    m_transitionParameters[0]=x1+":"+y1+":"+size1+":"+transp1;
    m_transitionParameters[1]=x2+":"+y2+":"+size2+":"+transp2;
    changeKeyFrame(radio_start->isChecked());

}


QString transitionPipWidget::parameters()
{
    QString x1 = m_transitionParameters[0].section(":",0,0)+"%";
    QString y1 = m_transitionParameters[0].section(":",1,1)+"%";
    QString size1 = m_transitionParameters[0].section(":",2,2)+"%";
    QString transp1 = QString::number(100 - m_transitionParameters[0].section(":",3,3).toInt());
    
    QString x2 = m_transitionParameters[1].section(":",0,0)+"%";
    QString y2 = m_transitionParameters[1].section(":",1,1)+"%";
    QString size2 = m_transitionParameters[1].section(":",2,2)+"%";
    QString transp2 = QString::number(100 - m_transitionParameters[1].section(":",3,3).toInt());
    
    return QString("0="+x1+","+y1+":"+size1+"x"+size1+":"+transp1+";-1="+x2+","+y2+":"+size2+"x"+size2+":"+transp2);
}

}  //  end GUI namespace
