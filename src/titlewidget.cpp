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



#include <QGraphicsView>
#include <QDomDocument>
#include <QGraphicsItem>
#include <QGraphicsSvgItem>
#include <QTimer>

#include <QToolBar>
#include <QMenu>

#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>

#include "titlewidget.h"
#include "kdenlivesettings.h"

int settingUp = false;

TitleWidget::TitleWidget(KUrl url, QString projectPath, Render *render, QWidget *parent): QDialog(parent), m_render(render), m_count(0), m_projectPath(projectPath) {
    setupUi(this);
    //frame_properties->
    setFont(KGlobalSettings::toolBarFont());
    //toolBox->setFont(KGlobalSettings::toolBarFont());
    frame_properties->setEnabled(false);
    m_frameWidth = render->renderWidth();
    m_frameHeight = render->renderHeight();
    //connect(newTextButton, SIGNAL(clicked()), this, SLOT(slotNewText()));
    //connect(newRectButton, SIGNAL(clicked()), this, SLOT(slotNewRect()));
    connect(kcolorbutton, SIGNAL(clicked()), this, SLOT(slotChangeBackground())) ;
    connect(horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(slotChangeBackground())) ;
    //connect(ktextedit, SIGNAL(textChanged()), this , SLOT(textChanged()));
    //connect (fontBold, SIGNAL ( clicked()), this, SLOT( setBold()) ) ;


    //ktextedit->setHidden(true);
    connect(fontColorButton, SIGNAL(clicked()), this, SLOT(slotUpdateText())) ;
    connect(font_family, SIGNAL(currentFontChanged(const QFont &)), this, SLOT(slotUpdateText())) ;
    connect(font_size, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateText())) ;
    connect(textAlpha, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateText()));
    //connect (ktextedit, SIGNAL(selectionChanged()), this , SLOT (textChanged()));

    connect(rectFAlpha, SIGNAL(valueChanged(int)), this, SLOT(rectChanged()));
    connect(rectBAlpha, SIGNAL(valueChanged(int)), this, SLOT(rectChanged()));
    connect(rectFColor, SIGNAL(clicked()), this, SLOT(rectChanged()));
    connect(rectBColor, SIGNAL(clicked()), this, SLOT(rectChanged()));
    connect(rectLineWidth, SIGNAL(valueChanged(int)), this, SLOT(rectChanged()));

    connect(startViewportX, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(startViewportY, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(startViewportSize, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(endViewportX, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(endViewportY, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));
    connect(endViewportSize, SIGNAL(valueChanged(int)), this, SLOT(setupViewports()));

    connect(zValue, SIGNAL(valueChanged(int)), this, SLOT(zIndexChanged(int)));
    connect(itemzoom, SIGNAL(valueChanged(int)), this, SLOT(itemScaled(int)));
    connect(itemrotate, SIGNAL(valueChanged(int)), this, SLOT(itemRotate(int)));
    connect(itemhcenter, SIGNAL(clicked()), this, SLOT(itemHCenter()));
    connect(itemvcenter, SIGNAL(clicked()), this, SLOT(itemVCenter()));

    connect(value_x, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(value_y, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(value_w, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(value_h, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(buttonFitZoom, SIGNAL(clicked()), this, SLOT(slotAdjustZoom()));
    connect(buttonRealSize, SIGNAL(clicked()), this, SLOT(slotZoomOneToOne()));
    connect(buttonBold, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonItalic, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(buttonUnder, SIGNAL(clicked()), this, SLOT(slotUpdateText()));
    connect(displayBg, SIGNAL(stateChanged(int)), this, SLOT(displayBackgroundFrame()));

    buttonFitZoom->setIcon(KIcon("zoom-fit-best"));
    buttonRealSize->setIcon(KIcon("zoom-original"));
    buttonBold->setIcon(KIcon("format-text-bold"));
    buttonItalic->setIcon(KIcon("format-text-italic"));
    buttonUnder->setIcon(KIcon("format-text-underline"));

    QHBoxLayout *layout = new QHBoxLayout;
    frame_toolbar->setLayout(layout);

    QToolBar *m_toolbar = new QToolBar("titleToolBar", this);

    m_buttonRect = m_toolbar->addAction(KIcon("insert-rect"), i18n("Add Rectangle"));
    m_buttonRect->setCheckable(true);
    connect(m_buttonRect, SIGNAL(triggered()), this, SLOT(slotRectTool()));

    m_buttonText = m_toolbar->addAction(KIcon("insert-text"), i18n("Add Text"));
    m_buttonText->setCheckable(true);
    connect(m_buttonText, SIGNAL(triggered()), this, SLOT(slotTextTool()));

    m_buttonImage = m_toolbar->addAction(KIcon("insert-image"), i18n("Add Image"));
    m_buttonImage->setCheckable(false);
    connect(m_buttonImage, SIGNAL(triggered()), this, SLOT(slotImageTool()));

    m_buttonCursor = m_toolbar->addAction(KIcon("select-rectangular"), i18n("Selection Tool"));
    m_buttonCursor->setCheckable(true);
    connect(m_buttonCursor, SIGNAL(triggered()), this, SLOT(slotSelectTool()));

    m_buttonLoad = m_toolbar->addAction(KIcon("document-open"), i18n("Open Document"));
    m_buttonLoad->setCheckable(false);
    connect(m_buttonLoad, SIGNAL(triggered()), this, SLOT(loadTitle()));

    m_buttonSave = m_toolbar->addAction(KIcon("document-save-as"), i18n("Save As"));
    m_buttonSave->setCheckable(false);
    connect(m_buttonSave, SIGNAL(triggered()), this, SLOT(saveTitle()));

    layout->addWidget(m_toolbar);
    text_properties->setHidden(true);

    // initialize graphic scene
    m_scene = new GraphicsSceneRectMove(this);
    graphicsView->setScene(m_scene);
    m_titledocument.setScene(m_scene);

    // a gradient background
    QRadialGradient *gradient = new QRadialGradient(0, 0, 10);
    gradient->setSpread(QGradient::ReflectSpread);
    //scene->setBackgroundBrush(*gradient);

    m_frameImage = new QGraphicsPixmapItem();
    QTransform qtrans;
    qtrans.scale(2.0, 2.0);
    m_frameImage->setTransform(qtrans);
    m_frameImage->setZValue(-1200);
    m_frameImage->setFlags(QGraphicsItem::ItemClipsToShape);
    displayBackgroundFrame();
    graphicsView->scene()->addItem(m_frameImage);

    connect(m_scene, SIGNAL(selectionChanged()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(itemMoved()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(sceneZoom(bool)), this , SLOT(slotZoom(bool)));
    connect(m_scene, SIGNAL(actionFinished()), this , SLOT(slotSelectTool()));
    connect(m_scene, SIGNAL(actionFinished()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(newRect(QGraphicsRectItem *)), this , SLOT(slotNewRect(QGraphicsRectItem *)));
    connect(m_scene, SIGNAL(newText(QGraphicsTextItem *)), this , SLOT(slotNewText(QGraphicsTextItem *)));
    connect(zoom_slider, SIGNAL(valueChanged(int)), this , SLOT(slotUpdateZoom(int)));

    QPen framepen(Qt::DotLine);
    framepen.setColor(Qt::red);

    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(QColor(0, 0, 0, 0));
    m_frameBorder->setFlags(QGraphicsItem::ItemClipsToShape);
    graphicsView->scene()->addItem(m_frameBorder);

    initViewports();

    graphicsView->show();
    graphicsView->setRenderHint(QPainter::Antialiasing);
    graphicsView->setInteractive(true);
    QTimer::singleShot(500, this, SLOT(slotAdjustZoom()));
    //graphicsView->resize(400, 300);
    kDebug() << "// TITLE WIDGWT: " << graphicsView->viewport()->width() << "x" << graphicsView->viewport()->height();
    toolBox->setItemEnabled(2, false);
    if (!url.isEmpty()) {
        m_count = m_titledocument.loadDocument(url, startViewport, endViewport) + 1;
        slotSelectTool();
    } else slotRectTool();
}


//virtual
void TitleWidget::resizeEvent(QResizeEvent * event) {
    //slotAdjustZoom();
}

void TitleWidget::slotTextTool() {
    rect_properties->setHidden(true);
    text_properties->setHidden(false);
    m_scene->setTool(TITLE_TEXT);
    m_buttonRect->setChecked(false);
    m_buttonCursor->setChecked(false);
}

void TitleWidget::slotRectTool() {
    text_properties->setHidden(true);
    rect_properties->setHidden(false);
    m_scene->setTool(TITLE_RECTANGLE);
    m_buttonText->setChecked(false);
    m_buttonCursor->setChecked(false);
}

void TitleWidget::slotSelectTool() {
    m_scene->setTool(TITLE_SELECT);
    m_buttonCursor->setChecked(true);
    m_buttonText->setChecked(false);
    m_buttonRect->setChecked(false);
}

void TitleWidget::slotImageTool() {
    KUrl url = KFileDialog::getOpenUrl(KUrl(), "*.svg *.png *.jpg *.jpeg *.gif *.raw", this, i18n("Load Image"));
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
    m_buttonRect->setChecked(false);
    m_buttonCursor->setChecked(true);
    m_buttonText->setChecked(false);
}

void TitleWidget::displayBackgroundFrame() {
    if (!displayBg->isChecked()) {
        QPixmap bg(m_frameWidth / 2, m_frameHeight / 2);
        QPixmap pattern(20, 20);
        pattern.fill();
        QPainter p;
        p.begin(&pattern);
        p.fillRect(QRect(0, 0, 10, 10), QColor(210, 210, 210));
        p.fillRect(QRect(10, 10, 20, 20), QColor(210, 210, 210));
        p.end();
        QBrush br(pattern);

        p.begin(&bg);
        p.fillRect(bg.rect(), br);
        p.end();
        m_frameImage->setPixmap(bg);
    } else {
        m_frameImage->setPixmap(m_render->extractFrame((int) m_render->seekPosition().frames(m_render->fps()), m_frameWidth / 2, m_frameHeight / 2));
    }
}

void TitleWidget::initViewports() {
    startViewport = new QGraphicsPolygonItem(QPolygonF(QRectF(0, 0, 0, 0)));
    endViewport = new QGraphicsPolygonItem(QPolygonF(QRectF(0, 0, 0, 0)));

    QPen startpen(Qt::DotLine);
    QPen endpen(Qt::DashDotLine);
    startpen.setColor(QColor(100, 200, 100, 140));
    endpen.setColor(QColor(200, 100, 100, 140));

    startViewport->setPen(startpen);
    endViewport->setPen(endpen);

    startViewportSize->setValue(40);
    endViewportSize->setValue(40);

    startViewport->setZValue(-1000);
    endViewport->setZValue(-1000);

    startViewport->setFlags(/*QGraphicsItem::ItemIsMovable|*/QGraphicsItem::ItemIsSelectable);
    endViewport->setFlags(/*QGraphicsItem::ItemIsMovable|*/QGraphicsItem::ItemIsSelectable);

    graphicsView->scene()->addItem(startViewport);
    graphicsView->scene()->addItem(endViewport);
}

void TitleWidget::slotUpdateZoom(int pos) {
    m_scene->setZoom((double) pos / 7);
    zoom_label->setText("x" + QString::number((double) pos / 7, 'g', 2));
}

void TitleWidget::slotZoom(bool up) {
    int pos = zoom_slider->value();
    if (up) pos++;
    else pos--;
    zoom_slider->setValue(pos);
}

void TitleWidget::slotAdjustZoom() {
    double scalex = graphicsView->width() / (double)(m_frameWidth * 1.2);
    double scaley = graphicsView->height() / (double)(m_frameHeight * 1.2);
    if (scalex > scaley) scalex = scaley;
    int zoompos = (int)(scalex * 7 + 0.5);
    zoom_slider->setValue(zoompos);
    graphicsView->centerOn(m_frameBorder);
}

void TitleWidget::slotZoomOneToOne() {
    zoom_slider->setValue(7);
    graphicsView->centerOn(m_frameBorder);
}

void TitleWidget::slotNewRect(QGraphicsRectItem * rect) {
    QColor f = rectFColor->color();
    f.setAlpha(rectFAlpha->value());
    QPen penf(f);
    penf.setWidth(rectLineWidth->value());
    rect->setPen(penf);
    QColor b = rectBColor->color();
    b.setAlpha(rectBAlpha->value());
    rect->setBrush(QBrush(b));
    rect->setZValue(m_count++);
    //setCurrentItem(rect);
    //graphicsView->setFocus();
}

void TitleWidget::slotNewText(QGraphicsTextItem *tt) {
    QFont font = font_family->currentFont();
    font.setPointSize(font_size->value());
    tt->setFont(font);
    QColor color = fontColorButton->color();
    color.setAlpha(textAlpha->value());
    tt->setDefaultTextColor(color);
    tt->setZValue(m_count++);
    setCurrentItem(tt);
}

void TitleWidget::setCurrentItem(QGraphicsItem *item) {
    m_scene->setSelectedItem(item);
}

void TitleWidget::zIndexChanged(int v) {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        l[0]->setZValue(v);
    }
}

void TitleWidget::selectionChanged() {
    if (m_scene->tool() != TITLE_SELECT) return;
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    //toolBox->setItemEnabled(2, false);
    //toolBox->setItemEnabled(3, false);
    value_x->blockSignals(true);
    value_y->blockSignals(true);
    value_w->blockSignals(true);
    value_h->blockSignals(true);
    kDebug() << "////////  SELECTION CHANGED; ITEMS: " << l.size();
    if (l.size() == 1) {
        if ((l[0])->type() == 8) {
            rect_properties->setHidden(true);
            text_properties->setHidden(false);
            QGraphicsTextItem* i = ((QGraphicsTextItem*)l[0]);
            //if (l[0]->hasFocus())
            //ktextedit->setHtml(i->toHtml());
            toolBox->setCurrentIndex(0);
            //toolBox->setItemEnabled(2, true);
            font_size->blockSignals(true);
            font_family->blockSignals(true);
            buttonBold->blockSignals(true);
            buttonItalic->blockSignals(true);
            buttonUnder->blockSignals(true);
            fontColorButton->blockSignals(true);
            textAlpha->blockSignals(true);

            QFont font = i->font();
            font_family->setCurrentFont(font);
            font_size->setValue(font.pointSize());
            buttonBold->setChecked(font.bold());
            buttonItalic->setChecked(font.italic());
            buttonUnder->setChecked(font.underline());

            QColor color = i->defaultTextColor();
            fontColorButton->setColor(color);
            textAlpha->setValue(color.alpha());

            font_size->blockSignals(false);
            font_family->blockSignals(false);
            buttonBold->blockSignals(false);
            buttonItalic->blockSignals(false);
            buttonUnder->blockSignals(false);
            fontColorButton->blockSignals(false);
            textAlpha->blockSignals(false);

            value_x->setValue((int) i->pos().x());
            value_y->setValue((int) i->pos().y());
            value_w->setValue((int) i->boundingRect().width());
            value_h->setValue((int) i->boundingRect().height());
            frame_properties->setEnabled(true);
            value_w->setEnabled(false);
            value_h->setEnabled(false);
        } else if ((l[0])->type() == 3) {
            rect_properties->setHidden(false);
            text_properties->setHidden(true);
            settingUp = true;
            QGraphicsRectItem *rec = ((QGraphicsRectItem*)l[0]);
            toolBox->setCurrentIndex(0);
            //toolBox->setItemEnabled(3, true);
            rectFAlpha->setValue(rec->pen().color().alpha());
            rectBAlpha->setValue(rec->brush().color().alpha());
            kDebug() << rec->brush().color().alpha();
            QColor fcol = rec->pen().color();
            QColor bcol = rec->brush().color();
            //fcol.setAlpha(255);
            //bcol.setAlpha(255);
            rectFColor->setColor(fcol);
            rectBColor->setColor(bcol);
            settingUp = false;
            rectLineWidth->setValue(rec->pen().width());
            value_x->setValue((int) rec->pos().x());
            value_y->setValue((int) rec->pos().y());
            value_w->setValue((int) rec->rect().width());
            value_h->setValue((int) rec->rect().height());
            frame_properties->setEnabled(true);
            value_w->setEnabled(true);
            value_h->setEnabled(true);
        } else {
            //toolBox->setCurrentIndex(0);
            frame_properties->setEnabled(false);
        }
        zValue->setValue((int)l[0]->zValue());
        itemzoom->setValue((int)(transformations[l[0]].scalex * 100));
        itemrotate->setValue((int)(transformations[l[0]].rotate));
        value_x->blockSignals(false);
        value_y->blockSignals(false);
        value_w->blockSignals(false);
        value_h->blockSignals(false);
    } else frame_properties->setEnabled(false);
}

void TitleWidget::slotAdjustSelectedItem() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        if (l[0]->type() == 3) {
            //rect item
            QGraphicsRectItem *rec = ((QGraphicsRectItem*)l[0]);
            rec->setPos(value_x->value(), value_y->value());
            rec->setRect(QRect(0, 0, value_w->value(), value_h->value()));
        } else if (l[0]->type() == 8) {
            //text item
            QGraphicsTextItem *rec = ((QGraphicsTextItem*)l[0]);
            rec->setPos(value_x->value(), value_y->value());
        }
    }
}

void TitleWidget::slotChangeBackground() {
    QColor color = kcolorbutton->color();
    color.setAlpha(horizontalSlider->value());
    m_frameBorder->setBrush(QBrush(color));
}

void TitleWidget::textChanged() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1 && (l[0])->type() == 8 && !l[0]->hasFocus()) {
        //kDebug() << ktextedit->document()->toHtml();
        //((QGraphicsTextItem*)l[0])->setHtml(ktextedit->toHtml());
    }
}

void TitleWidget::slotUpdateText() {
    QFont font = font_family->currentFont();
    font.setPointSize(font_size->value());
    font.setBold(buttonBold->isChecked());
    font.setItalic(buttonItalic->isChecked());
    font.setUnderline(buttonUnder->isChecked());
    QColor color = fontColorButton->color();
    color.setAlpha(textAlpha->value());

    QGraphicsTextItem* item = NULL;
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1 && (l[0])->type() == 8) {
        item = (QGraphicsTextItem*)l[0];
    }
    if (!item) return;
    //if (item->textCursor().selection ().isEmpty())
    {
        item->setFont(font);
        item->setDefaultTextColor(color);
    }
    /*else {
    QTextDocumentFragment selec = item->textCursor().selection ();
    selec.set
    }*/
    //if (ktextedit->textCursor().selectedText().isEmpty()) ktextedit->selectAll();

    //ktextedit->setCurrentFont(font);
    //ktextedit->setTextColor(color);
    /*QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1 && (l[0])->type() == 8 && l[0]->hasFocus()) {
    QGraphicsTextItem* item = static_cast <QGraphicsTextItem*> (l[0]);
    //item-
    }*/
}

void TitleWidget::rectChanged() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1 && (l[0])->type() == 3 && !settingUp) {
        QGraphicsRectItem *rec = (QGraphicsRectItem*)l[0];
        QColor f = rectFColor->color();
        f.setAlpha(rectFAlpha->value());
        QPen penf(f);
        penf.setWidth(rectLineWidth->value());
        rec->setPen(penf);
        QColor b = rectBColor->color();
        b.setAlpha(rectBAlpha->value());
        rec->setBrush(QBrush(b));
    }
}

void TitleWidget::fontBold() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1 && (l[0])->type() == 8 && !l[0]->hasFocus()) {
        //ktextedit->document()->setTextOption();
    }
}

