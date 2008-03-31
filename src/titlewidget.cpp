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
#include <QTextDocumentFragment>
#include <QTimer>

#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>

#include "titlewidget.h"
#include "kdenlivesettings.h"

int settingUp = false;

TitleWidget::TitleWidget(Render *render, QWidget *parent): QDialog(parent) {
    setupUi(this);
    frame_properties->setFont(KGlobalSettings::toolBarFont());
    toolBox->setFont(KGlobalSettings::toolBarFont());
    frame_properties->setEnabled(false);
    connect(newTextButton, SIGNAL(clicked()), this, SLOT(slotNewText()));
    connect(newRectButton, SIGNAL(clicked()), this, SLOT(slotNewRect()));
    connect(kcolorbutton, SIGNAL(clicked()), this, SLOT(slotChangeBackground())) ;
    connect(horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(slotChangeBackground())) ;
    connect(ktextedit, SIGNAL(textChanged()), this , SLOT(textChanged()));
    //connect (fontBold, SIGNAL ( clicked()), this, SLOT( setBold()) ) ;
    connect(loadButton, SIGNAL(clicked()), this, SLOT(loadTitle())) ;
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveTitle())) ;

    ktextedit->setHidden(true);
    connect(fontColorButton, SIGNAL(clicked()), this, SLOT(slotUpdateText())) ;
    connect(font_family, SIGNAL(currentFontChanged (const QFont &)), this, SLOT(slotUpdateText())) ;
    connect(font_size, SIGNAL(valueChanged (int)), this, SLOT(slotUpdateText())) ;
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
    connect(svgfilename, SIGNAL(urlSelected(const KUrl&)), this, SLOT(svgSelected(const KUrl &)));
    connect(itemzoom, SIGNAL(valueChanged(int)), this, SLOT(itemScaled(int)));
    connect(itemrotate, SIGNAL(valueChanged(int)), this, SLOT(itemRotate(int)));

    connect(value_x, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(value_y, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(value_w, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(value_h, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustSelectedItem()));
    connect(buttonFitZoom, SIGNAL(clicked()), this, SLOT(slotAdjustZoom()));
    connect(buttonRealSize, SIGNAL(clicked()), this, SLOT(slotZoomOneToOne()));

    buttonFitZoom->setIcon(KIcon("zoom-fit-best"));
    buttonRealSize->setIcon(KIcon("zoom-original"));
    m_scene = new GraphicsSceneRectMove(this);

    // a gradient background
    QRadialGradient *gradient = new QRadialGradient(0, 0, 10);
    gradient->setSpread(QGradient::ReflectSpread);
    //scene->setBackgroundBrush(*gradient);


    graphicsView->setScene(m_scene);
    m_titledocument.setScene(m_scene);
    connect(graphicsView->scene(), SIGNAL(selectionChanged()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(itemMoved()), this , SLOT(selectionChanged()));
    connect(m_scene, SIGNAL(sceneZoom(bool)), this , SLOT(slotZoom(bool)));
    connect(zoom_slider, SIGNAL(valueChanged(int)), this , SLOT(slotUpdateZoom(int)));


    QPen framepen(Qt::DotLine);
    framepen.setColor(Qt::red);
    m_frameWidth = render->renderWidth();
    m_frameHeight = render->renderHeight();
    QPixmap bg = render->extractFrame((int) render->seekPosition().frames(render->fps()), m_frameWidth, m_frameHeight);

    QGraphicsPixmapItem *fb = new QGraphicsPixmapItem(bg);
    fb->setZValue(-1000);
    fb->setFlags(QGraphicsItem::ItemClipsToShape);
    graphicsView->scene()->addItem(fb);

    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_frameWidth, m_frameHeight));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(-1000);
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
    toolBox->setItemEnabled(3, false);
}


