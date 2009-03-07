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
#include <QDomElement>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsSvgItem>
#include <KDebug>
#include <QFile>
#include <KTemporaryFile>
#include <kio/netaccess.h>

TitleDocument::TitleDocument() {
    scene = NULL;
}

void TitleDocument::setScene(QGraphicsScene* _scene) {
    scene = _scene;
}

QDomDocument TitleDocument::xml(QGraphicsPolygonItem* startv, QGraphicsPolygonItem* endv) {
    QDomDocument doc;

    QDomElement main = doc.createElement("kdenlivetitle");
    doc.appendChild(main);

    foreach(QGraphicsItem* item, scene->items()) {
        QDomElement e = doc.createElement("item");
        QDomElement content = doc.createElement("content");
        QFont font;
        QGraphicsTextItem *t;

        switch (item->type()) {
        case 7:
            e.setAttribute("type", "QGraphicsPixmapItem");
            content.setAttribute("url", item->data(Qt::UserRole).toString());
            break;
        case 13:
            e.setAttribute("type", "QGraphicsSvgItem");
            content.setAttribute("url", item->data(Qt::UserRole).toString());
            break;
        case 3:
            e.setAttribute("type", "QGraphicsRectItem");
            content.setAttribute("rect", rectFToString(((QGraphicsRectItem*)item)->rect()));
            content.setAttribute("pencolor", colorToString(((QGraphicsRectItem*)item)->pen().color()));
            content.setAttribute("penwidth", ((QGraphicsRectItem*)item)->pen().width());
            content.setAttribute("brushcolor", colorToString(((QGraphicsRectItem*)item)->brush().color()));
            break;
        case 8:
            e.setAttribute("type", "QGraphicsTextItem");
            t = static_cast<QGraphicsTextItem *>(item);
            //content.appendChild(doc.createTextNode(((QGraphicsTextItem*)item)->toHtml()));
            content.appendChild(doc.createTextNode(t->toPlainText()));
            font = t->font();
            content.setAttribute("font", font.family());
            content.setAttribute("font-bold", font.bold());
            content.setAttribute("font-size", font.pointSize());
            content.setAttribute("font-italic", font.italic());
            content.setAttribute("font-underline", font.underline());
            content.setAttribute("font-color", colorToString(t->defaultTextColor()));
            break;
        default:
            continue;
        }
        QDomElement pos = doc.createElement("position");
        pos.setAttribute("x", item->pos().x());
        pos.setAttribute("y", item->pos().y());
        QTransform transform = item->transform();
        QDomElement tr = doc.createElement("transform");
        tr.appendChild(doc.createTextNode(
                           QString("%1,%2,%3,%4,%5,%6,%7,%8,%9").arg(
                               transform.m11()).arg(transform.m12()).arg(transform.m13()).arg(transform.m21()).arg(transform.m22()).arg(transform.m23()).arg(transform.m31()).arg(transform.m32()).arg(transform.m33())
                       )
                      );
        e.setAttribute("z-index", item->zValue());
        pos.appendChild(tr);


        e.appendChild(pos);
        e.appendChild(content);
        if (item->zValue() > -1100) main.appendChild(e);
    }
    if (startv && endv) {
        QDomElement endp = doc.createElement("endviewport");
        QDomElement startp = doc.createElement("startviewport");
        endp.setAttribute("x", endv->pos().x());
        endp.setAttribute("y", endv->pos().y());
        endp.setAttribute("size", endv->sceneBoundingRect().width() / 2);

        startp.setAttribute("x", startv->pos().x());
        startp.setAttribute("y", startv->pos().y());
        startp.setAttribute("size", startv->sceneBoundingRect().width() / 2);

        startp.setAttribute("z-index", startv->zValue());
        endp.setAttribute("z-index", endv->zValue());
        main.appendChild(startp);
        main.appendChild(endp);
    }
    QDomElement backgr = doc.createElement("background");
    QColor color = getBackgroundColor();
    backgr.setAttribute("color", colorToString(color));
    main.appendChild(backgr);

    return doc;
}

