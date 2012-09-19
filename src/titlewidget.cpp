/***************************************************************************
                          titlewidget.cpp  -  description
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

#include "titlewidget.h"
#include "kdenlivesettings.h"
#include "KoSliderCombo.h"
#include "kthumb.h"
#include <cmath>

#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>
#include <KStandardDirs>
#include <KMessageBox>
#include <kio/netaccess.h>
#include <kdeversion.h>

#include <QDomDocument>
#include <QGraphicsItem>
#include <QGraphicsSvgItem>
#include <QTimer>
#include <QToolBar>
#include <QMenu>
#include <QSignalMapper>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QComboBox>
#include <QCryptographicHash>
#include <QKeyEvent>

#if QT_VERSION >= 0x040600
#include <QGraphicsEffect>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#endif

#include <iostream>

static QList<TitleTemplate> titletemplates;

// What exactly is this variable good for?
int settingUp = false;

const int IMAGEITEM = 7;
const int RECTITEM = 3;
const int TEXTITEM = 8;

const int NOEFFECT = 0;
const int BLUREFFECT = 1;
const int SHADOWEFFECT = 2;
const int TYPEWRITEREFFECT = 3;

TitleWidget::TitleWidget(KUrl url, Timecode tc, QString projectTitlePath, Render *render, QWidget *parent) :
        QDialog(parent),
        Ui::TitleWidget_UI(),
        m_startViewport(NULL),
        m_endViewport(NULL),
        m_render(render),
        m_count(0),
        m_unicodeDialog(new UnicodeDialog(UnicodeDialog::InputHex)),
        m_projectTitlePath(projectTitlePath),
        m_tc(tc)
{
    setupUi(this);
    setFont(KGlobalSettings::toolBarFont());
    frame_properties->setEnabled(false);
    frame_properties->setFixedHeight(frame_toolbar->height());
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

#if KDE_IS_VERSION(4,5,0)
    rectBColor->setAlphaChannelEnabled(true);
    delete rectBAlpha;
    rectFColor->setAlphaChannelEnabled(true);
    delete rectFAlpha;
    fontColorButton->setAlphaChannelEnabled(true);
    delete textAlpha;
    textOutlineColor->setAlphaChannelEnabled(true);
    delete textOutlineAlpha;
    
#else
    rectBAlpha->setMinimum(0);
    rectBAlpha->setMaximum(255);
    rectBAlpha->setDecimals(0);
    rectBAlpha->setValue(255);
    rectBAlpha->setToolTip(i18n("Color opacity"));

    rectFAlpha->setMinimum(0);
    rectFAlpha->setMaximum(255);
    rectFAlpha->setDecimals(0);
    rectFAlpha->setValue(255);
    rectFAlpha->setToolTip(i18n("Border opacity"));
    connect(rectFAlpha, SIGNAL(valueChanged(qreal, bool)), this, SLOT(rectChanged()));
    connect(rectBAlpha, SIGNAL(valueChanged(qreal, bool)), this, SLOT(rectChanged()));

    // Set combo sliders values
    textAlpha->setMinimum(0);
    textAlpha->setMaximum(255);
    textAlpha->setDecimals(0);
    textAlpha->setValue(255);
    textAlpha->setToolTip(i18n("Font color opacity"));

    textOutlineAlpha->setMinimum(0);
    textOutlineAlpha->setMaximum(255);
    textOutlineAlpha->setDecimals(0);
    textOutlineAlpha->setValue(255);
    textOutlineAlpha->setToolTip(i18n("Outline color opacity"));
    connect(textAlpha, SIGNAL(valueChanged(qreal, bool)), this, SLOT(slotUpdateText()));
    connect(textOutlineAlpha, SIGNAL(valueChanged(qreal, bool)), this, SLOT(slotUpdateText()));
#endif

    textOutline->setMinimum(0);
    textOutline->setMaximum(200);
    textOutline->setDecimals(0);
    textOutline->setValue(0);
    textOutline->setToolTip(i18n("Outline width"));

    backgroundAlpha->setMinimum(0);
    backgroundAlpha->setMaximum(255);
    backgroundAlpha->setDecimals(0);
    backgroundAlpha->setValue(0);
    backgroundAlpha->setToolTip(i18n("Background color opacity"));

    itemrotatex->setMinimum(-360);
    itemrotatex->setMaximum(360);
    itemrotatex->setDecimals(0);
    itemrotatex->setValue(0);
    itemrotatex->setToolTip(i18n("Rotation around the X axis"));

    itemrotatey->setMinimum(-360);
    itemrotatey->setMaximum(360);
    itemrotatey->setDecimals(0);
    itemrotatey->setValue(0);
    itemrotatey->setToolTip(i18n("Rotation around the Y axis"));

    itemrotatez->setMinimum(-360);
    itemrotatez->setMaximum(360);
    itemrotatez->setDecimals(0);
    itemrotatez->setValue(0);
    itemrotatez->setToolTip(i18n("Rotation around the Z axis"));

    rectLineWidth->setMinimum(0);
    rectLineWidth->setMaximum(100);
    rectLineWidth->setDecimals(0);
    rectLineWidth->setValue(0);
    rectLineWidth->setToolTip(i18n("Border width"));

    itemzoom->setSuffix(i18n("%"));
    m_frameWidth = render->renderWidth();
    m_frameHeight = render->renderHeight();
    showToolbars(TITLE_SELECT);

    splitter->setStretchFactor(0, 20);

    //If project is drop frame, set the input mask as such.
    title_duration->setInputMask(m_tc.mask());
    title_duration->setText(m_tc.reformatSeparators(KdenliveSettings::title_duration()));

    connect(backgroundColor, SIGNAL(changed(const QColor &)), this, SLOT(slotChangeBackground())) ;
    connect(backgroundAlpha, SIGNAL(valueChanged(qreal, bool)), this, SLOT(slotChangeBackground())) ;

    connect(fontColorButton, SIGNAL(changed(const QColor &)), this, SLOT(slotUpdateText())) ;
    connect(textOutlineColor, SIGNAL(changed(const QColor &)), this, SLOT(slotUpdateText())) ;
    connect(font_family, SIGNAL(currentFontChanged(const QFont &)), this, SLOT(slotUpdateText())) ;
    connect(font_size, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateText())) ;
    connect(textOutline, SIGNAL(valueChanged(qreal, bool)), this, SLOT(slotUpdateText()));
    connect(font_weight_box, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateText()));

    connect(font_family, SIGNAL(editTextChanged(const QString &)), this, SLOT(slotFontText(const QString&)));

    connect(rectFColor, SIGNAL(changed(const QColor &)), this, SLOT(rectChanged()));
    connect(rectBColor, SIGNAL(changed(const QColor &)), this, SLOT(rectChanged()));
    connect(rectLineWidth, SIGNAL(valueChanged(qreal, bool)), this, SLOT(rectChanged()));

    /*connect(startViewportX, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(startViewportY, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(startViewportSize, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(endViewportX, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(endViewportY, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(endViewportSize, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));*/

    // Fill effects
    effect_list->addItem(i18n("None"), NOEFFECT);
    effect_list->addItem(i18n("Typewriter"), TYPEWRITEREFFECT);
    effect_list->addItem(i18n("Blur"), BLUREFFECT);


    connect(zValue, SIGNAL(valueChanged(int)), this, SLOT(zIndexChanged(int)));
    connect(itemzoom, SIGNAL(valueChanged(int)), this, SLOT(itemScaled(int)));
    connect(itemrotatex, SIGNAL(valueChanged(qreal, bool)), this, SLOT(itemRotateX(qreal)));
    connect(itemrotatey, SIGNAL(valueChanged(qreal, bool)), this, SLOT(itemRotateY(qreal)));
    connect(itemrotatez, SIGNAL(valueChanged(qreal, bool)), this, SLOT(itemRotateZ(qreal)));
    connect(itemhcenter, SIGNAL(clicked()), this, SLOT(itemHCenter()));
    connect(itemvcenter, SIGNAL(clicked()), this, SLOT(itemVCenter()));
    connect(itemtop, SIGNAL(clicked()), this, SLOT(itemTop()));
    connect(itembottom, SIGNAL(clicked()), this, SLOT(itemBottom()));
    connect(itemleft, SIGNAL(clicked()), this, SLOT(itemLeft()));
    connect(itemright, SIGNAL(clicked()), this, SLOT(itemRight()));
    connect(effect_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotAddEffect(int)));
    connect(typewriter_delay, SIGNAL(valueChanged(int)), this, SLOT(slotEditTypewriter(int)));
    connect(typewriter_start, SIGNAL(valueChanged(int)), this, SLOT(slotEditTypewriter(int)));
    connect(blur_radius, SIGNAL(valueChanged(int)), this, SLOT(slotEditBlur(int)));
    connect(shadow_radius, SIGNAL(valueChanged(int)), this, SLOT(slotEditShadow()));
    connect(shadow_x, SIGNAL(valueChanged(int)), this, SLOT(slotEditShadow()));
    connect(shadow_y, SIGNAL(valueChanged(int)), this, SLOT(slotEditShadow()));
    effect_stack->setHidden(true);
    effect_frame->setEnabled(false);

    connect(origin_x_left, SIGNAL(clicked()), this, SLOT(slotOriginXClicked()));
    connect(origin_y_top, SIGNAL(clicked()), this, SLOT(slotOriginYClicked()));

    // Position and size
    m_signalMapper = new QSignalMapper(this);
    m_signalMapper->setMapping(value_w, ValueWidth);
    m_signalMapper->setMapping(value_h, ValueHeight);
    m_signalMapper->setMapping(value_x, ValueX);
    m_signalMapper->setMapping(value_y, ValueY);
    connect(value_w, SIGNAL(valueChanged(int)), m_signalMapper, SLOT(map()));
    connect(value_h, SIGNAL(valueChanged(int)), m_signalMapper, SLOT(map()));
    connect(value_x, SIGNAL(valueChanged(int)), m_signalMapper, SLOT(map()));
    connect(value_y, SIGNAL(valueChanged(int)), m_signalMapper, SLOT(map()));
    connect(m_signalMapper, SIGNAL(mapped(int)), this, SLOT(slotValueChanged(int)));

    connect(buttonFitZoom, SIGNAL(clicked()), this, SLOT(slotAdjustZoom()));
    connect(buttonRealSize, SIGNAL(clicked()), this, SLOT(slotZoomOneToOne()));
    connect(buttonItalic, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonUnder, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonAlignLeft, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonAlignRight, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonAlignCenter, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonAlignNone, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(displayBg, SIGNAL(stateChanged(int)), this, SLOT(displayBackgroundFrame()));

    connect(m_unicodeDialog, SIGNAL(charSelected(QString)), this, SLOT(slotInsertUnicodeString(QString)));

    // mbd
    connect(this, SIGNAL(accepted()), this, SLOT(slotAccepted()));

    font_weight_box->blockSignals(true);
    font_weight_box->addItem(i18nc("Font style", "Light"), QFont::Light);
    font_weight_box->addItem(i18nc("Font style", "Normal"), QFont::Normal);
    font_weight_box->addItem(i18nc("Font style", "Demi-Bold"), QFont::DemiBold);
    font_weight_box->addItem(i18nc("Font style", "Bold"), QFont::Bold);
    font_weight_box->addItem(i18nc("Font style", "Black"), QFont::Black);
    font_weight_box->setToolTip(i18n("Font weight"));
    font_weight_box->setCurrentIndex(1);
    font_weight_box->blockSignals(false);

    buttonFitZoom->setIconSize(iconSize);
    buttonRealSize->setIconSize(iconSize);
    buttonItalic->setIconSize(iconSize);
    buttonUnder->setIconSize(iconSize);
    buttonAlignCenter->setIconSize(iconSize);
    buttonAlignLeft->setIconSize(iconSize);
    buttonAlignRight->setIconSize(iconSize);
    buttonAlignNone->setIconSize(iconSize);
    
    buttonFitZoom->setIcon(KIcon("zoom-fit-best"));
    buttonRealSize->setIcon(KIcon("zoom-original"));
    buttonItalic->setIcon(KIcon("format-text-italic"));
    buttonUnder->setIcon(KIcon("format-text-underline"));
    buttonAlignCenter->setIcon(KIcon("format-justify-center"));
    buttonAlignLeft->setIcon(KIcon("format-justify-left"));
    buttonAlignRight->setIcon(KIcon("format-justify-right"));
    buttonAlignNone->setIcon(KIcon("kdenlive-align-none"));

    buttonAlignNone->setToolTip(i18n("No alignment"));
    buttonAlignRight->setToolTip(i18n("Align right"));
    buttonAlignLeft->setToolTip(i18n("Align left"));
    buttonAlignCenter->setToolTip(i18n("Align center"));

    m_unicodeAction = new QAction(KIcon("kdenlive-insert-unicode"), QString(), this);
    m_unicodeAction->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_U);
    m_unicodeAction->setToolTip(getTooltipWithShortcut(i18n("Insert Unicode character"), m_unicodeAction));
    connect(m_unicodeAction, SIGNAL(triggered()), this, SLOT(slotInsertUnicode()));
    buttonInsertUnicode->setDefaultAction(m_unicodeAction);

    m_zUp = new QAction(KIcon("kdenlive-zindex-up"), QString(), this);
    m_zUp->setShortcut(Qt::Key_PageUp);
    m_zUp->setToolTip(i18n("Raise object"));
    connect(m_zUp, SIGNAL(triggered()), this, SLOT(slotZIndexUp()));
    zUp->setDefaultAction(m_zUp);

    m_zDown = new QAction(KIcon("kdenlive-zindex-down"), QString(), this);
    m_zDown->setShortcut(Qt::Key_PageDown);
    m_zDown->setToolTip(i18n("Lower object"));
    connect(m_zDown, SIGNAL(triggered()), this, SLOT(slotZIndexDown()));
    zDown->setDefaultAction(m_zDown);

    m_zTop = new QAction(KIcon("kdenlive-zindex-top"), QString(), this);
    // TODO mbt 1414: Shortcut should change z index only if
    // cursor is NOT in a text field ...
    //m_zTop->setShortcut(Qt::Key_Home);
    m_zTop->setToolTip(i18n("Raise object to top"));
    connect(m_zTop, SIGNAL(triggered()), this, SLOT(slotZIndexTop()));
    zTop->setDefaultAction(m_zTop);

    m_zBottom = new QAction(KIcon("kdenlive-zindex-bottom"), QString(), this);
    // TODO mbt 1414
    //m_zBottom->setShortcut(Qt::Key_End);
    m_zBottom->setToolTip(i18n("Lower object to bottom"));
    connect(m_zBottom, SIGNAL(triggered()), this, SLOT(slotZIndexBottom()));
    zBottom->setDefaultAction(m_zBottom);

    m_selectAll = new QAction(KIcon("kdenlive-select-all"), QString(), this);
    m_selectAll->setShortcut(Qt::CTRL + Qt::Key_A);
    connect(m_selectAll, SIGNAL(triggered()), this, SLOT(slotSelectAll()));
    buttonSelectAll->setDefaultAction(m_selectAll);

    m_selectText = new QAction(KIcon("kdenlive-select-texts"), QString(), this);
    m_selectText->setShortcut(Qt::CTRL + Qt::Key_T);
    connect(m_selectText, SIGNAL(triggered()), this, SLOT(slotSelectText()));
    buttonSelectText->setDefaultAction(m_selectText);
    buttonSelectText->setEnabled(false);

    m_selectRects = new QAction(KIcon("kdenlive-select-rects"), QString(), this);
    m_selectRects->setShortcut(Qt::CTRL + Qt::Key_R);
    connect(m_selectRects, SIGNAL(triggered()), this, SLOT(slotSelectRects()));
    buttonSelectRects->setDefaultAction(m_selectRects);
    buttonSelectRects->setEnabled(false);

    m_selectImages = new QAction(KIcon("kdenlive-select-images"), QString(), this);
    m_selectImages->setShortcut(Qt::CTRL + Qt::Key_I);
    connect(m_selectImages, SIGNAL(triggered()), this, SLOT(slotSelectImages()));
    buttonSelectImages->setDefaultAction(m_selectImages);
    buttonSelectImages->setEnabled(false);

    m_unselectAll = new QAction(KIcon("kdenlive-unselect-all"), QString(), this);
    m_unselectAll->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_A);
    connect(m_unselectAll, SIGNAL(triggered()), this, SLOT(slotSelectNone()));
    buttonUnselectAll->setDefaultAction(m_unselectAll);
    buttonUnselectAll->setEnabled(false);

    zDown->setIconSize(iconSize);
    zTop->setIconSize(iconSize);
    zBottom->setIconSize(iconSize);
    
    zDown->setIcon(KIcon("kdenlive-zindex-down"));
    zTop->setIcon(KIcon("kdenlive-zindex-top"));
    zBottom->setIcon(KIcon("kdenlive-zindex-bottom"));
    connect(zDown, SIGNAL(clicked()), this, SLOT(slotZIndexDown()));
    connect(zTop, SIGNAL(clicked()), this, SLOT(slotZIndexTop()));
    connect(zBottom, SIGNAL(clicked()), this, SLOT(slotZIndexBottom()));

    origin_x_left->setToolTip(i18n("Invert x axis and change 0 point"));
    origin_y_top->setToolTip(i18n("Invert y axis and change 0 point"));
    rectBColor->setToolTip(i18n("Select fill color"));
    rectFColor->setToolTip(i18n("Select border color"));
    zoom_slider->setToolTip(i18n("Zoom"));
    buttonRealSize->setToolTip(i18n("Original size (1:1)"));
    buttonFitZoom->setToolTip(i18n("Fit zoom"));
    backgroundColor->setToolTip(i18n("Select background color"));
    backgroundAlpha->setToolTip(i18n("Background opacity"));
    buttonSelectAll->setToolTip(getTooltipWithShortcut(i18n("Select all"), m_selectAll));
    buttonSelectText->setToolTip(getTooltipWithShortcut(i18n("Select text items in current selection"), m_selectText));
    buttonSelectRects->setToolTip(getTooltipWithShortcut(i18n("Select rect items in current selection"), m_selectRects));
    buttonSelectImages->setToolTip(getTooltipWithShortcut(i18n("Select image items in current selection"), m_selectImages));
    buttonUnselectAll->setToolTip(getTooltipWithShortcut(i18n("Unselect all"), m_unselectAll));

    itemhcenter->setIconSize(iconSize);
    itemvcenter->setIconSize(iconSize);
    itemtop->setIconSize(iconSize);
    itembottom->setIconSize(iconSize);
    itemright->setIconSize(iconSize);
    itemleft->setIconSize(iconSize);
    
    itemhcenter->setIcon(KIcon("kdenlive-align-hor"));
    itemhcenter->setToolTip(i18n("Align item horizontally"));
    itemvcenter->setIcon(KIcon("kdenlive-align-vert"));
    itemvcenter->setToolTip(i18n("Align item vertically"));
    itemtop->setIcon(KIcon("kdenlive-align-top"));
    itemtop->setToolTip(i18n("Align item to top"));
    itembottom->setIcon(KIcon("kdenlive-align-bottom"));
    itembottom->setToolTip(i18n("Align item to bottom"));
    itemright->setIcon(KIcon("kdenlive-align-right"));
    itemright->setToolTip(i18n("Align item to right"));
    itemleft->setIcon(KIcon("kdenlive-align-left"));
    itemleft->setToolTip(i18n("Align item to left"));


    QHBoxLayout *layout = new QHBoxLayout;
    frame_toolbar->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    QToolBar *m_toolbar = new QToolBar("titleToolBar", this);
    m_toolbar->setIconSize(iconSize);

    m_buttonCursor = m_toolbar->addAction(KIcon("transform-move"), i18n("Selection Tool"));
    m_buttonCursor->setCheckable(true);
    m_buttonCursor->setShortcut(Qt::ALT + Qt::Key_S);
    m_buttonCursor->setToolTip(i18n("Selection Tool") + ' ' + m_buttonCursor->shortcut().toString());
    connect(m_buttonCursor, SIGNAL(triggered()), this, SLOT(slotSelectTool()));

    m_buttonText = m_toolbar->addAction(KIcon("insert-text"), i18n("Add Text"));
    m_buttonText->setCheckable(true);
    m_buttonText->setShortcut(Qt::ALT + Qt::Key_T);
    m_buttonText->setToolTip(i18n("Add Text") + ' ' + m_buttonText->shortcut().toString());
    connect(m_buttonText, SIGNAL(triggered()), this, SLOT(slotTextTool()));

    m_buttonRect = m_toolbar->addAction(KIcon("kdenlive-insert-rect"), i18n("Add Rectangle"));
    m_buttonRect->setCheckable(true);
    m_buttonRect->setShortcut(Qt::ALT + Qt::Key_R);
    m_buttonRect->setToolTip(i18n("Add Rectangle") + ' ' + m_buttonRect->shortcut().toString());
    connect(m_buttonRect, SIGNAL(triggered()), this, SLOT(slotRectTool()));

    m_buttonImage = m_toolbar->addAction(KIcon("insert-image"), i18n("Add Image"));
    m_buttonImage->setCheckable(false);
    m_buttonImage->setShortcut(Qt::ALT + Qt::Key_I);
    m_buttonImage->setToolTip(i18n("Add Image") + ' ' + m_buttonImage->shortcut().toString());
    connect(m_buttonImage, SIGNAL(triggered()), this, SLOT(slotImageTool()));

    m_toolbar->addSeparator();

    m_buttonLoad = m_toolbar->addAction(KIcon("document-open"), i18n("Open Document"));
    m_buttonLoad->setCheckable(false);
    m_buttonLoad->setShortcut(Qt::CTRL + Qt::Key_O);
    m_buttonLoad->setToolTip(i18n("Open Document") + ' ' + m_buttonImage->shortcut().toString());
    connect(m_buttonLoad, SIGNAL(triggered()), this, SLOT(loadTitle()));

    m_buttonSave = m_toolbar->addAction(KIcon("document-save-as"), i18n("Save As"));
    m_buttonSave->setCheckable(false);
    m_buttonSave->setShortcut(Qt::CTRL + Qt::Key_S);
    m_buttonSave->setToolTip(i18n("Save As") + ' ' + m_buttonImage->shortcut().toString());
    connect(m_buttonSave, SIGNAL(triggered()), this, SLOT(saveTitle()));

    layout->addWidget(m_toolbar);

    // initialize graphic scene
    m_scene = new GraphicsSceneRectMove(this);
    graphicsView->setScene(m_scene);
    graphicsView->setMouseTracking(true);
    graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    graphicsView->setRubberBandSelectionMode(Qt::ContainsItemBoundingRect);
    m_titledocument.setScene(m_scene, m_frameWidth, m_frameHeight);
    connect(m_scene, SIGNAL(changed(QList<QRectF>)), this, SLOT(slotChanged()));
    connect(font_size, SIGNAL(valueChanged(int)), m_scene, SLOT(slotUpdateFontSize(int)));

    // a gradient background
    /*QRadialGradient *gradient = new QRadialGradient(0, 0, 10);
    gradient->setSpread(QGradient::ReflectSpread);
    scene->setBackgroundBrush(*gradient);*/

    QPen framepen(Qt::DotLine);
    framepen.setColor(Qt::red);

    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(Qt::transparent);
    m_frameBorder->setFlags(0);
    graphicsView->scene()->addItem(m_frameBorder);
    
    m_frameImage = new QGraphicsPixmapItem();
    QTransform qtrans;
    qtrans.scale(2.0, 2.0);
    m_frameImage->setTransform(qtrans);
    m_frameImage->setZValue(-1200);
    m_frameImage->setFlags(0);
    displayBackgroundFrame();
    graphicsView->scene()->addItem(m_frameImage);

    connect(m_scene, SIGNAL(selectionChanged()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(itemMoved()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(sceneZoom(bool)), this , SLOT(slotZoom(bool)));
    connect(m_scene, SIGNAL(actionFinished()), this , SLOT(slotSelectTool()));
    //connect(m_scene, SIGNAL(actionFinished()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(newRect(QGraphicsRectItem *)), this , SLOT(slotNewRect(QGraphicsRectItem *)));
    connect(m_scene, SIGNAL(newText(QGraphicsTextItem *)), this , SLOT(slotNewText(QGraphicsTextItem *)));
    connect(zoom_slider, SIGNAL(valueChanged(int)), this , SLOT(slotUpdateZoom(int)));
    connect(zoom_spin, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateZoom(int)));

    // mbd: load saved settings
    readChoices();

    graphicsView->show();
    //graphicsView->setRenderHint(QPainter::Antialiasing);
    graphicsView->setInteractive(true);
    //graphicsView->resize(400, 300);
    kDebug() << "// TITLE WIDGWT: " << graphicsView->viewport()->width() << "x" << graphicsView->viewport()->height();
    //toolBox->setItemEnabled(2, false);
    m_startViewport = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_endViewport = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_startViewport->setData(0, m_frameWidth);
    m_startViewport->setData(1, m_frameHeight);
    m_endViewport->setData(0, m_frameWidth);
    m_endViewport->setData(1, m_frameHeight);

    // scale the view so that the title widget is not too big at startup
    graphicsView->scale(.5, .5);
    if (!url.isEmpty()) loadTitle(url);
    else {
        slotTextTool();
        QTimer::singleShot(200, this, SLOT(slotAdjustZoom()));
    }
    initAnimation();
    connect(anim_start, SIGNAL(toggled(bool)), this, SLOT(slotAnimStart(bool)));
    connect(anim_end, SIGNAL(toggled(bool)), this, SLOT(slotAnimEnd(bool)));
    connect(templateBox, SIGNAL(currentIndexChanged(int)), this, SLOT(templateIndexChanged(int)));

    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(KdenliveSettings::hastitleproducer());
    refreshTitleTemplates();
    //templateBox->setIconSize(QSize(60,60));
    templateBox->clear();
    templateBox->addItem("");
    foreach(const TitleTemplate &t, titletemplates) {
        templateBox->addItem(t.icon, t.name, t.file);
    }
    lastDocumentHash = QCryptographicHash::hash(xml().toString().toAscii(), QCryptographicHash::Md5).toHex();
    adjustSize();
}

