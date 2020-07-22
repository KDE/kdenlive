/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "documentvalidator.h"

#include "bin/binplaylist.hpp"
#include "core.h"
#include "definitions.h"
#include "effects/effectsrepository.hpp"
#include "mainwindow.h"
#include "transitions/transitionsrepository.hpp"
#include "xml/xml.hpp"

#include "kdenlive_debug.h"
#include <KMessageBox>
#include <klocalizedstring.h>

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <mlt++/Mlt.h>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

#include <QStandardPaths>
#include <lib/localeHandling.h>
#include <utility>

DocumentValidator::DocumentValidator(const QDomDocument &doc, QUrl documentUrl)
    : m_doc(doc)
    , m_url(std::move(documentUrl))
    , m_modified(false)
{
}

QPair<bool, QString> DocumentValidator::validate(const double currentVersion)
{
    QDomElement mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
    // At least the root element must be there
    if (mlt.isNull()) {
        return QPair<bool, QString>(false, QString());
    }
    QDomElement kdenliveDoc = mlt.firstChildElement(QStringLiteral("kdenlivedoc"));
    QString rootDir = mlt.attribute(QStringLiteral("root"));
    if (rootDir == QLatin1String("$CURRENTPATH")) {
        // The document was extracted from a Kdenlive archived project, fix root directory
        QString playlist = m_doc.toString();
        playlist.replace(QLatin1String("$CURRENTPATH"), m_url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile());
        m_doc.setContent(playlist);
        mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
        kdenliveDoc = mlt.firstChildElement(QStringLiteral("kdenlivedoc"));
    } else if (rootDir.isEmpty()) {
        mlt.setAttribute(QStringLiteral("root"), m_url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile());
    }

    QLocale documentLocale = QLocale::c(); // Document locale for conversion. Previous MLT / Kdenlive versions used C locale by default

    if (mlt.hasAttribute(QStringLiteral("LC_NUMERIC"))) { // Backwards compatibility
        // Check document numeric separator (added in Kdenlive 16.12.1 and removed in Kdenlive 20.08)
        QDomElement main_playlist = mlt.firstChildElement(QStringLiteral("playlist"));
        QString sep = Xml::getXmlProperty(main_playlist, "kdenlive:docproperties.decimalPoint", QString("."));
        QString mltLocale = mlt.attribute(QStringLiteral("LC_NUMERIC"), "C"); // Backwards compatibility
        qDebug() << "LOCALE: Document uses " << sep << " as decimal point and " << mltLocale << " as locale";

        auto localeMatch = LocaleHandling::getQLocaleForDecimalPoint(mltLocale, sep);
        qDebug() << "Searching for locale: Found " << localeMatch.first << " with match type " << (int)localeMatch.second;

        if (localeMatch.second == LocaleHandling::MatchType::NoMatch) {
            // Requested locale not available, ask for install
            KMessageBox::sorry(QApplication::activeWindow(),
                               i18n("The document was created in \"%1\" locale, which is not installed on your system. Please install that language pack. "
                                    "Until then, Kdenlive might not be able to correctly open the document.",
                                    mltLocale));
        }

        documentLocale.setNumberOptions(QLocale::OmitGroupSeparator);
        documentLocale = localeMatch.first;
    }

    double version = -1;
    if (kdenliveDoc.isNull() || !kdenliveDoc.hasAttribute(QStringLiteral("version"))) {
        // Newer Kdenlive document version
        QDomElement main = mlt.firstChildElement(QStringLiteral("playlist"));
        version = Xml::getXmlProperty(main, QStringLiteral("kdenlive:docproperties.version")).toDouble();
    } else {
        bool ok;
        version = documentLocale.toDouble(kdenliveDoc.attribute(QStringLiteral("version")), &ok);
        if (!ok) {
            // Could not parse version number, there is probably a conflict in decimal separator
            QLocale tempLocale = QLocale(mlt.attribute(QStringLiteral("LC_NUMERIC"))); // Try parsing version number with document locale
            version = tempLocale.toDouble(kdenliveDoc.attribute(QStringLiteral("version")), &ok);
            if (!ok) {
                version = kdenliveDoc.attribute(QStringLiteral("version")).toDouble(&ok);
            }
            if (!ok) {
                // Last try: replace comma with a dot
                QString versionString = kdenliveDoc.attribute(QStringLiteral("version"));
                if (versionString.contains(QLatin1Char(','))) {
                    versionString.replace(QLatin1Char(','), QLatin1Char('.'));
                }
                version = versionString.toDouble(&ok);
                if (!ok) {
                    qCDebug(KDENLIVE_LOG) << "// CANNOT PARSE VERSION NUMBER, ERROR!";
                }
            }
        }
    }
    // Upgrade the document to the latest version
    if (!upgrade(version, currentVersion)) {
        return QPair<bool, QString>(false, QString());
    }

    if (version < 0.97) {
        checkOrphanedProducers();
    }

    QString changedDecimalPoint;
    if (version < 1.00) {
        changedDecimalPoint = upgradeTo100(documentLocale);
    }

    return QPair<bool, QString>(true, changedDecimalPoint);
}

