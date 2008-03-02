/***************************************************************************
                          titledocument.h  -  description
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
#include "titledocument.h"
#include <QGraphicsScene>
#include <QDomDocument>
#include <QDomElement>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <KDebug>
#include <QFile>
#include <kio/netaccess.h>

TitleDocument::TitleDocument(){
	scene=NULL;	
}

void TitleDocument::setScene(QGraphicsScene* _scene){
	scene=_scene;
}

bool TitleDocument::saveDocument(const KUrl& url,QGraphicsPolygonItem* startv, QGraphicsPolygonItem* endv){
	QDomDocument doc;
	
	if (!scene)
		return false;
	
	QDomElement main=doc.createElement("kdenlivetitle");
	doc.appendChild(main);
	
	foreach(QGraphicsItem* item, scene->items()){
		QDomElement e=doc.createElement("item");
		QDomElement content=doc.createElement("content");

		switch (item->type()){
			case 3:
				e.setAttribute("type","QGraphicsRectItem");
				content.setAttribute("rect",rectFToString(((QGraphicsRectItem*)item)->rect() ));
				content.setAttribute("pencolor",colorToString(((QGraphicsRectItem*)item)->pen().color()) );
				content.setAttribute("penwidth",((QGraphicsRectItem*)item)->pen().width() );
				content.setAttribute("brushcolor",colorToString(((QGraphicsRectItem*)item)->brush().color()) );
				break;
			case 8:
				e.setAttribute("type","QGraphicsTextItem");
				content.appendChild(doc.createTextNode( ((QGraphicsTextItem*)item)->toHtml() ) );
				break;
			default:
				continue;
		}
		QDomElement pos=doc.createElement("position");
		pos.setAttribute("x",item->pos().x());
		pos.setAttribute("y",item->pos().y());
		QTransform transform=item->transform();
		QDomElement tr=doc.createElement("transform");
		tr.appendChild(doc.createTextNode(
		  QString("%1,%2,%3,%4,%5,%6,%7,%8,%9").arg(
		    transform.m11()).arg(transform.m12()).arg(transform.m13()).arg(transform.m21()).arg(transform.m22()).arg(transform.m23()).arg(transform.m31()).arg(transform.m32()).arg(transform.m33())
		  )
		);
		pos.appendChild(tr);
		
		
		e.appendChild(pos);
		e.appendChild(content);
		main.appendChild(e);
	}
	if (startv && endv){
		QDomElement endp=doc.createElement("endviewport");
		QDomElement startp=doc.createElement("startviewport");
		endp.setAttribute("x",endv->pos().x());
		endp.setAttribute("y",endv->pos().y());
		endp.setAttribute("size",endv->sceneBoundingRect().width()/2);
		
		startp.setAttribute("x",startv->pos().x());
		startp.setAttribute("y",startv->pos().y());
		startp.setAttribute("size",startv->sceneBoundingRect().width()/2);
		
		main.appendChild(startp);
		main.appendChild(endp);
	}
	QDomElement backgr=doc.createElement("background");
	backgr.setAttribute("color",colorToString(scene->backgroundBrush().color()));
	main.appendChild(backgr);
	
	QString tmpfile="/tmp/newtitle";
	QFile xmlf(tmpfile);
	xmlf.open(QIODevice::WriteOnly);
	xmlf.write(doc.toString().toAscii());
	xmlf.close();
	kDebug() << KIO::NetAccess::upload(tmpfile,url,0);
}

bool TitleDocument::loadDocument(const KUrl& url ,QGraphicsPolygonItem* startv, QGraphicsPolygonItem* endv) {
	QString tmpfile;
	QDomDocument doc;
	double aspect_ratio=4.0/3.0;
	if (!scene)
		return false;
	
	if (KIO::NetAccess::download(url, tmpfile, 0)) {
		QFile file(tmpfile);
		if (file.open(QIODevice::ReadOnly)) {
			doc.setContent(&file, false);
			file.close();
		}else
			return false;
		KIO::NetAccess::removeTempFile(tmpfile);
		QDomNodeList titles=doc.elementsByTagName("kdenlivetitle");
		if (titles.size()){
			
			QDomNodeList items=titles.item(0).childNodes();
			for(int i=0;i<items.count();i++){
				QGraphicsItem *gitem=NULL;
				kDebug() << items.item(i).attributes().namedItem("type").nodeValue();
				if (items.item(i).attributes().namedItem("type").nodeValue()=="QGraphicsTextItem"){
					QGraphicsTextItem *txt=scene->addText("");
					txt->setHtml(items.item(i).namedItem("content").firstChild().nodeValue());
					gitem=txt;
				}else
				if (items.item(i).attributes().namedItem("type").nodeValue()=="QGraphicsRectItem"){
					QString rect=items.item(i).namedItem("content").attributes().namedItem("rect").nodeValue();
					QString br_str=items.item(i).namedItem("content").attributes().namedItem("brushcolor").nodeValue();
					QString pen_str=items.item(i).namedItem("content").attributes().namedItem("pencolor").nodeValue();
					double penwidth=items.item(i).namedItem("content").attributes().namedItem("penwidth").nodeValue().toDouble();
					QGraphicsRectItem *rec=scene->addRect(stringToRect(rect),QPen(QBrush(stringToColor(pen_str)),penwidth),QBrush(stringToColor(br_str) ) );
					gitem=rec;
				}
				//pos and transform
				if (gitem ){
					QPointF p(items.item(i).namedItem("position").attributes().namedItem("x").nodeValue().toDouble(),
						items.item(i).namedItem("position").attributes().namedItem("y").nodeValue().toDouble());
					gitem->setPos(p);
					gitem->setTransform(stringToTransform(items.item(i).namedItem("position").firstChild().firstChild().nodeValue()));
				}
				if (items.item(i).nodeName()=="background"){
					kDebug() << items.item(i).attributes().namedItem("color").nodeValue();
					scene->setBackgroundBrush(QBrush(stringToColor(items.item(i).attributes().namedItem("color").nodeValue())));
				}else	if (items.item(i).nodeName()=="startviewport" && startv){
					QPointF p(items.item(i).attributes().namedItem("x").nodeValue().toDouble(),items.item(i).attributes().namedItem("y").nodeValue().toDouble());
					double width=items.item(i).attributes().namedItem("size").nodeValue().toDouble();
					QRectF rect(-width,-width/aspect_ratio,width*2.0,width/aspect_ratio*2.0);
					kDebug() << width << rect;
					startv->setPolygon(rect);
					startv->setPos(p);
				}else	if (items.item(i).nodeName()=="endviewport" && endv){
					QPointF p(items.item(i).attributes().namedItem("x").nodeValue().toDouble(),items.item(i).attributes().namedItem("y").nodeValue().toDouble());
					double width=items.item(i).attributes().namedItem("size").nodeValue().toDouble();
					QRectF rect(-width,-width/aspect_ratio,width*2.0,width/aspect_ratio*2.0);
					kDebug() << width << rect;
					endv->setPolygon(rect);
					endv->setPos(p);
				}
				
			}
			
			
		}
		
		
	}
	return true;
}

QString TitleDocument::colorToString(const QColor& c){
	QString ret="%1,%2,%3,%4";
	ret=ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
	return ret;
}

QString TitleDocument::rectFToString(const QRectF& c){
	QString ret="%1,%2,%3,%4";
	ret=ret.arg(c.x()).arg(c.y()).arg(c.width()).arg(c.height());
	return ret;
}

QRectF TitleDocument::stringToRect(const QString & s){
	
	QStringList l=s.split(",");
	if (l.size()<4)
		return QRectF();
	return QRectF(l[0].toDouble(),l[1].toDouble(),l[2].toDouble(),l[3].toDouble());
}

QColor TitleDocument::stringToColor(const QString & s){
	QStringList l=s.split(",");
	if (l.size()<4)
		return QColor();
	return QColor(l[0].toInt(),l[1].toInt(),l[2].toInt(),l[3].toInt());;
}
QTransform TitleDocument::stringToTransform(const QString& s){
	QStringList l=s.split(",");
	if (l.size()<9)
		return QTransform();
	return QTransform(
		l[0].toDouble(),l[1].toDouble(),l[2].toDouble(),
		l[3].toDouble(),l[4].toDouble(),l[5].toDouble(),
		l[6].toDouble(),l[7].toDouble(),l[8].toDouble()
		);
}