void TitleWidget::itemScaled(int val) {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        Transform x = transformations[l[0]];
        x.scalex = (double)val / 100.0;
        x.scaley = (double)val / 100.0;
        QTransform qtrans;
        qtrans.scale(x.scalex, x.scaley);
        qtrans.rotate(x.rotate);
        l[0]->setTransform(qtrans);
        transformations[l[0]] = x;
    }
}

void TitleWidget::itemRotate(int val) {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        Transform x = transformations[l[0]];
        x.rotate = (double)val;
        QTransform qtrans;
        qtrans.scale(x.scalex, x.scaley);
        qtrans.rotate(x.rotate);
        l[0]->setTransform(qtrans);
        transformations[l[0]] = x;
    }
}

void TitleWidget::itemHCenter() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l[0];
        QRectF br;
        if (item->type() == 3) {
            br = ((QGraphicsRectItem*)item)->rect();
        } else br = item->boundingRect();
        int width = (int) br.width();
        int newPos = (int)((m_frameWidth - width) / 2);
        item->setPos(newPos, item->pos().y());
    }
}

void TitleWidget::itemVCenter() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1) {
        QGraphicsItem *item = l[0];
        QRectF br;
        if (item->type() == 3) {
            br = ((QGraphicsRectItem*)item)->rect();
        } else br = item->boundingRect();
        int height = (int) br.height();
        int newPos = (int)((m_frameHeight - height) / 2);
        item->setPos(item->pos().x(), newPos);
    }
}

