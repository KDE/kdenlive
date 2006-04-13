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

ScreenPreview::ScreenPreview(
        QCanvas& c, QWidget* parent,
        const char* name, WFlags f) :
                QCanvasView(&c,parent,name,f)
{
        //Create temp file that will be used for preview in the Mlt monitor
        tmp=new KTempFile(QString::null,".png");
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
                int pos=selectedItem->z();
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
        int x = (moving->x() + frameWidth/2 - imageWidth/2) * 100.0 /(frameWidth);
        int y = (moving->y() + frameHeight/2 - imageHeight/2) * 100.0 /(frameHeight);
        emit positionRect(x,y);

}

void ScreenPreview::startResize(QPoint p)
{
        // User wants to resize a rectangle
        delete selection;
        selection=0;
        operationMode=ResizeMode;
        draw_start=p;
        canvas()->update();
}

void ScreenPreview::contentsMousePressEvent(QMouseEvent* e)
{
        QPoint p = inverseWorldMatrix().map(e->pos());


        // Deselect item if user clicks in an empty area
        QCanvasItemList l=canvas()->collisions(p);
        if (l.isEmpty() || (l.first()->z()<0)) {
                canvas()->update();
                return;
        }

	QCanvasItemList::Iterator it=l.begin();

        // Otherwise, select item and prepare for moving
/*        moving = *it;
        selectedItem = moving;*/


                //selectRectangle(*it);
                //emit selectedCanvasItem((QCanvasRectangle*)(*it));

        moving_start = p;
        canvas()->update();
}


void ScreenPreview::selectRectangle(QCanvasItem *it)
{
        // Draw selection rectangle around selected item
        selection = new QCanvasRectangle(((QCanvasRectangle *)(it))->rect(),canvas());
        // set its Z index to 1000, so it is not drawn by the export routine
        selection->setZ(1000);
	QPen pen = QPen(QColor(120,60,60));
        pen.setStyle(Qt::DotLine);
        selection->setPen(pen);
        selection->show();
        canvas()->update();
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
        canvas()->update();
    }
}

void ScreenPreview::moveY(int y)
{
    if ( moving ) {
        setCursor(QCursor(Qt::SizeAllCursor));
        moving->setY(imageHeight/2 + frameHeight * y/100.0 - frameHeight/2);
        canvas()->update();
    }
}

