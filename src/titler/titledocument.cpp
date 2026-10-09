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

#include "titledocument.h"
#include "gradientwidget.h"
#include "richtextgradient.h"
#include "richtextoutline.h"
#include "richtextspacing.h"
#include <QDebug>
#include <QFontInfo>

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "graphicsscenerectmove.h"
#include "kdenlivesettings.h"
#include "utils/timecode.h"
#include "xml/xml.hpp"

#include <KIO/FileCopyJob>
#include <KLocalizedString>
#include <KMessageBox>

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
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QtMath>
#include <memory>

#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsEffect>
#include <QPainter>

namespace {
QString titleRichTextHtml(const QTextDocument *source)
{
    // Qt writes every alpha-zero color as "transparent", discarding its RGB.
    // Use unused opaque colors in the export copy, then restore their exact
    // #AARRGGBB values in foreground declarations only.
    QSet<QRgb> usedColors;
    QMap<QRgb, QColor> transparentTokens;
    for (const QTextFormat &format : source->allFormats()) {
        const QBrush brush = format.foreground();
        if (!brush.color().isValid()) {
            continue;
        }
        usedColors.insert(brush.color().rgb());
        if (brush.style() == Qt::SolidPattern && brush.color().alpha() == 0) {
            transparentTokens.insert(brush.color().rgba(), QColor());
        }
    }
    QMap<QString, QString> originalColors;
    quint32 nextColor = 0;
    for (auto it = transparentTokens.begin(); it != transparentTokens.end(); ++it) {
        while (nextColor <= 0x00ffffffu && usedColors.contains(QRgb(0xff000000u | nextColor))) {
            ++nextColor;
        }
        if (nextColor > 0x00ffffffu) {
            qWarning() << "No unused foreground color for transparent title serialization";
            return source->toHtml();
        }
        const QColor token = QColor::fromRgb(QRgb(0xff000000u | nextColor++));
        usedColors.insert(token.rgb());
        it.value() = token;
        originalColors.insert(token.name(QColor::HexRgb), QColor::fromRgba(it.key()).name(QColor::HexArgb));
    }
    const auto encodeForeground = [&transparentTokens](const QTextCharFormat &format, QTextCharFormat &delta) {
        const QBrush brush = format.foreground();
        const auto token = transparentTokens.constFind(brush.color().rgba());
        if (brush.style() == Qt::SolidPattern && token != transparentTokens.cend()) {
            delta.setForeground(token.value());
            return true;
        }
        return false;
    };

    // Put decorations on character runs, not on the inherited HTML body font.
    std::unique_ptr<QTextDocument> copy(source->clone());
    for (QTextBlock block = source->begin(); block.isValid(); block = block.next()) {
        QTextCharFormat blockFormat = block.charFormat();
        if (encodeForeground(block.charFormat(), blockFormat)) {
            QTextCursor cursor(copy.get());
            cursor.setPosition(block.position());
            cursor.setBlockCharFormat(blockFormat);
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            const QFont font = format.font().resolve(source->defaultFont());
            QTextCharFormat explicitDecorations;
            encodeForeground(format, explicitDecorations);
            if (!format.hasProperty(QTextFormat::TextUnderlineStyle) && !format.hasProperty(QTextFormat::FontUnderline)) {
                explicitDecorations.setFontUnderline(font.underline());
            }
            if (!format.hasProperty(QTextFormat::FontOverline)) {
                explicitDecorations.setFontOverline(font.overline());
            }
            if (!format.hasProperty(QTextFormat::FontStrikeOut)) {
                explicitDecorations.setFontStrikeOut(font.strikeOut());
            }
            if (!explicitDecorations.isEmpty()) {
                QTextCursor cursor(copy.get());
                cursor.setPosition(fragment.position());
                cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                cursor.mergeCharFormat(explicitDecorations);
            }
        }
    }
    QFont defaultFont = copy->defaultFont();
    defaultFont.setUnderline(false);
    defaultFont.setOverline(false);
    defaultFont.setStrikeOut(false);
    copy->setDefaultFont(defaultFont);

    const QString serialized = copy->toHtml();
    QDomDocument html;
    if (!html.setContent(serialized, QDomDocument::ParseOption::PreserveSpacingOnlyNodes)) {
        qWarning() << "Could not parse generated title HTML for color serialization";
        // Never return the export copy's placeholder colors.
        return source->toHtml();
    }
    // Qt HTML accepts #AARRGGBB without the decimal-alpha conversion loss.
    // Only rewrite foreground-color declarations, never the title's text.
    const QRegularExpression rgba(QStringLiteral(R"(((?:^|;)\s*color\s*:\s*)rgba\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([0-9.]+(?:[eE][+-]?[0-9]+)?)\s*\))"));
    const QRegularExpression opaqueForeground(QStringLiteral(R"(((?:^|;)\s*color\s*:\s*)(#[0-9a-fA-F]{6})(?=\s*(?:;|$)))"));
    QList<QDomElement> pending{html.documentElement()};
    while (!pending.isEmpty()) {
        QDomElement element = pending.takeLast();
        const QString style = element.attribute(QStringLiteral("style"));
        QString normalized;
        qsizetype previous = 0;
        auto matches = rgba.globalMatch(style);
        while (matches.hasNext()) {
            const auto match = matches.next();
            bool redOk = false, greenOk = false, blueOk = false, alphaOk = false;
            const int red = match.captured(2).toInt(&redOk);
            const int green = match.captured(3).toInt(&greenOk);
            const int blue = match.captured(4).toInt(&blueOk);
            const double alpha = match.captured(5).toDouble(&alphaOk);
            if (!redOk || !greenOk || !blueOk || !alphaOk || red > 255 || green > 255 || blue > 255 || !qIsFinite(alpha) || alpha < 0 || alpha > 1) {
                continue;
            }
            normalized += style.mid(previous, match.capturedStart() - previous);
            normalized += match.captured(1);
            normalized += QColor(red, green, blue, qRound(alpha * 255)).name(QColor::HexArgb);
            previous = match.capturedEnd();
        }
        const QString exactStyle = previous > 0 ? normalized + style.mid(previous) : style;
        QString restored;
        previous = 0;
        auto tokens = opaqueForeground.globalMatch(exactStyle);
        while (tokens.hasNext()) {
            const auto match = tokens.next();
            const auto color = originalColors.constFind(match.captured(2).toLower());
            if (color == originalColors.cend()) {
                continue;
            }
            restored += exactStyle.mid(previous, match.capturedStart() - previous);
            restored += match.captured(1) + color.value();
            previous = match.capturedEnd();
        }
        const QString finalStyle = previous > 0 ? restored + exactStyle.mid(previous) : exactStyle;
        if (finalStyle != style) {
            element.setAttribute(QStringLiteral("style"), finalStyle);
        }
        for (QDomElement child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
            pending.append(child);
        }
    }
    return html.toString(-1);
}
} // namespace

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