TitleWidget::~TitleWidget()
{
    delete m_buttonRect;
    delete m_buttonText;
    delete m_buttonImage;
    delete m_buttonCursor;
    delete m_buttonSave;
    delete m_buttonLoad;
    delete m_unicodeAction;
    delete m_zUp;
    delete m_zDown;
    delete m_zTop;
    delete m_zBottom;
    delete m_selectAll;
    delete m_selectText;
    delete m_selectRects;
    delete m_selectImages;
    delete m_unselectAll;

    delete m_unicodeDialog;
    delete m_frameBorder;
    delete m_frameImage;
    delete m_startViewport;
    delete m_endViewport;
    delete m_scene;
    delete m_signalMapper;
}

QSize TitleWidget::sizeHint() const
{
    // Make sure the widget has minimum size on opening
    return QSize(200, 200);
}

//static
QStringList TitleWidget::getFreeTitleInfo(const KUrl &projectUrl, bool isClone)
{
    QStringList result;
    QString titlePath = projectUrl.path(KUrl::AddTrailingSlash) + "titles/";
    KStandardDirs::makeDir(titlePath);
    titlePath.append((isClone == false) ? "title" : "clone");
    int counter = 0;
    QString path;
    while (path.isEmpty() || QFile::exists(path)) {
        counter++;
        path = titlePath + QString::number(counter).rightJustified(3, '0', false) + ".png";
    }
    result.append(((isClone == false) ? i18n("Title") : i18n("Clone")) + ' ' + QString::number(counter).rightJustified(3, '0', false));
    result.append(path);
    return result;
}

