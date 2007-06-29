/***************************************************************************
                          titlewidget  -  description
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
#include <qslider.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <qimage.h>
#include <qtoolbutton.h>
#include <qcursor.h>
#include <qcheckbox.h>

#include <kpushbutton.h>
#include <kfontcombo.h>
#include <kcolorbutton.h>
#include <kinputdialog.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <ktempfile.h>
#include <klocale.h>
#include <krestrictedline.h>

#include "titlewidget.h"
#include "kdenlivesettings.h"
#include "timecode.h"

#define CursorMode 1
#define TextMode 2
#define RectMode 3
#define ResizeMode 4

// safety margin for text
#define horizontalMarginSize 20
#define verticalMarginSize 20

FigureEditor::FigureEditor(
        QCanvas& c, QWidget* parent, KURL tmpUrl, const char* name, WFlags f) :
                QCanvasView(&c,parent,name,f)
{
        //Create temp file that will be used for preview in the Mlt monitor
	if (!tmpUrl.isEmpty()) tmpFileName = tmpUrl.path();
	else tmpFileName = KTempFile(KdenliveSettings::currenttmpfolder(),".png").name();
        selection = 0;
        moving = 0;
        numItems = 0;
        drawingRect = 0;
        selectedItem = 0;
        operationMode = CursorMode;
        m_isDrawing = false;
	m_transparent = false;

        // Enable focus to grab keyboard events
        setFocusPolicy(QWidget::StrongFocus);
        setFocus();

        //TODO make background color configurable
        canvas()->setBackgroundColor(black);
        viewport()->setMouseTracking(true);

        // Draw rectangle showing safety margins for the text
        QCanvasRectangle *marginRect = new QCanvasRectangle(QRect(horizontalMarginSize,verticalMarginSize,canvas()->width()-(2*horizontalMarginSize),canvas()->height()-(2*verticalMarginSize)),canvas());
        marginRect->setZ(-100);
        marginRect->setPen(QPen(QColor(255,255,255)));
        marginRect->show();
}


FigureEditor::~FigureEditor()
{
    if (selection) delete selection;
    if (selectedItem) delete selectedItem;
    if (drawingRect) delete drawingRect;
    if (moving) delete moving;
}


void FigureEditor::setTransparency ( bool isOn )
{
    m_transparent = isOn;
}


void FigureEditor::resizeEvent ( QResizeEvent * e)
{
        //TODO make canvas keep a fixed ratio when resizing
        QWMatrix wm;
        wm.scale(((double) width()-10)/((double) canvas()->width()),((double) height()-10)/((double) canvas()->height()));
        setWorldMatrix (wm);
}


void FigureEditor::contentsMouseDoubleClickEvent(QMouseEvent* e)
{
        // Double clicking on a text item opens the text edit widget
        if (selection)
                delete selection;
        selection=0;
        QPoint p = inverseWorldMatrix().map(e->pos());
        QCanvasItemList l=canvas()->collisions(p);
        if (l.isEmpty())
                return;
        if ( (l.first())->rtti() == 3 )
                emit editCanvasItem((QCanvasText*)(l.first()));
}


void FigureEditor::contentsMouseReleaseEvent(QMouseEvent* e)
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
                m_isDrawing = false;
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
        else if (moving) {
                moving=0;
                setCursor(QCursor(Qt::ArrowCursor));
        }
}

void FigureEditor::keyPressEvent ( QKeyEvent * e )
{		
	// delete item on del key press
    if (e->key()==Qt::Key_Delete) {
        if (selectedItem) {
            deleteItem(selectedItem);
            delete selection;
            selection=0;
            canvas()->update();
        }
    }
    else if (e->key()==Key_Left) {
        if (selectedItem) {
            selection->moveBy(-10, 0);
            selectedItem->moveBy(-10, 0);
            canvas()->setAllChanged ();
            canvas()->update();
        }
    } 
    else if (e->key()==Key_Right) {
        if (selectedItem) {
            selection->moveBy(10, 0);
            selectedItem->moveBy(10, 0);
            canvas()->setAllChanged ();
            canvas()->update();
        }
    } 
    else if (e->key()==Key_Up) {
        if (selectedItem) {
            selection->moveBy(0, -10);
            selectedItem->moveBy(0, -10);
            canvas()->setAllChanged ();
            canvas()->update();
        }
    } 
    else if (e->key()==Key_Down) {
        if (selectedItem) {
            selection->moveBy(0, 10);
            selectedItem->moveBy(0, 10);
            canvas()->setAllChanged ();
            canvas()->update();
        }
    } 
    else
        e->ignore();
}


void FigureEditor::alignModeChanged(int index)
{
    if(!selectedItem || selectedItem->rtti() != 3)
        return;

    // normalise position before
    if(((QCanvasText*)(selectedItem))->textFlags() & Qt::AlignRight)
        ((QCanvasText*)(selectedItem))->setX(((QCanvasText*)(selectedItem))->x() - ((QCanvasText*)(selectedItem))->boundingRect ().width());
    else if(((QCanvasText*)(selectedItem))->textFlags() & Qt::AlignCenter)
    {
        ((QCanvasText*)(selectedItem))->setX(((QCanvasText*)(selectedItem))->x() - ((QCanvasText*)(selectedItem))->boundingRect ().width() / 2.0f);
        
        ((QCanvasText*)(selectedItem))->setY(((QCanvasText*)(selectedItem))->y() - (((QCanvasText*)(selectedItem))->boundingRect ().height() / 2.0f));
    }
    
    if(index==0)
    {
        ((QCanvasText*)(selectedItem))->setTextFlags(Qt::AlignLeft);
    }
    else if(index == 1)
    {
        ((QCanvasText*)(selectedItem))->setTextFlags(Qt::AlignRight);
        ((QCanvasText*)(selectedItem))->setX(((QCanvasText*)(selectedItem))->x() + ((QCanvasText*)(selectedItem))->boundingRect ().width());
        
    }
    else if(index == 2)
    {
        ((QCanvasText*)(selectedItem))->setTextFlags(Qt::AlignCenter);
        ((QCanvasText*)(selectedItem))->setX(((QCanvasText*)(selectedItem))->x() + ((QCanvasText*)(selectedItem))->boundingRect ().width() / 2.0f);
        
        ((QCanvasText*)(selectedItem))->setY(((QCanvasText*)(selectedItem))->y() + (((QCanvasText*)(selectedItem))->boundingRect ().height() / 2.0f));
    }
    else if(index == 3)
        ((QCanvasText*)(selectedItem))->setTextFlags(Qt::AlignJustify);
    
    updateSelection();
    canvas()->update();
    
}


void FigureEditor::itemHCenter()
{
        // move item to horizontal center
    if (!selectedItem)
        return;
      
    if(((QCanvasText*)(selectedItem))->textFlags() == Qt::AlignRight)
        selectedItem->setX((int) ((canvas()->width()/2.0f)+(((QCanvasText*)(selectedItem))->boundingRect ().width()/2.0f)));
    else if(((QCanvasText*)(selectedItem))->textFlags() == Qt::AlignCenter)
        selectedItem->setX((int) ((canvas()->width()/2.0f)));
    else 
        selectedItem->setX((int) ((canvas()->width()/2.0f)-(((QCanvasText*)(selectedItem))->boundingRect ().width()/2.0f)));
    
    selection->setX((int) ((canvas()->width()/2.0f)-(((QCanvasText*)(selectedItem))->boundingRect ().width()/2.0f)));
    
    updateSelection();
    canvas()->update();
}


void FigureEditor::itemVCenter()
{
        // move item to vertical center
    if (!selectedItem)
        return;
        
    if(((QCanvasText*)(selectedItem))->textFlags() == Qt::AlignCenter)
        selectedItem->setY((int) ((canvas()->height()/2.0f)));
    else
        selectedItem->setY((int) ((canvas()->height()/2.0f)-(((QCanvasText*)(selectedItem))->boundingRect ().height()/2.0f)));
    
    selection->setX((int) ((canvas()->width()/2.0f)-(((QCanvasText*)(selectedItem))->boundingRect ().width()/2.0f)));
    
    updateSelection();
    canvas()->update();
}


void FigureEditor::itemUp()
{
        // move item up
        if (!selectedItem)
                return;
        QCanvasItemList list = canvas()->allItems();
        QCanvasItemList::Iterator it = list.begin();
        for (; it != list.end(); ++it)
                if ((*it)->z() == selectedItem->z()+1) {
                        (*it)->setZ(selectedItem->z());
                        selectedItem->setZ(selectedItem->z()+1);
                        break;
                }
        canvas()->update();
}


void FigureEditor::itemDown()
{
        // move item down
        if (!selectedItem)
                return;
        QCanvasItemList list = canvas()->allItems();
        QCanvasItemList::Iterator it = list.begin();
        for (; it != list.end(); ++it)
                if ((*it)->z() == selectedItem->z()-1) {
                        (*it)->setZ(selectedItem->z());
                        selectedItem->setZ(selectedItem->z()-1);
                        break;
                }
        canvas()->update();
}


void FigureEditor::deleteItem(QCanvasItem *i)
{
        // delete item
	int deletedIndex = (int)i->z();
        delete i;
        numItems--;
        QCanvasItemList list = canvas()->allItems();
        QCanvasItemList::Iterator it = list.begin();
        for (; it != list.end(); ++it) {
            if ((*it)->z()>deletedIndex && (*it)->z()<1000)
                        (*it)->setZ((*it)->z()-1);
        }
}


void FigureEditor::startResize(QPoint p)
{
        // User wants to resize a rectangle
        delete selection;
        selection=0;
        operationMode=ResizeMode;
        draw_start=p;
        canvas()->update();
}

void FigureEditor::contentsMousePressEvent(QMouseEvent* e)
{
        setFocus();
        QPoint p = inverseWorldMatrix().map(e->pos());
        // Create new item if user wants to
        if (operationMode!=CursorMode) {
                delete selection;
                selection=0;
                moving = 0;
                selectedItem = 0;
                if (operationMode == TextMode)
                        emit addText(p);
                if (operationMode == RectMode)
                        m_isDrawing = true;
                        draw_start=p;
                return;
        }

        // Deselect item if user clicks in an empty area
        QCanvasItemList l=canvas()->collisions(p);
        if (l.isEmpty() || (l.first()->z()<0)) {
                if (selection)
                        delete selection;
                selection = 0;
                moving = 0;
                selectedItem=0;
		emit emptySelection();
                canvas()->update();
                return;
        }

	QCanvasItemList::Iterator it=l.begin();
        // If user clicked in a rectangle corner, start resizing
        for (; it!=l.end(); ++it) {
            if (*it && (*it)->rtti()!=3 && (*it)->z()>=0 && (*it)->z()<1000) {
                uint dist=(p-((QCanvasRectangle *)(*it))->rect().topLeft()).manhattanLength();
                if (dist<20) {
                    selectedItem = *it;
                    emit selectedCanvasItem((QCanvasRectangle*)(*it));
                    startResize(((QCanvasRectangle *)(*it))->rect().bottomRight());
                        return;
                }
                dist=(p-((QCanvasRectangle *)(*it))->rect().topRight()).manhattanLength();
                if (dist<20) {
                    selectedItem = *it;
                    emit selectedCanvasItem((QCanvasRectangle*)(*it));
                    startResize(((QCanvasRectangle *)(*it))->rect().bottomLeft());
                        return;
                }
                dist=(p-((QCanvasRectangle *)(*it))->rect().bottomRight()).manhattanLength();
                if (dist<20) {
                    selectedItem = *it;
                    emit selectedCanvasItem((QCanvasRectangle*)(*it));
                    startResize(((QCanvasRectangle *)(*it))->rect().topLeft());
                        return;
                }
                dist=(p-((QCanvasRectangle *)(*it))->rect().bottomLeft()).manhattanLength();
                if (dist<20) {
                    selectedItem = *it;
                    emit selectedCanvasItem((QCanvasRectangle*)(*it));
                    startResize(((QCanvasRectangle *)(*it))->rect().topRight());
                        return;
                }
            }
        }

        // Otherwise, select item and prepare for moving
        it = l.begin();
        if (*it == selection) it++;
        moving = *it;
        selectedItem = moving;
        if (selection)
                delete selection;
        selection=0;

        if ( (*it)->rtti() == 3 ) {
                selectRectangle(*it);
                emit selectedCanvasItem((QCanvasText*)(*it));
        }
        else if ( (*it)->rtti() == 5 ) {
                selectRectangle(*it);
                emit selectedCanvasItem((QCanvasRectangle*)(*it));
        }
        moving_start = p;
        canvas()->update();
}


void FigureEditor::selectRectangle(QCanvasItem *it)
{
        // Draw selection rectangle around selected item
        if ( (it)->rtti() == 3 )
                selection = new QCanvasRectangle(((QCanvasText*)(it))->boundingRect (),canvas());
        else
                if ( (it)->rtti() == 5 )
                        selection = new QCanvasRectangle(((QCanvasRectangle *)(it))->rect(),canvas());
        // set its Z index to 1000, so it is not drawn by the export routine
        selection->setZ(1000);
	QPen pen = QPen(QColor(120,60,60));
        pen.setStyle(Qt::DotLine);
        selection->setPen(pen);
        selection->show();
        canvas()->update();
}


void FigureEditor::updateSelection()
{
    if (!selectedItem)
        return;

	// Update selection rectangle
    delete selection;
    selectRectangle(selectedItem);
}


void FigureEditor::clear()
{
        // clear all canvas
        QCanvasItemList list = canvas()->allItems();
        QCanvasItemList::Iterator it = list.begin();
        for (; it != list.end(); ++it) {
                if ( *it )
                        delete *it;
        }
}


void FigureEditor::changeTextSize(int newSize)
{
    if (!selectedItem || selectedItem->rtti()!=3)
        return;
	
    QFont *tempFont = new QFont(((QCanvasText*)(selectedItem))->font().family(),newSize);
    tempFont->setStrikeOut((((QCanvasText*)(selectedItem))->font().strikeOut()));
    tempFont->setBold((((QCanvasText*)(selectedItem))->font().bold()));
    tempFont->setItalic((((QCanvasText*)(selectedItem))->font().italic()));
    tempFont->setUnderline((((QCanvasText*)(selectedItem))->font().underline()));

    ((QCanvasText*)(selectedItem))->setFont(*tempFont);

        // Update selection rectangle
    updateSelection();

    canvas()->update();
}

void FigureEditor::changeTextFace(const QString & newFace)
{
    if (!selectedItem || selectedItem->rtti()!=3)
        return;

    QFont *tempFont = new QFont(newFace,((QCanvasText*)(selectedItem))->font().pointSize());
    tempFont->setStrikeOut((((QCanvasText*)(selectedItem))->font().strikeOut()));
    tempFont->setBold((((QCanvasText*)(selectedItem))->font().bold()));
    tempFont->setItalic((((QCanvasText*)(selectedItem))->font().italic()));
    tempFont->setUnderline((((QCanvasText*)(selectedItem))->font().underline()));

    ((QCanvasText*)(selectedItem))->setFont(*tempFont);

        // Update selection rectangle
    updateSelection();

    canvas()->update();
}

void FigureEditor::changeColor(const QColor & newColor)
{
    if (!selectedItem)
        return;
    if (selectedItem->rtti()==3)
        ((QCanvasText*)(selectedItem))->setColor(newColor);
    if (selectedItem->rtti()==5) {
        ((QCanvasRectangle*)(selectedItem))->setBrush(newColor);
        ((QCanvasRectangle*)(selectedItem))->setPen(QPen(newColor));
    }
    canvas()->update();
}

void FigureEditor::toggleBold()
{
    if (!selectedItem)
        return;

    if (selectedItem->rtti()==3)
    {
        QFont *tempFont = new QFont(((QCanvasText*)(selectedItem))->font().family(),((QCanvasText*)(selectedItem))->font().pointSize());
        tempFont->setBold(!(((QCanvasText*)(selectedItem))->font().bold()));		
        tempFont->setItalic((((QCanvasText*)(selectedItem))->font().italic()));
        tempFont->setStrikeOut((((QCanvasText*)(selectedItem))->font().strikeOut()));
        tempFont->setUnderline((((QCanvasText*)(selectedItem))->font().underline()));

        ((QCanvasText*)(selectedItem))->setFont(*tempFont);
		
        updateSelection();

        canvas()->update();
    }
}


void FigureEditor::toggleItalic()
{
    if (!selectedItem)
        return;
	
    if (selectedItem->rtti()==3)
    {
        QFont *tempFont = new QFont(((QCanvasText*)(selectedItem))->font().family(),((QCanvasText*)(selectedItem))->font().pointSize());
        tempFont->setItalic(!(((QCanvasText*)(selectedItem))->font().italic()));
        tempFont->setBold((((QCanvasText*)(selectedItem))->font().bold()));
        tempFont->setStrikeOut((((QCanvasText*)(selectedItem))->font().strikeOut()));
        tempFont->setUnderline((((QCanvasText*)(selectedItem))->font().underline()));

        ((QCanvasText*)(selectedItem))->setFont(*tempFont);
		
        updateSelection();

        canvas()->update();
    }	
}


void FigureEditor::toggleStrikeOut()
{
    if (!selectedItem)
        return;

    if (selectedItem->rtti()==3)
    {
        QFont *tempFont = new QFont(((QCanvasText*)(selectedItem))->font().family(),((QCanvasText*)(selectedItem))->font().pointSize());
        tempFont->setStrikeOut(!(((QCanvasText*)(selectedItem))->font().strikeOut()));
        tempFont->setBold((((QCanvasText*)(selectedItem))->font().bold()));
        tempFont->setItalic((((QCanvasText*)(selectedItem))->font().italic()));
        tempFont->setUnderline((((QCanvasText*)(selectedItem))->font().underline()));

        ((QCanvasText*)(selectedItem))->setFont(*tempFont);
	
        canvas()->update();
    }	
}


void FigureEditor::toggleUnderline()
{
    if (!selectedItem)
        return;

    if (selectedItem->rtti()==3)
    {
        QFont *tempFont = new QFont(((QCanvasText*)(selectedItem))->font().family(),((QCanvasText*)(selectedItem))->font().pointSize());
		
        tempFont->setUnderline(!(((QCanvasText*)(selectedItem))->font().underline()));
        tempFont->setStrikeOut((((QCanvasText*)(selectedItem))->font().strikeOut()));
        tempFont->setBold((((QCanvasText*)(selectedItem))->font().bold()));
        tempFont->setItalic((((QCanvasText*)(selectedItem))->font().italic()));

        ((QCanvasText*)(selectedItem))->setFont(*tempFont);
	
        canvas()->update();
    }	
}

void FigureEditor::contentsMouseMoveEvent(QMouseEvent* e)
{
        QPoint p = inverseWorldMatrix().map(e->pos());
        QCanvasItemList l=canvas()->collisions(p);
        
        // move item
        if ( moving ) {
                setCursor(QCursor(Qt::SizeAllCursor));
                selection->moveBy(p.x() - moving_start.x(), p.y() - moving_start.y());
                moving->moveBy(p.x() - moving_start.x(), p.y() - moving_start.y());
                moving_start = p;
                if (operationMode!=CursorMode) {
                        operationMode=CursorMode;
                        setCursor(arrowCursor);
                        emit adjustButtons();
                }
                canvas()->setAllChanged ();
                canvas()->update();
        }
        // Creating rectangle
        else if ((operationMode == RectMode && m_isDrawing) || operationMode == ResizeMode) {
                if (drawingRect)
                        delete drawingRect;
                drawingRect = new QCanvasRectangle(QRect(draw_start,p),canvas());
                drawingRect->setPen(QPen(yellow));
                drawingRect->setZ(1001);
                drawingRect->show();
                canvas()->setAllChanged ();
                canvas()->update();
        }
        else if (!l.isEmpty() && operationMode == CursorMode) {
            QCanvasItemList::Iterator it=l.begin();
            //if (*it) setCursor(crossCursor);
            bool isInCorner = false;
            bool found = false;
            bool reverseCursor = false;
            for (; it!=l.end(); ++it) {
                if (*it && (*it)->z()>=0 && (*it)->z()<1000) {
                    found = true;
                    if ((*it)->rtti ()==5) {// can only resize rectangle items
                    
                    uint dist=(p-((QCanvasRectangle *)(*it))->rect().topLeft()).manhattanLength();
                    if (dist<20) {
                        isInCorner = true;
                        reverseCursor = true;
                        break;
                    }
                    dist=(p-((QCanvasRectangle *)(*it))->rect().topRight()).manhattanLength();
                    if (dist<20) {
                        isInCorner = true;
                        break;
                    }
                    dist=(p-((QCanvasRectangle *)(*it))->rect().bottomRight()).manhattanLength();
                    if (dist<20) {
                        isInCorner = true;
                        reverseCursor = true;
                        break;
                    }
                    dist=(p-((QCanvasRectangle *)(*it))->rect().bottomLeft()).manhattanLength();
                    if (dist<20) {
                        isInCorner = true;
                        break;
                    }
                }
            }
            }
            if (found) {
                if (isInCorner) {
                    if (reverseCursor) setCursor(SizeFDiagCursor);
                    else setCursor(SizeBDiagCursor);
                }
                else setCursor(sizeAllCursor);
            }
            else setCursor(arrowCursor);
        }
}


void FigureEditor::exportContent(KURL url)
{
    QPixmap im = drawContent();
        // Save resulting pixmap in a file for mlt
    /*if (KdenliveSettings::videoprofile() == "dv_wide") {
        QImage  src = im.convertToImage();
        QImage  dest = src.smoothScale( KdenliveSettings::defaultwidth(),KdenliveSettings::defaultheight());
        im.convertFromImage( dest );
    }*/
    if (!url.isEmpty()) im.save(url.path(),"PNG");
    else im.save(tmpFileName,"PNG");
}

