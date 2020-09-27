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
#include "gradientwidget.h"

#include "graphicsscenerectmove.h"
#include "kdenlivesettings.h"
#include "timecode.h"

#include <KIO/FileCopyJob>
#include <KLocalizedString>
#include <KMessageBox>

#include "kdenlive_debug.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QFontInfo>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QGraphicsTextItem>
#include <QSvgRenderer>
#include <QTemporaryFile>
#include <QTextCursor>
#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsEffect>
#include <QPainter>

QByteArray fileToByteArray(const QString &filename)
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
    m_scene = nullptr;
    m_width = 0;
    m_height = 0;
    m_missingElements = 0;
}

void TitleDocument::setScene(QGraphicsScene *_scene, int width, int height)
{
    m_scene = _scene;
    m_width = width;
    m_height = height;
}

int TitleDocument::base64ToUrl(QGraphicsItem *item, QDomElement &content, bool embed)
{
    if (embed) {
        if (!item->data(Qt::UserRole + 1).toString().isEmpty()) {
            content.setAttribute(QStringLiteral("base64"), item->data(Qt::UserRole + 1).toString());
        } else if (!item->data(Qt::UserRole).toString().isEmpty()) {
            content.setAttribute(QStringLiteral("base64"), fileToByteArray(item->data(Qt::UserRole).toString()).toBase64().data());
        }
        content.removeAttribute(QStringLiteral("url"));
    } else {
        // save for project files to disk
        QString base64 = item->data(Qt::UserRole + 1).toString();
        if (!base64.isEmpty()) {
            QString titlePath;
            if (!m_projectPath.isEmpty()) {
                titlePath = m_projectPath;
            } else {
                titlePath = QStringLiteral("/tmp/titles");
            }
            QString filename = extractBase64Image(titlePath, base64);
            if (!filename.isEmpty()) {
                content.setAttribute(QStringLiteral("url"), filename);
                content.removeAttribute(QStringLiteral("base64"));
            }

        } else {
            return 1;
        }
    }
    return 0;
}

// static
const QString TitleDocument::extractBase64Image(const QString &titlePath, const QString &data)
{
    QString filename = titlePath + QString(QCryptographicHash::hash(data.toLatin1(), QCryptographicHash::Md5).toHex().append(".titlepart"));
    QDir dir;
    dir.mkpath(titlePath);
    QFile f(filename);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QByteArray::fromBase64(data.toLatin1()));
        f.close();
        return filename;
    }
    return QString();
}