int TitleDocument::base64ToUrl(const QGraphicsItem *item, QDomElement &content, bool embed, const QString &projectPath)
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
            if (!projectPath.isEmpty()) {
                titlePath = projectPath;
            } else {
                titlePath = QStringLiteral("/tmp/titles/");
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
    QDir dir(titlePath);
    dir.mkpath(titlePath);
    const QString filename = dir.absoluteFilePath(QString(QCryptographicHash::hash(data.toLatin1(), QCryptographicHash::Md5).toHex().append(".titlepart")));
    QFile f(filename);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QByteArray::fromBase64(data.toLatin1()));
        f.close();
        return filename;
    }
    return QString();
}

QDomDocument TitleDocument::xml(QGraphicsRectItem *startv, QGraphicsRectItem *endv, bool embed, const QString &saveFolder)
{
    return xml(m_scene->items(), m_width, m_height, startv, endv, embed, saveFolder.isEmpty() ? m_projectPath : saveFolder);
}

QDomDocument TitleDocument::xml(const QList<QGraphicsItem *> &items, int width, int height, QGraphicsRectItem *startv, QGraphicsRectItem *endv,
                                bool embedImages, const QString &projectPath)
{
    QDomDocument doc;

    QDomElement main = doc.createElement(QStringLiteral("kdenlivetitle"));
    main.setAttribute(QStringLiteral("width"), width);
    main.setAttribute(QStringLiteral("height"), height);

    // Save locale. Since 20.08, we always use the C locale for serialising.
    main.setAttribute(QStringLiteral("LC_NUMERIC"), "C");
    doc.appendChild(main);

    for (QGraphicsItem *item : items) {
        if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
            continue;
        }

        QDomDocument xmlDocument = xmlItem(item, width, height, embedImages, projectPath);
        if (!xmlDocument.hasChildNodes()) continue;

        if (item->zValue() > -1000) {
            main.appendChild(doc.importNode(xmlDocument.documentElement(), true));
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
    QColor color = getBackgroundColor(items);
    backgr.setAttribute(QStringLiteral("color"), colorToString(color));
    main.appendChild(backgr);

    return doc;
}

QDomDocument TitleDocument::xmlItem(const QGraphicsItem *item, int width, int height, bool embedImages, const QString &projectPath)
{
    Q_UNUSED(height); // In case we need it (width is used though).

    QDomDocument doc;
    QDomElement e = doc.createElement(QStringLiteral("item"));
    QDomElement content = doc.createElement(QStringLiteral("content"));
    QString gradient;
    double xPosition = item->pos().x();

    switch (item->type()) {
    case QGraphicsPixmapItem::Type: {
        e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsPixmapItem"));
        std::pair<const QString, bool> adjustedPath = pCore->currentDoc()->ensureRelativePath(item->data(Qt::UserRole).toString(), projectPath);
        if (adjustedPath.second) {
            content.setAttribute(QStringLiteral("url"), adjustedPath.first);
        } else {
            content.setAttribute(QStringLiteral("url"), item->data(Qt::UserRole).toString());
        }
        base64ToUrl(item, content, embedImages, projectPath);
        break;
    }
    case QGraphicsSvgItem::Type: {
        e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsSvgItem"));
        std::pair<const QString, bool> adjustedPath = pCore->currentDoc()->ensureRelativePath(item->data(Qt::UserRole).toString(), projectPath);
        if (adjustedPath.second) {
            content.setAttribute(QStringLiteral("url"), adjustedPath.first);
        } else {
            content.setAttribute(QStringLiteral("url"), item->data(Qt::UserRole).toString());
        }
        base64ToUrl(item, content, embedImages, projectPath);
        break;
    }
    case QGraphicsRectItem::Type:
        e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsRectItem"));
        content.setAttribute(QStringLiteral("rect"), rectFToString(static_cast<const QGraphicsRectItem *>(item)->rect().normalized()));
        content.setAttribute(QStringLiteral("pencolor"), colorToString(static_cast<const QGraphicsRectItem *>(item)->pen().color()));
        if (static_cast<const QGraphicsRectItem *>(item)->pen() == Qt::NoPen) {
            content.setAttribute(QStringLiteral("penwidth"), 0);
        } else {
            content.setAttribute(QStringLiteral("penwidth"), static_cast<const QGraphicsRectItem *>(item)->pen().width());
        }
        content.setAttribute(QStringLiteral("brushcolor"), colorToString(static_cast<const QGraphicsRectItem *>(item)->brush().color()));
        gradient = item->data(TitleDocument::Gradient).toString();
        if (!gradient.isEmpty()) {
            content.setAttribute(QStringLiteral("gradient"), gradient);
        }
        content.setAttribute(QStringLiteral("cornerRadius"), static_cast<const MyRectItem *>(item)->cornerRadius());
        break;
    case QGraphicsEllipseItem::Type:
        e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsEllipseItem"));
        content.setAttribute(QStringLiteral("rect"), rectFToString(static_cast<const QGraphicsEllipseItem *>(item)->rect().normalized()));
        content.setAttribute(QStringLiteral("pencolor"), colorToString(static_cast<const QGraphicsEllipseItem *>(item)->pen().color()));
        if (static_cast<const QGraphicsEllipseItem *>(item)->pen() == Qt::NoPen) {
            content.setAttribute(QStringLiteral("penwidth"), 0);
        } else {
            content.setAttribute(QStringLiteral("penwidth"), static_cast<const QGraphicsEllipseItem *>(item)->pen().width());
        }
        content.setAttribute(QStringLiteral("brushcolor"), colorToString(static_cast<const QGraphicsEllipseItem *>(item)->brush().color()));
        gradient = item->data(TitleDocument::Gradient).toString();
        if (!gradient.isEmpty()) {
            content.setAttribute(QStringLiteral("gradient"), gradient);
        }
        break;
    case QGraphicsTextItem::Type: {
        QFont font;
        QTextCursor cur;
        QTextBlockFormat format;

        e.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsTextItem"));
        const MyTextItem *t = static_cast<const MyTextItem *>(item);
        // Don't save empty text nodes
        if (t->toPlainText().simplified().isEmpty()) {
            return {};
        }
        // Keep legacy plain text first so older Kdenlive/MLT versions still
        // have a usable fallback representation.
        content.appendChild(doc.createTextNode(t->toPlainText()));

        // Rich text: additive rich representation. Older readers ignore
        // this child and continue reading the first plain-text node above.
        QDomElement richText = doc.createElement(QStringLiteral("richtext"));
        richText.setAttribute(QStringLiteral("format"), QStringLiteral("qt-html-v1"));
        richText.appendChild(doc.createTextNode(titleRichTextHtml(t->document())));
        content.appendChild(richText);
        content.appendChild(TitlerSpacingV1::save(doc, t->document()));  // Rich text
        content.appendChild(TitlerGradientV1::save(doc, t->document())); // Rich text selective gradients
        const QDomElement outlines = TitlerOutline::save(doc, t->document());
        if (!outlines.isNull()) {
            content.appendChild(outlines);
        }

        // Use the first run for legacy fallback, without changing the live item.
        QTextCursor fallback(t->document());
        fallback.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        font = fallback.charFormat().font().resolve(t->font());
        if (font.pixelSize() <= 0) {
            font.setPixelSize(qMax(1, QFontInfo(font).pixelSize()));
        }
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
            // template text box, adjust size for later replacement text
            if (t->alignment() == Qt::AlignHCenter) {
                // grow dimensions on both sides
                double xcenter = item->pos().x() + (t->baseBoundingRect().width()) / 2;
                double offset = qMin(xcenter, width - xcenter);
                xPosition = xcenter - offset;
                content.setAttribute(QStringLiteral("box-width"), QString::number(2 * offset));
            } else if (t->alignment() == Qt::AlignRight) {
                // grow to the left
                double offset = item->pos().x() + (t->baseBoundingRect().width());
                xPosition = 0;
                content.setAttribute(QStringLiteral("box-width"), QString::number(offset));
            } else {
                // left align, grow on right side
                double offset = width - item->pos().x();
                content.setAttribute(QStringLiteral("box-width"), QString::number(offset));
            }
        } else {
            content.setAttribute(QStringLiteral("box-width"), QString::number(t->baseBoundingRect().width(), 'g', 17));
        }
        content.setAttribute(QStringLiteral("box-height"), QString::number(t->baseBoundingRect().height(), 'g', 17));
        if (!t->data(TitleDocument::LineSpacing).isNull()) {
            content.setAttribute(QStringLiteral("line-spacing"), QString::number(t->data(TitleDocument::LineSpacing).toInt()));
        }
        {
            // Set tab width for MLT
            QTextOption options = t->document()->defaultTextOption();
            qreal tabWidth = options.tabStopDistance();
            content.setAttribute(QStringLiteral("tab-width"), QString::number(int(tabWidth)));
            // Font outline
            QTextCursor cursor(t->document());
            cursor.select(QTextCursor::Document);
            const QBrush firstBrush = fallback.charFormat().foreground();
            QColor fontcolor = firstBrush.style() == Qt::SolidPattern ? firstBrush.color() : t->defaultTextColor();
            if (!fontcolor.isValid()) {
                fontcolor = Qt::white;
            }
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
            content.setAttribute(QStringLiteral("alignment"), int(t->alignment()));
        }

        content.setAttribute(QStringLiteral("shadow"), t->shadowInfo().join(QLatin1Char(';')));
        content.setAttribute(QStringLiteral("typewriter"), t->twInfo().join(QLatin1Char(';')));
        break;
    }
    default:
        return doc;
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

    doc.appendChild(e);
    return doc;
}

/** \brief Get the background color (incl. alpha) from the document, if possibly
 * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
QColor TitleDocument::getBackgroundColor() const
{
    QColor color(0, 0, 0, 0);
    if (m_scene) {
        return getBackgroundColor(m_scene->items());
    }
    return color;
}

/** \brief Get the background color (incl. alpha) from list of items, if possibly
 * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
QColor TitleDocument::getBackgroundColor(const QList<QGraphicsItem *> &items)
{
    QColor color(0, 0, 0, 0);
    for (auto item : std::as_const(items)) {
        if (int(item->zValue()) == -1100) {
            color = static_cast<QGraphicsRectItem *>(item)->brush().color();
            return color;
        }
    }
    return color;
}

bool TitleDocument::saveDocument(const QUrl &url, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int duration, bool embed)
{
    if (!m_scene) {
        return false;
    }

    QDomDocument doc = xml(startv, endv, embed, QDir::cleanPath(url.adjusted(QUrl::RemoveFilename).toLocalFile()) + QLatin1Char('/'));
    doc.documentElement().setAttribute(QStringLiteral("duration"), duration);
    // keep some time for backwards compatibility (opening projects with older versions) - 26/12/12
    doc.documentElement().setAttribute(QStringLiteral("out"), duration);
    return Xml::docContentToFile(doc, url.toLocalFile());
}

int TitleDocument::loadFromXml(const QString &path, const QDomDocument &doc, GraphicsSceneRectMove *scene, QGraphicsRectItem *startv, QGraphicsRectItem *endv,
                               int *duration, const QString &projectpath)
{
    m_projectPath = projectpath;

    QList<QGraphicsItem *> items;

    int width, height;
    int res = loadFromXml(path, doc, items, width, height, scene, startv, endv, duration, m_missingElements);

    if (m_width != width || m_height != height) {
        KMessageBox::information(QApplication::activeWindow(), i18n("This title clip was created with a different frame size."), i18n("Title Profile"));
        // TODO: convert using QTransform
        m_width = width;
        m_height = height;
    }

    for (auto i : std::as_const(items)) {
        m_scene->addItem(i);
    }
    return res;
}

int TitleDocument::loadFromXml(const QString &path, const QDomDocument &doc, QList<QGraphicsItem *> &gitems, int &width, int &height,
                               GraphicsSceneRectMove *scene, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int *duration, int &missingElements)
{
    for (auto *i : gitems) {
        delete i;
    }
    gitems.clear();

    missingElements = 0;
    QDomNodeList titles = doc.elementsByTagName(QStringLiteral("kdenlivetitle"));
    // TODO: Check if the opened title size is equal to project size, otherwise warn user and rescale
    if (doc.documentElement().hasAttribute(QStringLiteral("width")) && doc.documentElement().hasAttribute(QStringLiteral("height"))) {
        width = doc.documentElement().attribute(QStringLiteral("width")).toInt();
        height = doc.documentElement().attribute(QStringLiteral("height")).toInt();
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
            QDomNode itemNode = items.item(i);
            if (itemNode.isDocument() && !itemNode.firstChild().isNull()) {
                itemNode = itemNode.firstChild();
            }

            if (itemNode.nodeName() == QLatin1String("item")) {
                QGraphicsItem *gitem = loadItemFromXml(itemNode, path, width, height, missingElements, maxZValue);
                if (gitem) {
                    gitems.append(gitem);
                }
            } else if (itemNode.nodeName() == QLatin1String("background")) {
                QColor color = QColor(stringToColor(itemNode.attributes().namedItem(QStringLiteral("color")).nodeValue()));
                // color.setAlpha(itemNode.attributes().namedItem("alpha").nodeValue().toInt());
                if (scene) {
                    QList<QGraphicsItem *> sceneItems = scene->items();
                    for (auto sceneItem : std::as_const(sceneItems)) {
                        if (int(sceneItem->zValue()) == -1100) {
                            static_cast<QGraphicsRectItem *>(sceneItem)->setBrush(QBrush(color));
                            break;
                        }
                    }
                    scene->setBackgroundBrush(QBrush(color));
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

QGraphicsItem *TitleDocument::loadItemFromXml(const QDomNode &itemNode, const QString &path, int width, int height, int &missingElements, int &maxZValue)
{
    Q_UNUSED(width);

    QDir rootDir(QFileInfo(path).absolutePath());
    QGraphicsItem *gitem = nullptr;
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
                font.setWeight(QFont::Weight(txtProperties.namedItem(QStringLiteral("font-weight")).nodeValue().toInt()));
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

            QDomElement contentElement = itemNode.namedItem(QStringLiteral("content")).toElement();
            QDomElement richTextElement = contentElement.firstChildElement(QStringLiteral("richtext"));
            const bool hasRichText = !richTextElement.isNull() && richTextElement.attribute(QStringLiteral("format")) == QLatin1String("qt-html-v1") &&
                                     !richTextElement.text().isEmpty();

            // The first child remains the legacy plain-text fallback.
            MyTextItem *txt = new MyTextItem(contentElement.firstChild().nodeValue(), nullptr);
            if (hasRichText) {
                // The legacy fallback font describes the first run, not every character.
                font.setUnderline(false);
                font.setOverline(false);
                font.setStrikeOut(false);
            }
            txt->setFont(font);

            if (hasRichText) {
                // Rich text: restore the QTextDocument character runs before
                // applying the legacy object-level effects below.
                txt->setHtml(richTextElement.text());
                if (!TitlerSpacingV1::restore(contentElement, txt->document())) {
                    qWarning() << "Ignoring invalid title rich-text spacing metadata";
                }
                if (!TitlerGradientV1::restore(contentElement, txt->document())) {
                    qWarning() << "Ignoring invalid title rich-text gradient metadata";
                }
                txt->document()->setDocumentMargin(0);
                TitlerGradientV1::applyBrushes(txt->document(), int(txt->baseBoundingRect().width()), int(txt->baseBoundingRect().height()));
            }

            txt->setTextInteractionFlags(Qt::NoTextInteraction);

            QTextCursor cursor(txt->document());
            cursor.select(QTextCursor::Document);

            QTextCharFormat globalFormat;
            bool hasGlobalFormat = false;

            txt->setOutline(txtProperties.namedItem(QStringLiteral("font-outline")).nodeValue().toDouble(),
                            stringToColor(txtProperties.namedItem(QStringLiteral("font-outline-color")).nodeValue()));
            if (hasRichText && !TitlerOutline::restore(contentElement, txt->document())) {
                qWarning() << "Ignoring invalid title rich-text outline metadata";
            }

            if (!txtProperties.namedItem(QStringLiteral("line-spacing")).isNull()) {
                int lineSpacing = txtProperties.namedItem(QStringLiteral("line-spacing")).nodeValue().toInt();
                QTextBlockFormat format;
                format.setLineHeight(lineSpacing, QTextBlockFormat::LineDistanceHeight);
                cursor.mergeBlockFormat(format);
                txt->setData(TitleDocument::LineSpacing, lineSpacing);
            }

            if (!hasRichText) {
                // Legacy titles still get their object-level solid colour.
                txt->setDefaultTextColor(col);
                globalFormat.setForeground(QBrush(col));
                hasGlobalFormat = true;
            }

            if (!txtProperties.namedItem(QStringLiteral("gradient")).isNull()) {
                // Gradient is still object-level for now, but no longer
                // destroys the saved character fonts/weights/sizes.
                QString data = txtProperties.namedItem(QStringLiteral("gradient")).nodeValue();
                txt->setData(TitleDocument::Gradient, data);

                QLinearGradient gr = GradientWidget::gradientFromString(data, int(txt->baseBoundingRect().width()), int(txt->baseBoundingRect().height()));

                globalFormat.setForeground(QBrush(gr));
                hasGlobalFormat = true;
            }

            if (hasGlobalFormat) {
                cursor.mergeCharFormat(globalFormat);
            }

            if (!txtProperties.namedItem(QStringLiteral("alignment")).isNull()) {
                txt->setAlignment(Qt::Alignment(txtProperties.namedItem(QStringLiteral("alignment")).nodeValue().toInt()));
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
                QString info = txtProperties.namedItem(QStringLiteral("typewriter")).nodeValue();
                txt->loadTW(info.split(QLatin1Char(';')));
            }
            if (txt->toPlainText() == QLatin1String("%s")) {
                // template text box, adjust size for later replacement text
                if (txt->alignment() == Qt::AlignHCenter) {
                    // grow dimensions on both sides
                    double boxWidth = txtProperties.namedItem(QStringLiteral("box-width")).nodeValue().toDouble();
                    double xcenter = (boxWidth - xPosition) / 2.0;
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
            double cornerRadius = rectProperties.namedItem(QStringLiteral("cornerRadius")).nodeValue().toDouble();
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
                QLinearGradient gr = GradientWidget::gradientFromString(data, int(rec->rect().width()), int(rec->rect().height()));
                rec->setBrush(QBrush(gr));
            } else {
                rec->setBrush(QBrush(stringToColor(br_str)));
            }
            rec->setCornerRadius(cornerRadius);

            gitem = rec;
        } else if (itemNode.attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsEllipseItem")) {
            QDomNamedNodeMap ellipseProperties = itemNode.namedItem(QStringLiteral("content")).attributes();
            QString rect = ellipseProperties.namedItem(QStringLiteral("rect")).nodeValue();
            QString br_str = ellipseProperties.namedItem(QStringLiteral("brushcolor")).nodeValue();
            QString pen_str = ellipseProperties.namedItem(QStringLiteral("pencolor")).nodeValue();
            double penwidth = ellipseProperties.namedItem(QStringLiteral("penwidth")).nodeValue().toDouble();
            auto *ellipse = new MyEllipseItem();
            ellipse->setRect(stringToRect(rect));
            if (penwidth > 0) {
                ellipse->setPen(QPen(QBrush(stringToColor(pen_str)), penwidth, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
            } else {
                ellipse->setPen(Qt::NoPen);
            }
            if (!ellipseProperties.namedItem(QStringLiteral("gradient")).isNull()) {
                // Gradient color
                QString data = ellipseProperties.namedItem(QStringLiteral("gradient")).nodeValue();
                ellipse->setData(TitleDocument::Gradient, data);
                QLinearGradient gr = GradientWidget::gradientFromString(data, int(ellipse->rect().width()), int(ellipse->rect().height()));
                ellipse->setBrush(QBrush(gr));
            } else {
                ellipse->setBrush(QBrush(stringToColor(br_str)));
            }

            gitem = ellipse;
        } else if (itemNode.attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsPixmapItem")) {
            QString url = itemNode.namedItem(QStringLiteral("content")).attributes().namedItem(QStringLiteral("url")).nodeValue();
            QString base64 = itemNode.namedItem(QStringLiteral("content")).attributes().namedItem(QStringLiteral("base64")).nodeValue();
            QPixmap pix;
            bool missing = false;
            if (base64.isEmpty()) {
                if (QFileInfo(url).isRelative()) {
                    url = QDir::cleanPath(rootDir.absoluteFilePath(url));
                }
                pix.load(url);
                if (pix.isNull()) {
                    pix = createInvalidPixmap(url, width / 4, height / 4);
                    missingElements++;
                    missing = true;
                }
            } else {
                pix.loadFromData(QByteArray::fromBase64(base64.toLatin1()));
            }
            auto *rec = new MyPixmapItem(pix);
            if (missing) {
                rec->setData(Qt::UserRole + 2, 1);
            }
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
                if (QFileInfo(url).isRelative()) {
                    url = QDir::cleanPath(rootDir.absoluteFilePath(url));
                }
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
                rec->setData(Qt::UserRole, url);
                if (!base64.isEmpty()) {
                    rec->setData(Qt::UserRole + 1, base64);
                }
                gitem = rec;
            } else {
                QPixmap pix = createInvalidPixmap(url, width / 4, height / 4);
                missingElements++;
                auto *rec2 = new MyPixmapItem(pix);
                rec2->setData(Qt::UserRole + 2, 1);
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

    return gitem;
}

int TitleDocument::invalidCount() const
{
    return m_missingElements;
}

QPixmap TitleDocument::createInvalidPixmap(const QString &url, int width, int height)
{
    QPixmap pix(width, height);
    QIcon icon = QIcon::fromTheme(QStringLiteral("emblem-warning"));
    pix.fill(QColor(255, 0, 0, 50));
    QPainter ptr(&pix);
    int iconSize = qApp->style()->pixelMetric(QStyle::PM_LargeIconSize);
    icon.paint(&ptr, 4, 4, iconSize, iconSize);
    QPen pen(Qt::red);
    pen.setWidth(3);
    ptr.setPen(pen);
    ptr.drawText(QRectF(2, 2, width - 4, height - 4), Qt::AlignHCenter | Qt::AlignVCenter, QFileInfo(url).fileName());
    ptr.drawRect(2, 1, width - 4, height - 4);
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