//virtual
void TitleWidget::resizeEvent(QResizeEvent * event) {
    //slotAdjustZoom();
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

void TitleWidget::slotNewRect() {
    QGraphicsRectItem * ri = graphicsView->scene()->addRect(0, 0, 100, 100);
    ri->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    ri->setPos(m_frameWidth / 2, m_frameHeight / 2);
    QColor f = rectFColor->color();
    f.setAlpha(rectFAlpha->value());
    QPen penf(f);
    penf.setWidth(rectLineWidth->value());
    ri->setPen(penf);
    QColor b = rectBColor->color();
    b.setAlpha(rectBAlpha->value());
    ri->setBrush(QBrush(b));
}

void TitleWidget::slotNewText() {
    QGraphicsTextItem *tt = graphicsView->scene()->addText("Text here");
    tt->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    tt->setTextInteractionFlags(Qt::NoTextInteraction);
    tt->setPos(m_frameWidth / 2, m_frameHeight / 2);
    QFont font = font_family->currentFont();
    font.setPointSize (font_size->value());
    tt->setFont(font);
    connect(tt->document(), SIGNAL(contentsChanged()), this, SLOT(selectionChanged()));
    kDebug() << tt->metaObject()->className();
    /*QGraphicsRectItem * ri=graphicsView->scene()->addRect(-50,-50,100,100);
    ri->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);*/

}

void TitleWidget::zIndexChanged(int v) {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() >= 1) {
        l[0]->setZValue(v);
    }
}

void TitleWidget::selectionChanged() {
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    toolBox->setItemEnabled(2, false);
    toolBox->setItemEnabled(3, false);
    value_x->blockSignals(true);
    value_y->blockSignals(true);
    value_w->blockSignals(true);
    value_h->blockSignals(true);
    if (l.size() == 1) {
        if ((l[0])->type() == 8) {
            QGraphicsTextItem* i = ((QGraphicsTextItem*)l[0]);
            if (l[0]->hasFocus())
                ktextedit->setHtml(i->toHtml());
            toolBox->setCurrentIndex(2);
            toolBox->setItemEnabled(2, true);
            value_x->setValue((int) i->pos().x());
            value_y->setValue((int) i->pos().y());
            value_w->setValue((int) i->boundingRect().width());
            value_h->setValue((int) i->boundingRect().height());
            frame_properties->setEnabled(true);
            value_w->setEnabled(false);
            value_h->setEnabled(false);
        } else
            if ((l[0])->type() == 3) {
                settingUp = true;
                QGraphicsRectItem *rec = ((QGraphicsRectItem*)l[0]);
                toolBox->setCurrentIndex(3);
                toolBox->setItemEnabled(3, true);
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
        itemzoom->setValue((int)transformations[l[0]].scalex*100);
        itemrotate->setValue((int)transformations[l[0]].rotate);
        value_x->blockSignals(false);
        value_y->blockSignals(false);
        value_w->blockSignals(false);
        value_h->blockSignals(false);
    }
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
        kDebug() << ktextedit->document()->toHtml();
        ((QGraphicsTextItem*)l[0])->setHtml(ktextedit->toHtml());
    }
}

void TitleWidget::slotUpdateText() {
    QFont font = font_family->currentFont();
    font.setPointSize (font_size->value());
    QColor color = fontColorButton->color();
    color.setAlpha(textAlpha->value());

    QGraphicsTextItem* item = NULL;
    QList<QGraphicsItem*> l = graphicsView->scene()->selectedItems();
    if (l.size() == 1 && (l[0])->type() == 8) {
	item = (QGraphicsTextItem*)l[0];
    }
    if (!item) return;
    if (item->textCursor().selection ().isEmpty()) 
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

void TitleWidget::svgSelected(const KUrl& u) {
    QGraphicsSvgItem *svg = new QGraphicsSvgItem(u.toLocalFile());
    svg->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    graphicsView->scene()->addItem(svg);
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
    KUrl url = KFileDialog::getOpenUrl(KUrl(), "*.kdenlivetitle", this, tr("Save Title"));
    m_titledocument.loadDocument(url, startViewport, endViewport);
}

void TitleWidget::saveTitle() {
    KUrl url = KFileDialog::getSaveUrl(KUrl(), "*.kdenlivetitle", this, tr("Save Title"));
    m_titledocument.saveDocument(url, startViewport, endViewport);
}

#include "moc_titlewidget.cpp"