bool DocumentValidator::upgrade(double version, const double currentVersion)
{
    qCDebug(KDENLIVE_LOG) << "Opening a document with version " << version << " / " << currentVersion;

    // No conversion needed
    if (qFuzzyCompare(version, currentVersion)) {
        return true;
    }

    // The document is too new
    if (version > currentVersion) {
        // qCDebug(KDENLIVE_LOG) << "Unable to open document with version " << version;
        KMessageBox::sorry(
            QApplication::activeWindow(),
            i18n("This project type is unsupported (version %1) and cannot be loaded.\nPlease consider upgrading your Kdenlive version.", version),
            i18n("Unable to open project"));
        return false;
    }

    // Unsupported document versions
    if (qFuzzyCompare(version, 0.5) || qFuzzyCompare(version, 0.7)) {
        // 0.7 is unsupported
        // qCDebug(KDENLIVE_LOG) << "Unable to open document with version " << version;
        KMessageBox::sorry(QApplication::activeWindow(), i18n("This project type is unsupported (version %1) and cannot be loaded.", version),
                           i18n("Unable to open project"));
        return false;
    }

    // <kdenlivedoc />
    QDomNode infoXmlNode;
    QDomElement infoXml;
    QDomNodeList docs = m_doc.elementsByTagName(QStringLiteral("kdenlivedoc"));
    if (!docs.isEmpty()) {
        infoXmlNode = m_doc.elementsByTagName(QStringLiteral("kdenlivedoc")).at(0);
        infoXml = infoXmlNode.toElement();
        infoXml.setAttribute(QStringLiteral("upgraded"), 1);
    }
    m_doc.documentElement().setAttribute(QStringLiteral("upgraded"), 1);

    if (version <= 0.6) {
        QDomElement infoXml_old = infoXmlNode.cloneNode(true).toElement(); // Needed for folders
        QDomNode westley = m_doc.elementsByTagName(QStringLiteral("westley")).at(1);
        QDomNode tractor = m_doc.elementsByTagName(QStringLiteral("tractor")).at(0);
        QDomNode multitrack = m_doc.elementsByTagName(QStringLiteral("multitrack")).at(0);
        QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));

        QDomNode props = m_doc.elementsByTagName(QStringLiteral("properties")).at(0).toElement();
        QString profile = props.toElement().attribute(QStringLiteral("videoprofile"));
        int startPos = props.toElement().attribute(QStringLiteral("timeline_position")).toInt();
        if (profile == QLatin1String("dv_wide")) {
            profile = QStringLiteral("dv_pal_wide");
        }

        // move playlists outside of tractor and add the tracks instead
        int max = playlists.count();
        if (westley.isNull()) {
            westley = m_doc.createElement(QStringLiteral("westley"));
            m_doc.documentElement().appendChild(westley);
        }
        if (tractor.isNull()) {
            // qCDebug(KDENLIVE_LOG) << "// NO MLT PLAYLIST, building empty one";
            QDomElement blank_tractor = m_doc.createElement(QStringLiteral("tractor"));
            westley.appendChild(blank_tractor);
            QDomElement blank_playlist = m_doc.createElement(QStringLiteral("playlist"));
            blank_playlist.setAttribute(QStringLiteral("id"), QStringLiteral("black_track"));
            westley.insertBefore(blank_playlist, QDomNode());
            QDomElement blank_track = m_doc.createElement(QStringLiteral("track"));
            blank_track.setAttribute(QStringLiteral("producer"), QStringLiteral("black_track"));
            blank_tractor.appendChild(blank_track);

            QDomNodeList kdenlivetracks = m_doc.elementsByTagName(QStringLiteral("kdenlivetrack"));
            for (int i = 0; i < kdenlivetracks.count(); ++i) {
                blank_playlist = m_doc.createElement(QStringLiteral("playlist"));
                blank_playlist.setAttribute(QStringLiteral("id"), QStringLiteral("playlist") + QString::number(i));
                westley.insertBefore(blank_playlist, QDomNode());
                blank_track = m_doc.createElement(QStringLiteral("track"));
                blank_track.setAttribute(QStringLiteral("producer"), QStringLiteral("playlist") + QString::number(i));
                blank_tractor.appendChild(blank_track);
                if (kdenlivetracks.at(i).toElement().attribute(QStringLiteral("cliptype")) == QLatin1String("Sound")) {
                    blank_playlist.setAttribute(QStringLiteral("hide"), QStringLiteral("video"));
                    blank_track.setAttribute(QStringLiteral("hide"), QStringLiteral("video"));
                }
            }
        } else
            for (int i = 0; i < max; ++i) {
                QDomNode n = playlists.at(i);
                westley.insertBefore(n, QDomNode());
                QDomElement pl = n.toElement();
                QDomElement track = m_doc.createElement(QStringLiteral("track"));
                QString trackType = pl.attribute(QStringLiteral("hide"));
                if (!trackType.isEmpty()) {
                    track.setAttribute(QStringLiteral("hide"), trackType);
                }
                QString playlist_id = pl.attribute(QStringLiteral("id"));
                if (playlist_id.isEmpty()) {
                    playlist_id = QStringLiteral("black_track");
                    pl.setAttribute(QStringLiteral("id"), playlist_id);
                }
                track.setAttribute(QStringLiteral("producer"), playlist_id);
// tractor.appendChild(track);
#define KEEP_TRACK_ORDER 1
#ifdef KEEP_TRACK_ORDER
                tractor.insertAfter(track, QDomNode());
#else
                // Insert the new track in an order that hopefully matches the 3 video, then 2 audio tracks of Kdenlive 0.7.0
                // insertion sort - O( tracks*tracks )
                // Note, this breaks _all_ transitions - but you can move them up and down afterwards.
                QDomElement tractor_elem = tractor.toElement();
                if (!tractor_elem.isNull()) {
                    QDomNodeList tracks = tractor_elem.elementsByTagName("track");
                    int size = tracks.size();
                    if (size == 0) {
                        tractor.insertAfter(track, QDomNode());
                    } else {
                        bool inserted = false;
                        for (int i = 0; i < size; ++i) {
                            QDomElement track_elem = tracks.at(i).toElement();
                            if (track_elem.isNull()) {
                                tractor.insertAfter(track, QDomNode());
                                inserted = true;
                                break;
                            } else {
                                // qCDebug(KDENLIVE_LOG) << "playlist_id: " << playlist_id << " producer:" << track_elem.attribute("producer");
                                if (playlist_id < track_elem.attribute("producer")) {
                                    tractor.insertBefore(track, track_elem);
                                    inserted = true;
                                    break;
                                }
                            }
                        }
                        // Reach here, no insertion, insert last
                        if (!inserted) {
                            tractor.insertAfter(track, QDomNode());
                        }
                    }
                } else {
                    qCWarning(KDENLIVE_LOG) << "tractor was not a QDomElement";
                    tractor.insertAfter(track, QDomNode());
                }
#endif
            }
        tractor.removeChild(multitrack);

        // audio track mixing transitions should not be added to track view, so add required attribute
        QDomNodeList transitions = m_doc.elementsByTagName(QStringLiteral("transition"));
        max = transitions.count();
        for (int i = 0; i < max; ++i) {
            QDomElement tr = transitions.at(i).toElement();
            if (tr.attribute(QStringLiteral("combine")) == QLatin1String("1") && tr.attribute(QStringLiteral("mlt_service")) == QLatin1String("mix")) {
                QDomElement property = m_doc.createElement(QStringLiteral("property"));
                property.setAttribute(QStringLiteral("name"), QStringLiteral("internal_added"));
                QDomText value = m_doc.createTextNode(QStringLiteral("237"));
                property.appendChild(value);
                tr.appendChild(property);
                property = m_doc.createElement(QStringLiteral("property"));
                property.setAttribute(QStringLiteral("name"), QStringLiteral("mlt_service"));
                value = m_doc.createTextNode(QStringLiteral("mix"));
                property.appendChild(value);
                tr.appendChild(property);
            } else {
                // convert transition
                QDomNamedNodeMap attrs = tr.attributes();
                for (int j = 0; j < attrs.count(); ++j) {
                    QString attrName = attrs.item(j).nodeName();
                    if (attrName != QLatin1String("in") && attrName != QLatin1String("out") && attrName != QLatin1String("id")) {
                        QDomElement property = m_doc.createElement(QStringLiteral("property"));
                        property.setAttribute(QStringLiteral("name"), attrName);
                        QDomText value = m_doc.createTextNode(attrs.item(j).nodeValue());
                        property.appendChild(value);
                        tr.appendChild(property);
                    }
                }
            }
        }

        // move transitions after tracks
        for (int i = 0; i < max; ++i) {
            tractor.insertAfter(transitions.at(0), QDomNode());
        }

        // Fix filters format
        QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));
        max = entries.count();
        for (int i = 0; i < max; ++i) {
            QString last_id;
            int effectix = 0;
            QDomNode m = entries.at(i).firstChild();
            while (!m.isNull()) {
                if (m.toElement().tagName() == QLatin1String("filter")) {
                    QDomElement filt = m.toElement();
                    QDomNamedNodeMap attrs = filt.attributes();
                    QString current_id = filt.attribute(QStringLiteral("kdenlive_id"));
                    if (current_id != last_id) {
                        effectix++;
                        last_id = current_id;
                    }
                    QDomElement e = m_doc.createElement(QStringLiteral("property"));
                    e.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive_ix"));
                    QDomText value = m_doc.createTextNode(QString::number(effectix));
                    e.appendChild(value);
                    filt.appendChild(e);
                    for (int j = 0; j < attrs.count(); ++j) {
                        QDomAttr a = attrs.item(j).toAttr();
                        if (!a.isNull()) {
                            // qCDebug(KDENLIVE_LOG) << " FILTER; adding :" << a.name() << ':' << a.value();
                            auto property = m_doc.createElement(QStringLiteral("property"));
                            property.setAttribute(QStringLiteral("name"), a.name());
                            auto property_value = m_doc.createTextNode(a.value());
                            property.appendChild(property_value);
                            filt.appendChild(property);
                        }
                    }
                }
                m = m.nextSibling();
            }
        }

        // fix slowmotion
        QDomNodeList producers = westley.toElement().elementsByTagName(QStringLiteral("producer"));
        max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (prod.attribute(QStringLiteral("mlt_service")) == QLatin1String("framebuffer")) {
                QString slowmotionprod = prod.attribute(QStringLiteral("resource"));
                slowmotionprod.replace(QLatin1Char(':'), QLatin1Char('?'));
                // qCDebug(KDENLIVE_LOG) << "// FOUND WRONG SLOWMO, new: " << slowmotionprod;
                prod.setAttribute(QStringLiteral("resource"), slowmotionprod);
            }
        }
        // move producers to correct place, markers to a global list, fix clip descriptions
        QDomElement markers = m_doc.createElement(QStringLiteral("markers"));
        // This will get the xml producers:
        producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(0).toElement();
            QDomNode m = prod.firstChild();
            if (!m.isNull()) {
                if (m.toElement().tagName() == QLatin1String("markers")) {
                    QDomNodeList prodchilds = m.childNodes();
                    int maxchild = prodchilds.count();
                    for (int k = 0; k < maxchild; ++k) {
                        QDomElement mark = prodchilds.at(0).toElement();
                        mark.setAttribute(QStringLiteral("id"), prod.attribute(QStringLiteral("id")));
                        markers.insertAfter(mark, QDomNode());
                    }
                    prod.removeChild(m);
                } else if (prod.attribute(QStringLiteral("type")).toInt() == (int)ClipType::Text) {
                    // convert title clip
                    if (m.toElement().tagName() == QLatin1String("textclip")) {
                        QDomDocument tdoc;
                        QDomElement titleclip = m.toElement();
                        QDomElement title = tdoc.createElement(QStringLiteral("kdenlivetitle"));
                        tdoc.appendChild(title);
                        QDomNodeList objects = titleclip.childNodes();
                        int maxchild = objects.count();
                        for (int k = 0; k < maxchild; ++k) {
                            QDomElement ob = objects.at(k).toElement();
                            if (ob.attribute(QStringLiteral("type")) == QLatin1String("3")) {
                                // text object - all of this goes into "xmldata"...
                                QDomElement item = tdoc.createElement(QStringLiteral("item"));
                                item.setAttribute(QStringLiteral("z-index"), ob.attribute(QStringLiteral("z")));
                                item.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsTextItem"));
                                QDomElement position = tdoc.createElement(QStringLiteral("position"));
                                position.setAttribute(QStringLiteral("x"), ob.attribute(QStringLiteral("x")));
                                position.setAttribute(QStringLiteral("y"), ob.attribute(QStringLiteral("y")));
                                QDomElement content = tdoc.createElement(QStringLiteral("content"));
                                content.setAttribute(QStringLiteral("font"), ob.attribute(QStringLiteral("font_family")));
                                content.setAttribute(QStringLiteral("font-size"), ob.attribute(QStringLiteral("font_size")));
                                content.setAttribute(QStringLiteral("font-bold"), ob.attribute(QStringLiteral("bold")));
                                content.setAttribute(QStringLiteral("font-italic"), ob.attribute(QStringLiteral("italic")));
                                content.setAttribute(QStringLiteral("font-underline"), ob.attribute(QStringLiteral("underline")));
                                QString col = ob.attribute(QStringLiteral("color"));
                                QColor c(col);
                                content.setAttribute(QStringLiteral("font-color"), colorToString(c));
                                // todo: These fields are missing from the newly generated xmldata:
                                // transform, startviewport, endviewport, background

                                QDomText conttxt = tdoc.createTextNode(ob.attribute(QStringLiteral("text")));
                                content.appendChild(conttxt);
                                item.appendChild(position);
                                item.appendChild(content);
                                title.appendChild(item);
                            } else if (ob.attribute(QStringLiteral("type")) == QLatin1String("5")) {
                                // rectangle object
                                QDomElement item = tdoc.createElement(QStringLiteral("item"));
                                item.setAttribute(QStringLiteral("z-index"), ob.attribute(QStringLiteral("z")));
                                item.setAttribute(QStringLiteral("type"), QStringLiteral("QGraphicsRectItem"));
                                QDomElement position = tdoc.createElement(QStringLiteral("position"));
                                position.setAttribute(QStringLiteral("x"), ob.attribute(QStringLiteral("x")));
                                position.setAttribute(QStringLiteral("y"), ob.attribute(QStringLiteral("y")));
                                QDomElement content = tdoc.createElement(QStringLiteral("content"));
                                QString col = ob.attribute(QStringLiteral("color"));
                                QColor c(col);
                                content.setAttribute(QStringLiteral("brushcolor"), colorToString(c));
                                QString rect = QStringLiteral("0,0,");
                                rect.append(ob.attribute(QStringLiteral("width")));
                                rect.append(QLatin1String(","));
                                rect.append(ob.attribute(QStringLiteral("height")));
                                content.setAttribute(QStringLiteral("rect"), rect);
                                item.appendChild(position);
                                item.appendChild(content);
                                title.appendChild(item);
                            }
                        }
                        prod.setAttribute(QStringLiteral("xmldata"), tdoc.toString());
                        prod.removeChild(m);
                    } // End conversion of title clips.

                } else if (m.isText()) {
                    QString comment = m.nodeValue();
                    if (!comment.isEmpty()) {
                        prod.setAttribute(QStringLiteral("description"), comment);
                    }
                    prod.removeChild(m);
                }
            }
            int duration = prod.attribute(QStringLiteral("duration")).toInt();
            if (duration > 0) {
                prod.setAttribute(QStringLiteral("out"), QString::number(duration));
            }
            // The clip goes back in, but text clips should not go back in, at least not modified
            westley.insertBefore(prod, QDomNode());
        }

        QDomNode westley0 = m_doc.elementsByTagName(QStringLiteral("westley")).at(0);
        if (!markers.firstChild().isNull()) {
            westley0.appendChild(markers);
        }

        /*
         * Convert as much of the kdenlivedoc as possible. Use the producer in
         * westley. First, remove the old stuff from westley, and add a new
         * empty one. Also, track the max id in order to use it for the adding
         * of groups/folders
         */
        int max_kproducer_id = 0;
        westley0.removeChild(infoXmlNode);
        QDomElement infoXml_new = m_doc.createElement(QStringLiteral("kdenlivedoc"));
        infoXml_new.setAttribute(QStringLiteral("profile"), profile);
        infoXml.setAttribute(QStringLiteral("position"), startPos);

        // Add all the producers that has a resource in westley
        QDomElement westley_element = westley0.toElement();
        if (westley_element.isNull()) {
            qCWarning(KDENLIVE_LOG) << "westley0 element in document was not a QDomElement - unable to add producers to new kdenlivedoc";
        } else {
            QDomNodeList wproducers = westley_element.elementsByTagName(QStringLiteral("producer"));
            int kmax = wproducers.count();
            for (int i = 0; i < kmax; ++i) {
                QDomElement wproducer = wproducers.at(i).toElement();
                if (wproducer.isNull()) {
                    qCWarning(KDENLIVE_LOG) << "Found producer in westley0, that was not a QDomElement";
                    continue;
                }
                if (wproducer.attribute(QStringLiteral("id")) == QLatin1String("black")) {
                    continue;
                }
                // We have to do slightly different things, depending on the type
                // qCDebug(KDENLIVE_LOG) << "Converting producer element with type" << wproducer.attribute("type");
                if (wproducer.attribute(QStringLiteral("type")).toInt() == (int)ClipType::Text) {
                    // qCDebug(KDENLIVE_LOG) << "Found TEXT element in producer" << endl;
                    QDomElement kproducer = wproducer.cloneNode(true).toElement();
                    kproducer.setTagName(QStringLiteral("kdenlive_producer"));
                    infoXml_new.appendChild(kproducer);
                    /*
                     * TODO: Perhaps needs some more changes here to
                     * "frequency", aspect ratio as a float, frame_size,
                     * channels, and later, resource and title name
                     */
                } else {
                    QDomElement kproducer = m_doc.createElement(QStringLiteral("kdenlive_producer"));
                    kproducer.setAttribute(QStringLiteral("id"), wproducer.attribute(QStringLiteral("id")));
                    if (!wproducer.attribute(QStringLiteral("description")).isEmpty()) {
                        kproducer.setAttribute(QStringLiteral("description"), wproducer.attribute(QStringLiteral("description")));
                    }
                    kproducer.setAttribute(QStringLiteral("resource"), wproducer.attribute(QStringLiteral("resource")));
                    kproducer.setAttribute(QStringLiteral("type"), wproducer.attribute(QStringLiteral("type")));
                    // Testing fix for 358
                    if (!wproducer.attribute(QStringLiteral("aspect_ratio")).isEmpty()) {
                        kproducer.setAttribute(QStringLiteral("aspect_ratio"), wproducer.attribute(QStringLiteral("aspect_ratio")));
                    }
                    if (!wproducer.attribute(QStringLiteral("source_fps")).isEmpty()) {
                        kproducer.setAttribute(QStringLiteral("fps"), wproducer.attribute(QStringLiteral("source_fps")));
                    }
                    if (!wproducer.attribute(QStringLiteral("length")).isEmpty()) {
                        kproducer.setAttribute(QStringLiteral("duration"), wproducer.attribute(QStringLiteral("length")));
                    }
                    infoXml_new.appendChild(kproducer);
                }
                if (wproducer.attribute(QStringLiteral("id")).toInt() > max_kproducer_id) {
                    max_kproducer_id = wproducer.attribute(QStringLiteral("id")).toInt();
                }
            }
        }
#define LOOKUP_FOLDER 1
#ifdef LOOKUP_FOLDER
        /*
         * Look through all the folder elements of the old doc, for each folder,
         * for each producer, get the id, look it up in the new doc, set the
         * groupname and groupid. Note, this does not work at the moment - at
         * least one folder shows up missing, and clips with no folder does not
         * show up.
         */
        // QDomElement infoXml_old = infoXmlNode.toElement();
        if (!infoXml_old.isNull()) {
            QDomNodeList folders = infoXml_old.elementsByTagName(QStringLiteral("folder"));
            int fsize = folders.size();
            int groupId = max_kproducer_id + 1; // Start at +1 of max id of the kdenlive_producers
            for (int i = 0; i < fsize; ++i) {
                QDomElement folder = folders.at(i).toElement();
                if (!folder.isNull()) {
                    QString groupName = folder.attribute(QStringLiteral("name"));
                    // qCDebug(KDENLIVE_LOG) << "groupName: " << groupName << " with groupId: " << groupId;
                    QDomNodeList fproducers = folder.elementsByTagName(QStringLiteral("producer"));
                    int psize = fproducers.size();
                    for (int j = 0; j < psize; ++j) {
                        QDomElement fproducer = fproducers.at(j).toElement();
                        if (!fproducer.isNull()) {
                            QString id = fproducer.attribute(QStringLiteral("id"));
                            // This is not very effective, but compared to loading the clips, its a breeze
                            QDomNodeList kdenlive_producers = infoXml_new.elementsByTagName(QStringLiteral("kdenlive_producer"));
                            int kpsize = kdenlive_producers.size();
                            for (int k = 0; k < kpsize; ++k) {
                                QDomElement kproducer = kdenlive_producers.at(k).toElement(); // Its an element for sure
                                if (id == kproducer.attribute(QStringLiteral("id"))) {
                                    // We do not check that it already is part of a folder
                                    kproducer.setAttribute(QStringLiteral("groupid"), groupId);
                                    kproducer.setAttribute(QStringLiteral("groupname"), groupName);
                                    break;
                                }
                            }
                        }
                    }
                    ++groupId;
                }
            }
        }
