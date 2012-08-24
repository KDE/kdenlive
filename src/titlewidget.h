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

class TitleTemplate
{
public:
    QString file;
    QString name;
    QIcon icon;
};

class Transform
{
public:
    Transform() {
        scalex = 1.0;
        scaley = 1.0;
        rotatex = 0.0;
        rotatey = 0.0;
        rotatez = 0.0;
    }
    double scalex, scaley;
    double rotatex, rotatey, rotatez;
};


class TitleWidget : public QDialog , public Ui::TitleWidget_UI
{
    Q_OBJECT

public:

    /** @brief Draws the dialog and loads a title document (if any).
     * @param url title document to load
     * @param tc timecode of the project
     * @param projectPath default path to save to or load from title documents
     * @param render project renderer
     * @param parent (optional) parent widget */
    TitleWidget(KUrl url, Timecode tc, QString projectTitlePath, Render *render, QWidget *parent = 0);
    virtual ~TitleWidget();
    QDomDocument xml();
    void setXml(QDomDocument doc);

    /** @brief Finds the first available file name for a title document.
     * @deprecated With the titler module there's no need to save titles as images.
     * @param projectUrl project directory
     * @param isClone (optional) if true, the file name will be cloneXXX.png
     * @return list with title name (Title XXX or Clone XXX) and file name
     *
     * The path "/titles/" is appended to projectUrl, and the format of the file name is (title|clone)XXX.png. */
    static QStringList getFreeTitleInfo(const KUrl &projectUrl, bool isClone = false);

    /** @brief Checks for the images referenced by a title clip.
     * @param xml XML data representing the title
     * @return list of the image files */
    static QStringList extractImageList(QString xml);

    /** @brief Checks for the fonts referenced by a title clip.
     * @param xml XML data representing the title
     * @return list of the fonts */
    static QStringList extractFontList(QString xml);

    /** @brief Builds a file name for a title document.
     * @deprecated With the titler module there's no need to save titles as images.
     * @param projectUrl project directory
     * @param titleName name of title, in the form titleXXX
     * @return file name composed with the given arguments
     *
     * The path "/titles/" is appended to projectUrl, and .png is appended to
     * get the file name. There is no check for the existence of the file. */
    static QString getTitleResourceFromName(const KUrl &projectUrl, const QString &titleName);

    /** @brief Returns clip out position. */
    int outPoint() const;

    /** @brief Retrieves a list of all available title templates. */
    static void refreshTitleTemplates();

protected:
    virtual void resizeEvent(QResizeEvent * event);
    virtual void keyPressEvent(QKeyEvent *e);
    virtual QSize sizeHint() const;

private:

    /** @brief Rectangle describing the animation start viewport. */
    QGraphicsRectItem *m_startViewport;

    /** @brief Rectangle describing the animation end viewport. */
    QGraphicsRectItem *m_endViewport;

    /** @brief Scene for the titler. */
    GraphicsSceneRectMove *m_scene;

    /** @brief Initialises the animation properties (viewport size, etc.). */
    void initAnimation();
    QMap<QGraphicsItem*, Transform > m_transformations;
    TitleDocument m_titledocument;
    QGraphicsRectItem *m_frameBorder;
    QGraphicsPixmapItem *m_frameImage;
    int m_frameWidth;
    int m_frameHeight;
    Render *m_render;   // TODO Is NOT destroyed in the destructor. Deliberately?
    int m_count;
    QAction *m_buttonRect;
    QAction *m_buttonText;
    QAction *m_buttonImage;
    QAction *m_buttonCursor;
    QAction *m_buttonSave;
    QAction *m_buttonLoad;

    QAction *m_unicodeAction;
    QAction *m_zUp;
    QAction *m_zDown;
    QAction *m_zTop;
    QAction *m_zBottom;
    QAction *m_selectAll;
    QAction *m_selectText;
    QAction *m_selectRects;
    QAction *m_selectImages;
    QAction *m_unselectAll;

    /** @brief Dialog for entering Unicode characters in text fields. */
    UnicodeDialog *m_unicodeDialog;

    /** @brief Project path for storing title documents. */
    QString m_projectTitlePath;
    Timecode m_tc;
    QString lastDocumentHash;

    // See http://doc.trolltech.com/4.5/signalsandslots.html#advanced-signals-and-slots-usage.
    QSignalMapper *m_signalMapper;

    enum ValueType { ValueWidth = 1, ValueHeight = 2, ValueX = 4, ValueY = 8 };

    /** @brief Sets the font weight value in the combo box. (#909) */
    void setFontBoxWeight(int weight);

    /** @brief Stores the choices of font, background and rectangle values. */
    void writeChoices();

    /** @brief Reads the last stored choices into the dialog. */
    void readChoices();

    /** @brief Updates the displayed X/Y coordinates. */
    void updateCoordinates(QGraphicsItem *i);

    /** @brief Updates the displayed width/height/zindex values. */
    void updateDimension(QGraphicsItem *i);