void ScreenPreview::adjustSize(int x)
{
    if ( moving ) {
        setCursor(QCursor(Qt::SizeAllCursor));
        ((QCanvasRectangle*)(moving))->setSize(x/100.0 * frameWidth, x/100.0 * frameHeight);
        canvas()->update();
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


void ScreenPreview::exportContent()
{
        QPixmap im = drawContent();
        // Save resulting pixmap in a file for mlt
        im.save(tmp->name(),"PNG");
        tmp->sync();
        tmp->close();

        emit showPreview(tmp->name());
}

void ScreenPreview::exportContent(KURL url)
{
    QPixmap im = drawContent();
        // Save resulting pixmap in a file for mlt
    im.save(url.path(),"PNG");
}

void ScreenPreview::saveImage()
{
    QPixmap im = drawContent();
        // Save resulting pixmap in a file for mlt
    im.save(tmp->name(),"PNG");
    tmp->sync();
    tmp->close();
}

QPixmap ScreenPreview::drawContent()
{
           // Export content to a png image which can be used by mlt to create a video preview
        // All items are then drawed on the pixmap. To get transparency, it is required to
        // draw again all items on the alpha mask.

    QPixmap im(imageWidth,imageHeight);
    QPainter p;

        // Fill pixmap with color0, which sould be transparent but looks in fact to be black...
    im.fill(color0);

        // Create transparency mask
    im.setMask(im.createHeuristicMask());

        // Select all items
    QCanvasItemList list=canvas()->collisions(canvas()->rect());

        // Parse items in revers order to draw them on the pixmap
    QCanvasItemList::Iterator it = list.fromLast ();
    for (; it!=list.end(); --it) {
        if ( *it ) {
            if ((*it)->rtti ()==3) // text item
            {
                p.begin(&im);
                p.setPen(((QCanvasText*)(*it))->color());
                p.setFont(((QCanvasText*)(*it))->font());
                int wi=((QCanvasText*)(*it))->boundingRect().width()/2;
                int he=((QCanvasText*)(*it))->boundingRect().height();
                p.drawText(((QCanvasText*)(*it))->boundingRect(),Qt::AlignAuto,((QCanvasText*)(*it))->text());
                p.end();

                                // Draw again on transparency mask
                p.begin(im.mask());
                p.setPen(((QCanvasText*)(*it))->color());
                p.setFont(((QCanvasText*)(*it))->font());
                p.drawText(((QCanvasText*)(*it))->boundingRect(),Qt::AlignAuto,((QCanvasText*)(*it))->text());
                p.end();
            }

            if ((*it)->rtti ()==5 && (*it)->z()>=0 && (*it)->z()<1000) // rectangle item but don't draw the safe margins rectangle
            {
                p.begin(&im);
                p.setPen(((QCanvasPolygonalItem*)(*it))->pen());
                p.setBrush(((QCanvasPolygonalItem*)(*it))->brush());
                p.drawRect((*it)->x(),(*it)->y(),((QCanvasRectangle*)(*it))->width(),((QCanvasRectangle*)(*it))->height());
                p.end();

                                // Draw again on transparency mask
                p.begin(im.mask());
                p.setPen(QPen(color1,((QCanvasPolygonalItem*)(*it))->pen().width()));
                p.setBrush(QBrush(color1));
                p.drawRect((*it)->x(),(*it)->y(),((QCanvasRectangle*)(*it))->width(),((QCanvasRectangle*)(*it))->height());
                p.end();
            }
        }
    }
    return im;
}

QDomDocument ScreenPreview::toXml()
{
        // Select all items
    QCanvasItemList list=canvas()->allItems ();
    QDomDocument sceneList;
    QDomElement textclip = sceneList.createElement("textclip");
    sceneList.appendChild(textclip);
    
        // Parse items in revers order to draw them on the pixmap
    QCanvasItemList::Iterator it = list.fromLast ();
    for (; it!=list.end(); --it) {
        if ( *it ) {
            
            QDomElement producer = sceneList.createElement("object");
            producer.setAttribute("type", QString::number((*it)->rtti ()));
            producer.setAttribute("z", QString::number((*it)->z()));
            if ((*it)->rtti ()==3) {
                producer.setAttribute("color", ((QCanvasText*)(*it))->color().name());
                producer.setAttribute("font_family", ((QCanvasText*)(*it))->font().family());
                producer.setAttribute("font_size", QString::number(((QCanvasText*)(*it))->font().pointSize()));
                producer.setAttribute("text", ((QCanvasText*)(*it))->text());
                producer.setAttribute("x", QString::number(((QCanvasText*)(*it))->x()));
                producer.setAttribute("y", QString::number(((QCanvasText*)(*it))->y()));
            }
            else if ((*it)->rtti ()==5 && (*it)->z()>=0 && (*it)->z()<1000) {
                producer.setAttribute("color", ((QCanvasPolygonalItem*)(*it))->pen().color().name());
                producer.setAttribute("width", QString::number(((QCanvasRectangle*)(*it))->width()));
                producer.setAttribute("height", QString::number(((QCanvasRectangle*)(*it))->height()));
                producer.setAttribute("x", QString::number(((QCanvasRectangle*)(*it))->x()));
                producer.setAttribute("y", QString::number(((QCanvasRectangle*)(*it))->y()));
            }
            textclip.appendChild(producer);
        }
    }
    return sceneList;
}

void ScreenPreview::setXml(const QDomDocument &xml)
{
    QDomElement docElem = xml.documentElement();

    QDomNode n = docElem.firstChild();
    while( !n.isNull() ) {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        if( !e.isNull() ) {
            if (e.attribute("type")== "3") { // Insert text object
                QCanvasText* i = new QCanvasText(canvas());
                i->setZ(e.attribute("z").toDouble());
                i->setText(e.attribute("text"));
                i->setFont(QFont(e.attribute("font_family"),e.attribute("font_size").toInt()));
                i->setColor(e.attribute("color"));
                i->move(e.attribute("x").toDouble(),e.attribute("y").toDouble());
                i->show();
                numItems++;
            }
            else if (e.attribute("type")== "5") { // Insert rectangle object
                QCanvasRectangle* i = new QCanvasRectangle(QRect(e.attribute("x").toInt(), e.attribute("y").toInt(), e.attribute("width").toInt(), e.attribute("height").toInt()),canvas());
                i->setZ(e.attribute("z").toDouble());
                i->setBrush(QBrush(QColor(e.attribute("color"))));
                QPen pen = QPen(QColor(e.attribute("color")));
                pen.setWidth(0);
                i->setPen(pen);
                i->show();
                numItems++;
            }
        }
        n = n.nextSibling();
    }
    operationMode = CursorMode;
}


transitionPipWidget::transitionPipWidget(int width, int height, QWidget* parent, const char* name, WFlags fl ):
                transitionPip_UI(parent,name)
{
        frame_preview->setMinimumWidth(width);
	frame_preview->setMaximumWidth(width);
        frame_preview->setMinimumHeight(height);
	frame_preview->setMaximumHeight(height);
        canvas=new QCanvas(imageWidth,imageHeight);
        canview = new ScreenPreview(*canvas,frame_preview);
        canview->initRectangle(imageWidth/2-frameWidth/2,imageHeight/2-frameHeight/2,frameWidth,frameHeight);

        QHBoxLayout* flayout = new QHBoxLayout( frame_preview, 1, 1, "flayout");
        flayout->addWidget( canview, 1 );

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
        
        connect(radio_start, SIGNAL(stateChanged(int)), this, SLOT(changeKeyFrame(int)));
        m_transitionParameters[0]="0:0:100:0";
        m_transitionParameters[1]="50:0:100:0";
        changeKeyFrame(0);
}


transitionPipWidget::~transitionPipWidget()
{}


void transitionPipWidget::changeKeyFrame(int isOn)
{
    int x, y, size, transp, ix;
    if (isOn == QButton::On) ix = 0;
    else ix = 1;
    x = m_transitionParameters[ix].section(":",0,0).toInt();
    y = m_transitionParameters[ix].section(":",1,1).toInt();
    size = m_transitionParameters[ix].section(":",2,2).toInt();
    transp = m_transitionParameters[ix].section(":",3,3).toInt();
    slider_x->setValue(x);
    slider_y->setValue(y);
    slider_size->setValue(size);
    slider_transparency->setValue(transp);
}

void transitionPipWidget::adjustSize(int x)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s1 = m_transitionParameters[ix].section(":",0,1);
    QString s2 = m_transitionParameters[ix].section(":",3);
    m_transitionParameters[ix] = s1+":"+ QString::number(x)+":"+s2;
    canview->adjustSize(x);
}

void transitionPipWidget::adjustTransparency(int x)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s1 = m_transitionParameters[ix].section(":",0,2);
    m_transitionParameters[ix] = s1+":"+ QString::number(x);
    emit transitionChanged();
}