#endif
        QDomNodeList elements = westley.childNodes();
        max = elements.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = elements.at(0).toElement();
            westley0.insertAfter(prod, QDomNode());
        }

        westley0.appendChild(infoXml_new);

        westley0.removeChild(westley);

        // adds <avfile /> information to <kdenlive_producer />
        QDomNodeList kproducers = m_doc.elementsByTagName(QStringLiteral("kdenlive_producer"));
        QDomNodeList avfiles = infoXml_old.elementsByTagName(QStringLiteral("avfile"));
        // qCDebug(KDENLIVE_LOG) << "found" << avfiles.count() << "<avfile />s and" << kproducers.count() << "<kdenlive_producer />s";
        for (int i = 0; i < avfiles.count(); ++i) {
            QDomElement avfile = avfiles.at(i).toElement();
            QDomElement kproducer;
            if (avfile.isNull()) {
                qCWarning(KDENLIVE_LOG) << "found an <avfile /> that is not a QDomElement";
            } else {
                QString id = avfile.attribute(QStringLiteral("id"));
                // this is horrible, must be rewritten, it's just for test
                for (int j = 0; j < kproducers.count(); ++j) {
                    ////qCDebug(KDENLIVE_LOG) << "checking <kdenlive_producer /> with id" << kproducers.at(j).toElement().attribute("id");
                    if (kproducers.at(j).toElement().attribute(QStringLiteral("id")) == id) {
                        kproducer = kproducers.at(j).toElement();
                        break;
                    }
                }
                if (kproducer == QDomElement()) {
                    qCWarning(KDENLIVE_LOG) << "no match for <avfile /> with id =" << id;
                } else {
                    ////qCDebug(KDENLIVE_LOG) << "ready to set additional <avfile />'s attributes (id =" << id << ')';
                    kproducer.setAttribute(QStringLiteral("channels"), avfile.attribute(QStringLiteral("channels")));
                    kproducer.setAttribute(QStringLiteral("duration"), avfile.attribute(QStringLiteral("duration")));
                    kproducer.setAttribute(QStringLiteral("frame_size"),
                                           avfile.attribute(QStringLiteral("width")) + QLatin1Char('x') + avfile.attribute(QStringLiteral("height")));
                    kproducer.setAttribute(QStringLiteral("frequency"), avfile.attribute(QStringLiteral("frequency")));
                    if (kproducer.attribute(QStringLiteral("description")).isEmpty() && !avfile.attribute(QStringLiteral("description")).isEmpty()) {
                        kproducer.setAttribute(QStringLiteral("description"), avfile.attribute(QStringLiteral("description")));
                    }
                }
            }
        }
        infoXml = infoXml_new;
    }

    if (version <= 0.81) {
        // Add the tracks information
        QString tracksOrder = infoXml.attribute(QStringLiteral("tracks"));
        if (tracksOrder.isEmpty()) {
            QDomNodeList tracks = m_doc.elementsByTagName(QStringLiteral("track"));
            for (int i = 0; i < tracks.count(); ++i) {
                QDomElement track = tracks.at(i).toElement();
                if (track.attribute(QStringLiteral("producer")) != QLatin1String("black_track")) {
                    if (track.attribute(QStringLiteral("hide")) == QLatin1String("video")) {
                        tracksOrder.append(QLatin1Char('a'));
                    } else {
                        tracksOrder.append(QLatin1Char('v'));
                    }
                }
            }
        }
        QDomElement tracksinfo = m_doc.createElement(QStringLiteral("tracksinfo"));
        for (int i = 0; i < tracksOrder.size(); ++i) {
            QDomElement trackinfo = m_doc.createElement(QStringLiteral("trackinfo"));
            if (tracksOrder.data()[i] == QLatin1Char('a')) {
                trackinfo.setAttribute(QStringLiteral("type"), QStringLiteral("audio"));
                trackinfo.setAttribute(QStringLiteral("blind"), 1);
            } else {
                trackinfo.setAttribute(QStringLiteral("blind"), 0);
            }
            trackinfo.setAttribute(QStringLiteral("mute"), 0);
            trackinfo.setAttribute(QStringLiteral("locked"), 0);
            tracksinfo.appendChild(trackinfo);
        }
        infoXml.appendChild(tracksinfo);
    }

    if (version <= 0.82) {
        // Convert <westley />s in <mlt />s (MLT extreme makeover)
        QDomNodeList westleyNodes = m_doc.elementsByTagName(QStringLiteral("westley"));
        for (int i = 0; i < westleyNodes.count(); ++i) {
            QDomElement westley = westleyNodes.at(i).toElement();
            westley.setTagName(QStringLiteral("mlt"));
        }
    }

    if (version <= 0.83) {
        // Replace point size with pixel size in text titles
        if (m_doc.toString().contains(QStringLiteral("font-size"))) {
            KMessageBox::ButtonCode convert = KMessageBox::Continue;
            QDomNodeList kproducerNodes = m_doc.elementsByTagName(QStringLiteral("kdenlive_producer"));
            for (int i = 0; i < kproducerNodes.count() && convert != KMessageBox::No; ++i) {
                QDomElement kproducer = kproducerNodes.at(i).toElement();
                if (kproducer.attribute(QStringLiteral("type")).toInt() == (int)ClipType::Text) {
                    QDomDocument data;
                    data.setContent(kproducer.attribute(QStringLiteral("xmldata")));
                    QDomNodeList items = data.firstChild().childNodes();
                    for (int j = 0; j < items.count() && convert != KMessageBox::No; ++j) {
                        if (items.at(j).attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsTextItem")) {
                            QDomNamedNodeMap textProperties = items.at(j).namedItem(QStringLiteral("content")).attributes();
                            if (textProperties.namedItem(QStringLiteral("font-pixel-size")).isNull() &&
                                !textProperties.namedItem(QStringLiteral("font-size")).isNull()) {
                                // Ask the user if he wants to convert
                                if (convert != KMessageBox::Yes && convert != KMessageBox::No) {
                                    convert = (KMessageBox::ButtonCode)KMessageBox::warningYesNo(
                                        QApplication::activeWindow(),
                                        i18n("Some of your text clips were saved with size in points, which means different sizes on different displays. Do "
                                             "you want to convert them to pixel size, making them portable? It is recommended you do this on the computer they "
                                             "were first created on, or you could have to adjust their size."),
                                        i18n("Update Text Clips"));
                                }
                                if (convert == KMessageBox::Yes) {
                                    QFont font;
                                    font.setPointSize(textProperties.namedItem(QStringLiteral("font-size")).nodeValue().toInt());
                                    QDomElement content = items.at(j).namedItem(QStringLiteral("content")).toElement();
                                    content.setAttribute(QStringLiteral("font-pixel-size"), QFontInfo(font).pixelSize());
                                    content.removeAttribute(QStringLiteral("font-size"));
                                    kproducer.setAttribute(QStringLiteral("xmldata"), data.toString());
                                    /*
                                     * You may be tempted to delete the preview file
                                     * to force its recreation: bad idea (see
                                     * http://www.kdenlive.org/mantis/view.php?id=749)
                                     */
                                }
                            }
                        }
                    }
                }
            }
        }

        // Fill the <documentproperties /> element
        QDomElement docProperties = infoXml.firstChildElement(QStringLiteral("documentproperties"));
        if (docProperties.isNull()) {
            docProperties = m_doc.createElement(QStringLiteral("documentproperties"));
            docProperties.setAttribute(QStringLiteral("zonein"), infoXml.attribute(QStringLiteral("zonein")));
            docProperties.setAttribute(QStringLiteral("zoneout"), infoXml.attribute(QStringLiteral("zoneout")));
            docProperties.setAttribute(QStringLiteral("zoom"), infoXml.attribute(QStringLiteral("zoom")));
            docProperties.setAttribute(QStringLiteral("position"), infoXml.attribute(QStringLiteral("position")));
            infoXml.appendChild(docProperties);
        }
    }

    if (version <= 0.84) {
        // update the title clips to use the new MLT kdenlivetitle producer
        QDomNodeList kproducerNodes = m_doc.elementsByTagName(QStringLiteral("kdenlive_producer"));
        for (int i = 0; i < kproducerNodes.count(); ++i) {
            QDomElement kproducer = kproducerNodes.at(i).toElement();
            if (kproducer.attribute(QStringLiteral("type")).toInt() == (int)ClipType::Text) {
                QString data = kproducer.attribute(QStringLiteral("xmldata"));
                QString datafile = kproducer.attribute(QStringLiteral("resource"));
                if (!datafile.endsWith(QLatin1String(".kdenlivetitle"))) {
                    datafile = QString();
                    kproducer.setAttribute(QStringLiteral("resource"), QString());
                }
                QString id = kproducer.attribute(QStringLiteral("id"));
                QDomNodeList mltproducers = m_doc.elementsByTagName(QStringLiteral("producer"));
                bool foundData = false;
                bool foundResource = false;
                bool foundService = false;
                for (int j = 0; j < mltproducers.count(); ++j) {
                    QDomElement wproducer = mltproducers.at(j).toElement();
                    if (wproducer.attribute(QStringLiteral("id")) == id) {
                        QDomNodeList props = wproducer.childNodes();
                        for (int k = 0; k < props.count(); ++k) {
                            if (props.at(k).toElement().attribute(QStringLiteral("name")) == QLatin1String("xmldata")) {
                                props.at(k).firstChild().setNodeValue(data);
                                foundData = true;
                            } else if (props.at(k).toElement().attribute(QStringLiteral("name")) == QLatin1String("mlt_service")) {
                                props.at(k).firstChild().setNodeValue(QStringLiteral("kdenlivetitle"));
                                foundService = true;
                            } else if (props.at(k).toElement().attribute(QStringLiteral("name")) == QLatin1String("resource")) {
                                props.at(k).firstChild().setNodeValue(datafile);
                                foundResource = true;
                            }
                        }
                        if (!foundData) {
                            QDomElement e = m_doc.createElement(QStringLiteral("property"));
                            e.setAttribute(QStringLiteral("name"), QStringLiteral("xmldata"));
                            QDomText value = m_doc.createTextNode(data);
                            e.appendChild(value);
                            wproducer.appendChild(e);
                        }
                        if (!foundService) {
                            QDomElement e = m_doc.createElement(QStringLiteral("property"));
                            e.setAttribute(QStringLiteral("name"), QStringLiteral("mlt_service"));
                            QDomText value = m_doc.createTextNode(QStringLiteral("kdenlivetitle"));
                            e.appendChild(value);
                            wproducer.appendChild(e);
                        }
                        if (!foundResource) {
                            QDomElement e = m_doc.createElement(QStringLiteral("property"));
                            e.setAttribute(QStringLiteral("name"), QStringLiteral("resource"));
                            QDomText value = m_doc.createTextNode(datafile);
                            e.appendChild(value);
                            wproducer.appendChild(e);
                        }
                        break;
                    }
                }
            }
        }
    }
    if (version <= 0.85) {
        // update the LADSPA effects to use the new ladspa.id format instead of external xml file
        QDomNodeList effectNodes = m_doc.elementsByTagName(QStringLiteral("filter"));
        for (int i = 0; i < effectNodes.count(); ++i) {
            QDomElement effect = effectNodes.at(i).toElement();
            if (Xml::getXmlProperty(effect, QStringLiteral("mlt_service")) == QLatin1String("ladspa")) {
                // Needs to be converted
                QStringList info = getInfoFromEffectName(Xml::getXmlProperty(effect, QStringLiteral("kdenlive_id")));
                if (info.isEmpty()) {
                    continue;
                }
                // info contains the correct ladspa.id from kdenlive effect name, and a list of parameter's old and new names
                Xml::setXmlProperty(effect, QStringLiteral("kdenlive_id"), info.at(0));
                Xml::setXmlProperty(effect, QStringLiteral("tag"), info.at(0));
                Xml::setXmlProperty(effect, QStringLiteral("mlt_service"), info.at(0));
                Xml::removeXmlProperty(effect, QStringLiteral("src"));
                for (int j = 1; j < info.size(); ++j) {
                    QString value = Xml::getXmlProperty(effect, info.at(j).section(QLatin1Char('='), 0, 0));
                    if (!value.isEmpty()) {
                        // update parameter name
                        Xml::renameXmlProperty(effect, info.at(j).section(QLatin1Char('='), 0, 0), info.at(j).section(QLatin1Char('='), 1, 1));
                    }
                }
            }
        }
    }

    if (version <= 0.86) {
        // Make sure we don't have avformat-novalidate producers, since it caused crashes
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        int max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (Xml::getXmlProperty(prod, QStringLiteral("mlt_service")) == QLatin1String("avformat-novalidate")) {
                Xml::setXmlProperty(prod, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
            }
        }

        // There was a mistake in Geometry transitions where the last keyframe was created one frame after the end of transition, so fix it and move last
        // keyframe to real end of transition

        // Get profile info (width / height)
        int profileWidth;
        int profileHeight;
        QDomElement profile = m_doc.firstChildElement(QStringLiteral("profile"));
        if (profile.isNull()) {
            profile = infoXml.firstChildElement(QStringLiteral("profileinfo"));
            if (!profile.isNull()) {
                // old MLT format, we need to add profile
                QDomNode mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
                QDomNode firstProd = m_doc.firstChildElement(QStringLiteral("producer"));
                QDomElement pr = profile.cloneNode().toElement();
                pr.setTagName(QStringLiteral("profile"));
                mlt.insertBefore(pr, firstProd);
            }
        }
        if (profile.isNull()) {
            // could not find profile info, set PAL
            profileWidth = 720;
            profileHeight = 576;
        } else {
            profileWidth = profile.attribute(QStringLiteral("width")).toInt();
            profileHeight = profile.attribute(QStringLiteral("height")).toInt();
        }
        QDomNodeList transitions = m_doc.elementsByTagName(QStringLiteral("transition"));
        max = transitions.count();
        for (int i = 0; i < max; ++i) {
            QDomElement trans = transitions.at(i).toElement();
            int out = trans.attribute(QStringLiteral("out")).toInt() - trans.attribute(QStringLiteral("in")).toInt();
            QString geom = Xml::getXmlProperty(trans, QStringLiteral("geometry"));
            Mlt::Geometry *g = new Mlt::Geometry(geom.toUtf8().data(), out, profileWidth, profileHeight);
            Mlt::GeometryItem item;
            if (g->next_key(&item, out) == 0) {
                // We have a keyframe just after last frame, try to move it to last frame
                if (item.frame() == out + 1) {
                    item.frame(out);
                    g->insert(item);
                    g->remove(out + 1);
                    Xml::setXmlProperty(trans, QStringLiteral("geometry"), QString::fromLatin1(g->serialise()));
                }
            }
            delete g;
        }
    }

    if (version <= 0.87) {
        if (!m_doc.firstChildElement(QStringLiteral("mlt")).hasAttribute(QStringLiteral("LC_NUMERIC"))) {
            m_doc.firstChildElement(QStringLiteral("mlt")).setAttribute(QStringLiteral("LC_NUMERIC"), QStringLiteral("C"));
        }
    }

    if (version <= 0.88) {
        // convert to new MLT-only format
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        QDomDocumentFragment frag = m_doc.createDocumentFragment();

        // Create Bin Playlist
        QDomElement main_playlist = m_doc.createElement(QStringLiteral("playlist"));
        QDomElement prop = m_doc.createElement(QStringLiteral("property"));
        prop.setAttribute(QStringLiteral("name"), QStringLiteral("xml_retain"));
        QDomText val = m_doc.createTextNode(QStringLiteral("1"));
        prop.appendChild(val);
        main_playlist.appendChild(prop);

        // Move markers
        QDomNodeList markers = m_doc.elementsByTagName(QStringLiteral("marker"));
        for (int i = 0; i < markers.count(); ++i) {
            QDomElement marker = markers.at(i).toElement();
            QDomElement property = m_doc.createElement(QStringLiteral("property"));
            property.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:marker.") + marker.attribute(QStringLiteral("id")) + QLatin1Char(':') +
                                                              marker.attribute(QStringLiteral("time")));
            QDomText val_node = m_doc.createTextNode(marker.attribute(QStringLiteral("type")) + QLatin1Char(':') + marker.attribute(QStringLiteral("comment")));
            property.appendChild(val_node);
            main_playlist.appendChild(property);
        }

        // Move guides
        QDomNodeList guides = m_doc.elementsByTagName(QStringLiteral("guide"));
        for (int i = 0; i < guides.count(); ++i) {
            QDomElement guide = guides.at(i).toElement();
            QDomElement property = m_doc.createElement(QStringLiteral("property"));
            property.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:guide.") + guide.attribute(QStringLiteral("time")));
            QDomText val_node = m_doc.createTextNode(guide.attribute(QStringLiteral("comment")));
            property.appendChild(val_node);
            main_playlist.appendChild(property);
        }

        // Move folders
        QDomNodeList folders = m_doc.elementsByTagName(QStringLiteral("folder"));
        for (int i = 0; i < folders.count(); ++i) {
            QDomElement folder = folders.at(i).toElement();
            QDomElement property = m_doc.createElement(QStringLiteral("property"));
            property.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:folder.-1.") + folder.attribute(QStringLiteral("id")));
            QDomText val_node = m_doc.createTextNode(folder.attribute(QStringLiteral("name")));
            property.appendChild(val_node);
            main_playlist.appendChild(property);
        }

        QDomNode mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
        main_playlist.setAttribute(QStringLiteral("id"), BinPlaylist::binPlaylistId);
        mlt.toElement().setAttribute(QStringLiteral("producer"), BinPlaylist::binPlaylistId);
        QStringList ids;
        QStringList slowmotionIds;
        QDomNode firstProd = m_doc.firstChildElement(QStringLiteral("producer"));

        QDomNodeList kdenlive_producers = m_doc.elementsByTagName(QStringLiteral("kdenlive_producer"));

        // Rename all track producers to correct name: "id_playlistName" instead of "id_trackNumber"
        QMap<QString, QString> trackRenaming;
        // Create a list of which producers / track on which the producer is
        QMap<QString, QString> playlistForId;
        QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));
        for (int i = 0; i < entries.count(); i++) {
            QDomElement entry = entries.at(i).toElement();
            QString entryId = entry.attribute(QStringLiteral("producer"));
            if (entryId == QLatin1String("black")) {
                continue;
            }
            bool audioOnlyProducer = false;
            if (trackRenaming.contains(entryId)) {
                // rename
                entry.setAttribute(QStringLiteral("producer"), trackRenaming.value(entryId));
                continue;
            }
            if (entryId.endsWith(QLatin1String("_video"))) {
                // Video only producers are not track aware
                continue;
            }
            if (entryId.endsWith(QLatin1String("_audio"))) {
                // Audio only producer
                audioOnlyProducer = true;
                entryId = entryId.section(QLatin1Char('_'), 0, -2);
            }
            if (!entryId.contains(QLatin1Char('_'))) {
                // not a track producer
                playlistForId.insert(entryId, entry.parentNode().toElement().attribute(QStringLiteral("id")));
                continue;
            }
            if (entryId.startsWith(QLatin1String("slowmotion:"))) {
                // Check broken slowmotion producers (they should not be track aware)
                QString newId = QStringLiteral("slowmotion:") + entryId.section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0) + QLatin1Char(':') +
                                entryId.section(QLatin1Char(':'), 2);
                trackRenaming.insert(entryId, newId);
                entry.setAttribute(QStringLiteral("producer"), newId);
                continue;
            }
            QString track = entryId.section(QLatin1Char('_'), 1, 1);
            QString playlistId = entry.parentNode().toElement().attribute(QStringLiteral("id"));
            if (track == playlistId) {
                continue;
            }
            QString newId = entryId.section(QLatin1Char('_'), 0, 0) + QLatin1Char('_') + playlistId;
            if (audioOnlyProducer) {
                newId.append(QStringLiteral("_audio"));
                trackRenaming.insert(entryId + QStringLiteral("_audio"), newId);
            } else {
                trackRenaming.insert(entryId, newId);
            }
            entry.setAttribute(QStringLiteral("producer"), newId);
        }
        if (!trackRenaming.isEmpty()) {
            for (int i = 0; i < producers.count(); ++i) {
                QDomElement prod = producers.at(i).toElement();
                QString id = prod.attribute(QStringLiteral("id"));
                if (trackRenaming.contains(id)) {
                    prod.setAttribute(QStringLiteral("id"), trackRenaming.value(id));
                }
            }
        }

        // Create easily searchable index of original producers
        QMap<QString, QDomElement> m_source_producers;
        for (int j = 0; j < kdenlive_producers.count(); j++) {
            QDomElement prod = kdenlive_producers.at(j).toElement();
            QString id = prod.attribute(QStringLiteral("id"));
            m_source_producers.insert(id, prod);
        }

        for (int i = 0; i < producers.count(); ++i) {
            QDomElement prod = producers.at(i).toElement();
            QString id = prod.attribute(QStringLiteral("id"));
            if (id == QLatin1String("black")) {
                continue;
            }
            if (id.startsWith(QLatin1String("slowmotion"))) {
                // No need to process slowmotion producers
                QString slowmo = id.section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0);
                if (!slowmotionIds.contains(slowmo)) {
                    slowmotionIds << slowmo;
                }
                continue;
            }
            QString prodId = id.section(QLatin1Char('_'), 0, 0);
            if (ids.contains(prodId)) {
                // Make sure we didn't create a duplicate
                if (ids.contains(id)) {
                    // we have a duplicate, check if this needs to be a track producer
                    QString service = Xml::getXmlProperty(prod, QStringLiteral("mlt_service"));
                    int a_ix = Xml::getXmlProperty(prod, QStringLiteral("audio_index")).toInt();
                    if (service == QLatin1String("xml") || service == QLatin1String("consumer") ||
                        (service.contains(QStringLiteral("avformat")) && a_ix != -1)) {
                        // This should be a track producer, rename
                        QString newId = id + QLatin1Char('_') + playlistForId.value(id);
                        prod.setAttribute(QStringLiteral("id"), newId);
                        for (int j = 0; j < entries.count(); j++) {
                            QDomElement entry = entries.at(j).toElement();
                            QString entryId = entry.attribute(QStringLiteral("producer"));
                            if (entryId == id) {
                                entry.setAttribute(QStringLiteral("producer"), newId);
                            }
                        }
                    } else {
                        // This is a duplicate, remove
                        mlt.removeChild(prod);
                        i--;
                    }
                }
                // Already processed, continue
                continue;
            }
            if (id == prodId) {
                // This is an original producer, move it to the main playlist
                QDomElement entry = m_doc.createElement(QStringLiteral("entry"));
                entry.setAttribute(QStringLiteral("producer"), id);
                main_playlist.appendChild(entry);
                QString service = Xml::getXmlProperty(prod, QStringLiteral("mlt_service"));
                if (service == QLatin1String("kdenlivetitle")) {
                    fixTitleProducerLocale(prod);
                }
                QDomElement source = m_source_producers.value(id);
                if (!source.isNull()) {
                    updateProducerInfo(prod, source);
                    entry.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
                    entry.setAttribute(QStringLiteral("out"), QString::number(source.attribute(QStringLiteral("duration")).toInt() - 1));
                }
                frag.appendChild(prod);
                // Changing prod parent removes it from list, so rewind index
                i--;
            } else {
                QDomElement originalProd = prod.cloneNode().toElement();
                originalProd.setAttribute(QStringLiteral("id"), prodId);
                if (id.endsWith(QLatin1String("_audio"))) {
                    Xml::removeXmlProperty(originalProd, QStringLiteral("video_index"));
                } else if (id.endsWith(QLatin1String("_video"))) {
                    Xml::removeXmlProperty(originalProd, QStringLiteral("audio_index"));
                }
                QDomElement source = m_source_producers.value(prodId);
                QDomElement entry = m_doc.createElement(QStringLiteral("entry"));
                if (!source.isNull()) {
                    updateProducerInfo(originalProd, source);
                    entry.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
                    entry.setAttribute(QStringLiteral("out"), QString::number(source.attribute(QStringLiteral("duration")).toInt() - 1));
                }
                frag.appendChild(originalProd);
                entry.setAttribute(QStringLiteral("producer"), prodId);
                main_playlist.appendChild(entry);
            }
            ids.append(prodId);
        }

        // Make sure to include producers that were not in timeline
        for (int j = 0; j < kdenlive_producers.count(); j++) {
            QDomElement prod = kdenlive_producers.at(j).toElement();
            QString id = prod.attribute(QStringLiteral("id"));
            if (!ids.contains(id)) {
                // Clip was not in timeline, create it
                QDomElement originalProd = prod.cloneNode().toElement();
                originalProd.setTagName(QStringLiteral("producer"));
                Xml::setXmlProperty(originalProd, QStringLiteral("resource"), originalProd.attribute(QStringLiteral("resource")));
                updateProducerInfo(originalProd, prod);
                originalProd.removeAttribute(QStringLiteral("proxy"));
                originalProd.removeAttribute(QStringLiteral("type"));
                originalProd.removeAttribute(QStringLiteral("file_hash"));
                originalProd.removeAttribute(QStringLiteral("file_size"));
                originalProd.removeAttribute(QStringLiteral("frame_size"));
                originalProd.removeAttribute(QStringLiteral("zone_out"));
                originalProd.removeAttribute(QStringLiteral("zone_in"));
                originalProd.removeAttribute(QStringLiteral("name"));
                originalProd.removeAttribute(QStringLiteral("type"));
                originalProd.removeAttribute(QStringLiteral("duration"));
                originalProd.removeAttribute(QStringLiteral("cutzones"));

                int type = prod.attribute(QStringLiteral("type")).toInt();
                QString mltService;
                switch (type) {
                case 4:
                    mltService = QStringLiteral("colour");
                    break;
                case 5:
                case 7:
                    mltService = QStringLiteral("qimage");
                    break;
                case 6:
                    mltService = QStringLiteral("kdenlivetitle");
                    break;
                case 9:
                    mltService = QStringLiteral("xml");
                    break;
                default:
                    mltService = QStringLiteral("avformat");
                    break;
                }
                Xml::setXmlProperty(originalProd, QStringLiteral("mlt_service"), mltService);
                Xml::setXmlProperty(originalProd, QStringLiteral("mlt_type"), QStringLiteral("producer"));
                QDomElement entry = m_doc.createElement(QStringLiteral("entry"));
                entry.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
                entry.setAttribute(QStringLiteral("out"), QString::number(prod.attribute(QStringLiteral("duration")).toInt() - 1));
                entry.setAttribute(QStringLiteral("producer"), id);
                main_playlist.appendChild(entry);
                if (type == 6) {
                    fixTitleProducerLocale(originalProd);
                }
                frag.appendChild(originalProd);
                ids << id;
            }
        }

        // Set clip folders
        for (int j = 0; j < kdenlive_producers.count(); j++) {
            QDomElement prod = kdenlive_producers.at(j).toElement();
            QString id = prod.attribute(QStringLiteral("id"));
            QString folder = prod.attribute(QStringLiteral("groupid"));
            QDomNodeList mlt_producers = frag.childNodes();
            for (int k = 0; k < mlt_producers.count(); k++) {
                QDomElement mltprod = mlt_producers.at(k).toElement();
                if (mltprod.tagName() != QLatin1String("producer")) {
                    continue;
                }
                if (mltprod.attribute(QStringLiteral("id")) == id) {
                    if (!folder.isEmpty()) {
                        // We have found our producer, set folder info
                        QDomElement property = m_doc.createElement(QStringLiteral("property"));
                        property.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:folderid"));
                        QDomText val_node = m_doc.createTextNode(folder);
                        property.appendChild(val_node);
                        mltprod.appendChild(property);
                    }
                    break;
                }
            }
        }

        // Make sure all slowmotion producers have a master clip
        for (int i = 0; i < slowmotionIds.count(); i++) {
            const QString &slo = slowmotionIds.at(i);
            if (!ids.contains(slo)) {
                // rebuild producer from Kdenlive's old xml format
                for (int j = 0; j < kdenlive_producers.count(); j++) {
                    QDomElement prod = kdenlive_producers.at(j).toElement();
                    QString id = prod.attribute(QStringLiteral("id"));
                    if (id == slo) {
                        // We found the kdenlive_producer, build MLT producer
                        QDomElement original = m_doc.createElement(QStringLiteral("producer"));
                        original.setAttribute(QStringLiteral("in"), 0);
                        original.setAttribute(QStringLiteral("out"), prod.attribute(QStringLiteral("duration")).toInt() - 1);
                        original.setAttribute(QStringLiteral("id"), id);
                        QDomElement property = m_doc.createElement(QStringLiteral("property"));
                        property.setAttribute(QStringLiteral("name"), QStringLiteral("resource"));
                        QDomText val_node = m_doc.createTextNode(prod.attribute(QStringLiteral("resource")));
                        property.appendChild(val_node);
                        original.appendChild(property);
                        QDomElement prop2 = m_doc.createElement(QStringLiteral("property"));
                        prop2.setAttribute(QStringLiteral("name"), QStringLiteral("mlt_service"));
                        QDomText val2 = m_doc.createTextNode(QStringLiteral("avformat"));
                        prop2.appendChild(val2);
                        original.appendChild(prop2);
                        QDomElement prop3 = m_doc.createElement(QStringLiteral("property"));
                        prop3.setAttribute(QStringLiteral("name"), QStringLiteral("length"));
                        QDomText val3 = m_doc.createTextNode(prod.attribute(QStringLiteral("duration")));
                        prop3.appendChild(val3);
                        original.appendChild(prop3);
                        QDomElement entry = m_doc.createElement(QStringLiteral("entry"));
                        entry.setAttribute(QStringLiteral("in"), original.attribute(QStringLiteral("in")));
                        entry.setAttribute(QStringLiteral("out"), original.attribute(QStringLiteral("out")));
                        entry.setAttribute(QStringLiteral("producer"), id);
                        main_playlist.appendChild(entry);
                        frag.appendChild(original);
                        ids << slo;
                        break;
                    }
                }
            }
        }
        frag.appendChild(main_playlist);
        mlt.insertBefore(frag, firstProd);
    }

    if (version < 0.91) {
        // Migrate track properties
        QDomNode mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
        QDomNodeList old_tracks = m_doc.elementsByTagName(QStringLiteral("trackinfo"));
        QDomNodeList tracks = m_doc.elementsByTagName(QStringLiteral("track"));
        QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
        for (int i = 0; i < old_tracks.count(); i++) {
            QString playlistName = tracks.at(i + 1).toElement().attribute(QStringLiteral("producer"));
            // find playlist for track
            QDomElement trackPlaylist;
            for (int j = 0; j < playlists.count(); j++) {
                if (playlists.at(j).toElement().attribute(QStringLiteral("id")) == playlistName) {
                    trackPlaylist = playlists.at(j).toElement();
                    break;
                }
            }
            if (!trackPlaylist.isNull()) {
                QDomElement kdenliveTrack = old_tracks.at(i).toElement();
                if (kdenliveTrack.attribute(QStringLiteral("type")) == QLatin1String("audio")) {
                    Xml::setXmlProperty(trackPlaylist, QStringLiteral("kdenlive:audio_track"), QStringLiteral("1"));
                }
                if (kdenliveTrack.attribute(QStringLiteral("locked")) == QLatin1String("1")) {
                    Xml::setXmlProperty(trackPlaylist, QStringLiteral("kdenlive:locked_track"), QStringLiteral("1"));
                }
                Xml::setXmlProperty(trackPlaylist, QStringLiteral("kdenlive:track_name"), kdenliveTrack.attribute(QStringLiteral("trackname")));
            }
        }
        // Find bin playlist
        playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
        QDomElement playlist;
        for (int i = 0; i < playlists.count(); i++) {
            if (playlists.at(i).toElement().attribute(QStringLiteral("id")) == BinPlaylist::binPlaylistId) {
                playlist = playlists.at(i).toElement();
                break;
            }
        }
        // Migrate document notes
        QDomNodeList notesList = m_doc.elementsByTagName(QStringLiteral("documentnotes"));
        if (!notesList.isEmpty()) {
            QDomElement notes_elem = notesList.at(0).toElement();
            QString notes = notes_elem.firstChild().nodeValue();
            Xml::setXmlProperty(playlist, QStringLiteral("kdenlive:documentnotes"), notes);
        }
        // Migrate clip groups
        QDomNodeList groupElement = m_doc.elementsByTagName(QStringLiteral("groups"));
        if (!groupElement.isEmpty()) {
            QDomElement groups = groupElement.at(0).toElement();
            QDomDocument d2;
            d2.importNode(groups, true);
            Xml::setXmlProperty(playlist, QStringLiteral("kdenlive:clipgroups"), d2.toString());
        }
        // Migrate custom effects
        QDomNodeList effectsElement = m_doc.elementsByTagName(QStringLiteral("customeffects"));
        if (!effectsElement.isEmpty()) {
            QDomElement effects = effectsElement.at(0).toElement();
            QDomDocument d2;
            d2.importNode(effects, true);
            Xml::setXmlProperty(playlist, QStringLiteral("kdenlive:customeffects"), d2.toString());
        }
        Xml::setXmlProperty(playlist, QStringLiteral("kdenlive:docproperties.version"), QString::number(currentVersion));
        if (!infoXml.isNull()) {
            Xml::setXmlProperty(playlist, QStringLiteral("kdenlive:docproperties.projectfolder"), infoXml.attribute(QStringLiteral("projectfolder")));
        }

        // Remove deprecated Kdenlive extra info from xml doc before sending it to MLT
        QDomElement docXml = mlt.firstChildElement(QStringLiteral("kdenlivedoc"));
        if (!docXml.isNull()) {
            mlt.removeChild(docXml);
        }
    }

    if (version < 0.92) {
        // Luma transition used for wipe is deprecated, we now use a composite, convert
        QDomNodeList transitionList = m_doc.elementsByTagName(QStringLiteral("transition"));
        QDomElement trans;
        for (int i = 0; i < transitionList.count(); i++) {
            trans = transitionList.at(i).toElement();
            QString id = Xml::getXmlProperty(trans, QStringLiteral("kdenlive_id"));
            if (id == QLatin1String("luma")) {
                Xml::setXmlProperty(trans, QStringLiteral("kdenlive_id"), QStringLiteral("wipe"));
                Xml::setXmlProperty(trans, QStringLiteral("mlt_service"), QStringLiteral("composite"));
                bool reverse = Xml::getXmlProperty(trans, QStringLiteral("reverse")).toInt() != 0;
                Xml::setXmlProperty(trans, QStringLiteral("luma_invert"), Xml::getXmlProperty(trans, QStringLiteral("invert")));
                Xml::setXmlProperty(trans, QStringLiteral("luma"), Xml::getXmlProperty(trans, QStringLiteral("resource")));
                Xml::removeXmlProperty(trans, QStringLiteral("invert"));
                Xml::removeXmlProperty(trans, QStringLiteral("reverse"));
                Xml::removeXmlProperty(trans, QStringLiteral("resource"));
                if (reverse) {
                    Xml::setXmlProperty(trans, QStringLiteral("geometry"), QStringLiteral("0%/0%:100%x100%:100;-1=0%/0%:100%x100%:0"));
                } else {
                    Xml::setXmlProperty(trans, QStringLiteral("geometry"), QStringLiteral("0%/0%:100%x100%:0;-1=0%/0%:100%x100%:100"));
                }
                Xml::setXmlProperty(trans, QStringLiteral("aligned"), QStringLiteral("0"));
                Xml::setXmlProperty(trans, QStringLiteral("fill"), QStringLiteral("1"));
            }
        }
    }

    if (version < 0.93) {
        // convert old keyframe filters to animated
        // these filters were "animated" by adding several instance of the filter, each one having a start and end tag.
        // We convert by parsing the start and end tags vor values and adding all to the new animated parameter
        QMap<QString, QStringList> keyframeFilterToConvert;
        keyframeFilterToConvert.insert(QStringLiteral("volume"), QStringList() << QStringLiteral("gain") << QStringLiteral("end") << QStringLiteral("level"));
        keyframeFilterToConvert.insert(QStringLiteral("brightness"), QStringList()
                                                                         << QStringLiteral("start") << QStringLiteral("end") << QStringLiteral("level"));

        QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));
        for (int i = 0; i < entries.count(); i++) {
            QDomNode entry = entries.at(i);
            QDomNodeList effects = entry.toElement().elementsByTagName(QStringLiteral("filter"));
            QStringList parsedIds;
            for (int j = 0; j < effects.count(); j++) {
                QDomElement eff = effects.at(j).toElement();
                QString id = Xml::getXmlProperty(eff, QStringLiteral("kdenlive_id"));
                if (keyframeFilterToConvert.contains(id) && !parsedIds.contains(id)) {
                    parsedIds << id;
                    QMap<int, double> values;
                    QStringList conversionParams = keyframeFilterToConvert.value(id);
                    int offset = eff.attribute(QStringLiteral("in")).toInt();
                    int out = eff.attribute(QStringLiteral("out")).toInt();
                    convertKeyframeEffect_093(eff, conversionParams, values, offset);
                    Xml::removeXmlProperty(eff, conversionParams.at(0));
                    Xml::removeXmlProperty(eff, conversionParams.at(1));
                    for (int k = j + 1; k < effects.count(); k++) {
                        QDomElement subEffect = effects.at(k).toElement();
                        QString subId = Xml::getXmlProperty(subEffect, QStringLiteral("kdenlive_id"));
                        if (subId == id) {
                            convertKeyframeEffect_093(subEffect, conversionParams, values, offset);
                            out = subEffect.attribute(QStringLiteral("out")).toInt();
                            entry.removeChild(subEffect);
                            k--;
                        }
                    }
                    QStringList parsedValues;
                    QLocale locale; // Used for conversion of pre-1.0 version → OK
                    locale.setNumberOptions(QLocale::OmitGroupSeparator);
                    QMapIterator<int, double> l(values);
                    if (id == QLatin1String("volume")) {
                        // convert old volume range (0-300) to new dB values (-60-60)
                        while (l.hasNext()) {
                            l.next();
                            double v = l.value();
                            if (v <= 0) {
                                v = -60;
                            } else {
                                v = log10(v) * 20;
                            }
                            parsedValues << QString::number(l.key()) + QLatin1Char('=') + locale.toString(v);
                        }
                    } else {
                        while (l.hasNext()) {
                            l.next();
                            parsedValues << QString::number(l.key()) + QLatin1Char('=') + locale.toString(l.value());
                        }
                    }
                    Xml::setXmlProperty(eff, conversionParams.at(2), parsedValues.join(QLatin1Char(';')));
                    // Xml::setXmlProperty(eff, QStringLiteral("kdenlive:sync_in_out"), QStringLiteral("1"));
                    eff.setAttribute(QStringLiteral("out"), out);
                }
            }
        }
    }

    if (version < 0.94) {
        // convert slowmotion effects/producers
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        int max = producers.count();
        QStringList slowmoIds;
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            QString id = prod.attribute(QStringLiteral("id"));
            if (id.startsWith(QLatin1String("slowmotion"))) {
                QString service = Xml::getXmlProperty(prod, QStringLiteral("mlt_service"));
                if (service == QLatin1String("framebuffer")) {
                    // convert to new timewarp producer
                    prod.setAttribute(QStringLiteral("id"), id + QStringLiteral(":1"));
                    slowmoIds << id;
                    Xml::setXmlProperty(prod, QStringLiteral("mlt_service"), QStringLiteral("timewarp"));
                    QString resource = Xml::getXmlProperty(prod, QStringLiteral("resource"));
                    Xml::setXmlProperty(prod, QStringLiteral("warp_resource"), resource.section(QLatin1Char('?'), 0, 0));
                    Xml::setXmlProperty(prod, QStringLiteral("warp_speed"), resource.section(QLatin1Char('?'), 1).section(QLatin1Char(':'), 0, 0));
                    Xml::setXmlProperty(prod, QStringLiteral("resource"),
                                        resource.section(QLatin1Char('?'), 1) + QLatin1Char(':') + resource.section(QLatin1Char('?'), 0, 0));
                    Xml::setXmlProperty(prod, QStringLiteral("audio_index"), QStringLiteral("-1"));
                }
            }
        }
        if (!slowmoIds.isEmpty()) {
            producers = m_doc.elementsByTagName(QStringLiteral("entry"));
            max = producers.count();
            for (int i = 0; i < max; ++i) {
                QDomElement prod = producers.at(i).toElement();
                QString entryId = prod.attribute(QStringLiteral("producer"));
                if (slowmoIds.contains(entryId)) {
                    prod.setAttribute(QStringLiteral("producer"), entryId + QStringLiteral(":1"));
                }
            }
        }
        // qCDebug(KDENLIVE_LOG)<<"------------------------\n"<<m_doc.toString();
    }
    if (version < 0.95) {
        // convert slowmotion effects/producers
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        int max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (prod.isNull()) {
                continue;
            }
            QString id = prod.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
            if (id == QLatin1String("black")) {
                Xml::setXmlProperty(prod, QStringLiteral("set.test_audio"), QStringLiteral("0"));
                break;
            }
        }
    }
    if (version < 0.97) {
        // move guides to new JSON format
        QDomElement main_playlist = m_doc.documentElement().firstChildElement(QStringLiteral("playlist"));
        QDomNodeList props = main_playlist.elementsByTagName(QStringLiteral("property"));
        QJsonArray guidesList;
        QMap<QString, QJsonArray> markersList;
        QLocale locale; // Used for conversion of pre-1.0 version → OK
        for (int i = 0; i < props.count(); ++i) {
            QDomNode n = props.at(i);
            QString prop = n.toElement().attribute(QStringLiteral("name"));
            if (prop.startsWith(QLatin1String("kdenlive:guide."))) {
                // Process guide
                double guidePos = locale.toDouble(prop.section(QLatin1Char('.'), 1));
                QJsonObject currentGuide;
                currentGuide.insert(QStringLiteral("pos"), QJsonValue(GenTime(guidePos).frames(pCore->getCurrentFps())));
                currentGuide.insert(QStringLiteral("comment"), QJsonValue(n.firstChild().nodeValue()));
                currentGuide.insert(QStringLiteral("type"), QJsonValue(0));
                // Clear entry in old format
                n.toElement().setAttribute(QStringLiteral("name"), QStringLiteral("_"));
                guidesList.push_back(currentGuide);
            } else if (prop.startsWith(QLatin1String("kdenlive:marker."))) {
                // Process marker
                double markerPos = locale.toDouble(prop.section(QLatin1Char(':'), -1));
                QString markerBinClip = prop.section(QLatin1Char('.'), 1).section(QLatin1Char(':'), 0, 0);
                QString markerData = n.firstChild().nodeValue();
                int markerType = markerData.section(QLatin1Char(':'), 0, 0).toInt();
                QString markerComment = markerData.section(QLatin1Char(':'), 1);
                QJsonObject currentMarker;
                currentMarker.insert(QStringLiteral("pos"), QJsonValue(GenTime(markerPos).frames(pCore->getCurrentFps())));
                currentMarker.insert(QStringLiteral("comment"), QJsonValue(markerComment));
                currentMarker.insert(QStringLiteral("type"), QJsonValue(markerType));
                // Clear entry in old format
                n.toElement().setAttribute(QStringLiteral("name"), QStringLiteral("_"));
                if (markersList.contains(markerBinClip)) {
                    // we already have a marker list for this clip
                    QJsonArray markerList = markersList.value(markerBinClip);
                    markerList.push_back(currentMarker);
                    markersList.insert(markerBinClip, markerList);
                } else {
                    QJsonArray markerList;
                    markerList.push_back(currentMarker);
                    markersList.insert(markerBinClip, markerList);
                }
            }
        }
        if (!guidesList.isEmpty()) {
            QJsonDocument json(guidesList);
            Xml::setXmlProperty(main_playlist, QStringLiteral("kdenlive:docproperties.guides"), json.toJson());
        }

        // Update producers
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        int max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (prod.isNull()) continue;
            // Move to new kdenlive:id format
            const QString id = prod.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
            Xml::setXmlProperty(prod, QStringLiteral("kdenlive:id"), id);
            if (markersList.contains(id)) {
                QJsonDocument json(markersList.value(id));
                Xml::setXmlProperty(prod, QStringLiteral("kdenlive:markers"), json.toJson());
            }

            // Check image sequences with buggy begin frame number
            const QString service = Xml::getXmlProperty(prod, QStringLiteral("mlt_service"));
            if (service == QLatin1String("pixbuf") || service == QLatin1String("qimage")) {
                QString resource = Xml::getXmlProperty(prod, QStringLiteral("resource"));
                if (resource.contains(QStringLiteral("?begin:"))) {
                    resource.replace(QStringLiteral("?begin:"), QStringLiteral("?begin="));
                    Xml::setXmlProperty(prod, QStringLiteral("resource"), resource);
                }
            }
        }
    }
    if (version < 0.98) {
        // rename main bin playlist, create extra tracks for old type AV clips, port groups to JSon
        QJsonArray newGroups;
        QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
        QDomNodeList masterProducers = m_doc.elementsByTagName(QStringLiteral("producer"));
        QDomElement playlist;
        QDomNode mainplaylist;
        QDomNode mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
        QDomNode tractor = mlt.firstChildElement(QStringLiteral("tractor"));
        // Build start trackIndex
        QMap<QString, QString> trackIndex;
        QDomNodeList tracks = tractor.toElement().elementsByTagName(QStringLiteral("track"));
        for (int i = 0; i < tracks.count(); i++) {
            trackIndex.insert(QString::number(i), tracks.at(i).toElement().attribute(QStringLiteral("producer")));
        }

        int trackOffset = 0;
        // AV clips are not supported anymore. Check if we have some and add extra audio tracks if necessary
        // Update the main bin name as well to be xml compliant
        for (int i = 0; i < playlists.count(); i++) {
            if (playlists.at(i).toElement().attribute(QStringLiteral("id")) == QLatin1String("main bin") || playlists.at(i).toElement().attribute(QStringLiteral("id")) == QLatin1String("main_bin")) {
                playlists.at(i).toElement().setAttribute(QStringLiteral("id"), BinPlaylist::binPlaylistId);
                mainplaylist = playlists.at(i);
                QString oldGroups = Xml::getXmlProperty(mainplaylist.toElement(), QStringLiteral("kdenlive:clipgroups"));
                QDomDocument groupsDoc;
                groupsDoc.setContent(oldGroups);
                QDomNodeList groups = groupsDoc.elementsByTagName(QStringLiteral("group"));
                for (int g = 0; g < groups.count(); g++) {
                    QDomNodeList elements = groups.at(g).childNodes();
                    QJsonArray array;
                    for (int h = 0; h < elements.count(); h++) {
                        QJsonObject item;
                        item.insert(QLatin1String("type"), QJsonValue(QStringLiteral("Leaf")));
                        item.insert(QLatin1String("leaf"), QJsonValue(QLatin1String("clip")));
                        QString pos = elements.at(h).toElement().attribute(QStringLiteral("position"));
                        QString track = trackIndex.value(elements.at(h).toElement().attribute(QStringLiteral("track")));
                        item.insert(QLatin1String("data"), QJsonValue(QString("%1:%2").arg(track, pos)));
                        array.push_back(item);
                    }
                    QJsonObject currentGroup;
                    currentGroup.insert(QLatin1String("type"), QJsonValue(QStringLiteral("Normal")));
                    currentGroup.insert(QLatin1String("children"), array);
                    newGroups.push_back(currentGroup);
                }
            } else {
                if (Xml::getXmlProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:audio_track")) == QLatin1String("1")) {
                    // Audio track, no need to process
                    continue;
                }
                const QString playlistName = playlists.at(i).toElement().attribute(QStringLiteral("id"));
                QDomElement duplicate_playlist = m_doc.createElement(QStringLiteral("playlist"));
                duplicate_playlist.setAttribute(QStringLiteral("id"), QString("%1_duplicate").arg(playlistName));
                QDomElement pltype = m_doc.createElement(QStringLiteral("property"));
                pltype.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:audio_track"));
                pltype.setNodeValue(QStringLiteral("1"));
                QDomText value1 = m_doc.createTextNode(QStringLiteral("1"));
                pltype.appendChild(value1);
                duplicate_playlist.appendChild(pltype);
                QDomElement plname = m_doc.createElement(QStringLiteral("property"));
                plname.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:track_name"));
                QDomText value = m_doc.createTextNode(i18n("extra audio"));
                plname.appendChild(value);
                duplicate_playlist.appendChild(plname);
                QDomNodeList producers = playlists.at(i).childNodes();
                bool duplicationRequested = false;
                int pos = 0;
                for (int j = 0; j < producers.count(); j++) {
                    if (producers.at(j).nodeName() == QLatin1String("blank")) {
                        // blank, duplicate
                        duplicate_playlist.appendChild(producers.at(j).cloneNode());
                        pos += producers.at(j).toElement().attribute(QStringLiteral("length")).toInt();
                    } else if (producers.at(j).nodeName() == QLatin1String("filter")) {
                        // effect, duplicate
                        duplicate_playlist.appendChild(producers.at(j).cloneNode());
                    } else if (producers.at(j).nodeName() != QLatin1String("entry")) {
                        // property node, pass
                        continue;
                    } else if (producers.at(j).toElement().attribute(QStringLiteral("producer")).endsWith(playlistName)) {
                        // This is an AV clip
                        // Check master properties
                        bool hasAudio = true;
                        bool hasVideo = true;
                        const QString currentId = producers.at(j).toElement().attribute(QStringLiteral("producer"));
                        int in = producers.at(j).toElement().attribute(QStringLiteral("in")).toInt();
                        int out = producers.at(j).toElement().attribute(QStringLiteral("out")).toInt();
                        for (int k = 0; k < masterProducers.count(); k++) {
                            if (masterProducers.at(k).toElement().attribute(QStringLiteral("id")) == currentId) {
                                hasVideo = Xml::getXmlProperty(masterProducers.at(k).toElement(), QStringLiteral("video_index")) != QLatin1String("-1");
                                hasAudio = Xml::getXmlProperty(masterProducers.at(k).toElement(), QStringLiteral("audio_index")) != QLatin1String("-1");
                                break;
                            }
                        }
                        if (!hasAudio) {
                            // no duplication needed, replace with blank
                            QDomElement duplicate = m_doc.createElement(QStringLiteral("blank"));
                            duplicate.setAttribute(QStringLiteral("length"), QString::number(out - in + 1));
                            duplicate_playlist.appendChild(duplicate);
                            pos += out - in + 1;
                            continue;
                        }
                        QDomNode prod = producers.at(j).cloneNode();
                        Xml::setXmlProperty(prod.toElement(), QStringLiteral("set.test_video"), QStringLiteral("1"));
                        duplicate_playlist.appendChild(prod);
                        // Check if that is an audio clip on a video track
                        if (!hasVideo) {
                            // Audio clip on a video track, replace with blank and duplicate
                            producers.at(j).toElement().setTagName("blank");
                            producers.at(j).toElement().setAttribute("length", QString::number(out - in + 1));
                        } else {
                            // group newly created AVSplit group
                            // We temporarily store track with their playlist name since track index will change
                            // as we insert the duplicate tracks
                            QJsonArray array;
                            QJsonObject items;
                            items.insert(QLatin1String("type"), QJsonValue(QStringLiteral("Leaf")));
                            items.insert(QLatin1String("leaf"), QJsonValue(QLatin1String("clip")));
                            items.insert(QLatin1String("data"), QJsonValue(QString("%1:%2").arg(playlistName).arg(pos)));
                            array.push_back(items);
                            QJsonObject itemb;
                            itemb.insert(QLatin1String("type"), QJsonValue(QStringLiteral("Leaf")));
                            itemb.insert(QLatin1String("leaf"), QJsonValue(QLatin1String("clip")));
                            itemb.insert(QLatin1String("data"), QJsonValue(QString("%1:%2").arg(duplicate_playlist.attribute(QStringLiteral("id"))).arg(pos)));
                            array.push_back(itemb);
                            QJsonObject currentGroup;
                            currentGroup.insert(QLatin1String("type"), QJsonValue(QStringLiteral("AVSplit")));
                            currentGroup.insert(QLatin1String("children"), array);
                            newGroups.push_back(currentGroup);
                        }
                        duplicationRequested = true;
                        pos += out - in + 1;
                    } else {
                        // no duplication needed, replace with blank
                        QDomElement duplicate = m_doc.createElement(QStringLiteral("blank"));
                        int in = producers.at(j).toElement().attribute(QStringLiteral("in")).toInt();
                        int out = producers.at(j).toElement().attribute(QStringLiteral("out")).toInt();
                        duplicate.setAttribute(QStringLiteral("length"), QString::number(out - in + 1));
                        duplicate_playlist.appendChild(duplicate);
                        pos += out - in + 1;
                    }
                }
                if (duplicationRequested) {
                    // Plant the playlist at the end
                    mlt.insertBefore(duplicate_playlist, tractor);
                    QDomNode lastTrack = tractor.firstChildElement(QStringLiteral("track"));
                    QDomElement duplicate = m_doc.createElement(QStringLiteral("track"));
                    duplicate.setAttribute(QStringLiteral("producer"), QString("%1_duplicate").arg(playlistName));
                    duplicate.setAttribute(QStringLiteral("hide"), QStringLiteral("video"));
                    tractor.insertAfter(duplicate, lastTrack);
                    trackOffset++;
                }
            }
        }
        if (trackOffset > 0) {
            // Some tracks were added, adjust compositions
            QDomNodeList transitions = m_doc.elementsByTagName(QStringLiteral("transition"));
            int max = transitions.count();
            for (int i = 0; i < max; ++i) {
                QDomElement t = transitions.at(i).toElement();
                if (Xml::getXmlProperty(t, QStringLiteral("internal_added")).toInt() > 0) {
                    // internal transitions will be rebuilt, no need to correct
                    continue;
                }
                int a_track = Xml::getXmlProperty(t, QStringLiteral("a_track")).toInt();
                int b_track = Xml::getXmlProperty(t, QStringLiteral("b_track")).toInt();
                if (a_track > 0) {
                    Xml::setXmlProperty(t, QStringLiteral("a_track"), QString::number(a_track + trackOffset));
                }
                if (b_track > 0) {
                    Xml::setXmlProperty(t, QStringLiteral("b_track"), QString::number(b_track + trackOffset));
                }
            }
        }
        // Process groups data
        QJsonDocument json(newGroups);
        QString groupsData = QString(json.toJson());
        tracks = tractor.toElement().elementsByTagName(QStringLiteral("track"));
        for (int i = 0; i < tracks.count(); i++) {
            // Replace track names with their current index in our view
            const QString trackId = QString("%1:").arg(tracks.at(i).toElement().attribute(QStringLiteral("producer")));
            groupsData.replace(trackId, QString("%1:").arg(i - 1));
        }
        Xml::setXmlProperty(mainplaylist.toElement(), QStringLiteral("kdenlive:docproperties.groups"), groupsData);
    }
    if (version < 0.99) {
        // rename main bin playlist, create extra tracks for old type AV clips, port groups to JSon
        QDomNodeList masterProducers = m_doc.elementsByTagName(QStringLiteral("producer"));
        for (int i = 0; i < masterProducers.count(); i++) {
            QMap<QString, QString> map = Xml::getXmlPropertyByWildcard(masterProducers.at(i).toElement(), QLatin1String("kdenlive:clipzone."));
            if (map.isEmpty()) {
                continue;
            }
            QJsonArray list;
            QMapIterator<QString, QString> j(map);
            while (j.hasNext()) {
                j.next();
                Xml::removeXmlProperty(masterProducers.at(i).toElement(), j.key());
                QJsonObject currentZone;
                currentZone.insert(QLatin1String("name"), QJsonValue(j.key().section(QLatin1Char('.'),1)));
                if (!j.value().contains(QLatin1Char(';'))) {
                    // invalid zone
                    continue;
                }
                currentZone.insert(QLatin1String("in"), QJsonValue(j.value().section(QLatin1Char(';'), 0, 0).toInt()));
                currentZone.insert(QLatin1String("out"), QJsonValue(j.value().section(QLatin1Char(';'), 1, 1).toInt()));
                list.push_back(currentZone);
            }
            QJsonDocument json(list);
            Xml::setXmlProperty(masterProducers.at(i).toElement(), QStringLiteral("kdenlive:clipzones"), QString(json.toJson()));
        }
    }
    m_modified = true;
    return true;
}

