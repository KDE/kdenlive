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
#include "core.h"
#include "bin/bin.h"
#include "doc/kthumb.h"
#include "gradientwidget.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"

#include <cmath>

#include <KMessageBox>
#include <KMessageWidget>
#include <KRecentDirs>
#include <klocalizedstring.h>
#include <kns3/downloaddialog.h>

#include "kdenlive_debug.h"
#include <QButtonGroup>
#include <QCryptographicHash>
#include <QDomDocument>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsEffect>
#include <QGraphicsItem>
#include <QGraphicsSvgItem>
#include <QImageReader>
#include <QKeyEvent>
#include <QSignalMapper>
#include <QSpinBox>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>
#include <QMenu>

#include <QStandardPaths>
#include <iostream>
#include <mlt++/MltProfile.h>
#include <utility>

static QList<TitleTemplate> titletemplates;

// What exactly is this variable good for?
int settingUp = 0;

const int IMAGEITEM = 7;
const int RECTITEM = 3;
const int TEXTITEM = 8;

/*
const int NOEFFECT = 0;
const int BLUREFFECT = 1;
const int SHADOWEFFECT = 2;
const int TYPEWRITEREFFECT = 3;
*/

void TitleWidget::refreshTemplateBoxContents()
{
    templateBox->clear();
    templateBox->addItem(QString());
    for (const TitleTemplate &t : qAsConst(titletemplates)) {
        templateBox->addItem(t.icon, t.name, t.file);
    }
}