// static
QString TitleWidget::getTitleResourceFromName(const KUrl &projectUrl, const QString &titleName)
{
    QStringList result;
    QString titlePath = projectUrl.path(KUrl::AddTrailingSlash) + "titles/";
    KStandardDirs::makeDir(titlePath);
    return titlePath + titleName + ".png";
}

// static
QStringList TitleWidget::extractImageList(QString xml)
{
    QStringList result;
    if (xml.isEmpty()) return result;
    QDomDocument doc;
    doc.setContent(xml);
    QDomNodeList images = doc.elementsByTagName("content");
    for (int i = 0; i < images.count(); i++) {
        if (images.at(i).toElement().hasAttribute("url"))
            result.append(images.at(i).toElement().attribute("url"));
    }
    return result;
}

// static
QStringList TitleWidget::extractFontList(QString xml)
{
    QStringList result;
    if (xml.isEmpty()) return result;
    QDomDocument doc;
    doc.setContent(xml);
    QDomNodeList images = doc.elementsByTagName("content");
    for (int i = 0; i < images.count(); i++) {
        if (images.at(i).toElement().hasAttribute("font"))
            result.append(images.at(i).toElement().attribute("font"));
    }
    return result;
}
//static
void TitleWidget::refreshTitleTemplates()
{
    QStringList titlenamelist = QStringList() << i18n("None");
    QStringList titlefiles = QStringList() << QString();
    QStringList filters;
    filters << "*.kdenlivetitle" ;
    titletemplates.clear();
    QStringList titleTemplates = KGlobal::dirs()->findDirs("appdata", "titles");
    foreach(const QString & folder, titleTemplates) {
        QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
        foreach(const QString & fname, filesnames) {
            //titlenamelist.append(fname);
            //titlefiles.append(KUrl(folder).path(KUrl::AddTrailingSlash) + fname);
            TitleTemplate t;
            t.name = fname;
            t.file = KUrl(folder).path(KUrl::AddTrailingSlash) + fname;
            t.icon = QIcon(KThumb::getImage(t.file, 0, 60, 60));
            titletemplates.append(t);
        }
    }
    kDebug()  << titlenamelist << titlefiles;
}
void TitleWidget::templateIndexChanged(int index)
{
    QString item = templateBox->itemData(index).toString();
    if (!item.isEmpty()) {
        if (lastDocumentHash != QCryptographicHash::hash(xml().toString().toAscii(), QCryptographicHash::Md5).toHex()) {
            if (KMessageBox::questionYesNo(this, i18n("Do you really want to load a new template? Changes in this title will be lost!")) == KMessageBox::No) return;
        }
        loadTitle(item);

        // mbt 1607: Add property to distinguish between unchanged template titles and user titles.
        // Text of unchanged template titles should be selected when clicked.
        QList<QGraphicsItem *> list = graphicsView->scene()->items();
        foreach(QGraphicsItem * qgItem, list) {
            if (qgItem->type() == TEXTITEM) {
                QGraphicsTextItem *i;
                i = static_cast<QGraphicsTextItem *>(qgItem);
                i->setProperty("isTemplate", "true");
                i->setProperty("templateText", i->toHtml());
            }

        }
        lastDocumentHash = QCryptographicHash::hash(xml().toString().toAscii(), QCryptographicHash::Md5).toHex();
    }
}
//virtual
void TitleWidget::resizeEvent(QResizeEvent * /*event*/)
{
    //slotAdjustZoom();
}
//virtual
void TitleWidget::keyPressEvent(QKeyEvent *e){
    if(e->key()!=Qt::Key_Escape && e->key()!=Qt::Key_Return && e->key()!=Qt::Key_Enter) QDialog::keyPressEvent(e);
}
void TitleWidget::slotTextTool()
{
    m_scene->setTool(TITLE_TEXT);
    showToolbars(TITLE_TEXT);
    checkButton(TITLE_TEXT);
}

void TitleWidget::slotRectTool()
{
    m_scene->setTool(TITLE_RECTANGLE);
    showToolbars(TITLE_RECTANGLE);
    checkButton(TITLE_RECTANGLE);

    // Disable dragging mode, would make dragging a rect impossible otherwise ;)
    graphicsView->setDragMode(QGraphicsView::NoDrag);
}

void TitleWidget::slotSelectTool()
{
    m_scene->setTool(TITLE_SELECT);

    // Enable rubberband selecting mode.
    graphicsView->setDragMode(QGraphicsView::RubberBandDrag);

    // Find out which toolbars need to be shown, depending on selected item
    TITLETOOL t = TITLE_SELECT;
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() > 0) {
        switch (l.at(0)->type()) {
        case TEXTITEM:
            t = TITLE_TEXT;
            break;
        case RECTITEM:
            t = TITLE_RECTANGLE;
            break;
        case IMAGEITEM:
            t = TITLE_IMAGE;
            break;
        }
    }

    enableToolbars(t);
    if (t == TITLE_RECTANGLE && (l.at(0) == m_endViewport || l.at(0) == m_startViewport)) {
        //graphicsView->centerOn(l.at(0));
        t = TITLE_SELECT;
    }
    showToolbars(t);

    if (l.size() > 0) {
        updateCoordinates(l.at(0));
        updateDimension(l.at(0));
        updateRotZoom(l.at(0));
    }

    checkButton(TITLE_SELECT);
}

void TitleWidget::slotImageTool()
{
    // TODO: find a way to get a list of all supported image types...
    QString allExtensions = "image/gif image/jpeg image/png image/x-tga image/x-bmp image/svg+xml image/tiff image/x-xcf-gimp image/x-vnd.adobe.photoshop image/x-pcx image/x-exr";
    KUrl url = KFileDialog::getOpenUrl(KUrl(), allExtensions, this, i18n("Load Image")); //"*.svg *.png *.jpg *.jpeg *.gif *.raw"
    if (!url.isEmpty()) {
        if (url.path().endsWith(".svg")) {
            QGraphicsSvgItem *svg = new QGraphicsSvgItem(url.toLocalFile());
            svg->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
            svg->setZValue(m_count++);
            svg->setData(Qt::UserRole, url.path());
            graphicsView->scene()->addItem(svg);
        } else {
            QPixmap pix(url.path());
            QGraphicsPixmapItem *image = new QGraphicsPixmapItem(pix);
            image->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
            image->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
            image->setData(Qt::UserRole, url.path());
            image->setZValue(m_count++);
            graphicsView->scene()->addItem(image);
        }
    }
    m_scene->setTool(TITLE_SELECT);
    showToolbars(TITLE_SELECT);
    checkButton(TITLE_SELECT);
}

void TitleWidget::showToolbars(TITLETOOL toolType)
{
    switch (toolType) {
    case TITLE_SELECT:
        toolbar_stack->setCurrentIndex(0);
        break;
    case TITLE_IMAGE:
        toolbar_stack->setCurrentIndex(3);
        break;
    case TITLE_RECTANGLE:
        toolbar_stack->setCurrentIndex(1);
        break;
    case TITLE_TEXT:
        toolbar_stack->setCurrentIndex(2);
        break;
    }
}

void TitleWidget::enableToolbars(TITLETOOL toolType)
{
    // TITLETOOL is defined in graphicsscenerectmove.h
    bool enable = false;
    if (toolType == TITLE_RECTANGLE || toolType == TITLE_IMAGE) enable = true;
    value_w->setEnabled(enable);
    value_h->setEnabled(enable);
}

void TitleWidget::checkButton(TITLETOOL toolType)
{
    bool bSelect = false;
    bool bText = false;
    bool bRect = false;
    bool bImage = false;

    switch (toolType) {
    case TITLE_SELECT:
        bSelect = true;
        break;
    case TITLE_TEXT:
        bText = true;
        break;
    case TITLE_RECTANGLE:
        bRect = true;
        break;
    case TITLE_IMAGE:
        bImage = true;
        break;
    default:
        break;
    }

    m_buttonCursor->setChecked(bSelect);
    m_buttonText->setChecked(bText);
    m_buttonRect->setChecked(bRect);
    m_buttonImage->setChecked(bImage);
}

void TitleWidget::displayBackgroundFrame()
{
    QRectF r = m_frameBorder->sceneBoundingRect();
    if (!displayBg->isChecked()) {
        QPixmap pattern(20, 20);
        pattern.fill();
        QColor bgcolor(210, 210, 210);
        QPainter p(&pattern);
        p.fillRect(QRect(0, 0, 10, 10), bgcolor);
        p.fillRect(QRect(10, 10, 20, 20), bgcolor);
        p.end();
        QBrush br(pattern);
        QPixmap bg((int) (r.width() / 2), (int) (r.height()/ 2));
        QPainter p2(&bg);
        p2.fillRect(bg.rect(), br);
        p2.end();
        m_frameImage->setPixmap(bg);
    } else {
        QImage img = m_render->extractFrame((int) m_render->seekPosition().frames(m_render->fps()), QString(), m_render->frameRenderWidth() / 2, m_render->renderHeight() / 2);
        m_frameImage->setPixmap(QPixmap::fromImage(img.scaled(r.width() / 2, r.height() / 2)));
    }
}

void TitleWidget::initAnimation()
{
    align_box->setEnabled(false);
    QPen startpen(Qt::DotLine);
    QPen endpen(Qt::DashDotLine);
    startpen.setColor(QColor(100, 200, 100, 140));
    endpen.setColor(QColor(200, 100, 100, 140));

    m_startViewport->setPen(startpen);
    m_endViewport->setPen(endpen);

    m_startViewport->setZValue(-1000);
    m_endViewport->setZValue(-1000);

    m_startViewport->setFlags(0);
    m_endViewport->setFlags(0);

    graphicsView->scene()->addItem(m_startViewport);
    graphicsView->scene()->addItem(m_endViewport);

    connect(keep_aspect, SIGNAL(toggled(bool)), this, SLOT(slotKeepAspect(bool)));
    connect(resize50, SIGNAL(clicked()), this, SLOT(slotResize50()));
    connect(resize100, SIGNAL(clicked()), this, SLOT(slotResize100()));
    connect(resize200, SIGNAL(clicked()), this, SLOT(slotResize200()));
}

void TitleWidget::slotUpdateZoom(int pos)
{
    zoom_spin->setValue(pos);
    zoom_slider->setValue(pos);
    m_scene->setZoom((double) pos / 100);
}

void TitleWidget::slotZoom(bool up)
{
    int pos = zoom_slider->value();
    if (up) pos++;
    else pos--;
    zoom_slider->setValue(pos);
}