auto DocumentValidator::upgradeTo100(const QLocale &documentLocale) -> QString {

    bool modified = false;
    auto decimalPoint = documentLocale.decimalPoint();

    if (decimalPoint != '.') {
        qDebug() << "Decimal point is NOT OK and needs fixing. Converting to . from " << decimalPoint;

        auto fixTimecode = [decimalPoint] (QString &value) {
            QRegExp reTimecode("(\\d+:\\d+:\\d+)" + QString(decimalPoint) + "(\\d+)");
            QRegExp reValue("(=\\d+)" + QString(decimalPoint) + "(\\d+)");
            value.replace(reTimecode, "\\1.\\2")
                    .replace(reValue, "\\1.\\2");
        };


        // List of properties which always need to be fixed
        // Example: <property name="aspect_ratio">1,00247</property>
        QList<QString> generalPropertiesToFix = {
                "warp_speed",
                "length",
                "aspect_ratio",
                "kdenlive:clipanalysis.motion_vector_list",
                "kdenlive:original.meta.attr.com.apple.quicktime.rating.user.markup",
        };

        // Fix properties just by name, anywhere in the file
        auto props = m_doc.elementsByTagName(QStringLiteral("property"));
        qDebug() << "Found " << props.count() << " properties.";
        for (int i = 0; i < props.count(); i++) {
            QString propName = props.at(i).attributes().namedItem("name").nodeValue();
            QDomElement element = props.at(i).toElement();
            if (element.childNodes().size() == 1) {
                QDomText text = element.firstChild().toText();
                if (!text.isNull()) {

                    bool autoReplace = propName.endsWith("frame_rate")
                            || propName.endsWith("aspect_ratio")
                            || (generalPropertiesToFix.indexOf(propName) >= 0);

                    QString originalValue = text.nodeValue();
                    QString value(originalValue);
                    if (propName == "resource") {
                        // Fix entries like <property name="resource">0,500000:/path/to/video
                        value.replace(QRegExp("^(\\d+)" + QString(decimalPoint) + "(\\d+:)"), "\\1.\\2");
                    } else if (autoReplace) {
                        // Just replace decimal point
                        value.replace(decimalPoint, '.');
                    } else {
                        fixTimecode(value);
                    }

                    if (originalValue != value) {
                        text.setNodeValue(value);
                        qDebug() << "Decimal point: Converted " << propName << " from " << originalValue << " to "
                                 << value;
                    }
                }
            }
        }


        QList<QString> filterPropertiesToFix = {
                "version",
        };

        // Fix filter specific properties.
        // The first entry is the filter (MLT service) name, the second entry a list of properties to fix.
        // Warning: This list may not be complete!
        QMap <QString, QList<QString> > servicePropertiesToFix;
        servicePropertiesToFix.insert("panner", {"start"});
        servicePropertiesToFix.insert("volume", {"level"});
        servicePropertiesToFix.insert("window", {"gain"});
        servicePropertiesToFix.insert("lumaliftgaingamma", {"lift", "gain", "gamma"});


        // Fix filter properties.
        // Note that effect properties will be fixed when the effect is loaded
        // as there is more information available about the parameter type.
        auto filters = m_doc.elementsByTagName(QStringLiteral("filter"));
        qDebug() << "Found" << filters.count() << "filters.";
        for (int i = 0; i < filters.count(); i++) {
            QDomElement filter = filters.at(i).toElement();
            QString mltService = Xml::getXmlProperty(filter, "mlt_service");

            QList<QString> propertiesToFix;
            propertiesToFix.append(filterPropertiesToFix);

            if (servicePropertiesToFix.contains(mltService)) {
                propertiesToFix.append(servicePropertiesToFix.value(mltService));
            }

            qDebug() << "Decimal point: Found MLT service" << mltService << "and will fix " << propertiesToFix;

            for (const QString &property : propertiesToFix) {
                QString value = Xml::getXmlProperty(filter, property);
                if (!value.isEmpty()) {
                    QString newValue = QString(value).replace(decimalPoint, ".");
                    if (value != newValue) {
                        Xml::setXmlProperty(filter, property, newValue);
                        qDebug() << "Decimal point: Converted service property" << mltService << "from " << value << "to" << newValue;
                    }
                }
            }

        }

        auto fixAttribute = [fixTimecode](QDomElement &el, const QString &attributeName) {
            if (el.hasAttribute(attributeName)) {
                QString oldValue = el.attribute(attributeName, "");
                QString newValue(oldValue);
                fixTimecode(newValue);
                if (oldValue != newValue) {
                    el.setAttribute(attributeName, newValue);
                    qDebug() << "Decimal point: Converted" << el.nodeName() << attributeName << "from" << oldValue << "to" << newValue;
                }
            }
        };

        // Fix attributes
        QList<QString> tagsToFix = {"producer", "filter", "tractor", "entry", "transition", "blank"};
        for (const QString &tag : tagsToFix) {
            QDomNodeList elements = m_doc.elementsByTagName(tag);
            for (int i = 0; i < elements.count(); i++) {
                QDomElement el = elements.at(i).toElement();
                fixAttribute(el, "in");
                fixAttribute(el, "out");
                fixAttribute(el, "length");
            }
        }

        auto mltElements = m_doc.elementsByTagName("mlt");
        for (int i = 0; i < mltElements.count(); i++) {
            QDomElement mltElement = mltElements.at(i).toElement();
            if (mltElement.hasAttribute("LC_NUMERIC")) {
                qDebug() << "Removing LC_NUMERIC=" << mltElement.attribute("LC_NUMERIC") << "from root node";
                mltElement.removeAttribute("LC_NUMERIC");
            }
        }

        modified = true;
        qDebug() << "Decimal point: New XML: " << m_doc.toString(-1);

    } else {
        qDebug() << "Decimal point is OK";
    }

    m_modified |= modified;
    return modified ? decimalPoint : QString();
}