TitleWidget::TitleWidget(const QUrl &url, const Timecode &tc, QString projectTitlePath, Monitor *monitor, QWidget *parent)
    : QDialog(parent)
    , Ui::TitleWidget_UI()
    , m_startViewport(nullptr)
    , m_endViewport(nullptr)
    , m_count(0)
    , m_unicodeDialog(new UnicodeDialog(UnicodeDialog::InputHex))
    , m_missingMessage(nullptr)
    , m_projectTitlePath(std::move(projectTitlePath))
    , m_tc(tc)
    , m_fps(pCore->getCurrentFps())
    , m_guides(QList<QGraphicsLineItem *>())
{
    setupUi(this);
    setMinimumSize(200, 200);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    frame_properties->setEnabled(false);
    frame_properties->setFixedHeight(frame_toolbar->height());
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    rectBColor->setAlphaChannelEnabled(true);
    rectFColor->setAlphaChannelEnabled(true);
    fontColorButton->setAlphaChannelEnabled(true);
    textOutlineColor->setAlphaChannelEnabled(true);
    shadowColor->setAlphaChannelEnabled(true);

    auto *colorGroup = new QButtonGroup(this);
    colorGroup->addButton(gradient_color);
    colorGroup->addButton(plain_color);

    auto *alignGroup = new QButtonGroup(this);
    alignGroup->addButton(buttonAlignLeft);
    alignGroup->addButton(buttonAlignCenter);
    alignGroup->addButton(buttonAlignRight);

    textOutline->setMinimum(0);
    textOutline->setMaximum(200);
    // textOutline->setDecimals(0);
    textOutline->setValue(0);
    textOutline->setToolTip(i18n("Outline width"));

    backgroundAlpha->setMinimum(0);
    backgroundAlpha->setMaximum(255);
    bgAlphaSlider->setMinimum(0);
    bgAlphaSlider->setMaximum(255);
    backgroundAlpha->setValue(0);
    backgroundAlpha->setToolTip(i18n("Background color opacity"));

    itemrotatex->setMinimum(-360);
    itemrotatex->setMaximum(360);
    // itemrotatex->setDecimals(0);
    itemrotatex->setValue(0);
    itemrotatex->setToolTip(i18n("Rotation around the X axis"));

    itemrotatey->setMinimum(-360);
    itemrotatey->setMaximum(360);
    // itemrotatey->setDecimals(0);
    itemrotatey->setValue(0);
    itemrotatey->setToolTip(i18n("Rotation around the Y axis"));

    itemrotatez->setMinimum(-360);
    itemrotatez->setMaximum(360);
    // itemrotatez->setDecimals(0);
    itemrotatez->setValue(0);
    itemrotatez->setToolTip(i18n("Rotation around the Z axis"));

    rectLineWidth->setMinimum(0);
    rectLineWidth->setMaximum(500);
    // rectLineWidth->setDecimals(0);
    rectLineWidth->setValue(0);
    rectLineWidth->setToolTip(i18n("Border width"));

    itemzoom->setSuffix(i18n("%"));
    QSize profileSize = pCore->getCurrentFrameSize();
    m_frameWidth = (int)(profileSize.height() * pCore->getCurrentDar() + 0.5);
    m_frameHeight = profileSize.height();
    showToolbars(TITLE_SELECT);

    splitter->setStretchFactor(0, 20);

    // If project is drop frame, set the input mask as such.
    title_duration->setInputMask(m_tc.mask());
    title_duration->setText(m_tc.reformatSeparators(KdenliveSettings::title_duration()));

    connect(backgroundColor, &KColorButton::changed, this, &TitleWidget::slotChangeBackground);
    connect(backgroundAlpha, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotChangeBackground);

    connect(shadowBox, &QGroupBox::toggled, this, &TitleWidget::slotUpdateShadow);
    connect(shadowColor, &KColorButton::changed, this, &TitleWidget::slotUpdateShadow);
    connect(blur_radius, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateShadow);
    connect(shadowX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateShadow);
    connect(shadowY, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateShadow);

    connect(fontColorButton, &KColorButton::changed, this, &TitleWidget::slotUpdateText);
    connect(plain_color, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(gradient_color, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(gradients_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateText()));

    connect(textOutlineColor, &KColorButton::changed, this, &TitleWidget::slotUpdateText);
    connect(font_family, &QFontComboBox::currentFontChanged, this, &TitleWidget::slotUpdateText);
    connect(font_size, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateText);
    connect(letter_spacing, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateText);
    connect(line_spacing, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateText);
    connect(textOutline, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateText);
    connect(font_weight_box, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateText()));

    connect(rectFColor, &KColorButton::changed, this, &TitleWidget::rectChanged);
    connect(rectBColor, &KColorButton::changed, this, &TitleWidget::rectChanged);
    connect(plain_rect, &QAbstractButton::clicked, this, &TitleWidget::rectChanged);
    connect(gradient_rect, &QAbstractButton::clicked, this, &TitleWidget::rectChanged);
    connect(gradients_rect_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(rectChanged()));
    connect(rectLineWidth, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::rectChanged);

    connect(zValue, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::zIndexChanged);
    connect(itemzoom, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::itemScaled);
    connect(itemrotatex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::itemRotateX);
    connect(itemrotatey, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::itemRotateY);
    connect(itemrotatez, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::itemRotateZ);
    connect(itemhcenter, &QAbstractButton::clicked, this, &TitleWidget::itemHCenter);
    connect(itemvcenter, &QAbstractButton::clicked, this, &TitleWidget::itemVCenter);
    connect(itemtop, &QAbstractButton::clicked, this, &TitleWidget::itemTop);
    connect(itembottom, &QAbstractButton::clicked, this, &TitleWidget::itemBottom);
    connect(itemleft, &QAbstractButton::clicked, this, &TitleWidget::itemLeft);
    connect(itemright, &QAbstractButton::clicked, this, &TitleWidget::itemRight);

    connect(origin_x_left, &QAbstractButton::clicked, this, &TitleWidget::slotOriginXClicked);
    connect(origin_y_top, &QAbstractButton::clicked, this, &TitleWidget::slotOriginYClicked);

    connect(monitor, &Monitor::frameUpdated, this, &TitleWidget::slotGotBackground);
    connect(this, &TitleWidget::requestBackgroundFrame, monitor, &Monitor::slotGetCurrentImage);

    // Position and size
    connect(value_w, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int){slotValueChanged(ValueWidth);});
    connect(value_h, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int){slotValueChanged(ValueHeight);});
    connect(value_x, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int){slotValueChanged(ValueX);});
    connect(value_y, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int){slotValueChanged(ValueY);});

    connect(buttonFitZoom, &QAbstractButton::clicked, this, &TitleWidget::slotAdjustZoom);
    connect(buttonRealSize, &QAbstractButton::clicked, this, &TitleWidget::slotZoomOneToOne);
    connect(buttonItalic, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(buttonUnder, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(buttonAlignLeft, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(buttonAlignRight, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(buttonAlignCenter, &QAbstractButton::clicked, this, &TitleWidget::slotUpdateText);
    connect(edit_gradient, &QAbstractButton::clicked, this, &TitleWidget::slotEditGradient);
    connect(edit_rect_gradient, &QAbstractButton::clicked, this, &TitleWidget::slotEditGradient);
    connect(preserveAspectRatio, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), this, [&] () {
        slotValueChanged(ValueWidth);
    });

    displayBg->setChecked(KdenliveSettings::titlerShowbg());
    connect(displayBg, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), this, [&] (int state) {
        KdenliveSettings::setTitlerShowbg(state == Qt::Checked);
        displayBackgroundFrame();
    });

    connect(m_unicodeDialog, &UnicodeDialog::charSelected, this, &TitleWidget::slotInsertUnicodeString);

    // mbd
    connect(this, &QDialog::accepted, this, &TitleWidget::slotAccepted);

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
    buttonFitZoom->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    buttonRealSize->setIcon(QIcon::fromTheme(QStringLiteral("zoom-original")));
    buttonItalic->setIcon(QIcon::fromTheme(QStringLiteral("format-text-italic")));
    buttonUnder->setIcon(QIcon::fromTheme(QStringLiteral("format-text-underline")));
    buttonAlignCenter->setIcon(QIcon::fromTheme(QStringLiteral("format-justify-center")));
    buttonAlignLeft->setIcon(QIcon::fromTheme(QStringLiteral("format-justify-left")));
    buttonAlignRight->setIcon(QIcon::fromTheme(QStringLiteral("format-justify-right")));
    edit_gradient->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    edit_rect_gradient->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));

    buttonAlignRight->setToolTip(i18n("Align right"));
    buttonAlignLeft->setToolTip(i18n("Align left"));
    buttonAlignCenter->setToolTip(i18n("Align center"));
    if (qApp->isLeftToRight()) {
        buttonAlignRight->setChecked(true);
    } else {
        buttonAlignLeft->setChecked(true);
    }

    m_unicodeAction = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-insert-unicode")), QString(), this);
    m_unicodeAction->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_U);
    m_unicodeAction->setToolTip(getTooltipWithShortcut(i18n("Insert Unicode character"), m_unicodeAction));
    connect(m_unicodeAction, &QAction::triggered, this, &TitleWidget::slotInsertUnicode);
    buttonInsertUnicode->setDefaultAction(m_unicodeAction);

    m_zUp = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-up")), QString(), this);
    m_zUp->setShortcut(Qt::Key_PageUp);
    m_zUp->setToolTip(i18n("Raise object"));
    connect(m_zUp, &QAction::triggered, this, &TitleWidget::slotZIndexUp);
    zUp->setDefaultAction(m_zUp);

    m_zDown = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-down")), QString(), this);
    m_zDown->setShortcut(Qt::Key_PageDown);
    m_zDown->setToolTip(i18n("Lower object"));
    connect(m_zDown, &QAction::triggered, this, &TitleWidget::slotZIndexDown);
    zDown->setDefaultAction(m_zDown);

    m_zTop = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-top")), QString(), this);
    // TODO mbt 1414: Shortcut should change z index only if
    // cursor is NOT in a text field ...
    // m_zTop->setShortcut(Qt::Key_Home);
    m_zTop->setToolTip(i18n("Raise object to top"));
    connect(m_zTop, &QAction::triggered, this, &TitleWidget::slotZIndexTop);
    zTop->setDefaultAction(m_zTop);

    m_zBottom = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-bottom")), QString(), this);
    // TODO mbt 1414
    // m_zBottom->setShortcut(Qt::Key_End);
    m_zBottom->setToolTip(i18n("Lower object to bottom"));
    connect(m_zBottom, &QAction::triggered, this, &TitleWidget::slotZIndexBottom);
    zBottom->setDefaultAction(m_zBottom);

    m_selectAll = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-select-all")), QString(), this);
    m_selectAll->setShortcut(Qt::CTRL + Qt::Key_A);
    connect(m_selectAll, &QAction::triggered, this, &TitleWidget::slotSelectAll);
    buttonSelectAll->setDefaultAction(m_selectAll);

    m_selectText = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-select-texts")), QString(), this);
    m_selectText->setShortcut(Qt::CTRL + Qt::Key_T);
    connect(m_selectText, &QAction::triggered, this, &TitleWidget::slotSelectText);
    buttonSelectText->setDefaultAction(m_selectText);
    buttonSelectText->setEnabled(false);

    m_selectRects = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-select-rects")), QString(), this);
    m_selectRects->setShortcut(Qt::CTRL + Qt::Key_R);
    connect(m_selectRects, &QAction::triggered, this, &TitleWidget::slotSelectRects);
    buttonSelectRects->setDefaultAction(m_selectRects);
    buttonSelectRects->setEnabled(false);

    m_selectImages = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-select-images")), QString(), this);
    m_selectImages->setShortcut(Qt::CTRL + Qt::Key_I);
    connect(m_selectImages, &QAction::triggered, this, &TitleWidget::slotSelectImages);
    buttonSelectImages->setDefaultAction(m_selectImages);
    buttonSelectImages->setEnabled(false);

    m_unselectAll = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-unselect-all")), QString(), this);
    m_unselectAll->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_A);
    connect(m_unselectAll, &QAction::triggered, this, &TitleWidget::slotSelectNone);
    buttonUnselectAll->setDefaultAction(m_unselectAll);
    buttonUnselectAll->setEnabled(false);

    zDown->setIconSize(iconSize);
    zTop->setIconSize(iconSize);
    zBottom->setIconSize(iconSize);
    zDown->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-down")));
    zTop->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-top")));
    zBottom->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-zindex-bottom")));
    connect(zDown, &QAbstractButton::clicked, this, &TitleWidget::slotZIndexDown);
    connect(zTop, &QAbstractButton::clicked, this, &TitleWidget::slotZIndexTop);
    connect(zBottom, &QAbstractButton::clicked, this, &TitleWidget::slotZIndexBottom);

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

    itemhcenter->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-align-hor")));
    itemhcenter->setToolTip(i18n("Align item horizontally"));
    itemvcenter->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-align-vert")));
    itemvcenter->setToolTip(i18n("Align item vertically"));
    itemtop->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-align-top")));
    itemtop->setToolTip(i18n("Align item to top"));
    itembottom->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-align-bottom")));
    itembottom->setToolTip(i18n("Align item to bottom"));
    itemright->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-align-right")));
    itemright->setToolTip(i18n("Align item to right"));
    itemleft->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-align-left")));
    itemleft->setToolTip(i18n("Align item to left"));

    auto *layout = new QHBoxLayout;
    frame_toolbar->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    QToolBar *m_toolbar = new QToolBar(QStringLiteral("titleToolBar"), this);
    m_toolbar->setIconSize(iconSize);

    m_buttonCursor = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("transform-move")), i18n("Selection Tool"));
    m_buttonCursor->setCheckable(true);
    m_buttonCursor->setShortcut(Qt::ALT + Qt::Key_S);
    m_buttonCursor->setToolTip(i18n("Selection Tool") + QLatin1Char(' ') + m_buttonCursor->shortcut().toString());
    connect(m_buttonCursor, &QAction::triggered, this, &TitleWidget::slotSelectTool);

    m_buttonText = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("insert-text")), i18n("Add Text"));
    m_buttonText->setCheckable(true);
    m_buttonText->setShortcut(Qt::ALT + Qt::Key_T);
    m_buttonText->setToolTip(i18n("Add Text") + QLatin1Char(' ') + m_buttonText->shortcut().toString());
    connect(m_buttonText, &QAction::triggered, this, &TitleWidget::slotTextTool);

    m_buttonRect = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("kdenlive-insert-rect")), i18n("Add Rectangle"));
    m_buttonRect->setCheckable(true);
    m_buttonRect->setShortcut(Qt::ALT + Qt::Key_R);
    m_buttonRect->setToolTip(i18n("Add Rectangle") + QLatin1Char(' ') + m_buttonRect->shortcut().toString());
    connect(m_buttonRect, &QAction::triggered, this, &TitleWidget::slotRectTool);

    m_buttonImage = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("insert-image")), i18n("Add Image"));
    m_buttonImage->setCheckable(false);
    m_buttonImage->setShortcut(Qt::ALT + Qt::Key_I);
    m_buttonImage->setToolTip(i18n("Add Image") + QLatin1Char(' ') + m_buttonImage->shortcut().toString());
    connect(m_buttonImage, &QAction::triggered, this, &TitleWidget::slotImageTool);

    m_toolbar->addSeparator();

    m_buttonLoad = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Open Document"));
    m_buttonLoad->setCheckable(false);
    m_buttonLoad->setShortcut(Qt::CTRL + Qt::Key_O);
    m_buttonLoad->setToolTip(i18n("Open Document") + QLatin1Char(' ') + m_buttonLoad->shortcut().toString());
    connect(m_buttonLoad, SIGNAL(triggered()), this, SLOT(loadTitle()));

    m_buttonSave = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Save As"));
    m_buttonSave->setCheckable(false);
    m_buttonSave->setShortcut(Qt::CTRL + Qt::Key_S);
    m_buttonSave->setToolTip(i18n("Save As") + QLatin1Char(' ') + m_buttonSave->shortcut().toString());
    connect(m_buttonSave, SIGNAL(triggered()), this, SLOT(saveTitle()));

    m_buttonDownload = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("edit-download")), i18n("Download New Title Templates..."));
    m_buttonDownload->setCheckable(false);
    m_buttonDownload->setShortcut(Qt::ALT + Qt::Key_D);
    m_buttonDownload->setToolTip(i18n("Download New Title Templates...") + QLatin1Char(' ') + m_buttonDownload->shortcut().toString());
    connect(m_buttonDownload, &QAction::triggered, this, &TitleWidget::downloadTitleTemplates);

    layout->addWidget(m_toolbar);

    // initialize graphic scene
    m_scene = new GraphicsSceneRectMove(this);
    graphicsView->setScene(m_scene);
    graphicsView->setMouseTracking(true);
    graphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    graphicsView->setRubberBandSelectionMode(Qt::ContainsItemBoundingRect);
    m_titledocument.setScene(m_scene, m_frameWidth, m_frameHeight);
    connect(m_scene, &QGraphicsScene::changed, this, &TitleWidget::slotChanged);
    connect(font_size, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_scene, &GraphicsSceneRectMove::slotUpdateFontSize);
    connect(use_grid, &QAbstractButton::toggled, m_scene, &GraphicsSceneRectMove::slotUseGrid);

    // Video frame rect
    QPen framepen;
    framepen.setColor(Qt::red);
    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(1000);
    m_frameBorder->setBrush(Qt::transparent);
    m_frameBorder->setFlags(nullptr);
    m_frameBorder->setData(-1, -1);
    graphicsView->scene()->addItem(m_frameBorder);

    // Guides
    connect(show_guides, &QCheckBox::stateChanged, this, &TitleWidget::showGuides);
    show_guides->setChecked(KdenliveSettings::titlerShowGuides());
    hguides->setValue(KdenliveSettings::titlerHGuides());
    vguides->setValue(KdenliveSettings::titlerVGuides());
    guideColor->setColor(KdenliveSettings::titleGuideColor());
    connect(guideColor, &KColorButton::changed, this, &TitleWidget::guideColorChanged);
    connect(hguides, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::updateGuides);
    connect(vguides, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::updateGuides);
    updateGuides(0);

    // semi transparent safe zones
    framepen.setColor(QColor(255, 0, 0, 100));
    QGraphicsRectItem *safe1 = new QGraphicsRectItem(QRectF(m_frameWidth * 0.05, m_frameHeight * 0.05, m_frameWidth * 0.9, m_frameHeight * 0.9), m_frameBorder);
    safe1->setBrush(Qt::transparent);
    safe1->setPen(framepen);
    safe1->setFlags(nullptr);
    safe1->setData(-1, -1);
    QGraphicsRectItem *safe2 = new QGraphicsRectItem(QRectF(m_frameWidth * 0.1, m_frameHeight * 0.1, m_frameWidth * 0.8, m_frameHeight * 0.8), m_frameBorder);
    safe2->setBrush(Qt::transparent);
    safe2->setPen(framepen);
    safe2->setFlags(nullptr);
    safe2->setData(-1, -1);

    m_frameBackground = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_frameBackground->setZValue(-1100);
    m_frameBackground->setBrush(Qt::transparent);
    m_frameBackground->setFlags(nullptr);
    graphicsView->scene()->addItem(m_frameBackground);

    m_frameImage = new QGraphicsPixmapItem();
    QTransform qtrans;
    qtrans.scale(2.0, 2.0);
    m_frameImage->setTransform(qtrans);
    m_frameImage->setZValue(-1200);
    m_frameImage->setFlags(nullptr);
    displayBackgroundFrame();
    graphicsView->scene()->addItem(m_frameImage);

    bgBox->setCurrentIndex(KdenliveSettings::titlerbg());
    connect(bgBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&] (int ix) {
        KdenliveSettings::setTitlerbg(ix);
        displayBackgroundFrame();
    });

    connect(m_scene, &QGraphicsScene::selectionChanged, this, &TitleWidget::selectionChanged);
    connect(m_scene, &GraphicsSceneRectMove::itemMoved, this, &TitleWidget::selectionChanged);
    connect(m_scene, &GraphicsSceneRectMove::sceneZoom, this, &TitleWidget::slotZoom);
    connect(m_scene, &GraphicsSceneRectMove::actionFinished, this, &TitleWidget::slotSelectTool);
    connect(m_scene, &GraphicsSceneRectMove::newRect, this, &TitleWidget::slotNewRect);
    connect(m_scene, &GraphicsSceneRectMove::newText, this, &TitleWidget::slotNewText);
    connect(zoom_slider, &QAbstractSlider::valueChanged, this, &TitleWidget::slotUpdateZoom);
    connect(zoom_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TitleWidget::slotUpdateZoom);

    // mbd: load saved settings
    loadGradients();
    readChoices();

    graphicsView->show();
    graphicsView->setInteractive(true);
    // qCDebug(KDENLIVE_LOG) << "// TITLE WIDGWT: " << graphicsView->viewport()->width() << 'x' << graphicsView->viewport()->height();
    m_startViewport = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    // Setting data at -1 so that the item is recognized as undeletable by graphicsscenerectmove
    m_startViewport->setData(-1, -1);
    m_endViewport = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_endViewport->setData(-1, -1);
    m_startViewport->setData(0, m_frameWidth);
    m_startViewport->setData(1, m_frameHeight);
    m_endViewport->setData(0, m_frameWidth);
    m_endViewport->setData(1, m_frameHeight);

    // scale the view so that the title widget is not too big at startup
    graphicsView->scale(.5, .5);
    if (url.isValid()) {
        loadTitle(url);
    } else {
        prepareTools(nullptr);
        slotTextTool();
        QTimer::singleShot(200, this, &TitleWidget::slotAdjustZoom);
    }
    initAnimation();
    QColor color = backgroundColor->color();
    m_scene->setBackgroundBrush(QBrush(color));
    color.setAlpha(backgroundAlpha->value());
    m_frameBackground->setBrush(color);
    connect(anim_start, &QAbstractButton::toggled, this, &TitleWidget::slotAnimStart);
    connect(anim_end, &QAbstractButton::toggled, this, &TitleWidget::slotAnimEnd);
    connect(templateBox, SIGNAL(currentIndexChanged(int)), this, SLOT(templateIndexChanged(int)));

    createButton->setEnabled(KdenliveSettings::hastitleproducer());
    QMenu *addMenu = new QMenu(this);
    addMenu->addAction(i18n("Save and add to project"));
    m_createTitleAction = new QAction(i18n("Create Title"), this);
    createButton->setMenu(addMenu);
    connect(addMenu, &QMenu::triggered, this, [this]() {
        const QUrl url = saveTitle();
        if (!url.isEmpty()) {
            pCore->bin()->slotAddClipToProject(url);
            done(QDialog::Rejected);
        }
    });
    createButton->setDefaultAction(m_createTitleAction);
    connect(m_createTitleAction, &QAction::triggered, this, [this]() {
        done(QDialog::Accepted);
    });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        done(QDialog::Rejected);
    });
    refreshTitleTemplates(m_projectTitlePath);

    // templateBox->setIconSize(QSize(60,60));
    refreshTemplateBoxContents();
    m_lastDocumentHash = QCryptographicHash::hash(xml().toString().toLatin1(), QCryptographicHash::Md5).toHex();
}

