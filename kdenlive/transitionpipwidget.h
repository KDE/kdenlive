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

#ifndef TRANSITIONPIPWIDGET_H
#define TRANSITIONPIPWIDGET_H

#include <qcanvas.h>
#include <qpoint.h>
#include <qdom.h>
#include <qmap.h>

#include <kurl.h>


#include "transitionpip_ui.h"

namespace Gui {
    class KdenliveApp;

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

protected:
        void contentsMousePressEvent(QMouseEvent*);
        void contentsMouseMoveEvent(QMouseEvent*);
        void contentsMouseReleaseEvent(QMouseEvent* e);
        void resizeEvent ( QResizeEvent * );

signals:
        void status(const QString&);
        void editCanvasItem(QCanvasText *);
        void addRect(QRect,int);
        void selectedCanvasItem(QCanvasText *);
        void selectedCanvasItem(QCanvasRectangle *);
        void adjustButtons();
        void positionRect(int, int);

public slots:
        void moveX(int x);
        void moveY(int y);
        void adjustSize(int x);
        void initRectangle(int x, int y, int w, int h);
        void setSilent(bool);

private:
        QCanvasItem* moving;
        QPoint moving_start;
        QPoint draw_start;
        bool m_silent;
};


class transitionPipWidget : public transitionPip_UI
{
        Q_OBJECT
public:
        transitionPipWidget( KdenliveApp * app, int width, int height, QWidget* parent=0, const char* name=0, WFlags fl=0);
        virtual ~transitionPipWidget();
        ScreenPreview *canview;
private:
        QCanvas *canvas;
        QPoint start, end;
        QMap < int, QString > m_transitionParameters;
        /** when changing keyframe, emit only one refresh signal, not one for every parameter. m_silent is used for that...*/
        bool m_silent;
	KdenliveApp *m_app;

private slots:
        void changeKeyFrame(int ix);
        void adjustSize(int x);
        void moveY(int y);
        void moveX(int x);
        void adjustTransparency(int x);
        void adjustSliders(int x, int y);
	void focusInOut();

public slots:
        QString parameters();
        void setParameters(QString params);

signals:
        void transitionChanged();
};

}  //  end GUI namespace

#endif