    /** @brief Updates the displayed rotation/zoom values. Changes values of rotation/zoom GUI elements. */
    void updateRotZoom(QGraphicsItem *i);

    /** @brief Updates the item position (position read directly from the GUI). Does not change GUI elements. */
    void updatePosition(QGraphicsItem *i);
    /** @brief Updates the item position. Does not change GUI elements. */
    void updatePosition(QGraphicsItem *i, int x, int y);

    void textChanged(QGraphicsTextItem *i);
    void updateAxisButtons(QGraphicsItem *i);

    void updateTextOriginX();
    void updateTextOriginY();

    /** @brief Enables the toolbars suiting to toolType. */
    void enableToolbars(TITLETOOL toolType);

    /** @brief Shows the toolbars suiting to toolType. */
    void showToolbars(TITLETOOL toolType);
    
    /** @brief Set up the tools suiting referenceItem */
    void prepareTools(QGraphicsItem *referenceItem);

    /** @brief Checks a tool button. */
    void checkButton(TITLETOOL toolType);

    void adjustFrameSize();

    /** @brief Adds a "start" and "end" info text to the animation viewports. */
    void addAnimInfoText();

    /** @brief Updates the font for the "start" and "end" info text. */
    void updateInfoText();

    /** @brief Removes the "start" and "end" info text from animation viewports. */
    void deleteAnimInfoText();

    qreal maxZIndex();

    /** @brief Gets the minimum/maximum Z index of items.
     * @param maxBound true: use maximum Z index; false: use minimum
     * @param intersectingOnly if true, consider only the items intersecting
     *     with the currently selected item
     */
    qreal zIndexBounds(bool maxBound, bool intersectingOnly);

    void itemRotate(qreal val, int axis);

    void selectItems(int itemType);

    /** @brief Appends the shortcut of a QAction to a tooltip text */
    QString getTooltipWithShortcut(const QString& text, QAction *button);

public slots:
    void slotNewText(QGraphicsTextItem *tt);
    void slotNewRect(QGraphicsRectItem *rect);
    void slotChangeBackground();

    /** @brief Sets up the tools (toolbars etc.) according to the selected item. */
    void selectionChanged();
    void rectChanged();
    void setupViewports();
    void zIndexChanged(int);
    void itemScaled(int);
    void itemRotateX(qreal);
    void itemRotateY(qreal);
    void itemRotateZ(qreal);
    /** Save a title to a title file */
    void saveTitle(KUrl url = KUrl());
    /** Load a title from a title file */
    void loadTitle(KUrl url = KUrl());

private slots:

    /** @brief Switches the origin of the X axis between left and right border.
     *
     * It's called when the origin of the X coordinate has been changed. The X
     * origin will either be at the left or at the right side of the frame.
     *
     * When the origin of the X axis is at the left side, the user can enter the
     * distance between an element's left border and the left side of the frame.
     *
     * When on the right, the distance from the right border of the frame to the
     * right border of the element can be entered. This would result in negative
     * values as long as the element's right border is at the left of the
     * frame's right border. As that is usually the case, I additionally invert
     * the X axis.
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
     */
    void slotOriginXClicked();

    /** @brief Same as slotOriginXClicked(), but for the Y axis; default is top.
     * @ref slotOriginXClicked */
    void slotOriginYClicked();

    /** @brief Updates coordinates of text fields if necessary.
     *
     * It's called when something changes in the QGraphicsScene. */
    void slotChanged();

    /** 
     * Reacts to changes of widht/height/x/y QSpinBox values.
     * @brief Updates width, height, and position of the selected items.
     * @param valueType of type ValueType
     */
    void slotValueChanged(int valueType);

    void slotZoom(bool up);
    void slotUpdateZoom(int pos);
    void slotAdjustZoom();
    void slotZoomOneToOne();

    void slotSelectAll();
    void slotSelectText();
    void slotSelectRects();
    void slotSelectImages();
    void slotSelectNone();


    /** Called whenever text properties change (font e.g.) */
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

    /** @brief Called when accepted, stores user selections for next time use.
     * @ref writeChoices */
    void slotAccepted();

    void slotFontText(const QString& s);

    /** @brief Adds an effect to an element.
     * @param ix index of the effect in the effects menu
     *
     * The current implementation allows for one QGraphicsEffect to be added
     * along with the typewriter effect. This is not clear to the user: the
     * stack would help, and would permit us to make more QGraphicsEffects
     * coexist (with different layers of QGraphicsItems). */
    void slotAddEffect(int ix);
    void slotEditBlur(int ix);
    void slotEditShadow();
    void slotEditTypewriter(int ix);

    /** @brief Changes the Z index of objects. */
    void slotZIndexUp();
    void slotZIndexDown();
    void slotZIndexTop();
    void slotZIndexBottom();
    /** Called when the user wants to apply a different template to the title */
    void templateIndexChanged(int);
};

#endif