/** \brief Get the background color (incl. alpha) from the document, if possibly
  * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
QColor TitleDocument::getBackgroundColor() {
    QColor color(0, 0, 0, 0);
    if (scene) {
        QList<QGraphicsItem *> items = scene->items();
        for (int i = 0; i < items.size(); i++) {
            if (items.at(i)->zValue() == -1100) {
                color = ((QGraphicsRectItem *)items.at(i))->brush().color();
                return color;
            }
        }
    }
    return color;
}


bool TitleDocument::saveDocument(const KUrl& url, QGraphicsPolygonItem* startv, QGraphicsPolygonItem* endv) {
    if (!scene)
        return false;

    QDomDocument doc = xml(startv, endv);
    KTemporaryFile tmpfile;
    if (!tmpfile.open()) kWarning() << "/////  CANNOT CREATE TMP FILE in: " << tmpfile.fileName();
    QFile xmlf(tmpfile.fileName());
    xmlf.open(QIODevice::WriteOnly);
    xmlf.write(doc.toString().toUtf8());
    xmlf.close();
    kDebug() << KIO::NetAccess::upload(tmpfile.fileName(), url, 0);
    return true;
}

int TitleDocument::loadDocument(const KUrl& url, QGraphicsPolygonItem* startv, QGraphicsPolygonItem* endv) {
    QString tmpfile;
    QDomDocument doc;
    double aspect_ratio = 4.0 / 3.0;
    if (!scene)
        return -1;

    if (KIO::NetAccess::download(url, tmpfile, 0)) {
        QFile file(tmpfile);
        if (file.open(QIODevice::ReadOnly)) {
            doc.setContent(&file, false);
            file.close();
        } else
            return -1;
        KIO::NetAccess::removeTempFile(tmpfile);
        return loadFromXml(doc, startv, endv);
    }
    return -1;
}

int TitleDocument::loadFromXml(QDomDocument doc, QGraphicsPolygonItem* startv, QGraphicsPolygonItem* endv) {
    QDomNodeList titles = doc.elementsByTagName("kdenlivetitle");
    int maxZValue = 0;
    if (titles.size()) {

        QDomNodeList items = titles.item(0).childNodes();
        for (int i = 0;i < items.count();i++) {
            QGraphicsItem *gitem = NULL;
            kDebug() << items.item(i).attributes().namedItem("type").nodeValue();
            int zValue = items.item(i).attributes().namedItem("z-index").nodeValue().toInt();
            if (zValue > -1000)
                if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsTextItem") {
                    QFont font(items.item(i).namedItem("content").attributes().namedItem("font").nodeValue());
                    font.setBold(items.item(i).namedItem("content").attributes().namedItem("font-bold").nodeValue().toInt());
                    font.setItalic(items.item(i).namedItem("content").attributes().namedItem("font-italic").nodeValue().toInt());
                    font.setUnderline(items.item(i).namedItem("content").attributes().namedItem("font-underline").nodeValue().toInt());
                    font.setPointSize(items.item(i).namedItem("content").attributes().namedItem("font-size").nodeValue().toInt());
                    QColor col(stringToColor(items.item(i).namedItem("content").attributes().namedItem("font-color").nodeValue()));
                    QGraphicsTextItem *txt = scene->addText(items.item(i).namedItem("content").firstChild().nodeValue(), font);
                    txt->setDefaultTextColor(col);
                    txt->setTextInteractionFlags(Qt::NoTextInteraction);
                    gitem = txt;
                } else
                    if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsRectItem") {
                        QString rect = items.item(i).namedItem("content").attributes().namedItem("rect").nodeValue();
                        QString br_str = items.item(i).namedItem("content").attributes().namedItem("brushcolor").nodeValue();
                        QString pen_str = items.item(i).namedItem("content").attributes().namedItem("pencolor").nodeValue();
                        double penwidth = items.item(i).namedItem("content").attributes().namedItem("penwidth").nodeValue().toDouble();
                        QGraphicsRectItem *rec = scene->addRect(stringToRect(rect), QPen(QBrush(stringToColor(pen_str)), penwidth), QBrush(stringToColor(br_str)));
                        gitem = rec;
                    } else
                        if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsPixmapItem") {
                            QString url = items.item(i).namedItem("content").attributes().namedItem("url").nodeValue();
                            QPixmap pix(url);
                            QGraphicsPixmapItem *rec = scene->addPixmap(pix);
                            rec->setData(Qt::UserRole, url);
                            gitem = rec;
                        } else
                            if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsSvgItem") {
                                QString url = items.item(i).namedItem("content").attributes().namedItem("url").nodeValue();
                                QGraphicsSvgItem *rec = new QGraphicsSvgItem(url);
                                scene->addItem(rec);
                                rec->setData(Qt::UserRole, url);
                                gitem = rec;
                            }
            //pos and transform
            if (gitem) {
                QPointF p(items.item(i).namedItem("position").attributes().namedItem("x").nodeValue().toDouble(),
                          items.item(i).namedItem("position").attributes().namedItem("y").nodeValue().toDouble());
                gitem->setPos(p);
                gitem->setTransform(stringToTransform(items.item(i).namedItem("position").firstChild().firstChild().nodeValue()));
                int zValue = items.item(i).attributes().namedItem("z-index").nodeValue().toInt();
                if (zValue > maxZValue) maxZValue = zValue;
                gitem->setZValue(zValue);
                gitem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
            }
            if (items.item(i).nodeName() == "background") {
                kDebug() << items.item(i).attributes().namedItem("color").nodeValue();
                QColor color = QColor(stringToColor(items.item(i).attributes().namedItem("color").nodeValue()));
                //color.setAlpha(items.item(i).attributes().namedItem("alpha").nodeValue().toInt());
                QList<QGraphicsItem *> items = scene->items();
                for (int i = 0; i < items.size(); i++) {
                    if (items.at(i)->zValue() == -1100) {
                        ((QGraphicsRectItem *)items.at(i))->setBrush(QBrush(color));
                        break;
                    }
                }
            } /*else if (items.item(i).nodeName() == "startviewport" && startv) {
                    QPointF p(items.item(i).attributes().namedItem("x").nodeValue().toDouble(), items.item(i).attributes().namedItem("y").nodeValue().toDouble());
                    double width = items.item(i).attributes().namedItem("size").nodeValue().toDouble();
                    QRectF rect(-width, -width / aspect_ratio, width*2.0, width / aspect_ratio*2.0);
                    kDebug() << width << rect;
                    startv->setPolygon(rect);
                    startv->setPos(p);
                } else if (items.item(i).nodeName() == "endviewport" && endv) {
                    QPointF p(items.item(i).attributes().namedItem("x").nodeValue().toDouble(), items.item(i).attributes().namedItem("y").nodeValue().toDouble());
                    double width = items.item(i).attributes().namedItem("size").nodeValue().toDouble();
                    QRectF rect(-width, -width / aspect_ratio, width*2.0, width / aspect_ratio*2.0);
                    kDebug() << width << rect;
                    endv->setPolygon(rect);
                    endv->setPos(p);
                }*/
        }
    }
    return maxZValue;
}

QString TitleDocument::colorToString(const QColor& c) {
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

QString TitleDocument::rectFToString(const QRectF& c) {
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.x()).arg(c.y()).arg(c.width()).arg(c.height());
    return ret;
}

QRectF TitleDocument::stringToRect(const QString & s) {

    QStringList l = s.split(',');
    if (l.size() < 4)
        return QRectF();
    return QRectF(l[0].toDouble(), l[1].toDouble(), l[2].toDouble(), l[3].toDouble());
}

QColor TitleDocument::stringToColor(const QString & s) {
    QStringList l = s.split(',');
    if (l.size() < 4)
        return QColor();
    return QColor(l[0].toInt(), l[1].toInt(), l[2].toInt(), l[3].toInt());;
}
QTransform TitleDocument::stringToTransform(const QString& s) {
    QStringList l = s.split(',');
    if (l.size() < 9)
        return QTransform();
    return QTransform(
               l[0].toDouble(), l[1].toDouble(), l[2].toDouble(),
               l[3].toDouble(), l[4].toDouble(), l[5].toDouble(),
               l[6].toDouble(), l[7].toDouble(), l[8].toDouble()
           );
}