TitleWidget::~TitleWidget()
{
    m_scene->blockSignals(true);
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
}

// static
QStringList TitleWidget::extractImageList(const QString &xml)
{
    QStringList result;
    if (xml.isEmpty()) {
        return result;
    }
    QDomDocument doc;
    doc.setContent(xml);
    QDomNodeList images = doc.elementsByTagName(QStringLiteral("content"));
    for (int i = 0; i < images.count(); ++i) {
        if (images.at(i).toElement().hasAttribute(QStringLiteral("url"))) {
            result.append(images.at(i).toElement().attribute(QStringLiteral("url")));
        }
    }
    return result;
}

// static
QStringList TitleWidget::extractFontList(const QString &xml)
{
    QStringList result;
    if (xml.isEmpty()) {
        return result;
    }
    QDomDocument doc;
    doc.setContent(xml);
    QDomNodeList images = doc.elementsByTagName(QStringLiteral("content"));
    for (int i = 0; i < images.count(); ++i) {
        if (images.at(i).toElement().hasAttribute(QStringLiteral("font"))) {
            result.append(images.at(i).toElement().attribute(QStringLiteral("font")));
        }
    }
    return result;
}
// static
void TitleWidget::refreshTitleTemplates(const QString &projectPath)
{
    QStringList filters = QStringList() << QStringLiteral("*.kdenlivetitle");
    titletemplates.clear();

    // project templates
    QDir dir(projectPath);
    QStringList templateFiles = dir.entryList(filters, QDir::Files);
    for (const QString &fname : qAsConst(templateFiles)) {
        TitleTemplate t;
        t.name = fname;
        t.file = dir.absoluteFilePath(fname);
        t.icon = QIcon(KThumb::getImage(QUrl::fromLocalFile(t.file), 0, 60, -1));
        titletemplates.append(t);
    }

    // system templates
    QStringList titleTemplates = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("titles/"), QStandardPaths::LocateDirectory);
    for (const QString &folderpath : qAsConst(titleTemplates)) {
        QDir folder(folderpath);
        QStringList filesnames = folder.entryList(filters, QDir::Files);
        for (const QString &fname : qAsConst(filesnames)) {
            TitleTemplate t;
            t.name = fname;
            t.file = folder.absoluteFilePath(fname);
            t.icon = QIcon(KThumb::getImage(QUrl::fromLocalFile(t.file), 0, 60, -1));
            titletemplates.append(t);
        }
    }
}

void TitleWidget::templateIndexChanged(int index)
{
    QString item = templateBox->itemData(index).toString();
    if (!item.isEmpty()) {
        if (m_lastDocumentHash != QCryptographicHash::hash(xml().toString().toLatin1(), QCryptographicHash::Md5).toHex()) {
            if (KMessageBox::questionYesNo(this, i18n("Do you really want to load a new template? Changes in this title will be lost!")) == KMessageBox::No) {
                return;
            }
        }
        loadTitle(QUrl::fromLocalFile(item));

        // mbt 1607: Add property to distinguish between unchanged template titles and user titles.
        // Text of unchanged template titles should be selected when clicked.
        QList<QGraphicsItem *> list = graphicsView->scene()->items();
        for (QGraphicsItem *qgItem : qAsConst(list)) {
            if (qgItem->type() == TEXTITEM) {
                auto *i = static_cast<MyTextItem *>(qgItem);
                i->setProperty("isTemplate", "true");
                i->setProperty("templateText", i->toHtml());
            }
        }
        m_lastDocumentHash = QCryptographicHash::hash(xml().toString().toLatin1(), QCryptographicHash::Md5).toHex();
    }
}
// virtual
void TitleWidget::resizeEvent(QResizeEvent * /*event*/)
{
    // slotAdjustZoom();
}
// virtual
void TitleWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() != Qt::Key_Escape && e->key() != Qt::Key_Return && e->key() != Qt::Key_Enter) {
        QDialog::keyPressEvent(e);
    }
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
    if (!l.isEmpty()) {
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
        // graphicsView->centerOn(l.at(0));
        t = TITLE_SELECT;
    }
    showToolbars(t);

    if (!l.isEmpty()) {
        updateCoordinates(l.at(0));
        updateDimension(l.at(0));
        updateRotZoom(l.at(0));
    }

    checkButton(TITLE_SELECT);
}

void TitleWidget::slotImageTool()
{
    QList<QByteArray> supported = QImageReader::supportedImageFormats();
    QStringList mimeTypeFilters;
    QString allExtensions = i18n("All Images") + QStringLiteral(" (");
    for (const QByteArray &mimeType : qAsConst(supported)) {
        mimeTypeFilters.append(i18n("%1 Image", QString(mimeType)) + QStringLiteral("( *.") + QString(mimeType) + QLatin1Char(')'));
        allExtensions.append(QStringLiteral("*.") + mimeType + QLatin1Char(' '));
    }
    mimeTypeFilters.sort();
    allExtensions.append(QLatin1Char(')'));
    mimeTypeFilters.prepend(allExtensions);
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveImageFolder"));
    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    QFileDialog dialog(this, i18n("Add Image"), clipFolder);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setNameFilters(mimeTypeFilters);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QUrl url = QUrl::fromLocalFile(dialog.selectedFiles().at(0));
    if (url.isValid()) {
        KRecentDirs::add(QStringLiteral(":KdenliveImageFolder"), url.adjusted(QUrl::RemoveFilename).toLocalFile());
        if (url.toLocalFile().endsWith(QLatin1String(".svg"))) {
            MySvgItem *svg = new MySvgItem(url.toLocalFile());
            svg->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
            svg->setZValue(m_count++);
            svg->setData(Qt::UserRole, url.toLocalFile());
            m_scene->addNewItem(svg);
            prepareTools(svg);
        } else {
            QPixmap pix(url.toLocalFile());
            auto *image = new MyPixmapItem(pix);
            image->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
            image->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
            image->setData(Qt::UserRole, url.toLocalFile());
            image->setZValue(m_count++);
            m_scene->addNewItem(image);
            prepareTools(image);
        }
    }
    m_scene->setTool(TITLE_SELECT);
    showToolbars(TITLE_SELECT);
    checkButton(TITLE_SELECT);
}

void TitleWidget::showToolbars(TITLETOOL toolType)
{
    toolbar_stack->setEnabled(toolType != TITLE_SELECT);
    switch (toolType) {
    case TITLE_IMAGE:
        toolbar_stack->setCurrentIndex(2);
        break;
    case TITLE_RECTANGLE:
        toolbar_stack->setCurrentIndex(1);
        break;
    case TITLE_TEXT:
    default:
        toolbar_stack->setCurrentIndex(0);
        break;
    }
}

void TitleWidget::enableToolbars(TITLETOOL toolType)
{
    // TITLETOOL is defined in effectstack/graphicsscenerectmove.h
    bool enable = false;
    if (toolType == TITLE_RECTANGLE || toolType == TITLE_IMAGE) {
        enable = true;
    }
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
        switch (KdenliveSettings::titlerbg()) {
            case 0: {
                QPixmap pattern(20, 20);
                pattern.fill(Qt::gray);
                QColor bgcolor(180, 180, 180);
                QPainter p(&pattern);
                p.fillRect(QRect(0, 0, 10, 10), bgcolor);
                p.fillRect(QRect(10, 10, 20, 20), bgcolor);
                p.end();
                QBrush br(pattern);
                QPixmap bg((int)(r.width() / 2), (int)(r.height() / 2));
                QPainter p2(&bg);
                p2.fillRect(bg.rect(), br);
                p2.end();
                m_frameImage->setPixmap(bg);
                break;
            }
            default: {
                QColor col = KdenliveSettings::titlerbg() == 1 ? Qt::black : Qt::white;
                QPixmap bg((int)(r.width() / 2), (int)(r.height() / 2));
                QPainter p2(&bg);
                p2.fillRect(bg.rect(), col);
                p2.end();
                m_frameImage->setPixmap(bg);
            }
        }
    } else {
        emit requestBackgroundFrame(true);
    }
}