void TitleWidget::slotAdjustZoom()
{
    /*double scalex = graphicsView->width() / (double)(m_frameWidth * 1.2);
    double scaley = graphicsView->height() / (double)(m_frameHeight * 1.2);
    if (scalex > scaley) scalex = scaley;
    int zoompos = (int)(scalex * 7 + 0.5);*/
    graphicsView->fitInView(m_frameBorder, Qt::KeepAspectRatio);
    int zoompos = graphicsView->matrix().m11() * 100;
    zoom_slider->setValue(zoompos);
    graphicsView->centerOn(m_frameBorder);
}

void TitleWidget::slotZoomOneToOne()
{
    zoom_slider->setValue(100);
    graphicsView->centerOn(m_frameBorder);
}

void TitleWidget::slotNewRect(QGraphicsRectItem * rect)
{
    updateAxisButtons(rect); // back to default

    QColor f = rectFColor->color();
#if not KDE_IS_VERSION(4,5,0)
    f.setAlpha(rectFAlpha->value());
#endif
    QPen penf(f);
    penf.setWidth(rectLineWidth->value());
    penf.setJoinStyle(Qt::RoundJoin);
    rect->setPen(penf);
    QColor b = rectBColor->color();
#if not KDE_IS_VERSION(4,5,0)
    b.setAlpha(rectBAlpha->value());
#endif
    rect->setBrush(QBrush(b));
    rect->setZValue(m_count++);
    rect->setData(ZOOMFACTOR, 100);
    //setCurrentItem(rect);
    //graphicsView->setFocus();
}

void TitleWidget::slotNewText(QGraphicsTextItem *tt)
{
    updateAxisButtons(tt); // back to default

    QFont font = font_family->currentFont();
    font.setPixelSize(font_size->value());
    // mbd: issue 551:
    font.setWeight(font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    font.setItalic(buttonItalic->isChecked());
    font.setUnderline(buttonUnder->isChecked());

    tt->setFont(font);
    QColor color = fontColorButton->color();
    QColor outlineColor = textOutlineColor->color();
#if not KDE_IS_VERSION(4,5,0)    
    color.setAlpha(textAlpha->value());
    outlineColor.setAlpha(textOutlineAlpha->value());
#endif
    tt->setDefaultTextColor(color);

    QTextCursor cur(tt->document());
    cur.select(QTextCursor::Document);
    QTextBlockFormat format = cur.blockFormat();
    QTextCharFormat cformat = cur.charFormat();
    double outlineWidth = textOutline->value() / 10.0;

    tt->setData(101, outlineWidth);
    tt->setData(102, outlineColor);
    if (outlineWidth > 0.0) cformat.setTextOutline(QPen(outlineColor, outlineWidth));

    cformat.setForeground(QBrush(color));
    cur.setCharFormat(cformat);
    cur.setBlockFormat(format);
    tt->setTextCursor(cur);
    tt->setZValue(m_count++);
    setCurrentItem(tt);
}

void TitleWidget::setFontBoxWeight(int weight)
{
    int index = font_weight_box->findData(weight);
    if (index < 0) {
        index = font_weight_box->findData(QFont::Normal);
    }
    font_weight_box->setCurrentIndex(index);
}

void TitleWidget::setCurrentItem(QGraphicsItem *item)
{
    m_scene->setSelectedItem(item);
}

void TitleWidget::zIndexChanged(int v)
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    for (int i = 0; i < l.size(); i++) {
        l[i]->setZValue(v);
    }
}

void TitleWidget::selectionChanged()
{
    if (m_scene->tool() != TITLE_SELECT) return;

    std::cout << "Number of selected items: " << graphicsView->scene()->selectedItems().length() << "\n";

    QList<QGraphicsItem *> l;

    // mbt 1607: One text item might have grabbed the keyboard.
    // Ungrab it for all items that are not selected, otherwise
    // text input would only work for the text item that grabbed
    // the keyboard last.
    l = graphicsView->scene()->items();
    foreach(QGraphicsItem * item, l) {
        if (item->type() == TEXTITEM && !item->isSelected()) {
            QGraphicsTextItem *i = static_cast<QGraphicsTextItem *>(item);
            i->ungrabKeyboard();
        }
    }

    l = graphicsView->scene()->selectedItems();

    if (l.size() > 0) {
        buttonUnselectAll->setEnabled(true);
        // Enable all z index buttons if items selected.
        // We can selectively disable them later.
        zUp->setEnabled(true);
        zDown->setEnabled(true);
        zTop->setEnabled(true);
        zBottom->setEnabled(true);
    } else {
        buttonUnselectAll->setEnabled(false);
    }
    if (l.size() >= 2) {
        buttonSelectText->setEnabled(true);
        buttonSelectRects->setEnabled(true);
        buttonSelectImages->setEnabled(true);
    } else {
        buttonSelectText->setEnabled(false);
        buttonSelectRects->setEnabled(false);
        buttonSelectImages->setEnabled(false);
    }

    if (l.size() == 0) {
        prepareTools(NULL);
    } else if (l.size() == 1) {
        prepareTools(l.at(0));
    } else {
        /*
        For multiple selected objects we need to decide which tools to show.
        */
        int firstType = l.at(0)->type();
        bool allEqual = true;
        for (int i = 0; i < l.size(); i++) {
            if (l.at(i)->type() != firstType) {
                allEqual = false;
                break;
            }
        }
        std::cout << "All equal? " << allEqual << ".\n";
        if (allEqual) {
            prepareTools(l.at(0));
        } else {
            // Get the default toolset, but enable the property frame (x,y,w,h)
            prepareTools(NULL);
            frame_properties->setEnabled(true);

            // Enable x/y/w/h if it makes sense.
            value_x->setEnabled(true);
            value_y->setEnabled(true);
            bool containsTextitem = false;
            for (int i = 0; i < l.size(); i++) {
                if (l.at(i)->type() == TEXTITEM) {
                    containsTextitem = true;
                    break;
                }
            }
            if (!containsTextitem) {
                value_w->setEnabled(true);
                value_h->setEnabled(true);
            }
        }

        // Disable z index buttons if they don't make sense for the current selection
        int firstZindex = l.at(0)->zValue();
        allEqual = true;
        for (int i = 0; i < l.size(); i++) {
            if (l[i]->zValue() != firstZindex) {
                allEqual = false;
                break;
            }
        }
        if (!allEqual) {
            zUp->setEnabled(false);
            zDown->setEnabled(false);
        }


    }

}

void TitleWidget::slotValueChanged(int type)
{
    /*
    type tells us which QSpinBox value has changed.
    */

    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    std::cout << l.size() << " items to be resized\n";

    // Get the updated value here already to do less coding afterwards
    int val = 0;
    switch (type) {
    case ValueWidth:
        val = value_w->value();
        break;
    case ValueHeight:
        val = value_h->value();
        break;
    case ValueX:
        val = value_x->value();
        break;
    case ValueY:
        val = value_y->value();
        break;
    }

    for (int k = 0; k < l.size(); k++) {
        std::cout << "Type of item " << k << ": " << l.at(k)->type() << "\n";

        if (l.at(k)->type() == TEXTITEM) {
            // Just update the position. We don't allow setting width/height for text items yet.
            switch (type) {
            case ValueX:
                updatePosition(l.at(k), val, l.at(k)->pos().y());
                break;
            case ValueY:
                updatePosition(l.at(k), l.at(k)->pos().x(), val);
                break;
            }

        } else if (l.at(k)->type() == RECTITEM) {
            QGraphicsRectItem *rec = static_cast <QGraphicsRectItem *>(l.at(k));
            switch (type) {
            case ValueX:
                updatePosition(l.at(k), val, l.at(k)->pos().y());
                break;
            case ValueY:
                updatePosition(l.at(k), l.at(k)->pos().x(), val);
                break;
            case ValueWidth:
                rec->setRect(QRect(0, 0, val, rec->rect().height()));
                break;
            case ValueHeight:
                rec->setRect(QRect(0, 0, rec->rect().width(), val));
                break;
            }

        } else if (l.at(k)->type() == IMAGEITEM) {


            if (type == ValueX) {
                updatePosition(l.at(k), val, l.at(k)->pos().y());

            } else if (type == ValueY) {
                updatePosition(l.at(k), l.at(k)->pos().x(), val);

            } else {
                // Width/height has changed. This is more complex.

                QGraphicsItem *i = l.at(k);
                Transform t = m_transformations.value(i);

                // Ratio width:height
                double phi = (double) i->boundingRect().width() / i->boundingRect().height();
                // TODO: proper calculation for rotation around 3 axes
                double alpha = (double) t.rotatez / 180.0 * M_PI;

                // New length
                double length = val;

                // Scaling factor
                double scale = 1;

                // We want to keep the aspect ratio of the image as the user does not yet have the possibility
                // to restore the original ratio. You rarely want to change it anyway.
                switch (type) {
                case ValueWidth:
                    // Add 0.5 because otherwise incrementing by 1 might have no effect
                    length = val / (cos(alpha) + 1 / phi * sin(alpha)) + 0.5;
                    scale = length / i->boundingRect().width();
                    break;
                case ValueHeight:
                    length = val / (phi * sin(alpha) + cos(alpha)) + 0.5;
                    scale = length / i->boundingRect().height();
                    break;
                }

                t.scalex = scale;
                t.scaley = scale;
                QTransform qtrans;
                qtrans.scale(scale, scale);
                qtrans.rotate(t.rotatex, Qt::XAxis);
                qtrans.rotate(t.rotatey, Qt::YAxis);
                qtrans.rotate(t.rotatez, Qt::ZAxis);
                i->setTransform(qtrans);
                std::cout << "scale is: " << scale << "\n";
                std::cout << i->boundingRect().width() << ": new width\n";
                m_transformations[i] = t;

                if (l.size() == 1) {
                    // Only update the w/h values if the selection contains just one item.
                    // Otherwise, what should we do? ;)
                    // (Use the values of the first item? Of the second? Of the x-th?)
                    updateDimension(i);
                    // Update rotation/zoom values.
                    // These values are not yet able to handle multiple items!
                    updateRotZoom(i);
                }
            }

        }
    }


}

void TitleWidget::updateDimension(QGraphicsItem *i)
{
    bool wBlocked = value_w->signalsBlocked();
    bool hBlocked = value_h->signalsBlocked();
    bool zBlocked = zValue->signalsBlocked();
    value_w->blockSignals(true);
    value_h->blockSignals(true);
    zValue->blockSignals(true);

    zValue->setValue((int) i->zValue());
    if (i->type() == IMAGEITEM) {
        // Get multipliers for rotation/scaling

        /*Transform t = m_transformations.value(i);
        QRectF r = i->boundingRect();
        int width = (int) ( abs(r.width()*t.scalex * cos(t.rotate/180.0*M_PI))
                    + abs(r.height()*t.scaley * sin(t.rotate/180.0*M_PI)) );
        int height = (int) ( abs(r.height()*t.scaley * cos(t.rotate/180*M_PI))
                    + abs(r.width()*t.scalex * sin(t.rotate/180*M_PI)) );*/

        value_w->setValue(i->sceneBoundingRect().width());
        value_h->setValue(i->sceneBoundingRect().height());
    } else if (i->type() == RECTITEM) {
        QGraphicsRectItem *r = static_cast <QGraphicsRectItem *>(i);
        std::cout << "Rect width is: " << r->rect().width() << ", was: " << value_w->value() << "\n";
        value_w->setValue((int) r->rect().width());
        value_h->setValue((int) r->rect().height());
    } else if (i->type() == TEXTITEM) {
        QGraphicsTextItem *t = static_cast <QGraphicsTextItem *>(i);
        value_w->setValue((int) t->boundingRect().width());
        value_h->setValue((int) t->boundingRect().height());
    }

    zValue->blockSignals(zBlocked);
    value_w->blockSignals(wBlocked);
    value_h->blockSignals(hBlocked);
}

