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

#include <KDebug>
#include <KTemporaryFile>
#include <kio/netaccess.h>
#include <KApplication>
#include <KStandardDirs>
#include <KLocale>
#include <KMessageBox>

#include <QGraphicsScene>
#include <QDomElement>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsSvgItem>
#include <QCryptographicHash>
#include <QSvgRenderer>
#include <QFontInfo>
#include <QFile>
#include <QTextCursor>

#include <locale.h>

#if QT_VERSION >= 0x040600
#include <QGraphicsEffect>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#endif

QByteArray fileToByteArray(const QString& filename)
{
    QByteArray ret;
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            ret.append(file.readLine());
        }
    }
    return ret;
}

TitleDocument::TitleDocument()
{
    m_scene = NULL;
    m_width = 0;
    m_height = 0;
}

void TitleDocument::setScene(QGraphicsScene* _scene, int width, int height)
{
    m_scene = _scene;
    m_width = width;
    m_height = height;
}

int TitleDocument::base64ToUrl(QGraphicsItem* item, QDomElement& content, bool embed)
{
    if (embed) {
        if (!item->data(Qt::UserRole + 1).toString().isEmpty()) {
            content.setAttribute("base64", item->data(Qt::UserRole + 1).toString());
        } else if (!item->data(Qt::UserRole).toString().isEmpty()) {
            content.setAttribute("base64", fileToByteArray(item->data(Qt::UserRole).toString()).toBase64().data());
        }
        content.removeAttribute("url");
    } else {
        // save for project files to disk
        QString base64 = item->data(Qt::UserRole + 1).toString();
        if (!base64.isEmpty()) {
            QString titlePath;
            if (!m_projectPath.isEmpty()) {
                titlePath = m_projectPath;
            } else {
                titlePath = "/tmp/titles";
            }
            QString filename = extractBase64Image(titlePath, base64);
            if (!filename.isEmpty()) {
                content.setAttribute("url", filename);
                content.removeAttribute("base64");
            }

        } else {
            return 1;
        }
    }
    return 0;
}


//static
const QString TitleDocument::extractBase64Image(const QString &titlePath, const QString &data)
{
    QString filename = titlePath + QString(QCryptographicHash::hash(data.toAscii(), QCryptographicHash::Md5).toHex().append(".titlepart"));
    KStandardDirs::makeDir(titlePath);
    QFile f(filename);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QByteArray::fromBase64(data.toAscii())) ;
        f.close();
        return filename;
    }
    return QString();
}

