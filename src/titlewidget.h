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
#include "unicodedialog.h"
#include "timecode.h"

#include <QMap>
#include <QSignalMapper>


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
    TitleWidget(KUrl url, Timecode tc, QString projectTitlePath, Render *render, QWidget *parent = 0);
    virtual ~TitleWidget();
    QDomDocument xml();
    void setXml(QDomDocument doc);

    /** \brief Find first available filename of the form titleXXX.png in projectUrl + "/titles/" directory
    * \param projectUrl Url to directory of project.
     * \returns A list, with the name in the form of "/path/to/titles/titleXXX" as the first element, the extension
     * ".png" as the second element.
     *
     * The path "/titles/" is appended to projectUrl to locate the actual directory that contains the title pngs. */
    static QStringList getFreeTitleInfo(const KUrl &projectUrl, bool isClone = false);

    /** \brief Return a list af all images included in a title
     * \param xml The xml data for title
    */
    static QStringList extractImageList(QString xml);

    /** \brief Build a filename from a projectUrl and a titleName
     * \param projectUrl Url to directory of project.
     * \param titleName Name of title, on the form "titleXXX"
     *
     * The path "/titles/" is appended to projectUrl to build the directoryname, then .pgn is appended to
     * get the filename. It is not checked that the title png actually exists, only the name is build and
     * returned. */
    static QString getTitleResourceFromName(const KUrl &projectUrl, const QString &titleName);

    /** \brief Get clip duration. */
    int duration() const;

protected:
    virtual void resizeEvent(QResizeEvent * event);

private:
    /** \brief Rectangle describing animation start viewport */
    QGraphicsRectItem *m_startViewport;
    /** \brief Rectangle describing animation end viewport */
    QGraphicsRectItem *m_endViewport;
    /** \brief Scene for the titler */
    GraphicsSceneRectMove *m_scene;
    /** \brief Initialize the animation properties (viewport size,...) */
    void initAnimation();
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

    QAction *m_unicodeAction;

    /** \brief Dialog for entering unicode in text fields */
    UnicodeDialog *m_unicodeDialog;
    /** project path for storing title clips */
    QString m_projectTitlePath;
    Timecode m_tc;

    /** See http://doc.trolltech.com/4.5/signalsandslots.html#advanced-signals-and-slots-usage */
    QSignalMapper *m_signalMapper;

    enum ValueType { ValueWidth = 0, ValueHeight = 1 };

    /** \brief Sets the font weight value in the combo box. (#909) */
    void setFontBoxWeight(int weight);

    /** \brief Store the current choices of font, background and rect values */
    void writeChoices();
    /** \brief Read the last stored choices into the dialog */
    void readChoices();
    /** \brief Update the displayed X/Y coordinate values */
    void updateCoordinates(QGraphicsItem *i);
    /** \brief Update displayed width/height values */
    void updateDimension(QGraphicsItem *i);
    /** \brief Update displayed rotation/zoom values */
    void updateRotZoom(QGraphicsItem *i);

    /** \brief Update the item's position */
    void updatePosition(QGraphicsItem *i);

    void textChanged(QGraphicsTextItem *i);
    void updateAxisButtons(QGraphicsItem *i);

    void updateTextOriginX();
    void updateTextOriginY();

    /** \brief Enables the toolbars suiting to toolType */
    void enableToolbars(TITLETOOL toolType);
    /** \brief Shows the toolbars suiting to toolType */
    void showToolbars(TITLETOOL toolType);
    /** \brief Check a tool button. */
    void checkButton(TITLETOOL toolType);

    void adjustFrameSize();
    /** \brief Add a "start" and "end" info text to the animation viewports */
    void addAnimInfoText();
    /** \brief Update font for the "start" and "end" info text */
    void updateInfoText();
    /** \brief Remove the "start" and "end" info text from animation viewports */
    void deleteAnimInfoText();

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
    void saveTitle(KUrl url = KUrl());
    void loadTitle(KUrl url = KUrl());

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

    /** \param valueType Of type ValueType */
    void slotValueChanged(int valueType);

    void slotZoom(bool up);
    void slotUpdateZoom(int pos);
    void slotAdjustZoom();
    void slotZoomOneToOne();

    void slotUpdateText();
    void slotInsertUnicode();
    void slotInsertUnicodeString(QString);

    void displayBackgroundFrame();

    void setCurrentItem(QGraphicsItem *item);

    void slotTextTool();
    void slotRectTool();
    void slotSelectTool();
    void slotImageTool();

    void slotAnimStart(bool);
    void slotAnimEnd(bool);
    void slotKeepAspect(bool keep);

    void itemHCenter();
    void itemVCenter();
    void itemTop();
    void itemBottom();
    void itemLeft();
    void itemRight();
    void slotResize50();
    void slotResize100();
    void slotResize200();

    /** \brief Called when accepted, stores the user selections for next time use */
    void slotAccepted();
};


#endif