void TitleWidget::setupViewports() {
    double aspect_ratio = 4.0 / 3.0;//read from project

    QRectF sp(0, 0, 0, 0);
    QRectF ep(0, 0, 0, 0);

    double sv_size = startViewportSize->value();
    double ev_size = endViewportSize->value();
    sp.adjust(-sv_size, -sv_size / aspect_ratio, sv_size, sv_size / aspect_ratio);
    ep.adjust(-ev_size, -ev_size / aspect_ratio, ev_size, ev_size / aspect_ratio);

    startViewport->setPos(startViewportX->value(), startViewportY->value());
    endViewport->setPos(endViewportX->value(), endViewportY->value());

    startViewport->setPolygon(QPolygonF(sp));
    endViewport->setPolygon(QPolygonF(ep));

}

void TitleWidget::loadTitle() {
    KUrl url = KFileDialog::getOpenUrl(KUrl(m_projectPath), "*.kdenlivetitle", this, i18n("Load Title"));
    if (!url.isEmpty()) {
        QList<QGraphicsItem *> items = m_scene->items();
        for (int i = 0; i < items.size(); i++) {
            if (items.at(i)->zValue() > -1000) delete items.at(i);
        }
        m_count = m_titledocument.loadDocument(url, startViewport, endViewport) + 1;
        slotSelectTool();
    }
}

void TitleWidget::saveTitle(KUrl url) {
    if (url.isEmpty()) url = KFileDialog::getSaveUrl(KUrl(m_projectPath), "*.kdenlivetitle", this, i18n("Save Title"));
    if (!url.isEmpty()) m_titledocument.saveDocument(url, startViewport, endViewport);
}

QPixmap TitleWidget::renderedPixmap() {
    QPixmap pix(m_frameWidth, m_frameHeight);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);

    m_scene->clearSelection();
    QPen framepen = m_frameBorder->pen();
    m_frameBorder->setPen(Qt::NoPen);
    startViewport->setVisible(false);
    endViewport->setVisible(false);
    m_frameImage->setVisible(false);

    m_scene->render(&painter, QRectF(), QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_frameBorder->setPen(framepen);
    startViewport->setVisible(true);
    endViewport->setVisible(true);
    m_frameImage->setVisible(true);
    return pix;
}

#include "moc_titlewidget.cpp"