void TitleWidget::updateCoordinates(QGraphicsItem *i)
{
    // Block signals emitted by this method
    value_x->blockSignals(true);
    value_y->blockSignals(true);

    if (i->type() == TEXTITEM) {

        QGraphicsTextItem *rec = static_cast <QGraphicsTextItem *>(i);

        // Set the correct x coordinate value
        if (origin_x_left->isChecked()) {
            // Origin (0 point) is at m_frameWidth, coordinate axis is inverted
            value_x->setValue((int)(m_frameWidth - rec->pos().x() - rec->boundingRect().width()));
        } else {
            // Origin is at 0 (default)
            value_x->setValue((int) rec->pos().x());
        }

        // Same for y
        if (origin_y_top->isChecked()) {
            value_y->setValue((int)(m_frameHeight - rec->pos().y() - rec->boundingRect().height()));
        } else {
            value_y->setValue((int) rec->pos().y());
        }

    } else if (i->type() == RECTITEM) {

        QGraphicsRectItem *rec = static_cast <QGraphicsRectItem *>(i);

        if (origin_x_left->isChecked()) {
            // Origin (0 point) is at m_frameWidth
            value_x->setValue((int)(m_frameWidth - rec->pos().x() - rec->rect().width()));
        } else {
            // Origin is at 0 (default)
            value_x->setValue((int) rec->pos().x());
        }

        if (origin_y_top->isChecked()) {
            value_y->setValue((int)(m_frameHeight - rec->pos().y() - rec->rect().height()));
        } else {
            value_y->setValue((int) rec->pos().y());
        }

    } else if (i->type() == IMAGEITEM) {

        if (origin_x_left->isChecked()) {
            value_x->setValue((int)(m_frameWidth - i->pos().x() - i->sceneBoundingRect().width()));
        } else {
            value_x->setValue((int) i->pos().x());
        }

        if (origin_y_top->isChecked()) {
            value_y->setValue((int)(m_frameHeight - i->pos().y() - i->sceneBoundingRect().height()));
        } else {
            value_y->setValue((int) i->pos().y());
        }

    }

    // Stop blocking signals now
    value_x->blockSignals(false);
    value_y->blockSignals(false);
}

void TitleWidget::updateRotZoom(QGraphicsItem *i)
{
    itemzoom->blockSignals(true);
    itemrotatex->blockSignals(true);
    itemrotatey->blockSignals(true);
    itemrotatez->blockSignals(true);

    Transform t = m_transformations.value(i);

    if (!i->data(ZOOMFACTOR).isNull()) itemzoom->setValue(i->data(ZOOMFACTOR).toInt());
    else itemzoom->setValue((int)(t.scalex * 100.0 + 0.5));

    itemrotatex->setValue((int)(t.rotatex));
    itemrotatey->setValue((int)(t.rotatey));
    itemrotatez->setValue((int)(t.rotatez));

    itemzoom->blockSignals(false);
    itemrotatex->blockSignals(false);
    itemrotatey->blockSignals(false);
    itemrotatez->blockSignals(false);
}

void TitleWidget::updatePosition(QGraphicsItem *i)
{
    updatePosition(i, value_x->value(), value_y->value());
}

void TitleWidget::updatePosition(QGraphicsItem *i, int x, int y)
{
    if (i->type() == TEXTITEM) {
        QGraphicsTextItem *rec = static_cast <QGraphicsTextItem *>(i);

        int posX;
        if (origin_x_left->isChecked()) {
            /*
             * Origin of the X axis is at m_frameWidth, and distance from right
             * border of the item to the right border of the frame is taken. See
             * comment to slotOriginXClicked().
             */
            posX = m_frameWidth - x - rec->boundingRect().width();
        } else {
            posX = x;
        }

        int posY;
        if (origin_y_top->isChecked()) {
            /* Same for y axis */
            posY = m_frameHeight - y - rec->boundingRect().height();
        } else {
            posY = y;
        }

        rec->setPos(posX, posY);

    } else if (i->type() == RECTITEM) {

        QGraphicsRectItem *rec = static_cast <QGraphicsRectItem *>(i);

        int posX;
        if (origin_x_left->isChecked()) {
            posX = m_frameWidth - x - rec->rect().width();
        } else {
            posX = x;
        }

        int posY;
        if (origin_y_top->isChecked()) {
            posY = m_frameHeight - y - rec->rect().height();
        } else {
            posY = y;
        }

        rec->setPos(posX, posY);

    } else if (i->type() == IMAGEITEM) {
        int posX;
        if (origin_x_left->isChecked()) {
            // Use the sceneBoundingRect because this also regards transformations like zoom
            posX = m_frameWidth - x - i->sceneBoundingRect().width();
        } else {
            posX = x;
        }

        int posY;
        if (origin_y_top->isChecked()) {
            posY = m_frameHeight - y - i->sceneBoundingRect().height();
        } else {
            posY = y;
        }

        i->setPos(posX, posY);

    }

}

void TitleWidget::updateTextOriginX()
{
    if (origin_x_left->isChecked()) {
        origin_x_left->setText(i18n("\u2212X"));
    } else {
        origin_x_left->setText(i18n("+X"));
    }
}

void TitleWidget::slotOriginXClicked()
{
    // Update the text displayed on the button.
    updateTextOriginX();

    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        updateCoordinates(l.at(0));

        // Remember x axis setting
        l.at(0)->setData(TitleDocument::OriginXLeft, origin_x_left->isChecked() ?
                         TitleDocument::AxisInverted : TitleDocument::AxisDefault);
    }
    graphicsView->setFocus();
}

void TitleWidget::updateTextOriginY()
{
    if (origin_y_top->isChecked()) {
        origin_y_top->setText(i18n("\u2212Y"));
    } else {
        origin_y_top->setText(i18n("+Y"));
    }
}

void TitleWidget::slotOriginYClicked()
{
    // Update the text displayed on the button.
    updateTextOriginY();

    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        updateCoordinates(l.at(0));

        l.at(0)->setData(TitleDocument::OriginYTop, origin_y_top->isChecked() ?
                         TitleDocument::AxisInverted : TitleDocument::AxisDefault);

    }
    graphicsView->setFocus();
}

void TitleWidget::updateAxisButtons(QGraphicsItem *i)
{
    int xAxis = i->data(TitleDocument::OriginXLeft).toInt();
    int yAxis = i->data(TitleDocument::OriginYTop).toInt();
    origin_x_left->blockSignals(true);
    origin_y_top->blockSignals(true);

    if (xAxis == TitleDocument::AxisInverted) {
        origin_x_left->setChecked(true);
    } else {
        origin_x_left->setChecked(false);
    }
    updateTextOriginX();

    if (yAxis == TitleDocument::AxisInverted) {
        origin_y_top->setChecked(true);
    } else {
        origin_y_top->setChecked(false);
    }
    updateTextOriginY();

    origin_x_left->blockSignals(false);
    origin_y_top->blockSignals(false);
}

void TitleWidget::slotChangeBackground()
{
    QColor color = backgroundColor->color();
    m_scene->setBackgroundBrush(QBrush(color));
    color.setAlpha(backgroundAlpha->value());
    m_frameBorder->setBrush(QBrush(color));
}

void TitleWidget::slotChanged()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1 && l.at(0)->type() == TEXTITEM) {
        textChanged(static_cast <QGraphicsTextItem *>(l.at(0)));
    }
}

void TitleWidget::textChanged(QGraphicsTextItem *i)
{
    /*
     * If the user has set origin_x_left (the same for y), we need to look
     * whether a text element has been selected. If yes, we need to ensure that
     * the right border of the text field remains fixed also when some text has
     * been entered.
     *
     * This is also known as right-justified, with the difference that it is not
     * valid for text but for its boundingRect. Text may still be
     * left-justified.
     */
    updateDimension(i);

    if (origin_x_left->isChecked() || origin_y_top->isChecked()) {
        if (!i->toPlainText().isEmpty())
            updatePosition(i);
        else {
            /*
             * Don't do anything if the string is empty. If the position were
             * updated here, a newly created text field would be set to the
             * position of the last selected text field.
             */
        }
    }

    // mbt 1607: Template text has changed; don't auto-select content anymore.
    if (i->property("isTemplate").isValid()) {
        if (i->property("templateText").isValid()) {
            if (i->property("templateText") == i->toHtml()) {
                // Unchanged, do nothing.
            } else {
                i->setProperty("isTemplate", QVariant::Invalid);
                i->setProperty("templateText", QVariant::Invalid);
            }
        }
    }
}

void TitleWidget::slotInsertUnicode()
{
    m_unicodeDialog->exec();
}

void TitleWidget::slotInsertUnicodeString(QString text)
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() > 0) {
        if (l.at(0)->type() == TEXTITEM) {
            QGraphicsTextItem *t = static_cast <QGraphicsTextItem *>(l.at(0));
            t->textCursor().insertText(text);
        }
    }
}

void TitleWidget::slotUpdateText()
{
    QFont font = font_family->currentFont();
    font.setPixelSize(font_size->value());
    font.setItalic(buttonItalic->isChecked());
    font.setUnderline(buttonUnder->isChecked());
    font.setWeight(font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    QColor color = fontColorButton->color();
    QColor outlineColor = textOutlineColor->color();
#if not KDE_IS_VERSION(4,5,0)
    color.setAlpha(textAlpha->value());
    outlineColor.setAlpha(textOutlineAlpha->value());
#endif

    double outlineWidth = textOutline->value() / 10.0;

    int i;
    for (i = 0; i < graphicsView->scene()->selectedItems().length(); i++) {
        QGraphicsTextItem* item = NULL;
        QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
        if (l.at(i)->type() == TEXTITEM) {
            item = static_cast <QGraphicsTextItem *>(l.at(i));
        }
        if (!item) {
            // No text item, try next one.
            continue;
        }

        // Set alignment of all text in the text item
        QTextCursor cur(item->document());
        cur.select(QTextCursor::Document);
        QTextBlockFormat format = cur.blockFormat();
        if (buttonAlignLeft->isChecked() || buttonAlignCenter->isChecked() || buttonAlignRight->isChecked()) {
            item->setTextWidth(item->boundingRect().width());
            if (buttonAlignCenter->isChecked()) format.setAlignment(Qt::AlignHCenter);
            else if (buttonAlignRight->isChecked()) format.setAlignment(Qt::AlignRight);
            else if (buttonAlignLeft->isChecked()) format.setAlignment(Qt::AlignLeft);
        } else {
            format.setAlignment(Qt::AlignLeft);
            item->setTextWidth(-1);
        }

        // Set font properties
        item->setFont(font);
        QTextCharFormat cformat = cur.charFormat();

        item->setData(101, outlineWidth);
        item->setData(102, outlineColor);
        if (outlineWidth > 0.0) cformat.setTextOutline(QPen(outlineColor, outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        cformat.setForeground(QBrush(color));
        cur.setCharFormat(cformat);
        cur.setBlockFormat(format);
//  item->setTextCursor(cur);
        cur.clearSelection();
        item->setTextCursor(cur);
        item->setDefaultTextColor(color);
    }
}

void TitleWidget::rectChanged()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    int i;
    for (i = 0; i < l.length(); i++) {
        if (l.at(i)->type() == RECTITEM && !settingUp) {
            QGraphicsRectItem *rec = static_cast<QGraphicsRectItem *>(l.at(i));
            QColor f = rectFColor->color();
#if not KDE_IS_VERSION(4,5,0)
            f.setAlpha(rectFAlpha->value());
#endif
            QPen penf(f);
            penf.setWidth(rectLineWidth->value());
            penf.setJoinStyle(Qt::RoundJoin);
            rec->setPen(penf);
            QColor b = rectBColor->color();
#if not KDE_IS_VERSION(4,5,0)
            b.setAlpha(rectBAlpha->value());
#endif
            rec->setBrush(QBrush(b));
        }
    }
}

void TitleWidget::itemScaled(int val)
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        Transform x = m_transformations.value(l.at(0));
        x.scalex = (double)val / 100.0;
        x.scaley = (double)val / 100.0;
        QTransform qtrans;
        qtrans.scale(x.scalex, x.scaley);
        qtrans.rotate(x.rotatex, Qt::XAxis);
        qtrans.rotate(x.rotatey, Qt::YAxis);
        qtrans.rotate(x.rotatez, Qt::ZAxis);
        l[0]->setTransform(qtrans);
        l[0]->setData(ZOOMFACTOR, val);
        m_transformations[l.at(0)] = x;
        updateDimension(l.at(0));
    }
}

void TitleWidget::itemRotateX(qreal val)
{
    itemRotate(val, 0);
}

void TitleWidget::itemRotateY(qreal val)
{
    itemRotate(val, 1);
}

void TitleWidget::itemRotateZ(qreal val)
{
    itemRotate(val, 2);
}

void TitleWidget::itemRotate(qreal val, int axis)
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        Transform x = m_transformations[l.at(0)];
        switch (axis) {
        case 0:
            x.rotatex = val;
            break;
        case 1:
            x.rotatey = val;
            break;
        case 2:
            x.rotatez = val;
            break;
        }

        l[0]->setData(ROTATEFACTOR, QList<QVariant>() << QVariant(x.rotatex) << QVariant(x.rotatey) << QVariant(x.rotatez));

        QTransform qtrans;
        qtrans.scale(x.scalex, x.scaley);
        qtrans.rotate(x.rotatex, Qt::XAxis);
        qtrans.rotate(x.rotatey, Qt::YAxis);
        qtrans.rotate(x.rotatez, Qt::ZAxis);
        l[0]->setTransform(qtrans);
        m_transformations[l.at(0)] = x;
        if (l[0]->data(ZOOMFACTOR).isNull()) l[0]->setData(ZOOMFACTOR, 100);
        updateDimension(l.at(0));
    }
}

