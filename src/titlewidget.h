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


#include "ui_titlewidget_ui.h"
#include "titledocument.h"
#include "renderer.h"
#include "graphicsscenerectmove.h"

#include <QMap>


class Transform
{
public:
    Transform() {
        scalex = 1.0;
        scaley = 1.0;
        rotate = 0.0;
    }
    double scalex, scaley;
    double rotate;
};

class TitleWidget : public QDialog , public Ui::TitleWidget_UI
{
    Q_OBJECT

public:
    /** \brief Constructor
     * \param projectPath Path to use when user requests loading or saving of titles as .kdenlivetitle documents */
    TitleWidget(KUrl url, QString projectPath, Render *render, QWidget *parent = 0);
    virtual ~TitleWidget();
    QDomDocument xml();
    void setXml(QDomDocument doc);

    /** \brief Find first available filename of the form titleXXX.png in projectUrl + "/titles/" directory
    * \param projectUrl Url to directory of project.
     * \returns A list, with the name in the form of "/path/to/titles/titleXXX" as the first element, the extension
     * ".png" as the second element.
     *
     * The path "/titles/" is appended to projectUrl to locate the actual directory that contains the title pngs. */
    static QStringList getFreeTitleInfo(const KUrl &projectUrl);

    /** \brief Build a filename from a projectUrl and a titleName
     * \param projectUrl Url to directory of project.
     * \param titleName Name of title, on the form "titleXXX"
     *
     * The path "/titles/" is appended to projectUrl to build the directoryname, then .pgn is appended to
     * get the filename. It is not checked that the title png actually exists, only the name is build and
     * returned. */
    static QString getTitleResourceFromName(const KUrl &projectUrl, const QString &titleName);

protected:
    virtual void resizeEvent(QResizeEvent * event);

private:
    QGraphicsPolygonItem *m_startViewport, *m_endViewport;
    GraphicsSceneRectMove *m_scene;
    void initViewports();
    QMap<QGraphicsItem*, Transform > m_transformations;
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
    /** \brief Store the current choices of font, background and rect values */
    void writeChoices();
    /** \brief Read the last stored choices into the dialog */
    void readChoices();
	/** \brief Update the displayed X/Y coordinates */
	void updateCoordinates(QGraphicsItem *i);
	void updateDimension(QGraphicsItem *i);
	/** \brief Update the item's position */
	void updatePosition(QGraphicsItem *i);
	
	void textChanged(QGraphicsTextItem *i);
	void updateAxisButtons(QGraphicsItem *i);
	
	void updateTextOriginX();
	void updateTextOriginY();

public slots:
    void slotNewText(QGraphicsTextItem *tt);
    void slotNewRect(QGraphicsRectItem *rect);
    void slotChangeBackground();
    void selectionChanged();
    void rectChanged();
    void setupViewports();
    void zIndexChanged(int);
    void itemScaled(int);
    void itemRotate(int);
    void itemHCenter();
    void itemVCenter();
    void saveTitle(KUrl url = KUrl());
    void loadTitle();
    QImage renderedPixmap();

private slots:
    void slotAdjustSelectedItem();
	
	/** 
	 * \brief Switches the origin of the x axis between left and right 
	 * border of the frame (offset from left/right frame border)
	 * \param originLeft Take left border?
	 * 
	 * Called when the origin of the x coorinate has been changed. The 
	 * x origin will either be at the left or at the right side of the frame.
	 * 
	 * When the origin of the x axis is at the left side, the user can 
	 * enter the distance between an element's left border and the left
	 * side of the frame.
	 * 
	 * When on the right, the distance from the right border of the 
	 * frame to the right border of the element can be entered. This 
	 * would result in negative values as long as the element's right 
	 * border is at the left of the frame's right border. As that is
	 * usually the case, I additionally invert the x axis.
	 * 
	 * Default value is left.
	 * 
	 * |----l----->|#######|----r--->|              
	 * |           |---w-->|         |
	 * |           |#######|         |
	 * |                             |
	 * |----------m_frameWidth------>|
	 * |                             |
	 * 
	 * Left selected: Value = l
	 * Right selected: Value = r
	 * 
	 * To calculate between the two coorindate systems:
	 * l = m_frameWidth - w - r
	 * r = m_frameWidth - w - l
	 * 
	 */
	void slotOriginXClicked();
	/** \brief Same as slotOriginYChanged, but for the Y axis; default is top. 
	 *  \param originTop Take top border? */
	void slotOriginYClicked();
	
	/** \brief Update coorinates of text fields if necessary and text has changed */
	void slotChanged();
	
    void slotZoom(bool up);
    void slotUpdateZoom(int pos);
    void slotAdjustZoom();
    void slotZoomOneToOne();
	
    void slotUpdateText();
	
    void displayBackgroundFrame();
	
    void setCurrentItem(QGraphicsItem *item);
	
    void slotTextTool();
    void slotRectTool();
    void slotSelectTool();
    void slotImageTool();
	
    /** \brief Called when accepted, stores the user selections for next time use */
    void slotAccepted();
};


#endif