void FigureEditor::saveImage()
{
    QPixmap im = drawContent();
        // Save resulting pixmap in a file for mlt
    /*if (KdenliveSettings::videoprofile() == "dv_wide") {
        QImage  src = im.convertToImage();
        QImage  dest = src.smoothScale( KdenliveSettings::defaultwidth(),KdenliveSettings::defaultheight());
        im.convertFromImage( dest );
    }*/
    im.save(tmpFileName,"PNG");
}

QPixmap FigureEditor::drawContent()
{
           // Export content to a png image which can be used by mlt to create a video preview
        // All items are then drawed on the pixmap. To get transparency, it is required to
        // draw again all items on the alpha mask.

    QPixmap im(canvas()->width(), canvas()->height());
    QPainter p;

        // Fill pixmap with color0, which sould be transparent but looks in fact to be black...
    if (m_transparent) {
	im.fill(color0);
        // Create transparency mask
        im.setMask(im.createHeuristicMask());
        // Select all items
    }
    else im.fill(black);

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
               // int wi=((QCanvasText*)(*it))->boundingRect().width()/2;
               // int he=((QCanvasText*)(*it))->boundingRect().height();
                p.drawText(((QCanvasText*)(*it))->boundingRect(),((QCanvasText*)(*it))->textFlags(),((QCanvasText*)(*it))->text());
                p.end();

		if (m_transparent) {
                                // Draw again on transparency mask
                    p.begin(im.mask());
                    p.setPen(((QCanvasText*)(*it))->color());
                    p.setFont(((QCanvasText*)(*it))->font());
                    p.drawText(((QCanvasText*)(*it))->boundingRect(),((QCanvasText*)(*it))->textFlags(),((QCanvasText*)(*it))->text());
                    p.end();
		}
            }

            if ((*it)->rtti ()==5 && (*it)->z()>=0 && (*it)->z()<1000) // rectangle item but don't draw the safe margins rectangle
            {
                p.begin(&im);
                p.setPen(((QCanvasPolygonalItem*)(*it))->pen());
                p.setBrush(((QCanvasPolygonalItem*)(*it))->brush());
					 p.drawRect((int)((*it)->x()),(int)((*it)->y()),((QCanvasRectangle*)(*it))->width(),((QCanvasRectangle*)(*it))->height());
                p.end();

    		if (m_transparent) {
                                // Draw again on transparency mask
                    p.begin(im.mask());
                    p.setPen(QPen(color1,((QCanvasPolygonalItem*)(*it))->pen().width()));
                    p.setBrush(QBrush(color1));
		    p.drawRect((int)((*it)->x()),(int)((*it)->y()),((QCanvasRectangle*)(*it))->width(),((QCanvasRectangle*)(*it))->height());
                    p.end();
		}
            }
        }
    }
    return im;
}