void TitleWidget::slotGotBackground(const QImage &img)
{
    QRectF r = m_frameBorder->sceneBoundingRect();
    m_frameImage->setPixmap(QPixmap::fromImage(img.scaled(r.width() / 2, r.height() / 2)));
    emit requestBackgroundFrame(false);
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

    m_startViewport->setFlags(nullptr);
    m_endViewport->setFlags(nullptr);

    graphicsView->scene()->addItem(m_startViewport);
    graphicsView->scene()->addItem(m_endViewport);

    connect(keep_aspect, &QAbstractButton::toggled, this, &TitleWidget::slotKeepAspect);
    connect(resize50, &QAbstractButton::clicked, this, &TitleWidget::slotResize50);
    connect(resize100, &QAbstractButton::clicked, this, &TitleWidget::slotResize100);
    connect(resize200, &QAbstractButton::clicked, this, &TitleWidget::slotResize200);
}

void TitleWidget::slotUpdateZoom(int pos)
{
    zoom_spin->setValue(pos);
    zoom_slider->setValue(pos);
    m_scene->setZoom((double)pos / 100);
}

void TitleWidget::slotZoom(bool up)
{
    int pos = zoom_slider->value();
    if (up) {
        pos++;
    } else {
        pos--;
    }
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

void TitleWidget::slotNewRect(QGraphicsRectItem *rect)
{
    updateAxisButtons(rect); // back to default

    if (rectLineWidth->value() == 0) {
        rect->setPen(Qt::NoPen);
    } else {
        QPen penf(rectFColor->color());
        penf.setWidth(rectLineWidth->value());
        penf.setJoinStyle(Qt::RoundJoin);
        rect->setPen(penf);
    }
    if (plain_rect->isChecked()) {
        rect->setBrush(QBrush(rectBColor->color()));
        rect->setData(TitleDocument::Gradient, QVariant());
    } else {
        // gradient
        QString gradientData = gradients_rect_combo->currentData().toString();
        rect->setData(TitleDocument::Gradient, gradientData);
        QLinearGradient gr = GradientWidget::gradientFromString(gradientData, rect->boundingRect().width(), rect->boundingRect().height());
        rect->setBrush(QBrush(gr));
    }
    rect->setZValue(m_count++);
    rect->setData(TitleDocument::ZoomFactor, 100);
    prepareTools(rect);
    // setCurrentItem(rect);
    // graphicsView->setFocus();
}

void TitleWidget::slotNewText(MyTextItem *tt)
{
    updateAxisButtons(tt); // back to default

    letter_spacing->blockSignals(true);
    line_spacing->blockSignals(true);
    letter_spacing->setValue(0);
    line_spacing->setValue(0);
    letter_spacing->blockSignals(false);
    line_spacing->blockSignals(false);
    letter_spacing->setEnabled(true);
    line_spacing->setEnabled(true);
    QFont font = font_family->currentFont();
    font.setPixelSize(font_size->value());
    // mbd: issue 551:
    font.setWeight(font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    font.setItalic(buttonItalic->isChecked());
    font.setUnderline(buttonUnder->isChecked());

    tt->setFont(font);
    QColor color = fontColorButton->color();
    QColor outlineColor = textOutlineColor->color();
    tt->setTextColor(color);
    tt->document()->setDocumentMargin(0);

    QTextCursor cur(tt->document());
    cur.select(QTextCursor::Document);
    QTextBlockFormat format = cur.blockFormat();
    QTextCharFormat cformat = cur.charFormat();
    double outlineWidth = textOutline->value() / 10.0;

    tt->setData(TitleDocument::OutlineWidth, outlineWidth);
    tt->setData(TitleDocument::OutlineColor, outlineColor);
    if (outlineWidth > 0.0) {
        cformat.setTextOutline(QPen(outlineColor, outlineWidth));
    }
    tt->updateShadow(shadowBox->isChecked(), blur_radius->value(), shadowX->value(), shadowY->value(), shadowColor->color());
    if (gradient_color->isChecked()) {
        QString gradientData = gradients_combo->currentData().toString();
        tt->setData(TitleDocument::Gradient, gradientData);
        QLinearGradient gr = GradientWidget::gradientFromString(gradientData, tt->boundingRect().width(), tt->boundingRect().height());
        cformat.setForeground(QBrush(gr));
    } else {
        cformat.setForeground(QBrush(color));
    }
    cur.setCharFormat(cformat);
    cur.setBlockFormat(format);
    tt->setTextCursor(cur);
    tt->setZValue(m_count++);
    setCurrentItem(tt);
    prepareTools(tt);
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
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    for (auto &i : l) {
        i->setZValue(v);
    }
}

void TitleWidget::selectionChanged()
{
    if (m_scene->tool() != TITLE_SELECT) {
        return;
    }

    // qCDebug(KDENLIVE_LOG) << "Number of selected items: " << graphicsView->scene()->selectedItems().length() << '\n';

    QList<QGraphicsItem *> l;

    // mbt 1607: One text item might have grabbed the keyboard.
    // Ungrab it for all items that are not selected, otherwise
    // text input would only work for the text item that grabbed
    // the keyboard last.
    l = graphicsView->scene()->items();
    for (QGraphicsItem *item : qAsConst(l)) {
        if (item->type() == TEXTITEM && !item->isSelected()) {
            auto *i = static_cast<MyTextItem *>(item);
            i->clearFocus();
        }
    }

    l = graphicsView->scene()->selectedItems();

    if (!l.isEmpty()) {
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
        prepareTools(nullptr);
    } else if (l.size() == 1) {
        prepareTools(l.at(0));
    } else {
        /*
        For multiple selected objects we need to decide which tools to show.
        */
        int firstType = l.at(0)->type();
        bool allEqual = true;
        for (auto i : qAsConst(l)) {
            if (i->type() != firstType) {
                allEqual = false;
                break;
            }
        }
        // qCDebug(KDENLIVE_LOG) << "All equal? " << allEqual << ".\n";
        if (allEqual) {
            prepareTools(l.at(0));
        } else {
            // Get the default toolset, but enable the property frame (x,y,w,h)
            prepareTools(nullptr);
            frame_properties->setEnabled(true);

            // Enable x/y/w/h if it makes sense.
            value_x->setEnabled(true);
            value_y->setEnabled(true);
            bool containsTextitem = false;
            for (auto i : qAsConst(l)) {
                if (i->type() == TEXTITEM) {
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
        for (auto &i : l) {
            if ((int)i->zValue() != firstZindex) {
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
    // qCDebug(KDENLIVE_LOG) << l.size() << " items to be resized\n";

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

    for (int k = 0; k < l.size(); ++k) {
        // qCDebug(KDENLIVE_LOG) << "Type of item " << k << ": " << l.at(k)->type() << '\n';

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
            auto *rec = static_cast<QGraphicsRectItem *>(l.at(k));
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
                double phi = (double)i->boundingRect().width() / i->boundingRect().height();
                // TODO: proper calculation for rotation around 3 axes
                double alpha = (double)t.rotatez / 180.0 * M_PI;

                // New length
                double length;

                // Scaling factor
                double scalex = t.scalex;
                double scaley = t.scaley;

                // We want to keep the aspect ratio of the image as the user does not yet have the possibility
                // to restore the original ratio. You rarely want to change it anyway.
                switch (type) {
                case ValueWidth:
                    // Add 0.5 because otherwise incrementing by 1 might have no effect
                    length = val / (cos(alpha) + 1 / phi * sin(alpha)) + 0.5;
                    scalex = length / i->boundingRect().width();
                    if (preserveAspectRatio->isChecked()) {
                        scaley = scalex;
                    }
                    break;
                case ValueHeight:
                    length = val / (phi * sin(alpha) + cos(alpha)) + 0.5;
                    scaley = length / i->boundingRect().height();
                    if (preserveAspectRatio->isChecked()) {
                        scalex = scaley;
                    }
                    break;
                }
                t.scalex = scalex;
                t.scaley = scaley;
                QTransform qtrans;
                qtrans.scale(scalex, scaley);
                qtrans.rotate(t.rotatex, Qt::XAxis);
                qtrans.rotate(t.rotatey, Qt::YAxis);
                qtrans.rotate(t.rotatez, Qt::ZAxis);
                i->setTransform(qtrans);
                // qCDebug(KDENLIVE_LOG) << "scale is: " << scale << '\n';
                // qCDebug(KDENLIVE_LOG) << i->boundingRect().width() << ": new width\n";
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

    zValue->setValue((int)i->zValue());
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
        auto *r = static_cast<QGraphicsRectItem *>(i);
        // qCDebug(KDENLIVE_LOG) << "Rect width is: " << r->rect().width() << ", was: " << value_w->value() << '\n';
        value_w->setValue((int)r->rect().width());
        value_h->setValue((int)r->rect().height());
    } else if (i->type() == TEXTITEM) {
        auto *t = static_cast<MyTextItem *>(i);
        value_w->setValue((int)t->boundingRect().width());
        value_h->setValue((int)t->boundingRect().height());
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

        auto *rec = static_cast<MyTextItem *>(i);

        // Set the correct x coordinate value
        if (origin_x_left->isChecked()) {
            // Origin (0 point) is at m_frameWidth, coordinate axis is inverted
            value_x->setValue((int)(m_frameWidth - rec->pos().x() - rec->boundingRect().width()));
        } else {
            // Origin is at 0 (default)
            value_x->setValue((int)rec->pos().x());
        }

        // Same for y
        if (origin_y_top->isChecked()) {
            value_y->setValue((int)(m_frameHeight - rec->pos().y() - rec->boundingRect().height()));
        } else {
            value_y->setValue((int)rec->pos().y());
        }

    } else if (i->type() == RECTITEM) {

        auto *rec = static_cast<QGraphicsRectItem *>(i);

        if (origin_x_left->isChecked()) {
            // Origin (0 point) is at m_frameWidth
            value_x->setValue((int)(m_frameWidth - rec->pos().x() - rec->rect().width()));
        } else {
            // Origin is at 0 (default)
            value_x->setValue((int)rec->pos().x());
        }

        if (origin_y_top->isChecked()) {
            value_y->setValue((int)(m_frameHeight - rec->pos().y() - rec->rect().height()));
        } else {
            value_y->setValue((int)rec->pos().y());
        }

    } else if (i->type() == IMAGEITEM) {

        if (origin_x_left->isChecked()) {
            value_x->setValue((int)(m_frameWidth - i->pos().x() - i->sceneBoundingRect().width()));
        } else {
            value_x->setValue((int)i->pos().x());
        }

        if (origin_y_top->isChecked()) {
            value_y->setValue((int)(m_frameHeight - i->pos().y() - i->sceneBoundingRect().height()));
        } else {
            value_y->setValue((int)i->pos().y());
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

    if (!i->data(TitleDocument::ZoomFactor).isNull()) {
        itemzoom->setValue(i->data(TitleDocument::ZoomFactor).toInt());
    } else {
        itemzoom->setValue((int)(t.scalex * 100.0 + 0.5));
    }

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
        auto *rec = static_cast<MyTextItem *>(i);

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

        auto *rec = static_cast<QGraphicsRectItem *>(i);

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

    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        updateCoordinates(l.at(0));

        // Remember x axis setting
        l.at(0)->setData(TitleDocument::OriginXLeft, origin_x_left->isChecked() ? TitleDocument::AxisInverted : TitleDocument::AxisDefault);
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

    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        updateCoordinates(l.at(0));

        l.at(0)->setData(TitleDocument::OriginYTop, origin_y_top->isChecked() ? TitleDocument::AxisInverted : TitleDocument::AxisDefault);
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
    m_frameBackground->setBrush(QBrush(color));
}

void TitleWidget::slotChanged()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1 && l.at(0)->type() == TEXTITEM) {
        textChanged(static_cast<MyTextItem *>(l.at(0)));
    }
}

void TitleWidget::textChanged(MyTextItem *i)
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
        if (!i->document()->isEmpty()) {
            updatePosition(i);
        } else {
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

void TitleWidget::slotInsertUnicodeString(const QString &string)
{
    const QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (!l.isEmpty()) {
        if (l.at(0)->type() == TEXTITEM) {
            auto *t = static_cast<MyTextItem *>(l.at(0));
            t->textCursor().insertText(string);
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
    font.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing->value());
    QColor color = fontColorButton->color();
    QColor outlineColor = textOutlineColor->color();
    QString gradientData;
    if (gradient_color->isChecked()) {
        // user wants a gradient
        gradientData = gradients_combo->currentData().toString();
    }

    double outlineWidth = textOutline->value() / 10.0;

    int i;
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    for (i = 0; i < l.length(); ++i) {
        MyTextItem *item = nullptr;
        if (l.at(i)->type() == TEXTITEM) {
            item = static_cast<MyTextItem *>(l.at(i));
        }
        if (!item) {
            // No text item, try next one.
            continue;
        }

        // Set alignment of all text in the text item
        QTextCursor cur(item->document());
        cur.select(QTextCursor::Document);
        QTextBlockFormat format = cur.blockFormat();
        item->setData(TitleDocument::LineSpacing, line_spacing->value());
        format.setLineHeight(line_spacing->value(), QTextBlockFormat::LineDistanceHeight);
        if (buttonAlignLeft->isChecked() || buttonAlignCenter->isChecked() || buttonAlignRight->isChecked()) {
            if (buttonAlignCenter->isChecked()) {
                item->setAlignment(Qt::AlignHCenter);
            } else if (buttonAlignRight->isChecked()) {
                item->setAlignment(Qt::AlignRight);
            } else if (buttonAlignLeft->isChecked()) {
                item->setAlignment(Qt::AlignLeft);
            }
        } else {
            item->setAlignment(qApp->isLeftToRight() ? Qt::AlignRight : Qt::AlignLeft);
        }

        // Set font properties
        item->setFont(font);
        QTextCharFormat cformat = cur.charFormat();

        item->setData(TitleDocument::OutlineWidth, outlineWidth);
        item->setData(TitleDocument::OutlineColor, outlineColor);
        if (outlineWidth > 0.0) {
            cformat.setTextOutline(QPen(outlineColor, outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }

        if (gradientData.isEmpty()) {
            cformat.setForeground(QBrush(color));
        } else {
            QLinearGradient gr = GradientWidget::gradientFromString(gradientData, item->boundingRect().width(), item->boundingRect().height());
            cformat.setForeground(QBrush(gr));
        }
        // Store gradient in item properties
        item->setData(TitleDocument::Gradient, gradientData);
        cur.setCharFormat(cformat);
        cur.setBlockFormat(format);
        //  item->setTextCursor(cur);
        cur.clearSelection();
        item->setTextCursor(cur);
        item->setTextColor(color);
    }
}

void TitleWidget::rectChanged()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    for (auto i : qAsConst(l)) {
        if (i->type() == RECTITEM && (settingUp == 0)) {
            auto *rec = static_cast<QGraphicsRectItem *>(i);
            QColor f = rectFColor->color();
            if (rectLineWidth->value() == 0) {
                rec->setPen(Qt::NoPen);
            } else {
                QPen penf(f);
                penf.setWidth(rectLineWidth->value());
                penf.setJoinStyle(Qt::RoundJoin);
                rec->setPen(penf);
            }
            if (plain_rect->isChecked()) {
                rec->setBrush(QBrush(rectBColor->color()));
                rec->setData(TitleDocument::Gradient, QVariant());
            } else {
                // gradient
                QString gradientData = gradients_rect_combo->currentData().toString();
                rec->setData(TitleDocument::Gradient, gradientData);
                QLinearGradient gr = GradientWidget::gradientFromString(gradientData, rec->boundingRect().width(), rec->boundingRect().height());
                rec->setBrush(QBrush(gr));
            }
        }
    }
}

void TitleWidget::itemScaled(int val)
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
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
        l[0]->setData(TitleDocument::ZoomFactor, val);
        m_transformations[l.at(0)] = x;
        updateDimension(l.at(0));
    }
}

void TitleWidget::itemRotateX(int val)
{
    itemRotate(val, 0);
}

void TitleWidget::itemRotateY(int val)
{
    itemRotate(val, 1);
}

void TitleWidget::itemRotateZ(int val)
{
    itemRotate(val, 2);
}

void TitleWidget::itemRotate(int val, int axis)
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
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

        l[0]->setData(TitleDocument::RotateFactor, QList<QVariant>() << QVariant(x.rotatex) << QVariant(x.rotatey) << QVariant(x.rotatez));

        QTransform qtrans;
        qtrans.scale(x.scalex, x.scaley);
        qtrans.rotate(x.rotatex, Qt::XAxis);
        qtrans.rotate(x.rotatey, Qt::YAxis);
        qtrans.rotate(x.rotatez, Qt::ZAxis);
        l[0]->setTransform(qtrans);
        m_transformations[l.at(0)] = x;
        if (l[0]->data(TitleDocument::ZoomFactor).isNull()) {
            l[0]->setData(TitleDocument::ZoomFactor, 100);
        }
        updateDimension(l.at(0));
    }
}

void TitleWidget::itemHCenter()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
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
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
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
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QList <double> margins {m_frameHeight * 0.05, m_frameHeight * 0.1};
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.top() < 0.) {
            // align with big margin
            diff = margins.at(1) - br.top();
        } else if (qFuzzyIsNull(br.top())) {
            // align right with frame border
            diff = - br.bottom();
        } else if (br.top() <= margins.at(0)) {
            // align with 0
            diff = - br.top();
        } else if (br.top() <= margins.at(1)) {
            // align with small margin
            diff = margins.at(0) - br.top();
        } else {
            // align with big margin
            diff = margins.at(1) - br.top();
        }
        item->moveBy(0, diff);
        updateCoordinates(item);
    }
}

void TitleWidget::itemBottom()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QList <double> margins {m_frameHeight * 0.9, m_frameHeight* 0.95};
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.bottom() < margins.at(0)) {
            // align with small margin
            diff = margins.at(0) - br.bottom();
        } else if (br.bottom() < margins.at(1)) {
            // align big margin
            diff = margins.at(1) - br.bottom();
        } else if (br.bottom() < m_frameHeight) {
            // align with frame
            diff = m_frameHeight - br.bottom();
        } else if (br.top() < m_frameHeight) {
            // align left with frame
            diff = m_frameHeight - br.top();
        } else {
            // align with big margin
            diff = margins.at(0) - br.bottom();
        }
        item->moveBy(0, diff);
        updateCoordinates(item);
    }
}

void TitleWidget::itemLeft()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QList <double> margins {m_frameWidth * 0.05, m_frameWidth * 0.1};
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.left() < 0.) {
            // align with big margin
            diff = margins.at(1) - br.left();
        } else if (qFuzzyIsNull(br.left())) {
            // align right with frame border
            diff = - br.right();
        } else if (br.left() <= margins.at(0)) {
            // align with 0
            diff = - br.left();
        } else if (br.left() <= margins.at(1)) {
            // align with small margin
            diff = margins.at(0) - br.left();
        } else {
            // align with big margin
            diff = margins.at(1) - br.left();
        }
        item->moveBy(diff, 0);
        updateCoordinates(item);
    }
}

