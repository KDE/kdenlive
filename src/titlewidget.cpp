#include "titlewidget.h"
#include "graphicsscenerectmove.h"
#include <QGraphicsView>
#include <KDebug>
#include <QGraphicsItem>

TitleWidget::TitleWidget (QDialog *parent):QDialog(parent){
	setupUi(this);
	connect (newTextButton,SIGNAL(clicked()), this, SLOT( slotNewText()));
	connect (newRectButton,SIGNAL(clicked()), this, SLOT( slotNewRect()));
	connect (kcolorbutton, SIGNAL ( clicked()), this, SLOT( slotChangeBackground()) ) ;
	connect (horizontalSlider, SIGNAL ( valueChanged(int) ), this, SLOT( slotChangeBackground()) ) ;
	connect (ktextedit, SIGNAL(textChanged()), this , SLOT (textChanged()));
	
	GraphicsSceneRectMove *scene=new GraphicsSceneRectMove(this);
	
	
	
 // a gradient background
	QRadialGradient *gradient=new QRadialGradient(0, 0, 10);
	gradient->setSpread(QGradient::RepeatSpread);
	//scene->setBackgroundBrush(*gradient);
	
	graphicsView->setScene(scene);
	connect (graphicsView->scene(), SIGNAL (selectionChanged()), this , SLOT( selectionChanged()));
	
	graphicsView->show();
	graphicsView->setRenderHint(QPainter::Antialiasing);
	//graphicsView->setBackgroundBrush(QPixmap(":/images/cheese.jpg"));
	//graphicsView->setCacheMode(QGraphicsView::CacheBackground);
	//graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
	graphicsView->setInteractive(true);
	graphicsView->setWindowTitle(QT_TRANSLATE_NOOP(QGraphicsView, "Colliding Mice"));
	graphicsView->resize(400, 300);
	
	update();
}

void TitleWidget::slotNewRect(){
	
	QGraphicsRectItem * ri=graphicsView->scene()->addRect(-50,-50,100,100);
	ri->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);

}
void TitleWidget::slotNewText(){
	QGraphicsTextItem *tt=graphicsView->scene()->addText("Text here");
	tt->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);
	tt->setTextInteractionFlags (Qt::TextEditorInteraction);
	kDebug() << tt->metaObject()->className();
	/*QGraphicsRectItem * ri=graphicsView->scene()->addRect(-50,-50,100,100);
	ri->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);*/
	
}

void TitleWidget::selectionChanged(){
	QList<QGraphicsItem*> l=graphicsView->scene()->selectedItems();
	if (l.size()>0){
		kDebug() << (l[0])->type();
		if ((l[0])->type()==8){
			ktextedit->setHtml(((QGraphicsTextItem*)l[0])->toHtml());
			toolBox->setCurrentIndex(1);
		}else
		if ((l[0])->type()==3){
			
			toolBox->setCurrentIndex(2);
		}else{
			toolBox->setCurrentIndex(0);
		}
	}
}

void TitleWidget::slotChangeBackground(){
	QColor color=kcolorbutton->color();
	color.setAlpha(horizontalSlider->value());
	graphicsView->scene()->setBackgroundBrush(QBrush(color));
}

void TitleWidget::textChanged(){
	QList<QGraphicsItem*> l=graphicsView->scene()->selectedItems();
	if (l.size()>0 && (l[0])->type()==8 /*textitem*/){
		
		((QGraphicsTextItem*)l[0])->setHtml(ktextedit->toHtml());
	}
}


#include "moc_titlewidget.cpp"