QDomDocument FigureEditor::toXml()
{
        // Select all items
    QCanvasItemList list=canvas()->allItems ();
    QDomDocument sceneList;
    QDomElement textclip = sceneList.createElement("textclip");
    sceneList.appendChild(textclip);
    
        // Parse items in revers order to draw them on the pixmap
    QCanvasItemList::Iterator it = list.fromLast ();
    for (; it!=list.end(); --it) {
        if ( *it && (*it)->z()>=0 && (*it)->z()<1000) {
            
            QDomElement producer = sceneList.createElement("object");
            producer.setAttribute("type", QString::number((*it)->rtti ()));
            producer.setAttribute("z", QString::number((*it)->z()));
            if ((*it)->rtti ()==3) {
                producer.setAttribute("color", ((QCanvasText*)(*it))->color().name());
                producer.setAttribute("font_family", ((QCanvasText*)(*it))->font().family());
                producer.setAttribute("font_size", QString::number(((QCanvasText*)(*it))->font().pointSize()));
                producer.setAttribute("underline", QString::number(((QCanvasText*)(*it))->font().underline()));
                producer.setAttribute("bold", QString::number(((QCanvasText*)(*it))->font().bold()));
                producer.setAttribute("italic", QString::number(((QCanvasText*)(*it))->font().italic()));
                producer.setAttribute("strikeout", QString::number(((QCanvasText*)(*it))->font().strikeOut()));
                producer.setAttribute("textAlign", QString::number(((QCanvasText*)(*it))->textFlags()));
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

void FigureEditor::setXml(const QDomDocument &xml)
{
    QDomElement docElem = xml.documentElement();
    numItems = 0;
    QDomNode n = docElem.firstChild();
    while( !n.isNull() ) {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        if( !e.isNull() ) {
            if (e.attribute("type")== "3") { // Insert text object

                QFont *tempFont = new QFont(e.attribute("font_family"),e.attribute("font_size").toInt());
                tempFont->setBold(e.attribute("bold").toInt());
                tempFont->setItalic(e.attribute("italic").toInt());
                tempFont->setStrikeOut(e.attribute("strikeout").toInt());
                tempFont->setUnderline(e.attribute("underline").toInt());

                QCanvasText* i = new QCanvasText(canvas());
                i->setZ(e.attribute("z").toDouble());
                i->setText(e.attribute("text"));
                i->setFont(*tempFont);
                i->setColor(e.attribute("color"));
                i->setTextFlags(e.attribute("textAlign").toInt());
                i->move(e.attribute("x").toDouble(),e.attribute("y").toInt());
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


titleWidget::titleWidget(Gui::KMMScreen *screen, int width, int height, KURL tmpUrl, QWidget* parent, const char* name, WFlags fl ):
                titleBaseWidget(parent,name)
{
        frame->setMinimumWidth(width);
        frame->setMinimumHeight(height);
	QFont defFont = KdenliveSettings::titlerfont();
	m_screen = screen;

	fontFace->setCurrentFont(defFont.family());
	fontSize->setValue(defFont.pointSize());
	fontColor->setColor(KdenliveSettings::titlercolor());
        canvas=new QCanvas(width, height); //KdenliveSettings::defaultwidth(),KdenliveSettings::defaultheight());
        canview = new FigureEditor(*canvas, frame, tmpUrl);
	if (screen) {
	    int pos = screen->seekPosition().frames(KdenliveSettings::defaultfps()) * 100 / screen->getLength();
	    timelineSlider->setValue(pos);
	//canview->canvas()->setBackgroundPixmap(screen->extractFrame(pos, KdenliveSettings::defaultwidth(), KdenliveSettings::defaultheight()));
	}
	
        // Put icons on buttons
    textButton->setPixmap(KGlobal::iconLoader()->loadIcon("title_text",KIcon::Small,22));
    rectButton->setPixmap(KGlobal::iconLoader()->loadIcon("title_rect",KIcon::Small,22));
    cursorButton->setPixmap(KGlobal::iconLoader()->loadIcon("arrow",KIcon::Toolbar));
    upButton->setPixmap(KGlobal::iconLoader()->loadIcon("up",KIcon::Toolbar));
    downButton->setPixmap(KGlobal::iconLoader()->loadIcon("down",KIcon::Toolbar));
    centerLRButton->setPixmap(KGlobal::iconLoader()->loadIcon("hcenter",KIcon::Small,22));
    centerTBButton->setPixmap(KGlobal::iconLoader()->loadIcon("vcenter",KIcon::Small,22));
    boldButton->setPixmap(KGlobal::iconLoader()->loadIcon("text_bold",KIcon::Small,22));
    italicButton->setPixmap(KGlobal::iconLoader()->loadIcon("text_italic",KIcon::Small,22));
    strikeButton->setPixmap(KGlobal::iconLoader()->loadIcon("text_strike",KIcon::Small,22));
    underlineButton->setPixmap(KGlobal::iconLoader()->loadIcon("text_under",KIcon::Small,22));
	
    alignprobBox->insertItem ( KGlobal::iconLoader()->loadIcon("text_left",KIcon::Small,22), i18n( "Align Left" ));
    alignprobBox->insertItem ( KGlobal::iconLoader()->loadIcon("text_right",KIcon::Small,22), i18n( "Align Right" ));
    alignprobBox->insertItem ( KGlobal::iconLoader()->loadIcon("text_center",KIcon::Small,22), i18n( "Align Center" ));
    alignprobBox->insertItem ( KGlobal::iconLoader()->loadIcon("text_block",KIcon::Small,22), i18n( "Align Block" ));
			
    cursorButton->setOn(true);
    timelineposition->setEnabled(false);
    timelineSlider->setEnabled(false);

    QToolTip::add( textButton, i18n( "Add Text" ) );
    QToolTip::add( rectButton, i18n( "Add Rectangle" ) );
    QToolTip::add( cursorButton, i18n( "Select Objects" ) );
    QToolTip::add( upButton, i18n( "Bring to Front" ) );
    QToolTip::add( downButton, i18n( "Send to Background" ) );
    QToolTip::add( centerLRButton, i18n( "Center Horizontally" ) );
    QToolTip::add( centerTBButton, i18n( "Center Vertically" ) );

        QHBoxLayout* flayout = new QHBoxLayout( frame, 1, 1, "flayout");
        flayout->addWidget( canview, 1 );

    QObject::connect(canview,SIGNAL(adjustButtons()),this,SLOT(adjustButtons()));
    QObject::connect(canview,SIGNAL(addText(QPoint)),this,SLOT(addText(QPoint)));
    QObject::connect(textButton,SIGNAL(clicked()),this,SLOT(textMode()));
    QObject::connect(buttonOk,SIGNAL(clicked()),canview,SLOT(saveImage()));
    QObject::connect(transparentTitle, SIGNAL(toggled (bool)), this, SLOT(transparencyToggled(bool)));
    QObject::connect(cursorButton,SIGNAL(clicked()),this,SLOT(cursorMode()));
    QObject::connect(upButton,SIGNAL(clicked()),canview,SLOT(itemUp()));
    QObject::connect(downButton,SIGNAL(clicked()),canview,SLOT(itemDown()));
    QObject::connect(rectButton,SIGNAL(clicked()),this,SLOT(rectMode()));
    QObject::connect(canview,SIGNAL(addRect(QRect,int)),this,SLOT(addBlock(QRect,int)));
    QObject::connect(timelineSlider,SIGNAL(valueChanged(int)),this,SLOT(doPreview(int)));
    QObject::connect(boldButton,SIGNAL(clicked()),canview,SLOT(toggleBold()));
    QObject::connect(italicButton,SIGNAL(clicked()),canview,SLOT(toggleItalic()));
    QObject::connect(strikeButton,SIGNAL(clicked()),canview,SLOT(toggleStrikeOut()));
    QObject::connect(underlineButton,SIGNAL(clicked()),canview,SLOT(toggleUnderline()));
    QObject::connect(alignprobBox,SIGNAL(activated(int)),canview,SLOT(alignModeChanged(int)));
    
    QObject::connect(fontSize,SIGNAL(valueChanged(int)),canview,SLOT(changeTextSize(int)));
    QObject::connect(fontFace,SIGNAL(textChanged(const QString &)),canview,SLOT(changeTextFace(const QString &)));
    QObject::connect(fontColor,SIGNAL(changed(const QColor &)),canview,SLOT(changeColor(const QColor &)));
    QObject::connect(canview,SIGNAL(editCanvasItem(QCanvasText*)),this,SLOT(editText(QCanvasText*)));
    QObject::connect(canview,SIGNAL(selectedCanvasItem(QCanvasText*)),this,SLOT(adjustWidgets(QCanvasText*)));
    QObject::connect(canview,SIGNAL(selectedCanvasItem(QCanvasRectangle*)),this,SLOT(adjustWidgets(QCanvasRectangle*)));
    QObject::connect(canview,SIGNAL(emptySelection()),this,SLOT(adjustWidgets()));
    QObject::connect(timelineposition,SIGNAL(textChanged(const QString &)),this,SLOT(seekToPos(const QString &)));
    QObject::connect(centerLRButton,SIGNAL(clicked()),canview,SLOT(itemHCenter()));
    QObject::connect(centerTBButton,SIGNAL(clicked()),canview,SLOT(itemVCenter()));
}


titleWidget::~titleWidget()
{}

void titleWidget::transparencyToggled(bool isOn)
{
    timelineposition->setEnabled(isOn);
    timelineSlider->setEnabled(isOn);
    canview->setTransparency(isOn);
    doPreview(timelineSlider->value());
}


void titleWidget::adjustButtons()
{
        // switch buttons on/off depending on the action
        cursorButton->setOn(canview->operationMode == CursorMode);
        textButton->setOn(canview->operationMode == TextMode);
        rectButton->setOn(canview->operationMode == RectMode);
}


void titleWidget::cursorMode()
{
        canview->setCursor(arrowCursor);
        canview->operationMode=CursorMode;
        adjustButtons();
}

void titleWidget::textMode()
{
        canview->setCursor(ibeamCursor);
        canview->operationMode=TextMode;
        adjustButtons();
}

void titleWidget::rectMode()
{
        canview->setCursor(crossCursor);
        canview->operationMode=RectMode;
        adjustButtons();
}

void titleWidget::addText(QPoint p)
{

        QString txt=KInputDialog::getMultiLineText(i18n("Enter your text"),QString::null, QString::null,0,this);
        if (txt!=QString::null) {
                // Create new text item
                QCanvasText* i = new QCanvasText(canvas);
                i->setZ(canview->numItems);
                canview->numItems++;
                i->setText(txt);
                QFont *tempFont = new QFont(fontFace->currentFont(),fontSize->value());
            
                tempFont->setBold(boldButton->isDown());
                tempFont->setItalic(italicButton->isDown());
                tempFont->setStrikeOut(strikeButton->isDown());
                tempFont->setUnderline(underlineButton->isDown());

                i->setFont(*tempFont);
                i->setColor(fontColor->color());
                i->move(p.x(),p.y());
                i->show();

                // Select it
                delete canview->selection;
                canview->selection=0;
                canview->selectedItem=i;
                canview->selectRectangle(i);
                canvas->update();
        }
        canview->operationMode=CursorMode;
        canview->setCursor(arrowCursor);
        adjustButtons();

}

void titleWidget::addBlock(QRect rec,int pos)
{
        delete canview->selection;
        canview->selection=0;
        QCanvasRectangle* i = new QCanvasRectangle(rec,canvas);

        // Add new rectangle
        if (pos == -1) {
                i->setZ(canview->numItems);
                canview->numItems++;
        }
        // Resize existing rectangle
        else
                i->setZ(pos);

        i->setBrush(QBrush(fontColor->color()));
	QPen pen = QPen(fontColor->color());
        pen.setWidth(0);
        i->setPen(pen);
        i->show();

        // Select it
        canview->selectedItem=i;
        canview->selectRectangle(i);
        canview->operationMode=CursorMode;
        canview->setCursor(arrowCursor);
        canvas->update();
        adjustButtons();
}


void titleWidget::editText(QCanvasText* i)
{
        bool ok;
        QString txt=KInputDialog::getMultiLineText("Enter your text",QString::null, i->text(),&ok,this);
        if (!ok)
                return;
        // If empty text is returned, delete text item
        if (txt==QString::null) {
                canview->deleteItem((QCanvasItem *)(i));
                return;
        }
        i->setText(txt);

    i->show();
	
     // Select it
    canview->selectedItem=i;
    canview->selectRectangle(i);
    canview->operationMode=CursorMode;
    canview->setCursor(arrowCursor);
    canvas->update();
    adjustButtons();
    
    canvas->update();
    
}


void titleWidget::adjustWidgets()
{
        // User deselected item, reset buttons
    boldButton->setOn(false);
    italicButton->setOn(false);
    strikeButton->setOn(false);
    underlineButton->setOn(false);
    alignprobBox->setCurrentItem(0);
}


void titleWidget::adjustWidgets(QCanvasText* i)
{
        // Adjust font, size and color widgets according to the selected item
    fontFace->setCurrentFont(i->font().family());
    fontSize->setValue(i->font().pointSize ());
    fontColor->setColor(i->color());
    boldButton->setOn(i->font().bold());
    italicButton->setOn(i->font().italic());
    strikeButton->setOn(i->font().strikeOut());
    underlineButton->setOn(i->font().underline());
    
    int item = -1;
    if(i->textFlags() & Qt::AlignLeft)
        item = 0;
    if(i->textFlags() & Qt::AlignRight)
        item = 1;
    if(i->textFlags() & Qt::AlignCenter)
        item = 2;
    if(i->textFlags() & Qt::AlignJustify)
        item = 3;

    alignprobBox->setCurrentItem(item);
    
}

void titleWidget::adjustWidgets(QCanvasRectangle* i)
{
        // Adjust color widget according to the selected item
        fontColor->setColor(i->brush().color());
}

void titleWidget::doPreview(int pos)
{
        // Prepare for mlt preview
	if (!m_screen) return;
	int position = m_screen->getLength() * pos / 100;
	double fps = KdenliveSettings::defaultfps();
	Timecode tcode;
	if (fps == 30000.0 / 1001.0 ) tcode.setFormat(30, true);
        else tcode.setFormat(fps);
	timelineposition->setText(tcode.getTimecode(GenTime(position, fps), fps));
}

void titleWidget::seekToPos(const QString &)
{
	double fps = KdenliveSettings::defaultfps();
	Timecode tcode;
	if (fps == 30000.0 / 1001.0 ) tcode.setFormat(30, true);
        else tcode.setFormat(fps);
	QString dur = timelineposition->text();
	int frames = tcode.getFrameNumber(dur, fps);
	if (transparentTitle->isOn())
        canview->canvas()->setBackgroundPixmap(m_screen->extractFrame(frames, canvas->width(), canvas->height()));
	else canview->canvas()->setBackgroundPixmap(QPixmap());
}

void titleWidget::createImage(KURL url)
{
        // Save the title png image in url
    canview->exportContent(url);
}

KURL titleWidget::previewFile()
{
    return KURL(canview->tmpFileName);
}

QPixmap titleWidget::thumbnail(int width, int height)
{
    QPixmap pm = canview->drawContent();
    QImage  src = pm.convertToImage();
    QImage  dest = src.smoothScale( width, height );
    pm.convertFromImage( dest );
    return pm;
}

QDomDocument titleWidget::toXml()
{
    return canview->toXml();
}

void titleWidget::setXml(const QDomDocument &xml)
{
    canview->setXml(xml);
}