QDomDocument TitleDocument::xml(QGraphicsRectItem* startv, QGraphicsRectItem* endv, bool embed)
{
    QDomDocument doc;

    QDomElement main = doc.createElement("kdenlivetitle");
    main.setAttribute("width", m_width);
    main.setAttribute("height", m_height);
    // Save locale
    const char *locale = setlocale(LC_NUMERIC, NULL);
    main.setAttribute("LC_NUMERIC", locale);
    doc.appendChild(main);

    foreach(QGraphicsItem * item, m_scene->items()) {
        QDomElement e = doc.createElement("item");
        QDomElement content = doc.createElement("content");
        QFont font;
        QGraphicsTextItem *t;

        switch (item->type()) {
        case 7:
            e.setAttribute("type", "QGraphicsPixmapItem");
            content.setAttribute("url", item->data(Qt::UserRole).toString());
            base64ToUrl(item, content, embed);
            break;
        case 13:
            e.setAttribute("type", "QGraphicsSvgItem");
            content.setAttribute("url", item->data(Qt::UserRole).toString());
            base64ToUrl(item, content, embed);
            break;
        case 3:
            e.setAttribute("type", "QGraphicsRectItem");
            content.setAttribute("rect", rectFToString(((QGraphicsRectItem*)item)->rect().normalized()));
            content.setAttribute("pencolor", colorToString(((QGraphicsRectItem*)item)->pen().color()));
            content.setAttribute("penwidth", ((QGraphicsRectItem*)item)->pen().width());
            content.setAttribute("brushcolor", colorToString(((QGraphicsRectItem*)item)->brush().color()));
            break;
        case 8:
            e.setAttribute("type", "QGraphicsTextItem");
            t = static_cast<QGraphicsTextItem *>(item);
            // Don't save empty text nodes
            if (t->toPlainText().simplified().isEmpty()) continue;
            //content.appendChild(doc.createTextNode(((QGraphicsTextItem*)item)->toHtml()));
            content.appendChild(doc.createTextNode(t->toPlainText()));
            font = t->font();
            content.setAttribute("font", font.family());
            content.setAttribute("font-weight", font.weight());
            content.setAttribute("font-pixel-size", font.pixelSize());
            content.setAttribute("font-italic", font.italic());
            content.setAttribute("font-underline", font.underline());
            {
                QTextCursor cursor(t->document());
                cursor.select(QTextCursor::Document);
                QColor fontcolor = cursor.charFormat().foreground().color();
                content.setAttribute("font-color", colorToString(fontcolor));
                if (!t->data(101).isNull()) content.setAttribute("font-outline", t->data(101).toDouble());
                if (!t->data(102).isNull()) {
                    QVariant variant = t->data(102);
                    QColor outlineColor = variant.value<QColor>();
                    content.setAttribute("font-outline-color", colorToString(outlineColor));
                }
            }
            if (!t->data(100).isNull()) {
                QStringList effectParams = t->data(100).toStringList();
                QString effectName = effectParams.takeFirst();
                content.setAttribute(effectName, effectParams.join(";"));
            }

            // Only save when necessary.
            if (t->data(OriginXLeft).toInt() == AxisInverted) {
                content.setAttribute("kdenlive-axis-x-inverted", t->data(OriginXLeft).toInt());
            }
            if (t->data(OriginYTop).toInt() == AxisInverted) {
                content.setAttribute("kdenlive-axis-y-inverted", t->data(OriginYTop).toInt());
            }
            if (t->textWidth() != -1) {
                content.setAttribute("alignment", t->textCursor().blockFormat().alignment());
            }
            break;
        default:
            continue;
        }

        // position
        QDomElement pos = doc.createElement("position");
        pos.setAttribute("x", item->pos().x());
        pos.setAttribute("y", item->pos().y());
        QTransform transform = item->transform();
        QDomElement tr = doc.createElement("transform");
        if (!item->data(ZOOMFACTOR).isNull()) {
            tr.setAttribute("zoom", QString::number(item->data(ZOOMFACTOR).toInt()));
        }
        if (!item->data(ROTATEFACTOR).isNull()) {
            QList<QVariant> rotlist = item->data(ROTATEFACTOR).toList();
            tr.setAttribute("rotation", QString("%1,%2,%3").arg(rotlist[0].toDouble()).arg(rotlist[1].toDouble()).arg(rotlist[2].toDouble()));
        }
        tr.appendChild(doc.createTextNode(
                           QString("%1,%2,%3,%4,%5,%6,%7,%8,%9").arg(
                               transform.m11()).arg(transform.m12()).arg(transform.m13()).arg(transform.m21()).arg(transform.m22()).arg(transform.m23()).arg(transform.m31()).arg(transform.m32()).arg(transform.m33())
                       )
                      );
        e.setAttribute("z-index", item->zValue());
        pos.appendChild(tr);

#if QT_VERSION >= 0x040600
        // effects
        QGraphicsEffect *eff = item->graphicsEffect();
        if (eff) {
            QGraphicsBlurEffect *blur = static_cast <QGraphicsBlurEffect *>(eff);
            QDomElement effect = doc.createElement("effect");
            if (blur) {
                effect.setAttribute("type", "blur");
                effect.setAttribute("blurradius", blur->blurRadius());
            } else {
                QGraphicsDropShadowEffect *shadow = static_cast <QGraphicsDropShadowEffect *>(eff);
                if (shadow) {
                    effect.setAttribute("type", "shadow");
                    effect.setAttribute("blurradius", shadow->blurRadius());
                    effect.setAttribute("xoffset", shadow->xOffset());
                    effect.setAttribute("yoffset", shadow->yOffset());
                }
            }
            e.appendChild(effect);
        }
#endif

        e.appendChild(pos);
        e.appendChild(content);
        if (item->zValue() > -1000) main.appendChild(e);
    }
    if (startv && endv) {
        QDomElement endp = doc.createElement("endviewport");
        QDomElement startp = doc.createElement("startviewport");
        QRectF r(endv->pos().x(), endv->pos().y(), endv->rect().width(), endv->rect().height());
        endp.setAttribute("rect", rectFToString(r));
        QRectF r2(startv->pos().x(), startv->pos().y(), startv->rect().width(), startv->rect().height());
        startp.setAttribute("rect", rectFToString(r2));

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
QColor TitleDocument::getBackgroundColor()
{
    QColor color(0, 0, 0, 0);
    if (m_scene) {
        QList<QGraphicsItem *> items = m_scene->items();
        for (int i = 0; i < items.size(); i++) {
            if (items.at(i)->zValue() == -1100) {
                color = ((QGraphicsRectItem *)items.at(i))->brush().color();
                return color;
            }
        }
    }
    return color;
}


bool TitleDocument::saveDocument(const KUrl& url, QGraphicsRectItem* startv, QGraphicsRectItem* endv, int out, bool embed)
{
    if (!m_scene)
        return false;

    QDomDocument doc = xml(startv, endv, embed);
    doc.documentElement().setAttribute("out", out);
    KTemporaryFile tmpfile;
    if (!tmpfile.open()) {
        kWarning() << "/////  CANNOT CREATE TMP FILE in: " << tmpfile.fileName();
        return false;
    }
    QFile xmlf(tmpfile.fileName());
    if (!xmlf.open(QIODevice::WriteOnly))
        return false;
    xmlf.write(doc.toString().toUtf8());
    if (xmlf.error() != QFile::NoError) {
        xmlf.close();
        return false;
    }
    xmlf.close();
    return KIO::NetAccess::upload(tmpfile.fileName(), url, 0);
}

int TitleDocument::loadFromXml(QDomDocument doc, QGraphicsRectItem* startv, QGraphicsRectItem* endv, int *out, const QString& projectpath)
{
    m_projectPath = projectpath;
    QDomNodeList titles = doc.elementsByTagName("kdenlivetitle");
    //TODO: Check if the opened title size is equal to project size, otherwise warn user and rescale
    if (doc.documentElement().hasAttribute("width") && doc.documentElement().hasAttribute("height")) {
        int doc_width = doc.documentElement().attribute("width").toInt();
        int doc_height = doc.documentElement().attribute("height").toInt();
        if (doc_width != m_width || doc_height != m_height) {
            KMessageBox::information(kapp->activeWindow(), i18n("This title clip was created with a different frame size."), i18n("Title Profile"));
            //TODO: convert using QTransform
            m_width = doc_width;
            m_height = doc_height;
        }
    } else {
        // Document has no size info, it is likely an old version title, so ignore viewport data
        QDomNodeList viewportlist = doc.documentElement().elementsByTagName("startviewport");
        if (!viewportlist.isEmpty()) {
            doc.documentElement().removeChild(viewportlist.at(0));
        }
        viewportlist = doc.documentElement().elementsByTagName("endviewport");
        if (!viewportlist.isEmpty()) {
            doc.documentElement().removeChild(viewportlist.at(0));
        }
    }
    //TODO: get default title duration instead of hardcoded one
    if (doc.documentElement().hasAttribute("out"))
        *out = doc.documentElement().attribute("out").toInt();
    else
        *out = 125;

    int maxZValue = 0;
    if (titles.size()) {

        QDomNodeList items = titles.item(0).childNodes();
        for (int i = 0; i < items.count(); i++) {
            QGraphicsItem *gitem = NULL;
            kDebug() << items.item(i).attributes().namedItem("type").nodeValue();
            int zValue = items.item(i).attributes().namedItem("z-index").nodeValue().toInt();
            if (zValue > -1000) {
                if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsTextItem") {
                    QDomNamedNodeMap txtProperties = items.item(i).namedItem("content").attributes();
                    QFont font(txtProperties.namedItem("font").nodeValue());

                    QDomNode node = txtProperties.namedItem("font-bold");
                    if (!node.isNull()) {
                        // Old: Bold/Not bold.
                        font.setBold(node.nodeValue().toInt());
                    } else {
                        // New: Font weight (QFont::)
                        font.setWeight(txtProperties.namedItem("font-weight").nodeValue().toInt());
                    }
                    //font.setBold(txtProperties.namedItem("font-bold").nodeValue().toInt());
                    font.setItalic(txtProperties.namedItem("font-italic").nodeValue().toInt());
                    font.setUnderline(txtProperties.namedItem("font-underline").nodeValue().toInt());
                    // Older Kdenlive version did not store pixel size but point size
                    if (txtProperties.namedItem("font-pixel-size").isNull()) {
                        KMessageBox::information(kapp->activeWindow(), i18n("Some of your text clips were saved with size in points, which means different sizes on different displays. They will be converted to pixel size, making them portable, but you could have to adjust their size."), i18n("Text Clips Updated"));
                        QFont f2;
                        f2.setPointSize(txtProperties.namedItem("font-size").nodeValue().toInt());
                        font.setPixelSize(QFontInfo(f2).pixelSize());
                    } else
                        font.setPixelSize(txtProperties.namedItem("font-pixel-size").nodeValue().toInt());
                    QColor col(stringToColor(txtProperties.namedItem("font-color").nodeValue()));
                    QGraphicsTextItem *txt = m_scene->addText(items.item(i).namedItem("content").firstChild().nodeValue(), font);
                    QTextCursor cursor(txt->document());
                    cursor.select(QTextCursor::Document);
                    QTextCharFormat format;
                    if (txtProperties.namedItem("font-outline").nodeValue().toDouble() > 0.0) {
                        txt->setData(101, txtProperties.namedItem("font-outline").nodeValue().toDouble());
                        txt->setData(102, stringToColor(txtProperties.namedItem("font-outline-color").nodeValue()));
                        format.setTextOutline(
                            QPen(QColor(stringToColor(txtProperties.namedItem("font-outline-color").nodeValue())),
                                 txtProperties.namedItem("font-outline").nodeValue().toDouble(),
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
                        );

                    }
                    format.setForeground(QBrush(col));
                    cursor.mergeCharFormat(format);
                    txt->setTextInteractionFlags(Qt::NoTextInteraction);
                    if (txtProperties.namedItem("alignment").isNull() == false) {
                        txt->setTextWidth(txt->boundingRect().width());
                        QTextCursor cur = txt->textCursor();
                        QTextBlockFormat format = cur.blockFormat();
                        format.setAlignment((Qt::Alignment) txtProperties.namedItem("alignment").nodeValue().toInt());
                        cur.select(QTextCursor::Document);
                        cur.setBlockFormat(format);
                        txt->setTextCursor(cur);
                        cur.clearSelection();
                        txt->setTextCursor(cur);
                    }

                    if (!txtProperties.namedItem("kdenlive-axis-x-inverted").isNull()) {
                        txt->setData(OriginXLeft, txtProperties.namedItem("kdenlive-axis-x-inverted").nodeValue().toInt());
                    }
                    if (!txtProperties.namedItem("kdenlive-axis-y-inverted").isNull()) {
                        txt->setData(OriginYTop, txtProperties.namedItem("kdenlive-axis-y-inverted").nodeValue().toInt());
                    }

                    // Effects
                    if (!txtProperties.namedItem("typewriter").isNull()) {
                        QStringList effData = QStringList() << "typewriter" << txtProperties.namedItem("typewriter").nodeValue();
                        txt->setData(100, effData);
                    }

                    gitem = txt;
                } else if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsRectItem") {
                    QString rect = items.item(i).namedItem("content").attributes().namedItem("rect").nodeValue();
                    QString br_str = items.item(i).namedItem("content").attributes().namedItem("brushcolor").nodeValue();
                    QString pen_str = items.item(i).namedItem("content").attributes().namedItem("pencolor").nodeValue();
                    double penwidth = items.item(i).namedItem("content").attributes().namedItem("penwidth").nodeValue().toDouble();
                    QGraphicsRectItem *rec = m_scene->addRect(stringToRect(rect), QPen(QBrush(stringToColor(pen_str)), penwidth, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin), QBrush(stringToColor(br_str)));
                    gitem = rec;
                } else if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsPixmapItem") {
                    QString url = items.item(i).namedItem("content").attributes().namedItem("url").nodeValue();
                    QString base64 = items.item(i).namedItem("content").attributes().namedItem("base64").nodeValue();
                    QPixmap pix;
                    if (base64.isEmpty()) {
                        pix.load(url);
                    } else {
                        pix.loadFromData(QByteArray::fromBase64(base64.toAscii()));
                    }
                    QGraphicsPixmapItem *rec = m_scene->addPixmap(pix);
                    rec->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
                    rec->setData(Qt::UserRole, url);
                    if (!base64.isEmpty()) {
                        rec->setData(Qt::UserRole + 1, base64);
                    }
                    gitem = rec;
                } else if (items.item(i).attributes().namedItem("type").nodeValue() == "QGraphicsSvgItem") {
                    QString url = items.item(i).namedItem("content").attributes().namedItem("url").nodeValue();
                    QString base64 = items.item(i).namedItem("content").attributes().namedItem("base64").nodeValue();
                    QGraphicsSvgItem *rec = NULL;
                    if (base64.isEmpty()) {
                        rec = new QGraphicsSvgItem(url);
                    } else {
                        rec = new QGraphicsSvgItem();
                        QSvgRenderer *renderer = new QSvgRenderer(QByteArray::fromBase64(base64.toAscii()), rec);
                        rec->setSharedRenderer(renderer);
                        //QString elem=rec->elementId();
                        //QRectF bounds = renderer->boundsOnElement(elem);
                    }
                    if (rec) {
                        m_scene->addItem(rec);
                        rec->setData(Qt::UserRole, url);
                        if (!base64.isEmpty()) {
                            rec->setData(Qt::UserRole + 1, base64);
                        }
                        gitem = rec;
                    }
                }
            }
            //pos and transform
            if (gitem) {
                QPointF p(items.item(i).namedItem("position").attributes().namedItem("x").nodeValue().toDouble(),
                          items.item(i).namedItem("position").attributes().namedItem("y").nodeValue().toDouble());
                gitem->setPos(p);
                QDomElement trans = items.item(i).namedItem("position").firstChild().toElement();
                gitem->setTransform(stringToTransform(trans.firstChild().nodeValue()));
                QString rotate = trans.attribute("rotation");
                if (!rotate.isEmpty()) gitem->setData(ROTATEFACTOR, stringToList(rotate));
                QString zoom = trans.attribute("zoom");
                if (!zoom.isEmpty()) gitem->setData(ZOOMFACTOR, zoom.toInt());
                int zValue = items.item(i).attributes().namedItem("z-index").nodeValue().toInt();
                if (zValue > maxZValue) maxZValue = zValue;
                gitem->setZValue(zValue);
                gitem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

#if QT_VERSION >= 0x040600
                // effects
                QDomNode eff = items.item(i).namedItem("effect");
                if (!eff.isNull()) {
                    QDomElement e = eff.toElement();
                    if (e.attribute("type") == "blur") {
                        QGraphicsBlurEffect *blur = new QGraphicsBlurEffect();
                        blur->setBlurRadius(e.attribute("blurradius").toInt());
                        gitem->setGraphicsEffect(blur);
                    } else if (e.attribute("type") == "shadow") {
                        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
                        shadow->setBlurRadius(e.attribute("blurradius").toInt());
                        shadow->setOffset(e.attribute("xoffset").toInt(), e.attribute("yoffset").toInt());
                        gitem->setGraphicsEffect(shadow);
                    }
                }
#endif
            }

            if (items.item(i).nodeName() == "background") {
                kDebug() << items.item(i).attributes().namedItem("color").nodeValue();
                QColor color = QColor(stringToColor(items.item(i).attributes().namedItem("color").nodeValue()));
                //color.setAlpha(items.item(i).attributes().namedItem("alpha").nodeValue().toInt());
                QList<QGraphicsItem *> items = m_scene->items();
                for (int i = 0; i < items.size(); i++) {
                    if (items.at(i)->zValue() == -1100) {
                        ((QGraphicsRectItem *)items.at(i))->setBrush(QBrush(color));
                        break;
                    }
                }
            } else if (items.item(i).nodeName() == "startviewport" && startv) {
                QString rect = items.item(i).attributes().namedItem("rect").nodeValue();
                QRectF r = stringToRect(rect);
                startv->setRect(0, 0, r.width(), r.height());
                startv->setPos(r.topLeft());
            } else if (items.item(i).nodeName() == "endviewport" && endv) {
                QString rect = items.item(i).attributes().namedItem("rect").nodeValue();
                QRectF r = stringToRect(rect);
                endv->setRect(0, 0, r.width(), r.height());
                endv->setPos(r.topLeft());
            }
        }
    }
    return maxZValue;
}