void transitionPipWidget::moveX(int x)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s = m_transitionParameters[ix].section(":",1);
    m_transitionParameters[ix] = QString::number(x)+":"+s;
    canview->moveX(x);
    emit transitionChanged();
}

void transitionPipWidget::moveY(int y)
{
    int ix;
    if (radio_start->isChecked()) ix = 0;
    else ix = 1;
    QString s1 = m_transitionParameters[ix].section(":",0,0);
    QString s2 = m_transitionParameters[ix].section(":",2);
    m_transitionParameters[ix] = s1+":"+ QString::number(y)+":"+s2;
    canview->moveY(y);
    emit transitionChanged();
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

void transitionPipWidget::doPreview()
{
        // Prepare for mlt preview
        canview->exportContent();
}

void transitionPipWidget::createImage(KURL url)
{
        // Save the title png image in url
    canview->exportContent(url);
}

KURL transitionPipWidget::previewFile()
{
    return KURL(canview->tmp->name());
}

void transitionPipWidget::setParameters(QString params)
{
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
    changeKeyFrame(QButton::On);

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

QPixmap transitionPipWidget::thumbnail(int width, int height)
{
    QPixmap pm = canview->drawContent();
    QImage  src = pm.convertToImage();
    QImage  dest = src.smoothScale( width, height );
    pm.convertFromImage( dest );

    return pm;
}

QDomDocument transitionPipWidget::toXml()
{
    return canview->toXml();
}

void transitionPipWidget::setXml(const QDomDocument &xml)
{
    canview->setXml(xml);
}