void DocumentValidator::convertKeyframeEffect_093(const QDomElement &effect, const QStringList &params, QMap<int, double> &values, int offset)
{
    QLocale locale; // Used for upgrading to pre-1.0 version → OK
    int in = effect.attribute(QStringLiteral("in")).toInt() - offset;
    values.insert(in, locale.toDouble(Xml::getXmlProperty(effect, params.at(0))));
    QString endValue = Xml::getXmlProperty(effect, params.at(1));
    if (!endValue.isEmpty()) {
        int out = effect.attribute(QStringLiteral("out")).toInt() - offset;
        values.insert(out, locale.toDouble(endValue));
    }
}

void DocumentValidator::updateProducerInfo(const QDomElement &prod, const QDomElement &source)
{
    QString pxy = source.attribute(QStringLiteral("proxy"));
    if (pxy.length() > 1) {
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:proxy"), pxy);
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:originalurl"), source.attribute(QStringLiteral("resource")));
    }
    if (source.hasAttribute(QStringLiteral("file_hash"))) {
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:file_hash"), source.attribute(QStringLiteral("file_hash")));
    }
    if (source.hasAttribute(QStringLiteral("file_size"))) {
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:file_size"), source.attribute(QStringLiteral("file_size")));
    }
    if (source.hasAttribute(QStringLiteral("name"))) {
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:clipname"), source.attribute(QStringLiteral("name")));
    }
    if (source.hasAttribute(QStringLiteral("zone_out"))) {
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:zone_out"), source.attribute(QStringLiteral("zone_out")));
    }
    if (source.hasAttribute(QStringLiteral("zone_in"))) {
        Xml::setXmlProperty(prod, QStringLiteral("kdenlive:zone_in"), source.attribute(QStringLiteral("zone_in")));
    }
    if (source.hasAttribute(QStringLiteral("cutzones"))) {
        QString zoneData = source.attribute(QStringLiteral("cutzones"));
        const QStringList zoneList = zoneData.split(QLatin1Char(';'));
        int ct = 1;
        for (const QString &data : zoneList) {
            QString zoneName = data.section(QLatin1Char('-'), 2);
            if (zoneName.isEmpty()) {
                zoneName = i18n("Zone %1", ct++);
            }
            Xml::setXmlProperty(prod, QStringLiteral("kdenlive:clipzone.") + zoneName,
                                data.section(QLatin1Char('-'), 0, 0) + QLatin1Char(';') + data.section(QLatin1Char('-'), 1, 1));
        }
    }
}

