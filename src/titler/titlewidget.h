/*
    SPDX-FileCopyrightText: 2008 Marco Gittler <g.marco@freenet.de>
    SPDX-FileCopyrightText: Rafał Lalik

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

/*
 *                                                                         *
 *   Modifications by Rafał Lalik to implement Patterns mechanism          *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include "graphicsscenerectmove.h"
#include "utils/timecode.h"
#include "titler/titledocument.h"
#include "titler/unicodedialog.h"
#include "ui_titlewidget_ui.h"

#include <knewstuff_version.h>
#if KNEWSTUFF_VERSION >= QT_VERSION_CHECK(5, 90, 0)
#include <KNSWidgets/Action>
#endif
#include <QMap>
#include <QModelIndex>
#include <QSignalMapper>

class PatternsModel;

class Monitor;
class KMessageWidget;
class TimecodeDisplay;
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
    Transform()
    {
        scalex = 1.0;
        scaley = 1.0;
        rotatex = 0.0;
        rotatey = 0.0;
        rotatez = 0.0;
    }
    double scalex, scaley;
    int rotatex, rotatey, rotatez;
};

/** @class TitleWidget
 *  @brief Title creation dialog
 *  Instances of TitleWidget classes are instantiated by KdenliveDoc::slotCreateTextClip ()
*/
class TitleWidget : public QDialog, public Ui::TitleWidget_UI
{
    Q_OBJECT

public:
    /** @brief Draws the dialog and loads a title document (if any).
     * @param url title document to load
     * @param projectPath default path to save to or load from title documents
     * @param render project renderer
     * @param parent (optional) parent widget */
    explicit TitleWidget(const QUrl &url, QString projectTitlePath, Monitor *monitor, QWidget *parent = nullptr);
    ~TitleWidget() override;
    QDomDocument xml();
    void setXml(const QDomDocument &doc, const QString &id = QString());

    /** @brief Checks for the images referenced by a title clip.
     * @param xml XML data representing the title
     * @return list of the image files */
    static QStringList extractImageList(const QString &xml);

    /** @brief Checks for the fonts referenced by a title clip.\n
     * Called by DocumentChecker::hasErrorInClips () \n
     * @param xml XML data representing the title
     * @return list of the fonts in the title  */
    static QStringList extractFontList(const QString &xml);

    /** @brief Returns clip duration. */
    int duration() const;

    /** @brief Retrieves a list of all available title templates. */
    static void refreshTitleTemplates(const QString &projectPath);

    /** @brief Returns a title name suggestion based on content */
    const QString titleSuggest();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    /** @brief Rectangle describing the animation start viewport. */
    QGraphicsRectItem *m_startViewport;

    /** @brief Rectangle describing the animation end viewport. */
    QGraphicsRectItem *m_endViewport;

    /** @brief Scene for the titler. */
    GraphicsSceneRectMove *m_scene;

    /** @brief Initialises the animation properties (viewport size, etc.). */
    void initAnimation();
    QMap<QGraphicsItem *, Transform> m_transformations;
    TitleDocument m_titledocument;
    QGraphicsRectItem *m_frameBorder;
    QGraphicsRectItem *m_frameBackground;
    QGraphicsPixmapItem *m_frameImage;
    QButtonGroup *m_textAlignGroup;
    int m_frameWidth;
    int m_frameHeight;
    int m_count;
    /** @brief Dialog for entering Unicode characters in text fields. */
    UnicodeDialog *m_unicodeDialog;
    KMessageWidget *m_missingMessage;

    /** @brief Project path for storing title documents. */
    QString m_projectTitlePath;

    /** @brief The project framerate. */
    double m_fps;

    /** @brief The bin id of the clip currently edited, can be empty if this is a new clip. */
    QString m_clipId;

    QAction *m_buttonRect;
    QAction *m_buttonEllipse;
    QAction *m_buttonText;
    QAction *m_buttonImage;
    QAction *m_buttonCursor;
    QAction *m_buttonSave;
    QAction *m_buttonLoad;
#if KNEWSTUFF_VERSION < QT_VERSION_CHECK(5, 90, 0)
    QAction *m_buttonDownload;
#else
    KNSWidgets::Action *m_buttonDownload;
#endif

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
    QAction *m_createTitleAction;
    QString m_lastDocumentHash;
    QList<QGraphicsLineItem *> m_guides;