void TitleWidget::itemRight()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QList <double> margins {m_frameWidth * 0.9, m_frameWidth * 0.95};
        QGraphicsItem *item = l.at(0);
        QRectF br = item->sceneBoundingRect();
        double diff;
        if (br.right() < margins.at(0)) {
            // align with small margin
            diff = margins.at(0) - br.right();
        } else if (br.right() < margins.at(1)) {
            // align big margin
            diff = margins.at(1) - br.right();
        } else if (br.right() < m_frameWidth) {
            // align with frame
            diff = m_frameWidth - br.right();
        } else if (br.left() < m_frameWidth) {
            // align left with frame
            diff = m_frameWidth - br.left();
        } else {
            // align with big margin
            diff = margins.at(0) - br.right();
        }
        item->moveBy(diff, 0);
        updateCoordinates(item);
    }
}

void TitleWidget::loadTitle(QUrl url)
{
    if (!url.isValid()) {
        QString startFolder = KRecentDirs::dir(QStringLiteral(":KdenliveProjectsTitles"));
        url = QFileDialog::getOpenFileUrl(this, i18n("Load Title"), QUrl::fromLocalFile(startFolder.isEmpty() ? m_projectTitlePath : startFolder),
                                          i18n("Kdenlive title (*.kdenlivetitle)"));
    }
    if (url.isValid()) {
        // make sure we don't delete the guides
        qDeleteAll(m_guides);
        m_guides.clear();
        QList<QGraphicsItem *> items = m_scene->items();
        items.removeAll(m_frameBorder);
        items.removeAll(m_frameBackground);
        items.removeAll(m_frameImage);
        for (auto item : qAsConst(items)) {
            if (item->zValue() > -1000) {
                delete item;
            }
        }
        m_scene->clearTextSelection();
        QDomDocument doc;
        QFile file(url.toLocalFile());
        doc.setContent(&file, false);
        file.close();
        setXml(doc);
        updateGuides(0);
        m_projectTitlePath = QFileInfo(file).dir().absolutePath();
        KRecentDirs::add(QStringLiteral(":KdenliveProjectsTitles"), m_projectTitlePath);
    }
}

QUrl TitleWidget::saveTitle(QUrl url)
{
    if (anim_start->isChecked()) {
        slotAnimStart(false);
    }
    if (anim_end->isChecked()) {
        slotAnimEnd(false);
    }
    bool embed_image = false;

    // If we have images in the title, ask for embed
    QList<QGraphicsItem *> list = graphicsView->scene()->items();
    QGraphicsPixmapItem pix;
    int pixmapType = pix.type();
    for (const QGraphicsItem *item : qAsConst(list)) {
        if (item->type() == pixmapType && item != m_frameImage) {
            embed_image = true;
            break;
        }
    }
    if (embed_image && KMessageBox::questionYesNo(
                           this, i18n("Do you want to embed Images into this TitleDocument?\nThis is most needed for sharing Titles.")) != KMessageBox::Yes) {
        embed_image = false;
    }
    if (!url.isValid()) {
        QPointer<QFileDialog> fs = new QFileDialog(this, i18n("Save Title"), m_projectTitlePath);
        fs->setMimeTypeFilters(QStringList() << QStringLiteral("application/x-kdenlivetitle"));
        fs->setFileMode(QFileDialog::AnyFile);
        fs->setAcceptMode(QFileDialog::AcceptSave);
        fs->setDefaultSuffix(QStringLiteral("kdenlivetitle"));

        // TODO: KF5 porting?
        // fs->setConfirmOverwrite(true);
        // fs->setKeepLocation(true);
        if ((fs->exec() != 0) && !fs->selectedUrls().isEmpty()) {
            url = fs->selectedUrls().constFirst();
        }
        delete fs;
    }
    if (url.isValid()) {
        if (!m_titledocument.saveDocument(url, m_startViewport, m_endViewport, m_tc.getFrameCount(title_duration->text()), embed_image)) {
            KMessageBox::error(this, i18n("Cannot write to file %1", url.toLocalFile()));
        } else {
            return url;
        }
    }
    return QUrl();
}