QString TitleDocument::colorToString(const QColor& c)
{
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

QString TitleDocument::rectFToString(const QRectF& c)
{
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.left()).arg(c.top()).arg(c.width()).arg(c.height());
    return ret;
}

QRectF TitleDocument::stringToRect(const QString & s)
{

    QStringList l = s.split(',');
    if (l.size() < 4)
        return QRectF();
    return QRectF(l.at(0).toDouble(), l.at(1).toDouble(), l.at(2).toDouble(), l.at(3).toDouble()).normalized();
}

QColor TitleDocument::stringToColor(const QString & s)
{
    QStringList l = s.split(',');
    if (l.size() < 4)
        return QColor();
    return QColor(l.at(0).toInt(), l.at(1).toInt(), l.at(2).toInt(), l.at(3).toInt());;
}

QTransform TitleDocument::stringToTransform(const QString& s)
{
    QStringList l = s.split(',');
    if (l.size() < 9)
        return QTransform();
    return QTransform(
               l.at(0).toDouble(), l.at(1).toDouble(), l.at(2).toDouble(),
               l.at(3).toDouble(), l.at(4).toDouble(), l.at(5).toDouble(),
               l.at(6).toDouble(), l.at(7).toDouble(), l.at(8).toDouble()
           );
}

QList<QVariant> TitleDocument::stringToList(const QString & s)
{
    QStringList l = s.split(',');
    if (l.size() < 3)
        return QList<QVariant>();
    return QList<QVariant>() << QVariant(l.at(0).toDouble()) << QVariant(l.at(1).toDouble()) << QVariant(l.at(2).toDouble());
}

int TitleDocument::frameWidth() const
{
    return m_width;
}

int TitleDocument::frameHeight() const
{
    return m_height;
}