QStringList DocumentValidator::getInfoFromEffectName(const QString &oldName)
{
    QStringList info;
    // Returns a list to convert old Kdenlive ladspa effects
    if (oldName == QLatin1String("pitch_shift")) {
        info << QStringLiteral("ladspa.1433");
        info << QStringLiteral("pitch=0");
    } else if (oldName == QLatin1String("vinyl")) {
        info << QStringLiteral("ladspa.1905");
        info << QStringLiteral("year=0");
        info << QStringLiteral("rpm=1");
        info << QStringLiteral("warping=2");
        info << QStringLiteral("crackle=3");
        info << QStringLiteral("wear=4");
    } else if (oldName == QLatin1String("room_reverb")) {
        info << QStringLiteral("ladspa.1216");
        info << QStringLiteral("room=0");
        info << QStringLiteral("delay=1");
        info << QStringLiteral("damp=2");
    } else if (oldName == QLatin1String("reverb")) {
        info << QStringLiteral("ladspa.1423");
        info << QStringLiteral("room=0");
        info << QStringLiteral("damp=1");
    } else if (oldName == QLatin1String("rate_scale")) {
        info << QStringLiteral("ladspa.1417");
        info << QStringLiteral("rate=0");
    } else if (oldName == QLatin1String("pitch_scale")) {
        info << QStringLiteral("ladspa.1193");
        info << QStringLiteral("coef=0");
    } else if (oldName == QLatin1String("phaser")) {
        info << QStringLiteral("ladspa.1217");
        info << QStringLiteral("rate=0");
        info << QStringLiteral("depth=1");
        info << QStringLiteral("feedback=2");
        info << QStringLiteral("spread=3");
    } else if (oldName == QLatin1String("limiter")) {
        info << QStringLiteral("ladspa.1913");
        info << QStringLiteral("gain=0");
        info << QStringLiteral("limit=1");
        info << QStringLiteral("release=2");
    } else if (oldName == QLatin1String("equalizer_15")) {
        info << QStringLiteral("ladspa.1197");
        info << QStringLiteral("1=0");
        info << QStringLiteral("2=1");
        info << QStringLiteral("3=2");
        info << QStringLiteral("4=3");
        info << QStringLiteral("5=4");
        info << QStringLiteral("6=5");
        info << QStringLiteral("7=6");
        info << QStringLiteral("8=7");
        info << QStringLiteral("9=8");
        info << QStringLiteral("10=9");
        info << QStringLiteral("11=10");
        info << QStringLiteral("12=11");
        info << QStringLiteral("13=12");
        info << QStringLiteral("14=13");
        info << QStringLiteral("15=14");
    } else if (oldName == QLatin1String("equalizer")) {
        info << QStringLiteral("ladspa.1901");
        info << QStringLiteral("logain=0");
        info << QStringLiteral("midgain=1");
        info << QStringLiteral("higain=2");
    } else if (oldName == QLatin1String("declipper")) {
        info << QStringLiteral("ladspa.1195");
    }
    return info;
}