QDomDocument TitleDocument::xml(QGraphicsRectItem *startv, QGraphicsRectItem *endv, bool embed)
{
    QDomDocument doc;

    QDomElement main = doc.createElement(QStringLiteral("kdenlivetitle"));
    main.setAttribute(QStringLiteral("width"), m_width);
    main.setAttribute(QStringLiteral("height"), m_height);

    // Save locale. Since 20.08, we always use the C locale for serialising.
    main.setAttribute(QStringLiteral("LC_NUMERIC"), "C");
    doc.appendChild(main);
    QTextCursor cur;
    QTextBlockFormat format;

    for (QGraphicsItem *item : m_scene->items()) {
        if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
            continue;
        }
        QDomElement e = doc.createElement(QStringLiteral("item"));
        QDomElement content = doc.createElement(QStringLiteral("content"));
        QFont font;
        QString gradient;
        MyTextItem *t;
        double xPosition = item->pos().x();

        switch (item->type()) {
        case 7:
            e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsPixmapItem"));
            content.setAttribute(QStringLiteral("url"), item->data(Qt::UserRole).toString());
            base64ToUrl(item, content, embed);
            break;
        case 13:
            e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsSvgItem"));
            content.setAttribute(QStringLiteral("url"), item->data(Qt::UserRole).toString());
            base64ToUrl(item, content, embed);
            break;
        case 3:
            e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsRectItem"));
            content.setAttribute(QStringLiteral("rect"), rectFToString(static_cast<QGraphicsRectItem *>(item)->rect().normalized()));
            content.setAttribute(QStringLiteral("pencolor"), colorToString(static_cast<QGraphicsRectItem *>(item)->pen().color()));
            if (static_cast<QGraphicsRectItem *>(item)->pen() == Qt::NoPen) {
                content.setAttribute(QStringLiteral("penwidth"), 0);
            } else {
                content.setAttribute(QStringLiteral("penwidth"), static_cast<QGraphicsRectItem *>(item)->pen().width());
            }
            content.setAttribute(QStringLiteral("brushcolor"), colorToString(static_cast<QGraphicsRectItem *>(item)->brush().color()));
            gradient = item->data(TitleDocument::Gradient).toString();
            if (!gradient.isEmpty()) {
                content.setAttribute(QStringLiteral("gradient"), gradient);
            }
            break;
        case 8:
            e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsTextItem"));
            t = static_cast<MyTextItem *>(item);
            // Don't save empty text nodes
            if (t->toPlainText().simplified().isEmpty()) {
                continue;
            }
            // content.appendChild(doc.createTextNode(((QGraphicsTextItem*)item)->toHtml()));
            content.appendChild(doc.createTextNode(t->toPlainText()));
            font = t->font();
            content.setAttribute(QStringLiteral("font"), font.family());
            content.setAttribute(QStringLiteral("font-weight"), font.weight());
            content.setAttribute(QStringLiteral("font-pixel-size"), font.pixelSize());
            content.setAttribute(QStringLiteral("font-italic"), static_cast<int>(font.italic()));
            content.setAttribute(QStringLiteral("font-underline"), static_cast<int>(font.underline()));
            content.setAttribute(QStringLiteral("letter-spacing"), QString::number(font.letterSpacing()));
            gradient = item->data(TitleDocument::Gradient).toString();
            if (!gradient.isEmpty()) {
                content.setAttribute(QStringLiteral("gradient"), gradient);
            }
            cur = QTextCursor(t->document());
            cur.select(QTextCursor::Document);
            format = cur.blockFormat();
            if (t->toPlainText() == QLatin1String("%s")) {
                // template text box, adjust size for later remplacement text
                if (t->alignment() == Qt::AlignHCenter) {
                    // grow dimensions on both sides
                    double xcenter = item->pos().x() + (t->baseBoundingRect().width()) / 2;
                    double offset = qMin(xcenter, m_width - xcenter);
                    xPosition = xcenter - offset;
                    content.setAttribute(QStringLiteral("box-width"), QString::number(2 * offset));
                } else if (t->alignment() == Qt::AlignRight) {
                    // grow to the left
                    double offset = item->pos().x() + (t->baseBoundingRect().width());
                    xPosition = 0;
                    content.setAttribute(QStringLiteral("box-width"), QString::number(offset));
                } else {
                    // left align, grow on right side
                    double offset = m_width - item->pos().x();
                    content.setAttribute(QStringLiteral("box-width"), QString::number(offset));
                }
            } else {
                content.setAttribute(QStringLiteral("box-width"), QString::number(t->baseBoundingRect().width()));
            }
            content.setAttribute(QStringLiteral("box-height"), QString::number(t->baseBoundingRect().height()));
            if (!t->data(TitleDocument::LineSpacing).isNull()) {
                content.setAttribute(QStringLiteral("line-spacing"), QString::number(t->data(TitleDocument::LineSpacing).toInt()));
            }
            {
                QTextCursor cursor(t->document());
                cursor.select(QTextCursor::Document);
                QColor fontcolor = cursor.charFormat().foreground().color();
                content.setAttribute(QStringLiteral("font-color"), colorToString(fontcolor));
                if (!t->data(TitleDocument::OutlineWidth).isNull()) {
                    content.setAttribute(QStringLiteral("font-outline"), QString::number(t->data(TitleDocument::OutlineWidth).toDouble()));
                }
                if (!t->data(TitleDocument::OutlineColor).isNull()) {
                    QVariant variant = t->data(TitleDocument::OutlineColor);
                    QColor outlineColor = variant.value<QColor>();
                    content.setAttribute(QStringLiteral("font-outline-color"), colorToString(outlineColor));
                }
            }
            if (!t->data(100).isNull()) {
                QStringList effectParams = t->data(100).toStringList();
                QString effectName = effectParams.takeFirst();
                content.setAttribute(QStringLiteral("textwidth"), QString::number(t->sceneBoundingRect().width()));
                content.setAttribute(effectName, effectParams.join(QLatin1Char(';')));
            }

            // Only save when necessary.
            if (t->data(OriginXLeft).toInt() == AxisInverted) {
                content.setAttribute(QStringLiteral("kdenlive-axis-x-inverted"), t->data(OriginXLeft).toInt());
            }
            if (t->data(OriginYTop).toInt() == AxisInverted) {
                content.setAttribute(QStringLiteral("kdenlive-axis-y-inverted"), t->data(OriginYTop).toInt());
            }
            if (t->textWidth() > 0) {
                content.setAttribute(QStringLiteral("alignment"), (int)t->alignment());
            }

            content.setAttribute(QStringLiteral("shadow"), t->shadowInfo().join(QLatin1Char(';')));
            break;
        default:
            continue;
        }

        // position
        QDomElement pos = doc.createElement(QStringLiteral("position"));
        pos.setAttribute(QStringLiteral("x"), QString::number(xPosition));
        pos.setAttribute(QStringLiteral("y"), QString::number(item->pos().y()));
        QTransform transform = item->transform();
        QDomElement tr = doc.createElement(QStringLiteral("transform"));
        if (!item->data(TitleDocument::ZoomFactor).isNull()) {
            tr.setAttribute(QStringLiteral("zoom"), QString::number(item->data(TitleDocument::ZoomFactor).toInt()));
        }
        if (!item->data(TitleDocument::RotateFactor).isNull()) {
            QList<QVariant> rotlist = item->data(TitleDocument::RotateFactor).toList();
            tr.setAttribute(QStringLiteral("rotation"),
                            QStringLiteral("%1,%2,%3").arg(rotlist[0].toDouble()).arg(rotlist[1].toDouble()).arg(rotlist[2].toDouble()));
        }
        tr.appendChild(doc.createTextNode(QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9")
                                              .arg(transform.m11())
                                              .arg(transform.m12())
                                              .arg(transform.m13())
                                              .arg(transform.m21())
                                              .arg(transform.m22())
                                              .arg(transform.m23())
                                              .arg(transform.m31())
                                              .arg(transform.m32())
                                              .arg(transform.m33())));
        e.setAttribute(QStringLiteral("z-index"), item->zValue());
        pos.appendChild(tr);
        e.appendChild(pos);
        e.appendChild(content);
        if (item->zValue() > -1000) {
            main.appendChild(e);
        }
    }
    if ((startv != nullptr) && (endv != nullptr)) {
        QDomElement endport = doc.createElement(QStringLiteral("endviewport"));
        QDomElement startport = doc.createElement(QStringLiteral("startviewport"));
        QRectF r(endv->pos().x(), endv->pos().y(), endv->rect().width(), endv->rect().height());
        endport.setAttribute(QStringLiteral("rect"), rectFToString(r));
        QRectF r2(startv->pos().x(), startv->pos().y(), startv->rect().width(), startv->rect().height());
        startport.setAttribute(QStringLiteral("rect"), rectFToString(r2));

        main.appendChild(startport);
        main.appendChild(endport);
    }
    QDomElement backgr = doc.createElement(QStringLiteral("background"));
    QColor color = getBackgroundColor();
    backgr.setAttribute(QStringLiteral("color"), colorToString(color));
    main.appendChild(backgr);

    return doc;
}