void TitleWidget::downloadTitleTemplates()
{
    if (getNewStuff(QStringLiteral(":data/kdenlive_titles.knsrc")) > 0) {
        refreshTitleTemplates(m_projectTitlePath);
        refreshTemplateBoxContents();
    }
}

int TitleWidget::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec() != 0) {
        entries = dialog->changedEntries();
    }
    for (const KNS3::Entry &entry : qAsConst(entries)) {
        if (entry.status() == KNS3::Entry::Installed) {
            qCDebug(KDENLIVE_LOG) << "// Installed files: " << entry.installedFiles();
        }
    }
    delete dialog;
    return entries.size();
}

QDomDocument TitleWidget::xml()
{
    QDomDocument doc = m_titledocument.xml(m_startViewport, m_endViewport);
    int duration = m_tc.getFrameCount(title_duration->text());
    doc.documentElement().setAttribute(QStringLiteral("duration"), duration);
    doc.documentElement().setAttribute(QStringLiteral("out"), duration - 1);
    return doc;
}

int TitleWidget::duration() const
{
    return m_tc.getFrameCount(title_duration->text());
}

void TitleWidget::setXml(const QDomDocument &doc, const QString &id)
{
    m_clipId = id;
    int duration;
    if (m_missingMessage) {
        delete m_missingMessage;
        m_missingMessage = nullptr;
    }
    m_count = m_titledocument.loadFromXml(doc, m_startViewport, m_endViewport, &duration, m_projectTitlePath);
    adjustFrameSize();
    if (m_titledocument.invalidCount() > 0) {
        m_missingMessage = new KMessageWidget(this);
        m_missingMessage->setCloseButtonVisible(true);
        m_missingMessage->setWordWrap(true);
        m_missingMessage->setMessageType(KMessageWidget::Warning);
        m_missingMessage->setText(i18np("This title has 1 missing element", "This title has %1 missing elements", m_titledocument.invalidCount()));
        QAction *action = new QAction(i18n("Details"));
        m_missingMessage->addAction(action);
        connect(action, &QAction::triggered, this, &TitleWidget::showMissingItems);
        action = new QAction(i18n("Delete missing elements"));
        m_missingMessage->addAction(action);
        connect(action, &QAction::triggered, this, &TitleWidget::deleteMissingItems);
        messageLayout->addWidget(m_missingMessage);
        m_missingMessage->animatedShow();
    }
    title_duration->setText(m_tc.getTimecode(GenTime(duration, m_fps)));

    QDomElement e = doc.documentElement();
    m_transformations.clear();
    QList<QGraphicsItem *> items = graphicsView->scene()->items();
    const double PI = 4.0 * atan(1.0);
    for (int i = 0; i < items.count(); ++i) {
        QTransform t = items.at(i)->transform();
        Transform x;
        x.scalex = t.m11();
        x.scaley = t.m22();
        if (!items.at(i)->data(TitleDocument::RotateFactor).isNull()) {
            QList<QVariant> rotlist = items.at(i)->data(TitleDocument::RotateFactor).toList();
            if (rotlist.count() >= 3) {
                x.rotatex = rotlist[0].toDouble();
                x.rotatey = rotlist[1].toDouble();
                x.rotatez = rotlist[2].toDouble();

                // Try to adjust zoom
                t.rotate(x.rotatex * (-1), Qt::XAxis);
                t.rotate(x.rotatey * (-1), Qt::YAxis);
                t.rotate(x.rotatez * (-1), Qt::ZAxis);
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
    
    createButton->setMenu(nullptr);
    createButton->setPopupMode(QToolButton::DelayedPopup);
    m_createTitleAction->setText(i18n("Update Title"));
    

    QTimer::singleShot(200, this, &TitleWidget::slotAdjustZoom);
    slotSelectTool();
    selectionChanged();
}

void TitleWidget::slotAccepted()
{
    if (anim_start->isChecked()) {
        slotAnimStart(false);
    }
    if (anim_end->isChecked()) {
        slotAnimEnd(false);
    }
    writeChoices();
}

void TitleWidget::deleteMissingItems()
{
    m_missingMessage->animatedHide();
    QList<QGraphicsItem *> items = graphicsView->scene()->items();
    QList<QGraphicsItem *> toDelete;
    for (int i = 0; i < items.count(); ++i) {
        if (items.at(i)->data(Qt::UserRole + 2).toInt() == 1) {
            // We found a missing item
            toDelete << items.at(i);
        }
    }
    if (toDelete.size() != m_titledocument.invalidCount()) {
        qDebug() << "/// WARNING, INCOHERENT MISSING ELEMENTS in title: " << toDelete.size() << " != " << m_titledocument.invalidCount();
    }
    while (!toDelete.isEmpty()) {
        QGraphicsItem *item = toDelete.takeFirst();
        if (m_scene) {
            m_scene->removeItem(item);
        }
    }
    m_missingMessage->deleteLater();
}

void TitleWidget::showMissingItems()
{
    QList<QGraphicsItem *> items = graphicsView->scene()->items();
    QStringList missingUrls;
    for (int i = 0; i < items.count(); ++i) {
        if (items.at(i)->data(Qt::UserRole + 2).toInt() == 1) {
            // We found a missing item
            missingUrls << items.at(i)->data(Qt::UserRole).toString();
        }
    }
    missingUrls.removeDuplicates();
    KMessageBox::informationList(QApplication::activeWindow(), i18n("The following files are missing:"), missingUrls);
}

void TitleWidget::writeChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");
    // Write the entries
    titleConfig.writeEntry("dialog_geometry", saveGeometry().toBase64());
    titleConfig.writeEntry("font_family", font_family->currentFont());
    // titleConfig.writeEntry("font_size", font_size->value());
    titleConfig.writeEntry("font_pixel_size", font_size->value());
    titleConfig.writeEntry("font_color", fontColorButton->color());
    titleConfig.writeEntry("font_outline_color", textOutlineColor->color());
    titleConfig.writeEntry("font_outline", textOutline->value());
    titleConfig.writeEntry("font_weight", font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    titleConfig.writeEntry("font_italic", buttonItalic->isChecked());
    titleConfig.writeEntry("font_underlined", buttonUnder->isChecked());

    titleConfig.writeEntry("rect_background_color", rectBColor->color());
    titleConfig.writeEntry("rect_foreground_color", rectFColor->color());

    titleConfig.writeEntry("rect_background_alpha", rectBColor->color().alpha());
    titleConfig.writeEntry("rect_foreground_alpha", rectFColor->color().alpha());

    titleConfig.writeEntry("rect_line_width", rectLineWidth->value());

    titleConfig.writeEntry("background_color", backgroundColor->color());
    titleConfig.writeEntry("background_alpha", backgroundAlpha->value());

    titleConfig.writeEntry("use_grid", use_grid->isChecked());

    //! \todo Not sure if I should sync - it is probably safe to do it
    config->sync();
}

void TitleWidget::readChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");
    // read the entries
    const QByteArray geometry = titleConfig.readEntry("dialog_geometry", QByteArray());
    restoreGeometry(QByteArray::fromBase64(geometry));
    font_family->setCurrentFont(titleConfig.readEntry("font_family", font_family->currentFont()));
    font_size->setValue(titleConfig.readEntry("font_pixel_size", m_frameHeight > 0 ? (int)(m_frameHeight / 20): font_size->value()));
    m_scene->slotUpdateFontSize(font_size->value());
    QColor fontColor = QColor(titleConfig.readEntry("font_color", fontColorButton->color()));
    QColor outlineColor = QColor(titleConfig.readEntry("font_outline_color", textOutlineColor->color()));
    fontColor.setAlpha(titleConfig.readEntry("font_alpha", fontColor.alpha()));
    outlineColor.setAlpha(titleConfig.readEntry("font_outline_alpha", outlineColor.alpha()));
    fontColorButton->setColor(fontColor);
    textOutlineColor->setColor(outlineColor);
    textOutline->setValue(titleConfig.readEntry("font_outline", textOutline->value()));

    int weight;
    if (titleConfig.readEntry("font_bold", false)) {
        weight = QFont::Bold;
    } else {
        weight = titleConfig.readEntry("font_weight", font_weight_box->itemData(font_weight_box->currentIndex()).toInt());
    }
    setFontBoxWeight(weight);
    buttonItalic->setChecked(titleConfig.readEntry("font_italic", buttonItalic->isChecked()));
    buttonUnder->setChecked(titleConfig.readEntry("font_underlined", buttonUnder->isChecked()));

    QColor fgColor = QColor(titleConfig.readEntry("rect_foreground_color", rectFColor->color()));
    QColor bgColor = QColor(titleConfig.readEntry("rect_background_color", rectBColor->color()));

    fgColor.setAlpha(titleConfig.readEntry("rect_foreground_alpha", fgColor.alpha()));
    bgColor.setAlpha(titleConfig.readEntry("rect_background_alpha", bgColor.alpha()));
    rectFColor->setColor(fgColor);
    rectBColor->setColor(bgColor);

    rectLineWidth->setValue(titleConfig.readEntry("rect_line_width", rectLineWidth->value()));

    backgroundColor->setColor(titleConfig.readEntry("background_color", backgroundColor->color()));
    backgroundAlpha->setValue(titleConfig.readEntry("background_alpha", backgroundAlpha->value()));
    use_grid->setChecked(titleConfig.readEntry("use_grid", false));
    m_scene->slotUseGrid(use_grid->isChecked());
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
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->zValue() > -1000) {
            if (!list.at(i)->data(-1).isNull()) {
                continue;
            }
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
        if (m_startViewport->childItems().isEmpty()) {
            addAnimInfoText();
        }
    } else {
        m_startViewport->setZValue(-1000);
        m_startViewport->setBrush(QBrush());
        m_startViewport->setFlags(nullptr);
        if (!anim_end->isChecked()) {
            deleteAnimInfoText();
        }
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
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->zValue() > -1000) {
            if (!list.at(i)->data(-1).isNull()) {
                continue;
            }
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
        if (m_endViewport->childItems().isEmpty()) {
            addAnimInfoText();
        }
    } else {
        m_endViewport->setZValue(-1000);
        m_endViewport->setBrush(QBrush());
        m_endViewport->setFlags(nullptr);
        if (!anim_start->isChecked()) {
            deleteAnimInfoText();
        }
    }
}

void TitleWidget::addAnimInfoText()
{
    // add text to anim viewport
    QGraphicsTextItem *t = new QGraphicsTextItem(i18nc("Indicates the start of an animation", "Start"), m_startViewport);
    QGraphicsTextItem *t2 = new QGraphicsTextItem(i18nc("Indicates the end of an animation", "End"), m_endViewport);
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
        MyTextItem *item = static_cast<MyTextItem *>(m_startViewport->childItems().at(0));
        if (item) {
            QFont font = item->font();
            font.setPixelSize(m_startViewport->rect().width() / 10);
            item->setFont(font);
        }
    }
    if (!m_endViewport->childItems().isEmpty()) {
        MyTextItem *item = static_cast<MyTextItem *>(m_endViewport->childItems().at(0));
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
        if (m_scene) {
            m_scene->removeItem(item);
        }
    }
    while (!m_endViewport->childItems().isEmpty()) {
        QGraphicsItem *item = m_endViewport->childItems().at(0);
        if (m_scene) {
            m_scene->removeItem(item);
        }
    }
}