QString DocumentValidator::colorToString(const QColor &c)
{
    QString ret = QStringLiteral("%1,%2,%3,%4");
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

bool DocumentValidator::isProject() const
{
    return m_doc.documentElement().tagName() == QLatin1String("mlt");
}

bool DocumentValidator::isModified() const
{
    return m_modified;
}

bool DocumentValidator::checkMovit()
{
    QString playlist = m_doc.toString();
    if (!playlist.contains(QStringLiteral("movit."))) {
        // Project does not use Movit GLSL effects, we can load it
        return true;
    }
    if (KMessageBox::questionYesNo(QApplication::activeWindow(),
                                   i18n("The project file uses some GPU effects. GPU acceleration is not currently enabled.\nDo you want to convert the "
                                        "project to a non-GPU version?\nThis might result in data loss.")) != KMessageBox::Yes) {
        return false;
    }
    // Try to convert Movit filters to their non GPU equivalent
    QStringList convertedFilters;
    QStringList discardedFilters;
    bool hasWB = EffectsRepository::get()->exists(QStringLiteral("frei0r.colgate"));
    bool hasBlur = EffectsRepository::get()->exists(QStringLiteral("frei0r.IIRblur"));
    QString compositeTrans;
    if (TransitionsRepository::get()->exists(QStringLiteral("qtblend"))) {
        compositeTrans = QStringLiteral("qtblend");
    } else if (TransitionsRepository::get()->exists(QStringLiteral("frei0r.cairoblend"))) {
        compositeTrans = QStringLiteral("frei0r.cairoblend");
    }

    // Parse all effects in document
    QDomNodeList filters = m_doc.elementsByTagName(QStringLiteral("filter"));
    int max = filters.count();
    for (int i = 0; i < max; ++i) {
        QDomElement filt = filters.at(i).toElement();
        QString filterId = filt.attribute(QStringLiteral("id"));
        if (!filterId.startsWith(QLatin1String("movit."))) {
            continue;
        }
        if (filterId == QLatin1String("movit.white_balance") && hasWB) {
            // Convert to frei0r.colgate
            filt.setAttribute(QStringLiteral("id"), QStringLiteral("frei0r.colgate"));
            Xml::setXmlProperty(filt, QStringLiteral("kdenlive_id"), QStringLiteral("frei0r.colgate"));
            Xml::setXmlProperty(filt, QStringLiteral("tag"), QStringLiteral("frei0r.colgate"));
            Xml::setXmlProperty(filt, QStringLiteral("mlt_service"), QStringLiteral("frei0r.colgate"));
            Xml::renameXmlProperty(filt, QStringLiteral("neutral_color"), QStringLiteral("Neutral Color"));
            QString value = Xml::getXmlProperty(filt, QStringLiteral("color_temperature"));
            value = factorizeGeomValue(value, 15000.0);
            Xml::setXmlProperty(filt, QStringLiteral("color_temperature"), value);
            Xml::renameXmlProperty(filt, QStringLiteral("color_temperature"), QStringLiteral("Color Temperature"));
            convertedFilters << filterId;
            continue;
        }
        if (filterId == QLatin1String("movit.blur") && hasBlur) {
            // Convert to frei0r.IIRblur
            filt.setAttribute(QStringLiteral("id"), QStringLiteral("frei0r.IIRblur"));
            Xml::setXmlProperty(filt, QStringLiteral("kdenlive_id"), QStringLiteral("frei0r.IIRblur"));
            Xml::setXmlProperty(filt, QStringLiteral("tag"), QStringLiteral("frei0r.IIRblur"));
            Xml::setXmlProperty(filt, QStringLiteral("mlt_service"), QStringLiteral("frei0r.IIRblur"));
            Xml::renameXmlProperty(filt, QStringLiteral("radius"), QStringLiteral("Amount"));
            QString value = Xml::getXmlProperty(filt, QStringLiteral("Amount"));
            value = factorizeGeomValue(value, 14.0);
            Xml::setXmlProperty(filt, QStringLiteral("Amount"), value);
            convertedFilters << filterId;
            continue;
        }
        if (filterId == QLatin1String("movit.mirror")) {
            // Convert to MLT's mirror
            filt.setAttribute(QStringLiteral("id"), QStringLiteral("mirror"));
            Xml::setXmlProperty(filt, QStringLiteral("kdenlive_id"), QStringLiteral("mirror"));
            Xml::setXmlProperty(filt, QStringLiteral("tag"), QStringLiteral("mirror"));
            Xml::setXmlProperty(filt, QStringLiteral("mlt_service"), QStringLiteral("mirror"));
            Xml::setXmlProperty(filt, QStringLiteral("mirror"), QStringLiteral("flip"));
            convertedFilters << filterId;
            continue;
        }
        if (filterId.startsWith(QLatin1String("movit."))) {
            // TODO: implement conversion for more filters
            discardedFilters << filterId;
        }
    }

    // Parse all transitions in document
    QDomNodeList transitions = m_doc.elementsByTagName(QStringLiteral("transition"));
    max = transitions.count();
    for (int i = 0; i < max; ++i) {
        QDomElement t = transitions.at(i).toElement();
        QString transId = Xml::getXmlProperty(t, QStringLiteral("mlt_service"));
        if (!transId.startsWith(QLatin1String("movit."))) {
            continue;
        }
        if (transId == QLatin1String("movit.overlay") && !compositeTrans.isEmpty()) {
            // Convert to frei0r.cairoblend
            Xml::setXmlProperty(t, QStringLiteral("mlt_service"), compositeTrans);
            convertedFilters << transId;
            continue;
        }
        if (transId.startsWith(QLatin1String("movit."))) {
            // TODO: implement conversion for more filters
            discardedFilters << transId;
        }
    }

    convertedFilters.removeDuplicates();
    discardedFilters.removeDuplicates();
    if (discardedFilters.isEmpty()) {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following filters/transitions were converted to non GPU versions:"),
                                     convertedFilters);
    } else {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following filters/transitions were deleted from the project:"), discardedFilters);
    }
    m_modified = true;
    QString scene = m_doc.toString();
    scene.replace(QLatin1String("movit."), QString());
    m_doc.setContent(scene);
    return true;
}