void TitleWidget::itemHCenter()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        int width = (int)br.width();
        int newPos = (int)((m_frameWidth - width) / 2);
        newPos += item->pos().x() - br.left(); // Check item transformation
        item->setPos(newPos, item->pos().y());
        updateCoordinates(item);
    }
}

void TitleWidget::itemVCenter()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        int height = (int)br.height();
        int newPos = (int)((m_frameHeight - height) / 2);
        newPos += item->pos().y() - br.top(); // Check item transformation
        item->setPos(item->pos().x(), newPos);
        updateCoordinates(item);
    }
}

void TitleWidget::itemTop()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.top() > 0) diff = -br.top();
        else diff = -br.bottom();
        item->moveBy(0, diff);
        updateCoordinates(item);
    }
}

void TitleWidget::itemBottom()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.bottom() > m_frameHeight) diff = m_frameHeight - br.top();
        else diff = m_frameHeight - br.bottom();
        item->moveBy(0, diff);
        updateCoordinates(item);
    }
}

void TitleWidget::itemLeft()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.left() > 0) diff = -br.left();
        else diff = -br.right();
        item->moveBy(diff, 0);
        updateCoordinates(item);
    }
}

void TitleWidget::itemRight()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.right() < m_frameWidth) diff = m_frameWidth - br.right();
        else diff = m_frameWidth - br.left();
        item->moveBy(diff, 0);
        updateCoordinates(item);
    }
}

void TitleWidget::setupViewports()
{
    //double aspect_ratio = 4.0 / 3.0;//read from project
    //better zoom centered, but render uses only the created rect, so no problem to change the zoom function
    /*QRectF sp(0, 0, startViewportSize->value() * m_frameWidth / 100.0 , startViewportSize->value()* m_frameHeight / 100.0);
    QRectF ep(0, 0, endViewportSize->value() * m_frameWidth / 100.0, endViewportSize->value() * m_frameHeight / 100.0);
    // use a polygon thiat uses 16:9 and 4:3 rects forpreview the size in all aspect ratios ?
    QPolygonF spoly(sp);
    QPolygonF epoly(ep);
    spoly.translate(startViewportX->value(), startViewportY->value());
    epoly.translate(endViewportX->value(), endViewportY->value());
    m_startViewport->setPolygon(spoly);
    m_endViewport->setPolygon(epoly);
    if (! insertingValues) {
        m_startViewport->setData(0, startViewportX->value());
        m_startViewport->setData(1, startViewportY->value());
        m_startViewport->setData(2, startViewportSize->value());

        m_endViewport->setData(0, endViewportX->value());
        m_endViewport->setData(1, endViewportY->value());
        m_endViewport->setData(2, endViewportSize->value());
    }*/
}

void TitleWidget::loadTitle(KUrl url)
{
    if (url.isEmpty()) url = KFileDialog::getOpenUrl(KUrl(m_projectTitlePath), "application/x-kdenlivetitle", this, i18n("Load Title"));
    if (!url.isEmpty()) {
        QList<QGraphicsItem *> items = m_scene->items();
        for (int i = 0; i < items.size(); i++) {
            if (items.at(i)->zValue() > -1000) delete items.at(i);
        }
        m_scene->clearTextSelection();
        QDomDocument doc;
        QString tmpfile;

        if (KIO::NetAccess::download(url, tmpfile, 0)) {
            QFile file(tmpfile);
            if (file.open(QIODevice::ReadOnly)) {
                doc.setContent(&file, false);
                file.close();
            } else return;
            KIO::NetAccess::removeTempFile(tmpfile);
        }
        setXml(doc);

        /*int out;
        m_count = m_titledocument.loadDocument(url, m_startViewport, m_endViewport, &out) + 1;
        adjustFrameSize();
        title_duration->setText(m_tc.getTimecode(GenTime(out, m_render->fps())));
        insertingValues = true;
        startViewportX->setValue(m_startViewport->data(0).toInt());
        startViewportY->setValue(m_startViewport->data(1).toInt());
        startViewportSize->setValue(m_startViewport->data(2).toInt());
        endViewportX->setValue(m_endViewport->data(0).toInt());
        endViewportY->setValue(m_endViewport->data(1).toInt());
        endViewportSize->setValue(m_endViewport->data(2).toInt());

        insertingValues = false;
        slotSelectTool();
        slotAdjustZoom();*/
    }
}

void TitleWidget::saveTitle(KUrl url)
{
    if (anim_start->isChecked()) slotAnimStart(false);
    if (anim_end->isChecked()) slotAnimEnd(false);
    bool embed_image=false;
    if (KMessageBox::questionYesNo(this, i18n("Do you want to embed Images into this TitleDocument?\nThis is most needed for sharing Titles.")) != KMessageBox::No)
    {
        embed_image=true;	
    }
    if (url.isEmpty()) {
        QPointer<KFileDialog> fs = new KFileDialog(KUrl(m_projectTitlePath), "application/x-kdenlivetitle", this);
        fs->setOperationMode(KFileDialog::Saving);
        fs->setMode(KFile::File);
        fs->setConfirmOverwrite(true);
        fs->setKeepLocation(true);
        fs->exec();
        if (fs) url = fs->selectedUrl();
        delete fs;
    }
    if (!url.isEmpty()) {
        if (m_titledocument.saveDocument(url, m_startViewport, m_endViewport, m_tc.getFrameCount(title_duration->text()), embed_image) == false)
            KMessageBox::error(this, i18n("Cannot write to file %1", url.path()));
    }
}

QDomDocument TitleWidget::xml()
{
    QDomDocument doc = m_titledocument.xml(m_startViewport, m_endViewport);
    doc.documentElement().setAttribute("out", m_tc.getFrameCount(title_duration->text()));
    return doc;
}

int TitleWidget::outPoint() const
{
    return m_tc.getFrameCount(title_duration->text());
}

void TitleWidget::setXml(QDomDocument doc)
{
    int out;
    m_count = m_titledocument.loadFromXml(doc, m_startViewport, m_endViewport, &out, m_projectTitlePath);
    adjustFrameSize();
    title_duration->setText(m_tc.getTimecode(GenTime(out, m_render->fps())));
    /*if (doc.documentElement().hasAttribute("out")) {
    GenTime duration = GenTime(doc.documentElement().attribute("out").toDouble() / 1000.0);
    title_duration->setText(m_tc.getTimecode(duration));
    }
    else title_duration->setText(m_tc.getTimecode(GenTime(5000)));*/

    QDomElement e = doc.documentElement();
    m_transformations.clear();
    QList <QGraphicsItem *> items = graphicsView->scene()->items();
    const double PI = 4.0 * atan(1.0);
    for (int i = 0; i < items.count(); i++) {
        QTransform t = items.at(i)->transform();
        Transform x;
        x.scalex = t.m11();
        x.scaley = t.m22();
        if (!items.at(i)->data(ROTATEFACTOR).isNull()) {
            QList<QVariant> rotlist = items.at(i)->data(ROTATEFACTOR).toList();
            if (rotlist.count() >= 3) {
                x.rotatex = rotlist[0].toDouble();
                x.rotatey = rotlist[1].toDouble();
                x.rotatez = rotlist[2].toDouble();

                // Try to adjust zoom
                t.rotate(x.rotatex *(-1), Qt::XAxis);
                t.rotate(x.rotatey *(-1), Qt::YAxis);
                t.rotate(x.rotatez *(-1), Qt::ZAxis);
                x.scalex = t.m11();
                x.scaley = t.m22();
            } else {
                x.rotatex = 0;
                x.rotatey = 0;
                x.rotatez = 0;
            }
        } else {
            x.rotatex = 0;
            x.rotatey = 0;
            x.rotatez = 180. / PI * atan2(-t.m21(), t.m11());
        }
        m_transformations[items.at(i)] = x;
    }
    // mbd: Update the GUI color selectors to match the stuff from the loaded document
    QColor background_color = m_titledocument.getBackgroundColor();
    backgroundAlpha->blockSignals(true);
    backgroundColor->blockSignals(true);
    backgroundAlpha->setValue(background_color.alpha());
    background_color.setAlpha(255);
    backgroundColor->setColor(background_color);
    backgroundAlpha->blockSignals(false);
    backgroundColor->blockSignals(false);

    /*startViewportX->setValue(m_startViewport->data(0).toInt());
    startViewportY->setValue(m_startViewport->data(1).toInt());
    startViewportSize->setValue(m_startViewport->data(2).toInt());
    endViewportX->setValue(m_endViewport->data(0).toInt());
    endViewportY->setValue(m_endViewport->data(1).toInt());
    endViewportSize->setValue(m_endViewport->data(2).toInt());*/

    QTimer::singleShot(200, this, SLOT(slotAdjustZoom()));
    slotSelectTool();
    selectionChanged();
}

void TitleWidget::slotAccepted()
{
    if (anim_start->isChecked()) slotAnimStart(false);
    if (anim_end->isChecked()) slotAnimEnd(false);
    writeChoices();
}

void TitleWidget::writeChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup titleConfig(config, "TitleWidget");
    // Write the entries
    titleConfig.writeEntry("font_family", font_family->currentFont());
    //titleConfig.writeEntry("font_size", font_size->value());
    titleConfig.writeEntry("font_pixel_size", font_size->value());
    titleConfig.writeEntry("font_color", fontColorButton->color());
    titleConfig.writeEntry("font_outline_color", textOutlineColor->color());
#if KDE_IS_VERSION(4,5,0)
    titleConfig.writeEntry("font_alpha", fontColorButton->color().alpha());
    titleConfig.writeEntry("font_outline_alpha", textOutlineColor->color().alpha());
#else
    titleConfig.writeEntry("font_alpha", textAlpha->value());
    titleConfig.writeEntry("font_outline_alpha", textOutlineAlpha->value());
#endif
   
    titleConfig.writeEntry("font_outline", textOutline->value());
    titleConfig.writeEntry("font_weight", font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    titleConfig.writeEntry("font_italic", buttonItalic->isChecked());
    titleConfig.writeEntry("font_underlined", buttonUnder->isChecked());

    titleConfig.writeEntry("rect_background_color", rectBColor->color());
    titleConfig.writeEntry("rect_foreground_color", rectFColor->color());

#if KDE_IS_VERSION(4,5,0)
    titleConfig.writeEntry("rect_background_alpha", rectBColor->color().alpha());
    titleConfig.writeEntry("rect_foreground_alpha", rectFColor->color().alpha());
#else
    titleConfig.writeEntry("rect_background_alpha", rectBAlpha->value());
    titleConfig.writeEntry("rect_foreground_alpha", rectFAlpha->value());
#endif

    titleConfig.writeEntry("rect_line_width", rectLineWidth->value());

    titleConfig.writeEntry("background_color", backgroundColor->color());
    titleConfig.writeEntry("background_alpha", backgroundAlpha->value());

    //! \todo Not sure if I should sync - it is probably safe to do it
    config->sync();

}

void TitleWidget::readChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup titleConfig(config, "TitleWidget");
    // read the entries
    font_family->setCurrentFont(titleConfig.readEntry("font_family", font_family->currentFont()));
    font_size->setValue(titleConfig.readEntry("font_pixel_size", font_size->value()));
    m_scene->slotUpdateFontSize(font_size->value());
    QColor fontColor = QColor(titleConfig.readEntry("font_color", fontColorButton->color()));
    QColor outlineColor = QColor(titleConfig.readEntry("font_outline_color", textOutlineColor->color()));
#if KDE_IS_VERSION(4,5,0)
    fontColor.setAlpha(titleConfig.readEntry("font_alpha", fontColor.alpha()));
    outlineColor.setAlpha(titleConfig.readEntry("font_outline_alpha", outlineColor.alpha()));
#else
    textAlpha->setValue(titleConfig.readEntry("font_alpha", textAlpha->value()));
    textOutlineAlpha->setValue(titleConfig.readEntry("font_outline_alpha", textOutlineAlpha->value()));