void TitleWidget::slotKeepAspect(bool keep)
{
    if ((int)m_endViewport->zValue() == 1100) {
        m_endViewport->setData(0, keep ? m_frameWidth : QVariant());
        m_endViewport->setData(1, keep ? m_frameHeight : QVariant());
    } else {
        m_startViewport->setData(0, keep ? m_frameWidth : QVariant());
        m_startViewport->setData(1, keep ? m_frameHeight : QVariant());
    }
}

void TitleWidget::slotResize50()
{
    if ((int)m_endViewport->zValue() == 1100) {
        m_endViewport->setRect(0, 0, m_frameWidth / 2, m_frameHeight / 2);
    } else {
        m_startViewport->setRect(0, 0, m_frameWidth / 2, m_frameHeight / 2);
    }
}

void TitleWidget::slotResize100()
{
    if ((int)m_endViewport->zValue() == 1100) {
        m_endViewport->setRect(0, 0, m_frameWidth, m_frameHeight);
    } else {
        m_startViewport->setRect(0, 0, m_frameWidth, m_frameHeight);
    }
}

void TitleWidget::slotResize200()
{
    if ((int)m_endViewport->zValue() == 1100) {
        m_endViewport->setRect(0, 0, m_frameWidth * 2, m_frameHeight * 2);
    } else {
        m_startViewport->setRect(0, 0, m_frameWidth * 2, m_frameHeight * 2);
    }
}

void TitleWidget::slotAddEffect(int /*ix*/)
{
    /*
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
        for (QGraphicsItem * item :  list) {
            switch (effect) {
            case NOEFFECT:
                item->setData(100, QVariant());
                item->setGraphicsEffect(0);
                break;
            case TYPEWRITEREFFECT:
                if (item->type() == TEXTITEM) {
                    QStringList effdata = QStringList() << QStringLiteral("typewriter") << QString::number(typewriter_delay->value()) + QLatin1Char(';') +
       QString::number(typewriter_start->value());
                    item->setData(100, effdata);
                }
                break;
                // Do not remove the non-QGraphicsEffects.
            case BLUREFFECT:
                item->setGraphicsEffect(new QGraphicsBlurEffect());
                break;
            case SHADOWEFFECT:
                item->setGraphicsEffect(new QGraphicsDropShadowEffect());
                break;
            }
        }*/
}

