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

#include <qcanvas.h>
#include <qpoint.h>
#include <qdom.h>

#include <ktempfile.h>
#include <kurl.h>

#include "transitionpip_ui.h"

#ifndef TRANSITIONPIPWIDGET_H
#define TRANSITIONPIPWIDGET_H

class ScreenPreview : public QCanvasView
{
        Q_OBJECT

public:
        ScreenPreview(QCanvas&, QWidget* parent=0, const char* name=0, WFlags f=0);
        virtual ~ScreenPreview();
        void clear();
        QCanvasRectangle* selection;
        QCanvasRectangle* drawingRect;
        QCanvasItem* selectedItem;
        uint operationMode;
        uint numItems;
        KTempFile *tmp;

protected:
        void contentsMousePressEvent(QMouseEvent*);
        void contentsMouseMoveEvent(QMouseEvent*);
        void contentsMouseDoubleClickEvent(QMouseEvent* e);
        void contentsMouseReleaseEvent(QMouseEvent* e);
        void keyPressEvent ( QKeyEvent * e );
        void resizeEvent ( QResizeEvent * );

signals:
        void status(const QString&);
        void editCanvasItem(QCanvasText *);
        void addText(QPoint);
        void addRect(QRect,int);
        void selectedCanvasItem(QCanvasText *);
        void selectedCanvasItem(QCanvasRectangle *);
        void showPreview(QString);
        void adjustButtons();

public slots:
        void changeTextSize(int newSize);
        void changeTextFace(const QString & newFace);
        void changeColor(const QColor & newColor);
        void exportContent();
        void exportContent(KURL url);
        void selectRectangle(QCanvasItem *txt);
        void deleteItem(QCanvasItem *i);
        void itemUp();
        void itemDown();
        QPixmap drawContent();
        void saveImage();
        QDomDocument toXml();
        void setXml(const QDomDocument &xml);

private slots:
        void startResize(QPoint p);

private:
        QCanvasItem* moving;
        QPoint moving_start;
        QPoint draw_start;
};


class transitionPipWidget : public transitionPip_UI
{
        Q_OBJECT
public:
        transitionPipWidget( int width, int height, QWidget* parent=0, const char* name=0, WFlags fl=0);
        virtual ~transitionPipWidget();
        ScreenPreview *canview;
private:
        QCanvas *canvas;

private slots:
        void doPreview();

public slots:
    QPixmap thumbnail(int width, int height);
    KURL previewFile();
    QDomDocument toXml();
    void setXml(const QDomDocument &xml);
    void createImage(KURL url);
};
#endif