/** \brief Get the background color (incl. alpha) from the document, if possibly
 * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
QColor TitleDocument::getBackgroundColor() const
{
    QColor color(0, 0, 0, 0);
    if (m_scene) {
        QList<QGraphicsItem *> items = m_scene->items();
        for (auto item : qAsConst(items)) {
            if ((int)item->zValue() == -1100) {
                color = static_cast<QGraphicsRectItem *>(item)->brush().color();
                return color;
            }
        }
    }
    return color;
}

bool TitleDocument::saveDocument(const QUrl &url, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int duration, bool embed)
{
    if (!m_scene) {
        return false;
    }

    QDomDocument doc = xml(startv, endv, embed);
    doc.documentElement().setAttribute(QStringLiteral("duration"), duration);
    // keep some time for backwards compatibility (opening projects with older versions) - 26/12/12
    doc.documentElement().setAttribute(QStringLiteral("out"), duration);
    QTemporaryFile tmpfile;
    if (!tmpfile.open()) {
        qCWarning(KDENLIVE_LOG) << "/////  CANNOT CREATE TMP FILE in: " << tmpfile.fileName();
        return false;
    }
    QFile xmlf(tmpfile.fileName());
    if (!xmlf.open(QIODevice::WriteOnly)) {
        return false;
    }
    xmlf.write(doc.toString().toUtf8());
    if (xmlf.error() != QFile::NoError) {
        xmlf.close();
        return false;
    }
    xmlf.close();
    KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(tmpfile.fileName()), url, -1, KIO::Overwrite);
    bool result = copyjob->exec();
    delete copyjob;
    return result;
}

int TitleDocument::loadFromXml(const QDomDocument &doc, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int *duration, const QString &projectpath)
{
    m_projectPath = projectpath;
    m_missingElements = 0;
    QDomNodeList titles = doc.elementsByTagName(QStringLiteral("kdenlivetitle"));
    // TODO: Check if the opened title size is equal to project size, otherwise warn user and rescale
    if (doc.documentElement().hasAttribute(QStringLiteral("width")) && doc.documentElement().hasAttribute(QStringLiteral("height"))) {
        int doc_width = doc.documentElement().attribute(QStringLiteral("width")).toInt();
        int doc_height = doc.documentElement().attribute(QStringLiteral("height")).toInt();
        if (doc_width != m_width || doc_height != m_height) {
            KMessageBox::information(QApplication::activeWindow(), i18n("This title clip was created with a different frame size."), i18n("Title Profile"));
            // TODO: convert using QTransform
            m_width = doc_width;
            m_height = doc_height;
        }
    } else {
        // Document has no size info, it is likely an old version title, so ignore viewport data
        QDomNodeList viewportlist = doc.documentElement().elementsByTagName(QStringLiteral("startviewport"));
        if (!viewportlist.isEmpty()) {
            doc.documentElement().removeChild(viewportlist.at(0));
        }
        viewportlist = doc.documentElement().elementsByTagName(QStringLiteral("endviewport"));
        if (!viewportlist.isEmpty()) {
            doc.documentElement().removeChild(viewportlist.at(0));
        }
    }
    if (doc.documentElement().hasAttribute(QStringLiteral("duration"))) {
        *duration = doc.documentElement().attribute(QStringLiteral("duration")).toInt();
    } else if (doc.documentElement().hasAttribute(QStringLiteral("out"))) {
        *duration = doc.documentElement().attribute(QStringLiteral("out")).toInt();
    } else {
        *duration = Timecode().getFrameCount(KdenliveSettings::title_duration());
    }

    int maxZValue = 0;
    if (!titles.isEmpty()) {
        QDomNodeList items = titles.item(0).childNodes();
        for (int i = 0; i < items.count(); ++i) {
            QGraphicsItem *gitem = nullptr;
            QDomNode itemNode = items.item(i);
            // qCDebug(KDENLIVE_LOG) << items.item(i).attributes().namedItem("type").nodeValue();
            int zValue = itemNode.attributes().namedItem(QStringLiteral("z-index")).nodeValue().toInt();
            double xPosition = itemNode.namedItem(QStringLiteral("position")).attributes().namedItem(QStringLiteral("x")).nodeValue().toDouble();
            if (zValue > -1000) {
                if (itemNode.attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsTextItem")) {
                    QDomNamedNodeMap txtProperties = itemNode.namedItem(QStringLiteral("content")).attributes();
                    QFont font(txtProperties.namedItem(QStringLiteral("font")).nodeValue());

                    QDomNode node = txtProperties.namedItem(QStringLiteral("font-bold"));
                    if (!node.isNull()) {
                        // Old: Bold/Not bold.
                        font.setBold(node.nodeValue().toInt() != 0);
                    } else {
                        // New: Font weight (QFont::)
                        font.setWeight(txtProperties.namedItem(QStringLiteral("font-weight")).nodeValue().toInt());
                    }
                    // font.setBold(txtProperties.namedItem("font-bold").nodeValue().toInt());
                    font.setItalic(txtProperties.namedItem(QStringLiteral("font-italic")).nodeValue().toInt() != 0);
                    font.setUnderline(txtProperties.namedItem(QStringLiteral("font-underline")).nodeValue().toInt() != 0);
                    // Older Kdenlive version did not store pixel size but point size
                    if (txtProperties.namedItem(QStringLiteral("font-pixel-size")).isNull()) {
                        KMessageBox::information(QApplication::activeWindow(),
                                                 i18n("Some of your text clips were saved with size in points, which means "
                                                      "different sizes on different displays. They will be converted to pixel "
                                                      "size, making them portable, but you could have to adjust their size."),
                                                 i18n("Text Clips Updated"));
                        QFont f2;
                        f2.setPointSize(txtProperties.namedItem(QStringLiteral("font-size")).nodeValue().toInt());
                        font.setPixelSize(QFontInfo(f2).pixelSize());
                    } else {
                        font.setPixelSize(txtProperties.namedItem(QStringLiteral("font-pixel-size")).nodeValue().toInt());
                    }
                    font.setLetterSpacing(QFont::AbsoluteSpacing, txtProperties.namedItem(QStringLiteral("letter-spacing")).nodeValue().toInt());
                    QColor col(stringToColor(txtProperties.namedItem(QStringLiteral("font-color")).nodeValue()));
                    MyTextItem *txt = new MyTextItem(itemNode.namedItem(QStringLiteral("content")).firstChild().nodeValue(), nullptr);
                    m_scene->addItem(txt);
                    txt->setFont(font);
                    txt->setTextInteractionFlags(Qt::NoTextInteraction);
                    QTextCursor cursor(txt->document());
                    cursor.select(QTextCursor::Document);
                    QTextCharFormat cformat = cursor.charFormat();
                    if (txtProperties.namedItem(QStringLiteral("font-outline")).nodeValue().toDouble() > 0.0) {
                        txt->setData(TitleDocument::OutlineWidth, txtProperties.namedItem(QStringLiteral("font-outline")).nodeValue().toDouble());
                        txt->setData(TitleDocument::OutlineColor, stringToColor(txtProperties.namedItem(QStringLiteral("font-outline-color")).nodeValue()));
                        cformat.setTextOutline(QPen(QColor(stringToColor(txtProperties.namedItem(QStringLiteral("font-outline-color")).nodeValue())),
                                                    txtProperties.namedItem(QStringLiteral("font-outline")).nodeValue().toDouble(), Qt::SolidLine, Qt::RoundCap,
                                                    Qt::RoundJoin));
                    }
                    if (!txtProperties.namedItem(QStringLiteral("line-spacing")).isNull()) {
                        int lineSpacing = txtProperties.namedItem(QStringLiteral("line-spacing")).nodeValue().toInt();
                        QTextBlockFormat format = cursor.blockFormat();
                        format.setLineHeight(lineSpacing, QTextBlockFormat::LineDistanceHeight);
                        cursor.setBlockFormat(format);
                        txt->setData(TitleDocument::LineSpacing, lineSpacing);
                    }
                    txt->setTextColor(col);
                    cformat.setForeground(QBrush(col));
                    cursor.setCharFormat(cformat);
                    if (!txtProperties.namedItem(QStringLiteral("gradient")).isNull()) {
                        // Gradient color
                        QString data = txtProperties.namedItem(QStringLiteral("gradient")).nodeValue();
                        txt->setData(TitleDocument::Gradient, data);
                        QLinearGradient gr = GradientWidget::gradientFromString(data, txt->boundingRect().width(), txt->boundingRect().height());
                        cformat.setForeground(QBrush(gr));
                        cursor.setCharFormat(cformat);
                    }

                    if (!txtProperties.namedItem(QStringLiteral("alignment")).isNull()) {
                        txt->setAlignment((Qt::Alignment)txtProperties.namedItem(QStringLiteral("alignment")).nodeValue().toInt());
                    }

                    if (!txtProperties.namedItem(QStringLiteral("kdenlive-axis-x-inverted")).isNull()) {
                        txt->setData(OriginXLeft, txtProperties.namedItem(QStringLiteral("kdenlive-axis-x-inverted")).nodeValue().toInt());
                    }
                    if (!txtProperties.namedItem(QStringLiteral("kdenlive-axis-y-inverted")).isNull()) {
                        txt->setData(OriginYTop, txtProperties.namedItem(QStringLiteral("kdenlive-axis-y-inverted")).nodeValue().toInt());
                    }

                    if (!txtProperties.namedItem(QStringLiteral("shadow")).isNull()) {
                        QString info = txtProperties.namedItem(QStringLiteral("shadow")).nodeValue();
                        txt->loadShadow(info.split(QLatin1Char(';')));
                    }

                    // Effects
                    if (!txtProperties.namedItem(QStringLiteral("typewriter")).isNull()) {
                        QStringList effData = QStringList()
                                              << QStringLiteral("typewriter") << txtProperties.namedItem(QStringLiteral("typewriter")).nodeValue();
                        txt->setData(100, effData);
                    }
                    if (txt->toPlainText() == QLatin1String("%s")) {
                        // template text box, adjust size for later remplacement text
                        if (txt->alignment() == Qt::AlignHCenter) {
                            // grow dimensions on both sides
                            double width = txtProperties.namedItem(QStringLiteral("box-width")).nodeValue().toDouble();
                            double xcenter = (width - xPosition) / 2.0;
                            xPosition = xcenter - txt->boundingRect().width() / 2;
                        } else if (txt->alignment() == Qt::AlignRight) {
                            // grow to the left
                            xPosition = xPosition + txtProperties.namedItem(QStringLiteral("box-width")).nodeValue().toDouble() - txt->boundingRect().width();
                        } else {
                            // left align, grow on right side, nothing to do
                        }
                    }

                    gitem = txt;
                } else if (itemNode.attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsRectItem")) {
                    QDomNamedNodeMap rectProperties = itemNode.namedItem(QStringLiteral("content")).attributes();
                    QString rect = rectProperties.namedItem(QStringLiteral("rect")).nodeValue();
                    QString br_str = rectProperties.namedItem(QStringLiteral("brushcolor")).nodeValue();
                    QString pen_str = rectProperties.namedItem(QStringLiteral("pencolor")).nodeValue();
                    double penwidth = rectProperties.namedItem(QStringLiteral("penwidth")).nodeValue().toDouble();
                    auto *rec = new MyRectItem();
                    rec->setRect(stringToRect(rect));
                    if (penwidth > 0) {
                        rec->setPen(QPen(QBrush(stringToColor(pen_str)), penwidth, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
                    } else {
                        rec->setPen(Qt::NoPen);
                    }
                    if (!rectProperties.namedItem(QStringLiteral("gradient")).isNull()) {
                        // Gradient color
                        QString data = rectProperties.namedItem(QStringLiteral("gradient")).nodeValue();
                        rec->setData(TitleDocument::Gradient, data);
                        QLinearGradient gr = GradientWidget::gradientFromString(data, rec->rect().width(), rec->rect().height());
                        rec->setBrush(QBrush(gr));
                    } else {
                        rec->setBrush(QBrush(stringToColor(br_str)));
                    }
                    m_scene->addItem(rec);

                    gitem = rec;
                } else if (itemNode.attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsPixmapItem")) {
                    QString url = itemNode.namedItem(QStringLiteral("content")).attributes().namedItem(QStringLiteral("url")).nodeValue();
                    QString base64 = itemNode.namedItem(QStringLiteral("content")).attributes().namedItem(QStringLiteral("base64")).nodeValue();
                    QPixmap pix;
                    bool missing = false;
                    if (base64.isEmpty()) {
                        pix.load(url);
                        if (pix.isNull()) {
                            pix = createInvalidPixmap(url);
                            m_missingElements++;
                            missing = true;
                        }
                    } else {
                        pix.loadFromData(QByteArray::fromBase64(base64.toLatin1()));
                    }
                    auto *rec = new MyPixmapItem(pix);
                    if (missing) {
                        rec->setData(Qt::UserRole + 2, 1);
                    }
                    m_scene->addItem(rec);
                    rec->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
                    rec->setData(Qt::UserRole, url);
                    if (!base64.isEmpty()) {
                        rec->setData(Qt::UserRole + 1, base64);
                    }
                    gitem = rec;
                } else if (itemNode.attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsSvgItem")) {
                    QString url = itemNode.namedItem(QStringLiteral("content")).attributes().namedItem(QStringLiteral("url")).nodeValue();
                    QString base64 = itemNode.namedItem(QStringLiteral("content")).attributes().namedItem(QStringLiteral("base64")).nodeValue();
                    QGraphicsSvgItem *rec = nullptr;
                    if (base64.isEmpty()) {
                        if (QFile::exists(url)) {
                            rec = new MySvgItem(url);
                        }
                    } else {
                        rec = new MySvgItem();
                        QSvgRenderer *renderer = new QSvgRenderer(QByteArray::fromBase64(base64.toLatin1()), rec);
                        rec->setSharedRenderer(renderer);
                        // QString elem=rec->elementId();
                        // QRectF bounds = renderer->boundsOnElement(elem);
                    }
                    if (rec) {
                        m_scene->addItem(rec);
                        rec->setData(Qt::UserRole, url);
                        if (!base64.isEmpty()) {
                            rec->setData(Qt::UserRole + 1, base64);
                        }
                        gitem = rec;
                    } else {
                        QPixmap pix = createInvalidPixmap(url);
                        m_missingElements++;
                        auto *rec2 = new MyPixmapItem(pix);
                        rec2->setData(Qt::UserRole + 2, 1);
                        m_scene->addItem(rec2);
                        rec2->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
                        rec2->setData(Qt::UserRole, url);
                        gitem = rec2;
                    }
                }
            }
            // pos and transform
            if (gitem) {
                QPointF p(xPosition, itemNode.namedItem(QStringLiteral("position")).attributes().namedItem(QStringLiteral("y")).nodeValue().toDouble());
                gitem->setPos(p);
                QDomElement trans = itemNode.namedItem(QStringLiteral("position")).firstChild().toElement();
                gitem->setTransform(stringToTransform(trans.firstChild().nodeValue()));
                QString rotate = trans.attribute(QStringLiteral("rotation"));
                if (!rotate.isEmpty()) {
                    gitem->setData(TitleDocument::RotateFactor, stringToList(rotate));
                }
                QString zoom = trans.attribute(QStringLiteral("zoom"));
                if (!zoom.isEmpty()) {
                    gitem->setData(TitleDocument::ZoomFactor, zoom.toInt());
                }
                if (zValue >= maxZValue) {
                    maxZValue = zValue + 1;
                }
                gitem->setZValue(zValue);
                gitem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);

                // effects
                QDomNode eff = itemNode.namedItem(QStringLiteral("effect"));
                if (!eff.isNull()) {
                    QDomElement e = eff.toElement();
                    if (e.attribute(QStringLiteral("type")) == QLatin1String("blur")) {
                        auto *blur = new QGraphicsBlurEffect();
                        blur->setBlurRadius(e.attribute(QStringLiteral("blurradius")).toInt());
                        gitem->setGraphicsEffect(blur);
                    } else if (e.attribute(QStringLiteral("type")) == QLatin1String("shadow")) {
                        auto *shadow = new QGraphicsDropShadowEffect();
                        shadow->setBlurRadius(e.attribute(QStringLiteral("blurradius")).toInt());
                        shadow->setOffset(e.attribute(QStringLiteral("xoffset")).toInt(), e.attribute(QStringLiteral("yoffset")).toInt());
                        gitem->setGraphicsEffect(shadow);
                    }
                }
            }

            if (itemNode.nodeName() == QLatin1String("background")) {
                // qCDebug(KDENLIVE_LOG) << items.item(i).attributes().namedItem("color").nodeValue();
                QColor color = QColor(stringToColor(itemNode.attributes().namedItem(QStringLiteral("color")).nodeValue()));
                // color.setAlpha(itemNode.attributes().namedItem("alpha").nodeValue().toInt());
                QList<QGraphicsItem *> sceneItems = m_scene->items();
                for (auto sceneItem : qAsConst(sceneItems)) {
                    if ((int)sceneItem->zValue() == -1100) {
                        static_cast<QGraphicsRectItem *>(sceneItem)->setBrush(QBrush(color));
                        break;
                    }
                }
            } else if (itemNode.nodeName() == QLatin1String("startviewport") && (startv != nullptr)) {
                QString rect = itemNode.attributes().namedItem(QStringLiteral("rect")).nodeValue();
                QRectF r = stringToRect(rect);
                startv->setRect(0, 0, r.width(), r.height());
                startv->setPos(r.topLeft());
            } else if (itemNode.nodeName() == QLatin1String("endviewport") && (endv != nullptr)) {
                QString rect = itemNode.attributes().namedItem(QStringLiteral("rect")).nodeValue();
                QRectF r = stringToRect(rect);
                endv->setRect(0, 0, r.width(), r.height());
                endv->setPos(r.topLeft());
            }
        }
    }
    return maxZValue;
}

int TitleDocument::invalidCount() const
{
    return m_missingElements;
}

QPixmap TitleDocument::createInvalidPixmap(const QString &url)
{
    int missingHeight = m_height / 10;
    QPixmap pix(missingHeight, missingHeight);
    QIcon icon = QIcon::fromTheme(QStringLiteral("messagebox_warning"));
    pix.fill(QColor(255, 0, 0, 50));
    QPainter ptr(&pix);
    icon.paint(&ptr, 0, 0, missingHeight / 2, missingHeight / 2);
    QPen pen(Qt::red);
    pen.setWidth(3);
    ptr.setPen(pen);
    ptr.drawText(QRectF(2, 2, missingHeight - 4, missingHeight - 4), Qt::AlignHCenter | Qt::AlignBottom, QFileInfo(url).fileName());
    ptr.drawRect(2, 1, missingHeight - 4, missingHeight - 4);
    ptr.end();
    return pix;
}

QString TitleDocument::colorToString(const QColor &c)
{
    QString ret = QStringLiteral("%1,%2,%3,%4");
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

QString TitleDocument::rectFToString(const QRectF &c)
{
    QString ret = QStringLiteral("%1,%2,%3,%4");
    ret = ret.arg(c.left()).arg(c.top()).arg(c.width()).arg(c.height());
    return ret;
}

QRectF TitleDocument::stringToRect(const QString &s)
{

    QStringList l = s.split(QLatin1Char(','));
    if (l.size() < 4) {
        return {};
    }
    return QRectF(l.at(0).toDouble(), l.at(1).toDouble(), l.at(2).toDouble(), l.at(3).toDouble()).normalized();
}

QColor TitleDocument::stringToColor(const QString &s)
{
    QStringList l = s.split(QLatin1Char(','));
    if (l.size() < 4) {
        return QColor();
    }
    return {l.at(0).toInt(), l.at(1).toInt(), l.at(2).toInt(), l.at(3).toInt()};
}

QTransform TitleDocument::stringToTransform(const QString &s)
{
    QStringList l = s.split(QLatin1Char(','));
    if (l.size() < 9) {
        return QTransform();
    }
    return {l.at(0).toDouble(), l.at(1).toDouble(), l.at(2).toDouble(), l.at(3).toDouble(), l.at(4).toDouble(),
            l.at(5).toDouble(), l.at(6).toDouble(), l.at(7).toDouble(), l.at(8).toDouble()};
}

QList<QVariant> TitleDocument::stringToList(const QString &s)
{
    QStringList l = s.split(QLatin1Char(','));
    if (l.size() < 3) {
        return QList<QVariant>();
    }
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
