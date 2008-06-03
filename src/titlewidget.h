/***************************************************************************
                          titlewidget.h  -  description
                             -------------------
    begin                : Feb 28 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H


#include <QDialog>

#include <QMap>

#include "ui_titlewidget_ui.h"
#include "titledocument.h"
#include "renderer.h"
#include "graphicsscenerectmove.h"



class Transform {
public:
    Transform() {
        scalex = 1.0;
        scaley = 1.0;
        rotate = 0.0;
    }
    double scalex, scaley;
    double rotate;
};

class TitleWidget : public QDialog , public Ui::TitleWidget_UI {
    Q_OBJECT
public:
    TitleWidget(KUrl url, QString projectPath, Render *render, QWidget *parent = 0);

protected:
    virtual void resizeEvent(QResizeEvent * event);

private:
    QGraphicsPolygonItem *startViewport, *endViewport;
    GraphicsSceneRectMove *m_scene;
    void initViewports();
    QMap<QGraphicsItem*, Transform > transformations;
    TitleDocument m_titledocument;
    QGraphicsRectItem *m_frameBorder;
    QGraphicsPixmapItem *m_frameImage;
    int m_frameWidth;
    int m_frameHeight;
    Render *m_render;
    int m_count;
    QAction *m_buttonRect;
    QAction *m_buttonText;
    QAction *m_buttonImage;
    QAction *m_buttonCursor;
    QAction *m_buttonSave;
    QAction *m_buttonLoad;
    /** project path for storing title clips */
    QString m_projectPath;

public slots:
    void slotNewText(QGraphicsTextItem *tt);
    void slotNewRect(QGraphicsRectItem *rect);
    void slotChangeBackground();
    void selectionChanged();
    void textChanged();
    void rectChanged();
    void fontBold();
    void setupViewports();
    void zIndexChanged(int);
    void itemScaled(int);
    void itemRotate(int);
    void itemHCenter();
    void itemVCenter();
    void saveTitle(KUrl url = KUrl());
    void loadTitle();
    QPixmap renderedPixmap();

private slots:
    void slotAdjustSelectedItem();
    void slotUpdateZoom(int pos);
    void slotZoom(bool up);
    void slotAdjustZoom();
    void slotZoomOneToOne();
    void slotUpdateText();
    void displayBackgroundFrame();
    void setCurrentItem(QGraphicsItem *item);
    void slotTextTool();
    void slotRectTool();
    void slotSelectTool();
    void slotImageTool();
};


#endif