#endif
    fontColorButton->setColor(fontColor);
    textOutlineColor->setColor(outlineColor);
    textOutline->setValue(titleConfig.readEntry("font_outline", textOutline->value()));

    int weight;
    if (titleConfig.readEntry("font_bold", false)) weight = QFont::Bold;
    else weight = titleConfig.readEntry("font_weight", font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    setFontBoxWeight(weight);
    buttonItalic->setChecked(titleConfig.readEntry("font_italic", buttonItalic->isChecked()));
    buttonUnder->setChecked(titleConfig.readEntry("font_underlined", buttonUnder->isChecked()));

    QColor fgColor = QColor(titleConfig.readEntry("rect_foreground_color", rectFColor->color()));
    QColor bgColor = QColor(titleConfig.readEntry("rect_background_color", rectBColor->color()));

#if KDE_IS_VERSION(4,5,0)
    fgColor.setAlpha(titleConfig.readEntry("rect_background_alpha", fgColor.alpha()));
    bgColor.setAlpha(titleConfig.readEntry("rect_background_alpha", bgColor.alpha()));
#else
    rectFAlpha->setValue(titleConfig.readEntry("rect_foreground_alpha", rectFAlpha->value()));
    rectBAlpha->setValue(titleConfig.readEntry("rect_background_alpha", rectBAlpha->value()));
#endif
    rectFColor->setColor(fgColor);
    rectBColor->setColor(bgColor);

    rectLineWidth->setValue(titleConfig.readEntry("rect_line_width", rectLineWidth->value()));

    backgroundColor->setColor(titleConfig.readEntry("background_color", backgroundColor->color()));
    backgroundAlpha->setValue(titleConfig.readEntry("background_alpha", backgroundAlpha->value()));
}

void TitleWidget::adjustFrameSize()
{
    m_frameWidth = m_titledocument.frameWidth();
    m_frameHeight = m_titledocument.frameHeight();
    m_frameBorder->setRect(0, 0, m_frameWidth, m_frameHeight);
    displayBackgroundFrame();
}

void TitleWidget::slotAnimStart(bool anim)
{
    if (anim && anim_end->isChecked()) {
        anim_end->setChecked(false);
        m_endViewport->setZValue(-1000);
        m_endViewport->setBrush(QBrush());
    }
    slotSelectTool();
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->zValue() > -1000) {
            list.at(i)->setFlag(QGraphicsItem::ItemIsMovable, !anim);
            list.at(i)->setFlag(QGraphicsItem::ItemIsSelectable, !anim);
        }
    }
    align_box->setEnabled(anim);
    itemzoom->setEnabled(!anim);
    itemrotatex->setEnabled(!anim);
    itemrotatey->setEnabled(!anim);
    itemrotatez->setEnabled(!anim);
    frame_toolbar->setEnabled(!anim);
    toolbar_stack->setEnabled(!anim);
    if (anim) {
        keep_aspect->setChecked(!m_startViewport->data(0).isNull());
        m_startViewport->setZValue(1100);
        QColor col = m_startViewport->pen().color();
        col.setAlpha(100);
        m_startViewport->setBrush(col);
        m_startViewport->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        m_startViewport->setSelected(true);
        selectionChanged();
        slotSelectTool();
        if (m_startViewport->childItems().isEmpty()) addAnimInfoText();
    } else {
        m_startViewport->setZValue(-1000);
        m_startViewport->setBrush(QBrush());
        m_startViewport->setFlags(0);
        if (!anim_end->isChecked()) deleteAnimInfoText();
    }

}

void TitleWidget::slotAnimEnd(bool anim)
{
    if (anim && anim_start->isChecked()) {
        anim_start->setChecked(false);
        m_startViewport->setZValue(-1000);
        m_startViewport->setBrush(QBrush());
    }
    slotSelectTool();
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->zValue() > -1000) {
            list.at(i)->setFlag(QGraphicsItem::ItemIsMovable, !anim);
            list.at(i)->setFlag(QGraphicsItem::ItemIsSelectable, !anim);
        }
    }
    align_box->setEnabled(anim);
    itemzoom->setEnabled(!anim);
    itemrotatex->setEnabled(!anim);
    itemrotatey->setEnabled(!anim);
    itemrotatez->setEnabled(!anim);
    frame_toolbar->setEnabled(!anim);
    toolbar_stack->setEnabled(!anim);
    if (anim) {
        keep_aspect->setChecked(!m_endViewport->data(0).isNull());
        m_endViewport->setZValue(1100);
        QColor col = m_endViewport->pen().color();
        col.setAlpha(100);
        m_endViewport->setBrush(col);
        m_endViewport->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        m_endViewport->setSelected(true);
        selectionChanged();
        slotSelectTool();
        if (m_endViewport->childItems().isEmpty()) addAnimInfoText();
    } else {
        m_endViewport->setZValue(-1000);
        m_endViewport->setBrush(QBrush());
        m_endViewport->setFlags(0);
        if (!anim_start->isChecked()) deleteAnimInfoText();
    }
}

void TitleWidget::addAnimInfoText()
{
    // add text to anim viewport
    QGraphicsTextItem *t = new QGraphicsTextItem(i18n("Start"), m_startViewport);
    QGraphicsTextItem *t2 = new QGraphicsTextItem(i18n("End"), m_endViewport);
    QFont font = t->font();
    font.setPixelSize(m_startViewport->rect().width() / 10);
    QColor col = m_startViewport->pen().color();
    col.setAlpha(255);
    t->setDefaultTextColor(col);
    t->setFont(font);
    font.setPixelSize(m_endViewport->rect().width() / 10);
    col = m_endViewport->pen().color();
    col.setAlpha(255);
    t2->setDefaultTextColor(col);
    t2->setFont(font);
}

void TitleWidget::updateInfoText()
{
    // update info text font
    if (!m_startViewport->childItems().isEmpty()) {
        QGraphicsTextItem *item = static_cast <QGraphicsTextItem *>(m_startViewport->childItems().at(0));
        if (item) {
            QFont font = item->font();
            font.setPixelSize(m_startViewport->rect().width() / 10);
            item->setFont(font);
        }
    }
    if (!m_endViewport->childItems().isEmpty()) {
        QGraphicsTextItem *item = static_cast <QGraphicsTextItem *>(m_endViewport->childItems().at(0));
        if (item) {
            QFont font = item->font();
            font.setPixelSize(m_endViewport->rect().width() / 10);
            item->setFont(font);
        }
    }
}

void TitleWidget::deleteAnimInfoText()
{
    // end animation editing, remove info text
    while (!m_startViewport->childItems().isEmpty()) {
        QGraphicsItem *item = m_startViewport->childItems().at(0);
        m_scene->removeItem(item);
        delete item;
    }
    while (!m_endViewport->childItems().isEmpty()) {
        QGraphicsItem *item = m_endViewport->childItems().at(0);
        m_scene->removeItem(item);
        delete item;
    }
}

void TitleWidget::slotKeepAspect(bool keep)
{
    if (m_endViewport->zValue() == 1100) {
        m_endViewport->setData(0, keep == true ? m_frameWidth : QVariant());
        m_endViewport->setData(1, keep == true ? m_frameHeight : QVariant());
    } else {
        m_startViewport->setData(0, keep == true ? m_frameWidth : QVariant());
        m_startViewport->setData(1, keep == true ? m_frameHeight : QVariant());
    }
}

void TitleWidget::slotResize50()
{
    if (m_endViewport->zValue() == 1100) {
        m_endViewport->setRect(0, 0, m_frameWidth / 2, m_frameHeight / 2);
    } else m_startViewport->setRect(0, 0, m_frameWidth / 2, m_frameHeight / 2);
}

void TitleWidget::slotResize100()
{
    if (m_endViewport->zValue() == 1100) {
        m_endViewport->setRect(0, 0, m_frameWidth, m_frameHeight);
    } else m_startViewport->setRect(0, 0, m_frameWidth, m_frameHeight);
}

void TitleWidget::slotResize200()
{
    if (m_endViewport->zValue() == 1100) {
        m_endViewport->setRect(0, 0, m_frameWidth * 2, m_frameHeight * 2);
    } else m_startViewport->setRect(0, 0, m_frameWidth * 2, m_frameHeight * 2);
}

void TitleWidget::slotAddEffect(int ix)
{
    QList<QGraphicsItem *> list = graphicsView->scene()->selectedItems();
    int effect = effect_list->itemData(ix).toInt();

    if (list.size() == 1) {
        if (effect == NOEFFECT)
            effect_stack->setHidden(true);
        else {
            effect_stack->setCurrentIndex(effect - 1);
            effect_stack->setHidden(false);
        }
    } else // Hide the effects stack when more than one element is selected.
        effect_stack->setHidden(true);

    foreach(QGraphicsItem * item, list) {
        switch (effect) {
        case NOEFFECT:
            item->setData(100, QVariant());
#if QT_VERSION >= 0x040600
            item->setGraphicsEffect(0);
#endif
            break;
        case TYPEWRITEREFFECT:
            /*
             * Allow the user to set the typewriter effect to more than one
             * element, but do not add it to non-text elements.
             */
            if (item->type() == TEXTITEM) {
                QStringList effdata = QStringList() << "typewriter" << QString::number(typewriter_delay->value()) + ';' + QString::number(typewriter_start->value());
                item->setData(100, effdata);
            }
            break;
#if QT_VERSION >= 0x040600
            // Do not remove the non-QGraphicsEffects.
        case BLUREFFECT:
            item->setGraphicsEffect(new QGraphicsBlurEffect());
            break;
        case SHADOWEFFECT:
            item->setGraphicsEffect(new QGraphicsDropShadowEffect());
            break;
#endif
        }
    }
}

void TitleWidget::slotFontText(const QString& s)
{
    const QFont f(s);
    if (f.exactMatch()) {
        // Font really exists (could also just be a «d» if the user
        // starts typing «dejavu» for example).
        font_family->setCurrentFont(f);
    }
    // Note: Typing dejavu serif does not recognize the font (takes sans)
    // in older Qt versions. Case must match there (except for first letter)
}

void TitleWidget::slotEditTypewriter(int /*ix*/)
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QStringList effdata = QStringList() << "typewriter" << QString::number(typewriter_delay->value()) + ';' + QString::number(typewriter_start->value());
        l[0]->setData(100, effdata);
    }
}

void TitleWidget::slotEditBlur(int ix)
{
#if QT_VERSION < 0x040600
    return;
#else
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsEffect *eff = l[0]->graphicsEffect();
        QGraphicsBlurEffect *blur = static_cast <QGraphicsBlurEffect *>(eff);
        if (blur) blur->setBlurRadius(ix);
    }
#endif
}

void TitleWidget::slotEditShadow()
{
#if QT_VERSION < 0x040600
    return;
#else
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsEffect *eff = l[0]->graphicsEffect();
        QGraphicsDropShadowEffect *shadow = static_cast <QGraphicsDropShadowEffect *>(eff);
        if (shadow) {
            shadow->setBlurRadius(shadow_radius->value());
            shadow->setOffset(shadow_x->value(), shadow_y->value());
        }
    }
#endif
}

qreal TitleWidget::zIndexBounds(bool maxBound, bool intersectingOnly)
{
    qreal bound = maxBound ? -99 : 99;
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() > 0) {
        QList<QGraphicsItem*> lItems;
        // Get items (all or intersecting only)
        if (intersectingOnly) {
            lItems = graphicsView->scene()->items(l[0]->sceneBoundingRect(), Qt::IntersectsItemShape);
        } else {
            lItems = graphicsView->scene()->items();
        }
        if (lItems.size() > 0) {
            int n = lItems.size();
            qreal z;
            if (maxBound) {
                for (int i = 0; i < n; i++) {
                    z = lItems[i]->zValue();
                    if (z > bound && !lItems[i]->isSelected()) {
                        bound = z;
                    } else if (z - 1 > bound) {
                        // To get the maximum index even if it is of an item of the current selection.
                        // Used when updating multiple items, to get all to the same level.
                        // Otherwise, the maximum would stay at -99 if the highest item is in the selection.
                        bound = z - 1;
                    }
                }
            } else {
                // Get minimum z index.
                for (int i = 0; i < n; i++) {
                    z = lItems[i]->zValue();
                    if (z < bound && !lItems[i]->isSelected() && z > -999) {
                        // There are items at the very bottom (background e.g.) with z-index < -1000.
                        bound = z;
                    } else if (z + 1 < bound && z > -999) {
                        bound = z + 1;
                    }
                }
            }
        }
    }
    return bound;
}

void TitleWidget::slotZIndexUp()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        qreal currentZ = l[0]->zValue();
        qreal max = zIndexBounds(true, true);
        if (currentZ <= max) {
            l[0]->setZValue(currentZ + 1);
            updateDimension(l[0]);
        }
    }
}

void TitleWidget::slotZIndexTop()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    qreal max = zIndexBounds(true, false);
    std::cout << "Max z-index is " << max << ".\n";
    for (int i = 0; i < l.size(); i++) {
        qreal currentZ = l[i]->zValue();
        if (currentZ <= max) {
            std::cout << "Updating item " << i << ", is " << currentZ << ".\n";
            l[i]->setZValue(max + 1);
        } else {
            std::cout << "Not updating " << i << ", is " << currentZ << ".\n";
        }
    }
    // Update the z index value in the GUI
    if (l.size() > 0) {
        updateDimension(l[0]);
    }
}

void TitleWidget::slotZIndexDown()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        qreal currentZ = l[0]->zValue();
        qreal min = zIndexBounds(false, true);
        if (currentZ >= min) {
            l[0]->setZValue(currentZ - 1);
            updateDimension(l[0]);
        }
    }
}