qreal TitleWidget::zIndexBounds(bool maxBound, bool intersectingOnly)
{
    qreal bound = maxBound ? -99 : 99;
    const QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    if (!l.isEmpty()) {
        QList<QGraphicsItem *> lItems;
        // Get items (all or intersecting only)
        if (intersectingOnly) {
            lItems = graphicsView->scene()->items(l[0]->sceneBoundingRect(), Qt::IntersectsItemShape);
        } else {
            lItems = graphicsView->scene()->items();
        }
        if (!lItems.isEmpty()) {
            int n = lItems.size();
            qreal z;
            if (maxBound) {
                for (int i = 0; i < n; ++i) {
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
                for (int i = 0; i < n; ++i) {
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
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
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
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    qreal max = zIndexBounds(true, false);
    // qCDebug(KDENLIVE_LOG) << "Max z-index is " << max << ".\n";
    for (auto &i : l) {
        qreal currentZ = i->zValue();
        if (currentZ <= max) {
            // qCDebug(KDENLIVE_LOG) << "Updating item " << i << ", is " << currentZ << ".\n";
            i->setZValue(max + 1);
        } else {
            // qCDebug(KDENLIVE_LOG) << "Not updating " << i << ", is " << currentZ << ".\n";
        }
    }
    // Update the z index value in the GUI
    if (!l.isEmpty()) {
        updateDimension(l[0]);
    }
}

void TitleWidget::slotZIndexDown()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
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
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    qreal min = zIndexBounds(false, false);
    for (auto &i : l) {
        qreal currentZ = i->zValue();
        if (currentZ >= min) {
            i->setZValue(min - 1);
        }
    }
    // Update the z index value in the GUI
    if (!l.isEmpty()) {
        updateDimension(l[0]);
    }
}

void TitleWidget::slotSelectAll()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->items();
    for (auto i : qAsConst(l)) {
        i->setSelected(true);
    }
}

void TitleWidget::selectItems(int itemType)
{
    QList<QGraphicsItem *> l;
    if (!graphicsView->scene()->selectedItems().isEmpty()) {
        l = graphicsView->scene()->selectedItems();
        for (auto i : qAsConst(l)) {
            if (i->type() != itemType) {
                i->setSelected(false);
            }
        }
    } else {
        l = graphicsView->scene()->items();
        for (auto i : qAsConst(l)) {
            if (i->type() == itemType) {
                i->setSelected(true);
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
    QList<QGraphicsItem *> l = graphicsView->scene()->items();
    for (auto i : qAsConst(l)) {
        i->setSelected(false);
    }
    graphicsView->blockSignals(false);
    selectionChanged();
}

QString TitleWidget::getTooltipWithShortcut(const QString &tipText, QAction *button)
{
    return tipText + QStringLiteral("  <b>") + button->shortcut().toString() + QStringLiteral("</b>");
}

void TitleWidget::prepareTools(QGraphicsItem *referenceItem)
{
    // Let some GUI elements block signals. We may want to change their values without any sideeffects.
    // Additionally, store the previous blocking state to avoid side effects when this function is called from within another one.
    // Note: Disabling an element also blocks signals. So disabled elements don't need to be set to blocking too.
    bool blockOX = origin_x_left->signalsBlocked();
    bool blockOY = origin_y_top->signalsBlocked();
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
    itemrotatex->blockSignals(true);
    itemrotatey->blockSignals(true);
    itemrotatez->blockSignals(true);
    itemzoom->blockSignals(true);
    value_x->blockSignals(true);
    value_y->blockSignals(true);
    value_w->blockSignals(true);
    value_h->blockSignals(true);

    if (referenceItem == nullptr) {
        // qCDebug(KDENLIVE_LOG) << "nullptr item.\n";
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
        toolbar_stack->setEnabled(false);
        /*letter_spacing->setEnabled(false);
        line_spacing->setEnabled(false);
        letter_spacing->setValue(0);
        line_spacing->setValue(0);*/
    } else {
        toolbar_stack->setEnabled(true);
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

        letter_spacing->setEnabled(referenceItem->type() == TEXTITEM);
        line_spacing->setEnabled(referenceItem->type() == TEXTITEM);

        if (referenceItem->type() == TEXTITEM) {
            showToolbars(TITLE_TEXT);
            auto *i = static_cast<MyTextItem *>(referenceItem);
            if (!i->document()->isEmpty()) {
                // We have an existing text item selected
                if (!i->data(100).isNull()) {
                    // Item has an effect
                    /*QStringList effdata = i->data(100).toStringList();
                    QString effectName = effdata.takeFirst();
                    if (effectName == QLatin1String("typewriter")) {
                        QStringList params = effdata.at(0).split(QLatin1Char(';'));
                        typewriter_delay->setValue(params.at(0).toInt());
                        typewriter_start->setValue(params.at(1).toInt());
                        effect_list->setCurrentIndex(effect_list->findData((int)TYPEWRITEREFFECT));
                    }*/
                } else {
                    /*if (i->graphicsEffect()) {
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
                    }*/
                }
                font_size->blockSignals(true);
                font_family->blockSignals(true);
                font_weight_box->blockSignals(true);
                buttonItalic->blockSignals(true);
                buttonUnder->blockSignals(true);
                fontColorButton->blockSignals(true);
                buttonAlignLeft->blockSignals(true);
                buttonAlignRight->blockSignals(true);
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
                fontColorButton->setColor(color);

                if (!i->data(TitleDocument::OutlineWidth).isNull()) {
                    textOutline->blockSignals(true);
                    textOutline->setValue(i->data(TitleDocument::OutlineWidth).toDouble() * 10);
                    textOutline->blockSignals(false);
                } else {
                    textOutline->blockSignals(true);
                    textOutline->setValue(0);
                    textOutline->blockSignals(false);
                }
                if (!i->data(TitleDocument::OutlineColor).isNull()) {
                    textOutlineColor->blockSignals(true);
                    QVariant variant = i->data(TitleDocument::OutlineColor);
                    color = variant.value<QColor>();
                    textOutlineColor->setColor(color);
                    textOutlineColor->blockSignals(false);
                }
                if (!i->data(TitleDocument::Gradient).isNull()) {
                    gradients_combo->blockSignals(true);
                    gradient_color->setChecked(true);
                    QString gradientData = i->data(TitleDocument::Gradient).toString();
                    int ix = gradients_combo->findData(gradientData);
                    if (ix == -1) {
                        // This gradient does not exist in our settings, store it
                        storeGradient(gradientData);
                        ix = gradients_combo->findData(gradientData);
                    }
                    gradients_combo->setCurrentIndex(ix);
                    gradients_combo->blockSignals(false);
                } else {
                    plain_color->setChecked(true);
                }
                if (i->alignment() == Qt::AlignHCenter) {
                    buttonAlignCenter->setChecked(true);
                } else if (i->alignment() == Qt::AlignRight) {
                    buttonAlignRight->setChecked(true);
                } else {
                    buttonAlignLeft->setChecked(true);
                }

                QStringList sInfo = i->shadowInfo();
                if (sInfo.count() >= 5) {
                    shadowBox->setChecked(static_cast<bool>(sInfo.at(0).toInt()));
                    shadowBox->blockSignals(true);
                    shadowColor->setColor(QColor(sInfo.at(1)));
                    blur_radius->setValue(sInfo.at(2).toInt());
                    shadowX->setValue(sInfo.at(3).toInt());
                    shadowY->setValue(sInfo.at(4).toInt());
                    shadowBox->blockSignals(false);
                }

                letter_spacing->blockSignals(true);
                line_spacing->blockSignals(true);
                QTextCursor cur = i->textCursor();
                QTextBlockFormat format = cur.blockFormat();
                letter_spacing->setValue(font.letterSpacing());
                line_spacing->setValue(format.lineHeight());
                letter_spacing->blockSignals(false);
                line_spacing->blockSignals(false);

                font_size->blockSignals(false);
                font_family->blockSignals(false);
                font_weight_box->blockSignals(false);
                buttonItalic->blockSignals(false);
                buttonUnder->blockSignals(false);
                fontColorButton->blockSignals(false);
                buttonAlignLeft->blockSignals(false);
                buttonAlignRight->blockSignals(false);
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
            }

            updateAxisButtons(i);
            updateCoordinates(i);
            updateDimension(i);
            enableToolbars(TITLE_TEXT);

        } else if ((referenceItem)->type() == RECTITEM) {
            showToolbars(TITLE_RECTANGLE);
            settingUp = 1;
            auto *rec = static_cast<QGraphicsRectItem *>(referenceItem);
            if (rec == m_startViewport || rec == m_endViewport) {
                enableToolbars(TITLE_SELECT);
            } else {
                QColor fcol = rec->pen().color();
                QColor bcol = rec->brush().color();
                rectFColor->setColor(fcol);
                QString gradientData = rec->data(TitleDocument::Gradient).toString();
                if (gradientData.isEmpty()) {
                    plain_rect->setChecked(true);
                    rectBColor->setColor(bcol);
                } else {
                    gradient_rect->setChecked(true);
                    gradients_rect_combo->blockSignals(true);
                    int ix = gradients_rect_combo->findData(gradientData);
                    if (ix == -1) {
                        storeGradient(gradientData);
                        ix = gradients_rect_combo->findData(gradientData);
                    }
                    gradients_rect_combo->setCurrentIndex(ix);
                    gradients_rect_combo->blockSignals(false);
                }
                settingUp = 0;
                if (rec->pen() == Qt::NoPen) {
                    rectLineWidth->setValue(0);
                } else {
                    rectLineWidth->setValue(rec->pen().width());
                }
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
            QSignalBlocker bk(preserveAspectRatio);
            Transform t = m_transformations.value(referenceItem);
            preserveAspectRatio->setChecked(qFuzzyCompare(t.scalex, t.scaley));

        } else {
            showToolbars(TITLE_SELECT);
            enableToolbars(TITLE_SELECT);
            frame_properties->setEnabled(false);
        }
        zValue->setValue((int)referenceItem->zValue());
        if (!referenceItem->data(TitleDocument::ZoomFactor).isNull()) {
            itemzoom->setValue(referenceItem->data(TitleDocument::ZoomFactor).toInt());
        } else {
            itemzoom->setValue((int)(m_transformations.value(referenceItem).scalex * 100.0 + 0.5));
        }
        itemrotatex->setValue((int)(m_transformations.value(referenceItem).rotatex));
        itemrotatey->setValue((int)(m_transformations.value(referenceItem).rotatey));
        itemrotatez->setValue((int)(m_transformations.value(referenceItem).rotatez));
    }

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

void TitleWidget::slotEditGradient()
{
    auto *caller = qobject_cast<QToolButton *>(QObject::sender());
    if (!caller) {
        return;
    }
    QComboBox *combo = nullptr;
    if (caller == edit_gradient) {
        combo = gradients_combo;
    } else {
        combo = gradients_rect_combo;
    }
    QMap<QString, QString> gradients;
    for (int i = 0; i < combo->count(); i++) {
        gradients.insert(combo->itemText(i), combo->itemData(i).toString());
    }
    GradientWidget d(gradients, combo->currentIndex());
    if (d.exec() == QDialog::Accepted) {
        // Save current gradients
        QMap<QString, QString> gradMap = d.gradients();
        QList<QIcon> icons = d.icons();
        QMap<QString, QString>::const_iterator i = gradMap.constBegin();
        KSharedConfigPtr config = KSharedConfig::openConfig();
        KConfigGroup group(config, "TitleGradients");
        group.deleteGroup();
        combo->clear();
        gradients_rect_combo->clear();
        int ix = 0;
        while (i != gradMap.constEnd()) {
            group.writeEntry(i.key(), i.value());
            gradients_combo->addItem(icons.at(ix), i.key(), i.value());
            gradients_rect_combo->addItem(icons.at(ix), i.key(), i.value());
            ++i;
            ix++;
        }
        group.sync();
        combo->setCurrentIndex(d.selectedGradient());
    }
}

void TitleWidget::storeGradient(const QString &gradientData)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group(config, "TitleGradients");
    QMap<QString, QString> values = group.entryMap();
    int ix = qMax(1, values.count());
    QString gradName = i18n("Gradient %1", ix);
    while (values.contains(gradName)) {
        ix++;
        gradName = i18n("Gradient %1", ix);
    }
    group.writeEntry(gradName, gradientData);
    group.sync();
    QPixmap pix(30, 30);
    pix.fill(Qt::transparent);
    QLinearGradient gr = GradientWidget::gradientFromString(gradientData, pix.width(), pix.height());
    gr.setStart(0, pix.height() / 2);
    gr.setFinalStop(pix.width(), pix.height() / 2);
    QPainter painter(&pix);
    painter.fillRect(0, 0, pix.width(), pix.height(), QBrush(gr));
    painter.end();
    QIcon icon(pix);
    gradients_combo->addItem(icon, gradName, gradientData);
    gradients_rect_combo->addItem(icon, gradName, gradientData);
}

void TitleWidget::loadGradients()
{
    gradients_combo->blockSignals(true);
    gradients_rect_combo->blockSignals(true);
    QString grad_data = gradients_combo->currentData().toString();
    QString rect_data = gradients_rect_combo->currentData().toString();
    gradients_combo->clear();
    gradients_rect_combo->clear();
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group(config, "TitleGradients");
    QMap<QString, QString> values = group.entryMap();
    if (values.isEmpty()) {
        // Ensure we at least always have one sample black to white gradient
        values.insert(i18n("Gradient"), QStringLiteral("#ffffffff;#ff000000;0;100;90"));
    }
    QMapIterator<QString, QString> k(values);
    while (k.hasNext()) {
        k.next();
        QPixmap pix(30, 30);
        pix.fill(Qt::transparent);
        QLinearGradient gr = GradientWidget::gradientFromString(k.value(), pix.width(), pix.height());
        gr.setStart(0, pix.height() / 2);
        gr.setFinalStop(pix.width(), pix.height() / 2);
        QPainter painter(&pix);
        painter.fillRect(0, 0, pix.width(), pix.height(), QBrush(gr));
        painter.end();
        QIcon icon(pix);
        gradients_combo->addItem(icon, k.key(), k.value());
        gradients_rect_combo->addItem(icon, k.key(), k.value());
    }
    int ix = gradients_combo->findData(grad_data);
    if (ix >= 0) {
        gradients_combo->setCurrentIndex(ix);
    }
    ix = gradients_rect_combo->findData(rect_data);
    if (ix >= 0) {
        gradients_rect_combo->setCurrentIndex(ix);
    }
    gradients_combo->blockSignals(false);
    gradients_rect_combo->blockSignals(false);
}

void TitleWidget::slotUpdateShadow()
{
    QList<QGraphicsItem *> l = graphicsView->scene()->selectedItems();
    for (int i = 0; i < graphicsView->scene()->selectedItems().length(); ++i) {
        MyTextItem *item = nullptr;
        if (l.at(i)->type() == TEXTITEM) {
            item = static_cast<MyTextItem *>(l.at(i));
        }
        if (!item) {
            // No text item, try next one.
            continue;
        }
        item->updateShadow(shadowBox->isChecked(), blur_radius->value(), shadowX->value(), shadowY->value(), shadowColor->color());
    }
}

const QString TitleWidget::titleSuggest()
{
    // Find top item to extract title proposal
    QList<QGraphicsItem *> list = graphicsView->scene()->items();
    int y = m_frameHeight;
    QString title;
    for (QGraphicsItem *qgItem : qAsConst(list)) {
        if (qgItem->pos().y() < y && qgItem->type() == TEXTITEM) {
            auto *i = static_cast<MyTextItem *>(qgItem);
            QString currentTitle = i->toPlainText().simplified();
            if (currentTitle.length() > 2) {
                title = currentTitle.length() > 12 ? currentTitle.left(12) + QStringLiteral("...") : currentTitle;
                y = qgItem->pos().y();
            }
        }
    }
    return title;
}

void TitleWidget::showGuides(int state)
{
    for (QGraphicsLineItem *it : qAsConst(m_guides)) {
        it->setVisible(state == Qt::Checked);
    }
    KdenliveSettings::setTitlerShowGuides(state == Qt::Checked);
}

void TitleWidget::updateGuides(int)
{
    KdenliveSettings::setTitlerHGuides(hguides->value());
    KdenliveSettings::setTitlerVGuides(vguides->value());
    if (!m_guides.isEmpty()) {
        qDeleteAll(m_guides);
        m_guides.clear();
    }
    QPen framepen;
    QColor gColor(KdenliveSettings::titleGuideColor());
    framepen.setColor(gColor);

    // Guides
    // Horizontal guides
    int max = hguides->value();
    bool guideVisible = show_guides->checkState() == Qt::Checked;
    for (int i = 0; i < max; i++) {
        auto *line1 = new QGraphicsLineItem(0, (i + 1) * m_frameHeight / (max + 1), m_frameWidth, (i + 1) * m_frameHeight / (max + 1), m_frameBorder);
        line1->setPen(framepen);
        line1->setFlags({});
        line1->setData(-1, -1);
        line1->setVisible(guideVisible);
        m_guides << line1;
    }
    max = vguides->value();
    for (int i = 0; i < max; i++) {
        auto *line1 = new QGraphicsLineItem((i + 1) * m_frameWidth / (max + 1), 0, (i + 1) * m_frameWidth / (max + 1), m_frameHeight, m_frameBorder);
        line1->setPen(framepen);
        line1->setFlags({});
        line1->setData(-1, -1);
        line1->setVisible(guideVisible);
        m_guides << line1;
    }

    gColor.setAlpha(160);
    framepen.setColor(gColor);

    auto *line6 = new QGraphicsLineItem(0, 0, m_frameWidth, m_frameHeight, m_frameBorder);
    line6->setPen(framepen);
    line6->setFlags({});
    line6->setData(-1, -1);
    line6->setVisible(guideVisible);
    m_guides << line6;

    auto *line7 = new QGraphicsLineItem(m_frameWidth, 0, 0, m_frameHeight, m_frameBorder);
    line7->setPen(framepen);
    line7->setFlags({});
    line7->setData(-1, -1);
    line7->setVisible(guideVisible);
    m_guides << line7;
}

void TitleWidget::guideColorChanged(const QColor &col)
{
    KdenliveSettings::setTitleGuideColor(col);
    QColor guideCol(col);
    for (QGraphicsLineItem *it : qAsConst(m_guides)) {
        int alpha = it->pen().color().alpha();
        guideCol.setAlpha(alpha);
        QPen framePen(guideCol);
        it->setPen(framePen);
    }
}