QString DocumentValidator::factorizeGeomValue(const QString &value, double factor)
{
    const QStringList vals = value.split(QLatin1Char(';'));
    QString result;
    for (int i = 0; i < vals.count(); i++) {
        const QString &s = vals.at(i);
        QString key = s.section(QLatin1Char('='), 0, 0);
        QString val = s.section(QLatin1Char('='), 1, 1);
        double v = val.toDouble() / factor;
        result.append(key + QLatin1Char('=') + QString::number(v, 'f'));
        if (i + 1 < vals.count()) {
            result.append(QLatin1Char(';'));
        }
    }
    return result;
}

void DocumentValidator::checkOrphanedProducers()
{
    QDomElement mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
    QDomElement main = mlt.firstChildElement(QStringLiteral("playlist"));
    QDomNodeList bin_producers = main.childNodes();
    QStringList binProducers;
    for (int k = 0; k < bin_producers.count(); k++) {
        QDomElement mltprod = bin_producers.at(k).toElement();
        if (mltprod.tagName() != QLatin1String("entry")) {
            continue;
        }
        binProducers << mltprod.attribute(QStringLiteral("producer"));
    }

    QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
    int max = producers.count();
    QStringList allProducers;
    for (int i = 0; i < max; ++i) {
        QDomElement prod = producers.item(i).toElement();
        if (prod.isNull()) {
            continue;
        }
        allProducers << prod.attribute(QStringLiteral("id"));
    }

    QDomDocumentFragment frag = m_doc.createDocumentFragment();
    QDomDocumentFragment trackProds = m_doc.createDocumentFragment();
    for (int i = 0; i < producers.count(); ++i) {
        QDomElement prod = producers.item(i).toElement();
        if (prod.isNull()) {
            continue;
        }
        QString id = prod.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
        if (id.startsWith(QLatin1String("slowmotion")) || id == QLatin1String("black")) {
            continue;
        }
        if (!binProducers.contains(id)) {
            QString binId = Xml::getXmlProperty(prod, QStringLiteral("kdenlive:binid"));
            Xml::setXmlProperty(prod, QStringLiteral("kdenlive:id"), binId);
            if (!binId.isEmpty() && binProducers.contains(binId)) {
                continue;
            }
            qCWarning(KDENLIVE_LOG) << " ///////// WARNING, FOUND UNKNOWN PRODUDER: " << id << " ----------------";
            // This producer is unknown to Bin
            QString service = Xml::getXmlProperty(prod, QStringLiteral("mlt_service"));
            QString distinctiveTag(QStringLiteral("resource"));
            if (service == QLatin1String("kdenlivetitle")) {
                distinctiveTag = QStringLiteral("xmldata");
            }
            QString orphanValue = Xml::getXmlProperty(prod, distinctiveTag);
            for (int j = 0; j < producers.count(); j++) {
                // Search for a similar producer
                QDomElement binProd = producers.item(j).toElement();
                binId = binProd.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
                if (service != QLatin1String("timewarp") && (binId.startsWith(QLatin1String("slowmotion")) || !binProducers.contains(binId))) {
                    continue;
                }
                QString binService = Xml::getXmlProperty(binProd, QStringLiteral("mlt_service"));
                qCDebug(KDENLIVE_LOG) << " / /LKNG FOR: " << service << " / " << orphanValue << ", checking: " << binProd.attribute(QStringLiteral("id"));
                if (service != binService) {
                    continue;
                }
                QString binValue = Xml::getXmlProperty(binProd, distinctiveTag);
                if (binValue == orphanValue) {
                    // Found probable source producer, replace
                    frag.appendChild(prod);
                    if (i > 0) {
                        i--;
                    }
                    QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));
                    for (int k = 0; k < entries.count(); k++) {
                        QDomElement entry = entries.at(k).toElement();
                        if (entry.attribute(QStringLiteral("producer")) == id) {
                            QString entryId = binId;
                            if (service.contains(QStringLiteral("avformat")) || service == QLatin1String("xml") || service == QLatin1String("consumer")) {
                                // We must use track producer, find track for this entry
                                QString trackPlaylist = entry.parentNode().toElement().attribute(QStringLiteral("id"));
                                entryId.append(QLatin1Char('_') + trackPlaylist);
                            }
                            if (!allProducers.contains(entryId)) {
                                // The track producer does not exist, create a clone for it
                                QDomElement cloned = binProd.cloneNode(true).toElement();
                                cloned.setAttribute(QStringLiteral("id"), entryId);
                                trackProds.appendChild(cloned);
                                allProducers << entryId;
                            }
                            entry.setAttribute(QStringLiteral("producer"), entryId);
                            m_modified = true;
                        }
                    }
                    continue;
                }
            }
        }
    }
    if (!trackProds.isNull()) {
        QDomNode firstProd = m_doc.firstChildElement(QStringLiteral("producer"));
        mlt.insertBefore(trackProds, firstProd);
    }
}

void DocumentValidator::fixTitleProducerLocale(QDomElement &producer)
{
    QString data = Xml::getXmlProperty(producer, QStringLiteral("xmldata"));
    QDomDocument doc;
    doc.setContent(data);
    QDomNodeList nodes = doc.elementsByTagName(QStringLiteral("position"));
    bool fixed = false;
    for (int i = 0; i < nodes.count(); i++) {
        QDomElement pos = nodes.at(i).toElement();
        QString x = pos.attribute(QStringLiteral("x"));
        QString y = pos.attribute(QStringLiteral("y"));
        if (x.contains(QLatin1Char(','))) {
            // x pos was saved in locale format, fix
            x = x.section(QLatin1Char(','), 0, 0);
            pos.setAttribute(QStringLiteral("x"), x);
            fixed = true;
        }
        if (y.contains(QLatin1Char(','))) {
            // x pos was saved in locale format, fix
            y = y.section(QLatin1Char(','), 0, 0);
            pos.setAttribute(QStringLiteral("y"), y);
            fixed = true;
        }
    }
    nodes = doc.elementsByTagName(QStringLiteral("content"));
    for (int i = 0; i < nodes.count(); i++) {
        QDomElement pos = nodes.at(i).toElement();
        QString x = pos.attribute(QStringLiteral("font-outline"));
        QString y = pos.attribute(QStringLiteral("textwidth"));
        if (x.contains(QLatin1Char(','))) {
            // x pos was saved in locale format, fix
            x = x.section(QLatin1Char(','), 0, 0);
            pos.setAttribute(QStringLiteral("font-outline"), x);
            fixed = true;
        }
        if (y.contains(QLatin1Char(','))) {
            // x pos was saved in locale format, fix
            y = y.section(QLatin1Char(','), 0, 0);
            pos.setAttribute(QStringLiteral("textwidth"), y);
            fixed = true;
        }
    }
    if (fixed) {
        Xml::setXmlProperty(producer, QStringLiteral("xmldata"), doc.toString());
    }
}
