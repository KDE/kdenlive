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

#include <qcanvas.h>
#include <qpoint.h>
#include <qdom.h>

#include <kurl.h>

#include "titlebasewidget_ui.h"
#include "kmmscreen.h"

#ifndef TITLE_H
#define TITLE_H

class FigureEditor : public QCanvasView
{
        Q_OBJECT

public:
        FigureEditor(QCanvas&, QWidget* parent=0, KURL tmpUrl = NULL, const char* name=0, WFlags f=0);
        virtual ~FigureEditor();
        void clear();
        QCanvasRectangle* selection;
        QCanvasRectangle* drawingRect;
        QCanvasItem* selectedItem;
        uint operationMode;
        uint numItems;
	QString tmpFileName;
	bool m_transparent;

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
        void adjustButtons();
	void emptySelection();

public slots:
        void changeTextSize(int newSize);
        void changeTextFace(const QString & newFace);
        void changeColor(const QColor & newColor);
        void exportContent(KURL url = NULL);
        void selectRectangle(QCanvasItem *txt);
        void deleteItem(QCanvasItem *i);
        void itemUp();
        void itemDown();
        QPixmap drawContent();
        void saveImage();
        QDomDocument toXml();
        void setXml(const QDomDocument &xml);
        void setTransparency(bool isOn);
        void itemHCenter();
        void itemVCenter();
        void toggleBold();
        void toggleItalic();
        void toggleStrikeOut();
        void toggleUnderline();
        void updateSelection();
        void alignModeChanged(int index );

private slots:
        void startResize(QPoint p);

private:
        QCanvasItem* moving;
        QPoint moving_start;
        QPoint draw_start;
        bool m_isDrawing;
};


class titleWidget : public titleBaseWidget
{
        Q_OBJECT
public:
        titleWidget(Gui::KMMScreen *screen, int width, int height, KURL tmpUrl = NULL, QWidget* parent=0, const char* name=0, WFlags fl=0);
        virtual ~titleWidget();
        FigureEditor *canview;
private:
        QCanvas *canvas;
	Gui::KMMScreen *m_screen;

private slots:
        void textMode();
        void rectMode();
        void cursorMode();
        void addText(QPoint p);
        void addBlock(QRect rec,int pos=-1);
        void doPreview(int pos);
        void editText(QCanvasText*);
        void adjustWidgets();
        void adjustWidgets(QCanvasText* i);
        void adjustWidgets(QCanvasRectangle* i);
        void adjustButtons();
	void transparencyToggled(bool isOn);
	void seekToPos(const QString &str = QString::null);

public slots:
    QPixmap thumbnail(int width, int height);
    KURL previewFile();
    QDomDocument toXml();
    void setXml(const QDomDocument &xml);
    void createImage(KURL url);
};
#endif