    PatternsModel *m_patternsModel;

    //QList<TitleTemplate> m_titleTemplates;

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

    void textChanged(MyTextItem *i);
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

    /** @brief Refreshes the contents of combobox based on list of title templates. */
    void refreshTemplateBoxContents();

    qreal maxZIndex();

    /** @brief Gets the minimum/maximum Z index of items.
     * @param maxBound true: use maximum Z index; false: use minimum
     * @param intersectingOnly if true, consider only the items intersecting
     *     with the currently selected item
     */
    qreal zIndexBounds(bool maxBound, bool intersectingOnly);

    void itemRotate(int val, int axis);

    void selectItems(int itemType);

    /** @brief Appends the shortcut of a QAction to a tooltip text */
    QString getTooltipWithShortcut(const QString &tipText, QAction *button);
    void loadGradients();
    void storeGradient(const QString &gradientData);

#if KNEWSTUFF_VERSION < QT_VERSION_CHECK(5, 90, 0)
    /** @brief Open title download dialog */
    void downloadTitleTemplates();
#endif

    /** @brief Read patterns from config file
     */
    void readPatterns();
    /** @brief Write patterns to config file
     */
    void writePatterns();

public slots:
    void slotNewText(MyTextItem *tt);
    void slotNewRect(QGraphicsRectItem *rect);
    void slotNewEllipse(QGraphicsEllipseItem *rect);
    void slotChangeBackground();

    /** @brief Sets up the tools (toolbars etc.) according to the selected item. */
    void selectionChanged();
    void rectChanged();
    void zIndexChanged(int);
    void itemScaled(int);
    void itemRotateX(int);
    void itemRotateY(int);
    void itemRotateZ(int);
    /** Save a title to a title file, returns the saved url or empty if error */
    QUrl saveTitle(QUrl url = QUrl());
    /** Load a title from a title file */
    void loadTitle(QUrl url = QUrl());
    void slotGotBackground(const QImage &img);

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
     * To calculate between the two coordinate systems:
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
     * Reacts to changes of width/height/x/y QSpinBox values.
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
    void slotSelectEllipses();
    void slotSelectImages();
    void slotSelectNone();

    /** Called whenever text properties change (font e.g.) */
    void slotUpdateText();
    void slotInsertUnicode();
    void slotInsertUnicodeString(const QString &string);

    void displayBackgroundFrame();

    void setCurrentItem(QGraphicsItem *item);

    void slotTextTool();
    void slotRectTool();
    void slotEllipseTool();
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
    void slotResize(int percentSize);
    /** @brief Show hide guides */
    void showGuides(int state);
    /** @brief Build guides */
    void updateGuides(int);
    /** @brief guide color changed, repaint */
    void guideColorChanged(const QColor &col);

    /** @brief Called when accepted, stores user selections for next time use.
     * @ref writeChoices */
    void slotAccepted();

    /** @brief Adds an effect to an element.
     * @param ix index of the effect in the effects menu
     *
     * The current implementation allows for one QGraphicsEffect to be added
     * along with the typewriter effect. This is not clear to the user: the
     * stack would help, and would permit us to make more QGraphicsEffects
     * coexist (with different layers of QGraphicsItems). */
    void slotAddEffect(int ix);

    /** @brief Changes the Z index of objects. */
    void slotZIndexUp();
    void slotZIndexDown();
    void slotZIndexTop();
    void slotZIndexBottom();
    /** Called when the user wants to apply a different template to the title */
    void templateIndexChanged(int);
    void slotEditGradient();
    void slotUpdateShadow();
    /** TW stuff */
    void slotUpdateTW();

    /** @brief Remove missing items from the scene. */
    void deleteMissingItems();
    /** @brief List missing items from the scene. */
    void showMissingItems();

    // slots for patterns list

    /** @brief When scale slider is changed. */
    void slotPatternsTileWidth(int width);
    /** @brief Pattern in the list double clicked. */
    void slotPatternDblClicked(const QModelIndex & idx);
    /** @brief Pattern add button clicked. */
    void slotPatternBtnAddClicked();
    /** @brief Pattern remove button clicked. */
    void slotPatternBtnRemoveClicked();

signals:
    void requestBackgroundFrame(bool request);
    void updatePatternsBackgroundFrame();
};