void TitleWidget::slotZIndexBottom()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    qreal min = zIndexBounds(false, false);
    for (int i = 0; i < l.size(); i++) {
        qreal currentZ = l[i]->zValue();
        if (currentZ >= min) {
            l[i]->setZValue(min - 1);
        }
    }
    // Update the z index value in the GUI
    if (l.size() > 0) {
        updateDimension(l[0]);
    }
}

void TitleWidget::slotSelectAll()
{
    QList<QGraphicsItem*> l = graphicsView->scene()->items();
    for (int i = 0; i < l.size(); i++) {
        l.at(i)->setSelected(true);
    }
}

void TitleWidget::selectItems(int itemType)
{
    QList<QGraphicsItem*> l;
    if (graphicsView->scene()->selectedItems().size() > 0) {
        l = graphicsView->scene()->selectedItems();
        for (int i = 0; i < l.size(); i++) {
            if (l.at(i)->type() != itemType) {
                l.at(i)->setSelected(false);
            }
        }
    } else {
        l = graphicsView->scene()->items();
        for (int i = 0; i < l.size(); i++) {
            if (l.at(i)->type() == itemType) {
                l.at(i)->setSelected(true);
            }
        }
    }
}

void TitleWidget::slotSelectText()
{
    selectItems(TEXTITEM);
}

void TitleWidget::slotSelectRects()
{
    selectItems(RECTITEM);
}

void TitleWidget::slotSelectImages()
{
    selectItems(IMAGEITEM);
}

void TitleWidget::slotSelectNone()
{
    graphicsView->blockSignals(true);
    QList<QGraphicsItem*> l = graphicsView->scene()->items();
    for (int i = 0; i < l.size(); i++) {
        l.at(i)->setSelected(false);
    }
    graphicsView->blockSignals(false);
    selectionChanged();
}

QString TitleWidget::getTooltipWithShortcut(const QString &text, QAction *button)
{
    return text + "  <b>" + button->shortcut().toString() + "</b>";
}

void TitleWidget::prepareTools(QGraphicsItem *referenceItem)
{
    // Let some GUI elements block signals. We may want to change their values without any sideeffects.
    // Additionally, store the previous blocking state to avoid side effects when this function is called from within another one.
    // Note: Disabling an element also blocks signals. So disabled elements don't need to be set to blocking too.
    bool blockOX = origin_x_left->signalsBlocked();
    bool blockOY = origin_y_top->signalsBlocked();
    bool blockEff = effect_list->signalsBlocked();
    bool blockRX = itemrotatex->signalsBlocked();
    bool blockRY = itemrotatey->signalsBlocked();
    bool blockRZ = itemrotatez->signalsBlocked();
    bool blockZoom = itemzoom->signalsBlocked();
    bool blockX = value_x->signalsBlocked();
    bool blockY = value_y->signalsBlocked();
    bool blockW = value_w->signalsBlocked();
    bool blockH = value_h->signalsBlocked();
    origin_x_left->blockSignals(true);
    origin_y_top->blockSignals(true);
    effect_list->blockSignals(true);
    itemrotatex->blockSignals(true);
    itemrotatey->blockSignals(true);
    itemrotatez->blockSignals(true);
    itemzoom->blockSignals(true);
    value_x->blockSignals(true);
    value_y->blockSignals(true);
    value_w->blockSignals(true);
    value_h->blockSignals(true);

    if (referenceItem == NULL) {
        std::cout << "NULL item.\n";
        effect_stack->setHidden(true);
        effect_frame->setEnabled(false);
        effect_list->setCurrentIndex(0);
        origin_x_left->setChecked(false);
        origin_y_top->setChecked(false);
        updateTextOriginX();
        updateTextOriginY();
        enableToolbars(TITLE_SELECT);
        showToolbars(TITLE_SELECT);

        itemzoom->setEnabled(false);
        itemrotatex->setEnabled(false);
        itemrotatey->setEnabled(false);
        itemrotatez->setEnabled(false);
        frame_properties->setEnabled(false);
    } else {
        effect_frame->setEnabled(true);
        frame_properties->setEnabled(true);
        if (referenceItem != m_startViewport && referenceItem != m_endViewport) {
            itemzoom->setEnabled(true);
            itemrotatex->setEnabled(true);
            itemrotatey->setEnabled(true);
            itemrotatez->setEnabled(true);
        } else {
            itemzoom->setEnabled(false);
            itemrotatex->setEnabled(false);
            itemrotatey->setEnabled(false);
            itemrotatez->setEnabled(false);
            updateInfoText();
        }
        if (referenceItem->type() == TEXTITEM) {
            showToolbars(TITLE_TEXT);
            QGraphicsTextItem* i = static_cast <QGraphicsTextItem *>(referenceItem);
            if (!i->data(100).isNull()) {
                // Item has an effect
                QStringList effdata = i->data(100).toStringList();
                QString effectName = effdata.takeFirst();
                if (effectName == "typewriter") {
                    QStringList params = effdata.at(0).split(';');
                    typewriter_delay->setValue(params.at(0).toInt());
                    typewriter_start->setValue(params.at(1).toInt());
                    effect_list->setCurrentIndex(effect_list->findData((int) TYPEWRITEREFFECT));
                    effect_stack->setHidden(false);
                }
            } else {
#if QT_VERSION >= 0x040600
                if (i->graphicsEffect()) {
                    QGraphicsBlurEffect *blur = static_cast <QGraphicsBlurEffect *>(i->graphicsEffect());
                    if (blur) {
                        effect_list->setCurrentIndex(effect_list->findData((int) BLUREFFECT));
                        int rad = (int) blur->blurRadius();
                        blur_radius->setValue(rad);
                        effect_stack->setHidden(false);
                    } else {
                        QGraphicsDropShadowEffect *shad = static_cast <QGraphicsDropShadowEffect *>(i->graphicsEffect());
                        if (shad) {
                            effect_list->setCurrentIndex(effect_list->findData((int) SHADOWEFFECT));
                            shadow_radius->setValue(shad->blurRadius());
                            shadow_x->setValue(shad->xOffset());
                            shadow_y->setValue(shad->yOffset());
                            effect_stack->setHidden(false);
                        }
                    }
                } else {
                    effect_list->setCurrentIndex(effect_list->findData((int) NOEFFECT));
                    effect_stack->setHidden(true);
                }
#else
                effect_list->setCurrentIndex(effect_list->findData((int) NOEFFECT));
                effect_stack->setHidden(true);
#endif
            }
            font_size->blockSignals(true);
            font_family->blockSignals(true);
            font_weight_box->blockSignals(true);
            buttonItalic->blockSignals(true);
            buttonUnder->blockSignals(true);
            fontColorButton->blockSignals(true);
#if not KDE_IS_VERSION(4,5,0)
            textAlpha->blockSignals(true);
#endif
            buttonAlignLeft->blockSignals(true);
            buttonAlignRight->blockSignals(true);
            buttonAlignNone->blockSignals(true);
            buttonAlignCenter->blockSignals(true);

            QFont font = i->font();
            font_family->setCurrentFont(font);
            font_size->setValue(font.pixelSize());
            m_scene->slotUpdateFontSize(font.pixelSize());
            buttonItalic->setChecked(font.italic());
            buttonUnder->setChecked(font.underline());
            setFontBoxWeight(font.weight());

            QTextCursor cursor(i->document());
            cursor.select(QTextCursor::Document);
            QColor color = cursor.charFormat().foreground().color();
#if not KDE_IS_VERSION(4,5,0)
            textAlpha->setValue(color.alpha());
            color.setAlpha(255);
#endif
            fontColorButton->setColor(color);

            if (!i->data(101).isNull()) {
                textOutline->blockSignals(true);
                textOutline->setValue(i->data(101).toDouble() * 10);
                textOutline->blockSignals(false);
            }
            if (!i->data(102).isNull()) {
                textOutlineColor->blockSignals(true);
                QVariant variant = i->data(102);
                color = variant.value<QColor>();
#if not KDE_IS_VERSION(4,5,0)
                textOutlineAlpha->blockSignals(true);
                textOutlineAlpha->setValue(color.alpha());
                color.setAlpha(255);
                textOutlineAlpha->blockSignals(false);
#endif
                textOutlineColor->setColor(color);
                textOutlineColor->blockSignals(false);
            }
            QTextCursor cur = i->textCursor();
            QTextBlockFormat format = cur.blockFormat();
            if (i->textWidth() == -1) buttonAlignNone->setChecked(true);
            else if (format.alignment() == Qt::AlignHCenter) buttonAlignCenter->setChecked(true);
            else if (format.alignment() == Qt::AlignRight) buttonAlignRight->setChecked(true);
            else if (format.alignment() == Qt::AlignLeft) buttonAlignLeft->setChecked(true);

            font_size->blockSignals(false);
            font_family->blockSignals(false);
            font_weight_box->blockSignals(false);
            buttonItalic->blockSignals(false);
            buttonUnder->blockSignals(false);
            fontColorButton->blockSignals(false);
#if not KDE_IS_VERSION(4,5,0)
            textAlpha->blockSignals(false);
#endif
            buttonAlignLeft->blockSignals(false);
            buttonAlignRight->blockSignals(false);
            buttonAlignNone->blockSignals(false);
            buttonAlignCenter->blockSignals(false);

            // mbt 1607: Select text if the text item is an unchanged template item.
            if (i->property("isTemplate").isValid()) {
                cur.setPosition(0, QTextCursor::MoveAnchor);
                cur.select(QTextCursor::Document);
                i->setTextCursor(cur);
                // Make text editable now.
                i->grabKeyboard();
                i->setTextInteractionFlags(Qt::TextEditorInteraction);
            }

            updateAxisButtons(i);
            updateCoordinates(i);
            updateDimension(i);
            enableToolbars(TITLE_TEXT);

        } else if ((referenceItem)->type() == RECTITEM) {
            showToolbars(TITLE_RECTANGLE);
            settingUp = true;
            QGraphicsRectItem *rec = static_cast <QGraphicsRectItem *>(referenceItem);
            if (rec == m_startViewport || rec == m_endViewport) {
                /*toolBox->setCurrentIndex(3);
                toolBox->widget(0)->setEnabled(false);
                toolBox->widget(1)->setEnabled(false);*/
                enableToolbars(TITLE_SELECT);
            } else {
                /*toolBox->widget(0)->setEnabled(true);
                toolBox->widget(1)->setEnabled(true);
                toolBox->setCurrentIndex(0);*/
                //toolBox->setItemEnabled(3, true);
#if not KDE_IS_VERSION(4,5,0)                
                rectFAlpha->setValue(rec->pen().color().alpha());
                rectBAlpha->setValue(rec->brush().color().alpha());
#endif
                //kDebug() << rec->brush().color().alpha();
                QColor fcol = rec->pen().color();
                QColor bcol = rec->brush().color();
                //fcol.setAlpha(255);
                //bcol.setAlpha(255);
                rectFColor->setColor(fcol);
                rectBColor->setColor(bcol);
                settingUp = false;
                rectLineWidth->setValue(rec->pen().width());
                enableToolbars(TITLE_RECTANGLE);
            }

            updateAxisButtons(referenceItem);
            updateCoordinates(rec);
            updateDimension(rec);

        } else if (referenceItem->type() == IMAGEITEM) {
            showToolbars(TITLE_IMAGE);

            updateCoordinates(referenceItem);
            updateDimension(referenceItem);

            enableToolbars(TITLE_IMAGE);

        } else {
            //toolBox->setCurrentIndex(0);
            showToolbars(TITLE_SELECT);
            enableToolbars(TITLE_SELECT);
            frame_properties->setEnabled(false);
        }
        zValue->setValue((int)referenceItem->zValue());
        if (!referenceItem->data(ZOOMFACTOR).isNull()) itemzoom->setValue(referenceItem->data(ZOOMFACTOR).toInt());
        else itemzoom->setValue((int)(m_transformations.value(referenceItem).scalex * 100.0 + 0.5));
        itemrotatex->setValue((int)(m_transformations.value(referenceItem).rotatex));
        itemrotatey->setValue((int)(m_transformations.value(referenceItem).rotatey));
        itemrotatez->setValue((int)(m_transformations.value(referenceItem).rotatez));
    }


    effect_list->blockSignals(blockEff);
    itemrotatex->blockSignals(blockRX);
    itemrotatey->blockSignals(blockRY);
    itemrotatez->blockSignals(blockRZ);
    itemzoom->blockSignals(blockZoom);
    origin_x_left->blockSignals(blockOX);
    origin_y_top->blockSignals(blockOY);
    value_x->blockSignals(blockX);
    value_y->blockSignals(blockY);
    value_w->blockSignals(blockW);
    value_h->blockSignals(blockH);
}
