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

#include "definitions.h"
#include "effectslist/initeffects.h"
#include "mainwindow.h"
#include "core.h"
#include "mltcontroller/bincontroller.h"

#include <QDebug>
#include <KMessageBox>
#include <klocalizedstring.h>

#include <QFile>
#include <QColor>
#include <QString>
#include <QDir>
#include <QScriptEngine>

#include <mlt++/Mlt.h>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

#include <QStandardPaths>


DocumentValidator::DocumentValidator(const QDomDocument &doc, const QUrl &documentUrl):
        m_doc(doc),
        m_url(documentUrl),
        m_modified(false)
{}

bool DocumentValidator::validate(const double currentVersion)
{
    QDomElement mlt = m_doc.firstChildElement("mlt");
    // At least the root element must be there
    if (mlt.isNull())
        return false;

    QDomElement kdenliveDoc = mlt.firstChildElement("kdenlivedoc");
    QString rootDir = mlt.attribute("root");
    if (rootDir == "$CURRENTPATH") {
        // The document was extracted from a Kdenlive archived project, fix root directory
        QString playlist = m_doc.toString();
        playlist.replace("$CURRENTPATH", m_url.adjusted(QUrl::RemoveFilename).path());
        m_doc.setContent(playlist);
        mlt = m_doc.firstChildElement("mlt");
        kdenliveDoc = mlt.firstChildElement("kdenlivedoc");
    }

    // Previous MLT / Kdenlive versions used C locale by default
    QLocale documentLocale = QLocale::c();

    if (mlt.hasAttribute("LC_NUMERIC")) {
        // Set locale for the document
#ifndef Q_OS_MAC
        const QString newLocale = setlocale(LC_NUMERIC, mlt.attribute("LC_NUMERIC").toUtf8().constData());
#else
        const QString newLocale = setlocale(LC_NUMERIC_MASK, mlt.attribute("LC_NUMERIC").toUtf8().constData());
#endif
        documentLocale = QLocale(mlt.attribute("LC_NUMERIC"));

        // Make sure Qt locale and C++ locale have the same numeric separator, might not be the case
        // With some locales since C++ and Qt use a different database for locales
        char *separator = localeconv()->decimal_point;
	if (newLocale.isEmpty()) {
            // Requested locale not available, ask for install
            KMessageBox::sorry(QApplication::activeWindow(), i18n("The document was created in \"%1\" locale, which is not installed on your system. Please install that language pack. Until then, Kdenlive might not be able to correctly open the document.", mlt.attribute("LC_NUMERIC")));
        }
	
        if (separator != documentLocale.decimalPoint()) {
	    KMessageBox::sorry(QApplication::activeWindow(), i18n("There is a locale conflict on your system. The document uses locale %1 which uses a \"%2\" as numeric separator (in system libraries) but Qt expects \"%3\". You might not be able to correctly open the project.", mlt.attribute("LC_NUMERIC"), separator, documentLocale.decimalPoint()));
            //qDebug()<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
            // HACK: There is a locale conflict, so set locale to at least have correct decimal point
            if (strncmp(separator, ".", 1) == 0) documentLocale = QLocale::c();
            else if (strncmp(separator, ",", 1) == 0) documentLocale = QLocale("fr_FR.UTF-8");
        }
    }
    
    documentLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    if (documentLocale.decimalPoint() != QLocale().decimalPoint()) {
        // If loading an older MLT file without LC_NUMERIC, set locale to C which was previously the default
        if (!mlt.hasAttribute("LC_NUMERIC")) {
#ifndef Q_OS_MAC
            setlocale(LC_NUMERIC, "C");
#else
            setlocale(LC_NUMERIC_MASK, "C");
#endif
	}

        QLocale::setDefault(documentLocale);
        // locale conversion might need to be redone
#ifndef Q_OS_MAC
        initEffects::parseEffectFiles(pCore->binController()->mltRepository(), setlocale(LC_NUMERIC, NULL));
#else
        initEffects::parseEffectFiles(pCore->binController()->mltRepository(), setlocale(LC_NUMERIC_MASK, NULL));
#endif
    }
    double version = -1;
    if (kdenliveDoc.isNull() || !kdenliveDoc.hasAttribute("version")) {
        // Newer Kdenlive document version
        QDomElement main = mlt.firstChildElement("playlist");
        version = EffectsList::property(main, "kdenlive:docproperties.version").toDouble();
    }
    else {
        bool ok;
        version = documentLocale.toDouble(kdenliveDoc.attribute("version"), &ok);
        if (!ok) {
            // Could not parse version number, there is probably a conflict in decimal separator
            QLocale tempLocale = QLocale(mlt.attribute("LC_NUMERIC"));
            version = tempLocale.toDouble(kdenliveDoc.attribute("version"), &ok);
            if (!ok) version = kdenliveDoc.attribute("version").toDouble(&ok);
            if (!ok) {
                // Last try: replace comma with a dot
                QString versionString = kdenliveDoc.attribute("version");
                if (versionString.contains(',')) versionString.replace(',', '.');
                version = versionString.toDouble(&ok);
                if (!ok) qDebug()<<"// CANNOT PARSE VERSION NUMBER, ERROR!";
            }
        }
    }
    // Upgrade the document to the latest version
    if (!upgrade(version, currentVersion))
        return false;

    checkOrphanedProducers();

    return true;
    /*
    // Check the syntax (this will be replaced by XSD validation with Qt 4.6)
    // and correct some errors
    {
        // Return (or create) the tractor
        QDomElement tractor = mlt.firstChildElement("tractor");
        if (tractor.isNull()) {
            m_modified = true;
            tractor = m_doc.createElement("tractor");
            tractor.setAttribute("global_feed", "1");
            tractor.setAttribute("in", "0");
            tractor.setAttribute("out", "-1");
            tractor.setAttribute("id", "maintractor");
            mlt.appendChild(tractor);
        }

        // Make sure at least one track exists, and they're equal in number to
        // to the maximum between MLT and Kdenlive playlists and tracks
        //
        // In older Kdenlive project files, one playlist is not a real track (the black track), we have: track count = playlist count- 1
        // In newer Qt5 Kdenlive, the Bin playlist should not appear as a track. So we should have: track count = playlist count- 2
        int trackOffset = 1;
        QDomNodeList playlists = m_doc.elementsByTagName("playlist");
	// Remove "main bin" playlist that simply holds the bin's clips and is not a real playlist
	for (int i = 0; i < playlists.count(); ++i) {
	    QString playlistId = playlists.at(i).toElement().attribute("id");
	    if (playlistId == BinController::binPlaylistId()) {
		// remove pseudo-playlist
		//playlists.at(i).parentNode().removeChild(playlists.at(i));
                trackOffset = 2;
		break;
	    }
	}
	
        int tracksMax = playlists.count() - trackOffset; // Remove the black track and bin track
        QDomNodeList tracks = tractor.elementsByTagName("track");
        tracksMax = qMax(tracks.count() - 1, tracksMax);
        QDomNodeList tracksinfo = kdenliveDoc.elementsByTagName("trackinfo");
        tracksMax = qMax(tracksinfo.count(), tracksMax);
        tracksMax = qMax(1, tracksMax); // Force existence of one track
        if (playlists.count() - trackOffset < tracksMax ||
                tracks.count() < tracksMax ||
                tracksinfo.count() < tracksMax) {
            qDebug() << "//// WARNING, PROJECT IS CORRUPTED, MISSING TRACK";
            m_modified = true;
            int difference;
            // use the MLT tracks as reference
            if (tracks.count() - 1 < tracksMax) {
                // Looks like one MLT track is missing, remove the extra Kdenlive track if there is one.
                if (tracksinfo.count() != tracks.count() - 1) {
                    // The Kdenlive tracks are not ok, clear and rebuild them
                    QDomNode tinfo = kdenliveDoc.firstChildElement("tracksinfo");
                    QDomNode tnode = tinfo.firstChild();
                    while (!tnode.isNull()) {
                        tinfo.removeChild(tnode);
                        tnode = tinfo.firstChild();
                    }

                    for (int i = 1; i < tracks.count(); ++i) {
                        QString hide = tracks.at(i).toElement().attribute("hide");
                        QDomElement newTrack = m_doc.createElement("trackinfo");
                        if (hide == "video") {
                            // audio track;
                            newTrack.setAttribute("type", "audio");
                            newTrack.setAttribute("blind", 1);
                            newTrack.setAttribute("mute", 0);
                            newTrack.setAttribute("lock", 0);
                        } else {
                            newTrack.setAttribute("blind", 0);
                            newTrack.setAttribute("mute", 0);
                            newTrack.setAttribute("lock", 0);
                        }
                        tinfo.appendChild(newTrack);
                    }
                }
            }

            if (playlists.count() - 1 < tracksMax) {
                difference = tracksMax - (playlists.count() - 1);
                for (int i = 0; i < difference; ++i) {
                    QDomElement playlist = m_doc.createElement("playlist");
                    mlt.appendChild(playlist);
                }
            }
            if (tracks.count() - 1 < tracksMax) {
                difference = tracksMax - (tracks.count() - 1);
                for (int i = 0; i < difference; ++i) {
                    QDomElement track = m_doc.createElement("track");
                    tractor.appendChild(track);
                }
            }
            if (tracksinfo.count() < tracksMax) {
                QDomElement tracksinfoElm = kdenliveDoc.firstChildElement("tracksinfo");
                if (tracksinfoElm.isNull()) {
                    tracksinfoElm = m_doc.createElement("tracksinfo");
                    kdenliveDoc.appendChild(tracksinfoElm);
                }
                difference = tracksMax - tracksinfo.count();
                for (int i = 0; i < difference; ++i) {
                    QDomElement trackinfo = m_doc.createElement("trackinfo");
                    trackinfo.setAttribute("mute", "0");
                    trackinfo.setAttribute("locked", "0");
                    tracksinfoElm.appendChild(trackinfo);
                }
            }
        }
        // TODO: check the tracks references
        // TODO: check internal mix transitions
    }

    updateEffects();

    return true;
    */
}

bool DocumentValidator::upgrade(double version, const double currentVersion)
{
    qDebug() << "Opening a document with version " << version << " / "<<currentVersion;

    // No conversion needed
    if (version == currentVersion) {
        return true;
    }

    // The document is too new
    if (version > currentVersion) {
        //qDebug() << "Unable to open document with version " << version;
        KMessageBox::sorry(QApplication::activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.\nPlease consider upgrading your Kdenlive version.", version), i18n("Unable to open project"));
        return false;
    }

    // Unsupported document versions
    if (version == 0.5 || version == 0.7) {
        //qDebug() << "Unable to open document with version " << version;
        KMessageBox::sorry(QApplication::activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.", version), i18n("Unable to open project"));
        return false;
    }

    // <kdenlivedoc />
    QDomNode infoXmlNode;
    QDomElement infoXml;
    QDomNodeList docs = m_doc.elementsByTagName("kdenlivedoc");
    if (!docs.isEmpty()) {
        infoXmlNode = m_doc.elementsByTagName("kdenlivedoc").at(0);
        infoXml = infoXmlNode.toElement();
        infoXml.setAttribute("upgraded", "1");
    }
    m_doc.documentElement().setAttribute("upgraded", "1");

    if (version <= 0.6) {
        QDomElement infoXml_old = infoXmlNode.cloneNode(true).toElement(); // Needed for folders
        QDomNode westley = m_doc.elementsByTagName("westley").at(1);
        QDomNode tractor = m_doc.elementsByTagName("tractor").at(0);
        QDomNode multitrack = m_doc.elementsByTagName("multitrack").at(0);
        QDomNodeList playlists = m_doc.elementsByTagName("playlist");

        QDomNode props = m_doc.elementsByTagName("properties").at(0).toElement();
        QString profile = props.toElement().attribute("videoprofile");
        int startPos = props.toElement().attribute("timeline_position").toInt();
        if (profile == "dv_wide")
            profile = "dv_pal_wide";

        // move playlists outside of tractor and add the tracks instead
        int max = playlists.count();
        if (westley.isNull()) {
            westley = m_doc.createElement("westley");
            m_doc.documentElement().appendChild(westley);
        }
        if (tractor.isNull()) {
            //qDebug() << "// NO MLT PLAYLIST, building empty one";
            QDomElement blank_tractor = m_doc.createElement("tractor");
            westley.appendChild(blank_tractor);
            QDomElement blank_playlist = m_doc.createElement("playlist");
            blank_playlist.setAttribute("id", "black_track");
            westley.insertBefore(blank_playlist, QDomNode());
            QDomElement blank_track = m_doc.createElement("track");
            blank_track.setAttribute("producer", "black_track");
            blank_tractor.appendChild(blank_track);

            QDomNodeList kdenlivetracks = m_doc.elementsByTagName("kdenlivetrack");
            for (int i = 0; i < kdenlivetracks.count(); ++i) {
                blank_playlist = m_doc.createElement("playlist");
                blank_playlist.setAttribute("id", "playlist" + QString::number(i));
                westley.insertBefore(blank_playlist, QDomNode());
                blank_track = m_doc.createElement("track");
                blank_track.setAttribute("producer", "playlist" + QString::number(i));
                blank_tractor.appendChild(blank_track);
                if (kdenlivetracks.at(i).toElement().attribute("cliptype") == "Sound") {
                    blank_playlist.setAttribute("hide", "video");
                    blank_track.setAttribute("hide", "video");
                }
            }
        } else for (int i = 0; i < max; ++i) {
                QDomNode n = playlists.at(i);
                westley.insertBefore(n, QDomNode());
                QDomElement pl = n.toElement();
                QDomElement track = m_doc.createElement("track");
                QString trackType = pl.attribute("hide");
                if (!trackType.isEmpty())
                    track.setAttribute("hide", trackType);
                QString playlist_id =  pl.attribute("id");
                if (playlist_id.isEmpty()) {
                    playlist_id = "black_track";
                    pl.setAttribute("id", playlist_id);
                }
                track.setAttribute("producer", playlist_id);
                //tractor.appendChild(track);
#define KEEP_TRACK_ORDER 1
#ifdef KEEP_TRACK_ORDER
                tractor.insertAfter(track, QDomNode());
#else
                // Insert the new track in an order that hopefully matches the 3 video, then 2 audio tracks of Kdenlive 0.7.0
                // insertion sort - O( tracks*tracks )
                // Note, this breaks _all_ transitions - but you can move them up and down afterwards.
                QDomElement tractor_elem = tractor.toElement();
                if (! tractor_elem.isNull()) {
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
                                //qDebug() << "playlist_id: " << playlist_id << " producer:" << track_elem.attribute("producer");
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
                    qWarning() << "tractor was not a QDomElement";
                    tractor.insertAfter(track, QDomNode());
                }
#endif
            }
        tractor.removeChild(multitrack);

        // audio track mixing transitions should not be added to track view, so add required attribute
        QDomNodeList transitions = m_doc.elementsByTagName("transition");
        max = transitions.count();
        for (int i = 0; i < max; ++i) {
            QDomElement tr = transitions.at(i).toElement();
            if (tr.attribute("combine") == "1" && tr.attribute("mlt_service") == "mix") {
                QDomElement property = m_doc.createElement("property");
                property.setAttribute("name", "internal_added");
                QDomText value = m_doc.createTextNode("237");
                property.appendChild(value);
                tr.appendChild(property);
                property = m_doc.createElement("property");
                property.setAttribute("name", "mlt_service");
                value = m_doc.createTextNode("mix");
                property.appendChild(value);
                tr.appendChild(property);
            } else {
                // convert transition
                QDomNamedNodeMap attrs = tr.attributes();
                for (int j = 0; j < attrs.count(); ++j) {
                    QString attrName = attrs.item(j).nodeName();
                    if (attrName != "in" && attrName != "out" && attrName != "id") {
                        QDomElement property = m_doc.createElement("property");
                        property.setAttribute("name", attrName);
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
        QDomNodeList entries = m_doc.elementsByTagName("entry");
        max = entries.count();
        for (int i = 0; i < max; ++i) {
            QString last_id;
            int effectix = 0;
            QDomNode m = entries.at(i).firstChild();
            while (!m.isNull()) {
                if (m.toElement().tagName() == "filter") {
                    QDomElement filt = m.toElement();
                    QDomNamedNodeMap attrs = filt.attributes();
                    QString current_id = filt.attribute("kdenlive_id");
                    if (current_id != last_id) {
                        effectix++;
                        last_id = current_id;
                    }
                    QDomElement e = m_doc.createElement("property");
                    e.setAttribute("name", "kdenlive_ix");
                    QDomText value = m_doc.createTextNode(QString::number(effectix));
                    e.appendChild(value);
                    filt.appendChild(e);
                    for (int j = 0; j < attrs.count(); ++j) {
                        QDomAttr a = attrs.item(j).toAttr();
                        if (!a.isNull()) {
                            //qDebug() << " FILTER; adding :" << a.name() << ':' << a.value();
                            QDomElement e = m_doc.createElement("property");
                            e.setAttribute("name", a.name());
                            QDomText value = m_doc.createTextNode(a.value());
                            e.appendChild(value);
                            filt.appendChild(e);

                        }
                    }
                }
                m = m.nextSibling();
            }
        }

        /*
            QDomNodeList filters = m_doc.elementsByTagName("filter");
            max = filters.count();
            QString last_id;
            int effectix = 0;
            for (int i = 0; i < max; ++i) {
                QDomElement filt = filters.at(i).toElement();
                QDomNamedNodeMap attrs = filt.attributes();
        QString current_id = filt.attribute("kdenlive_id");
        if (current_id != last_id) {
            effectix++;
            last_id = current_id;
        }
        QDomElement e = m_doc.createElement("property");
                e.setAttribute("name", "kdenlive_ix");
                QDomText value = m_doc.createTextNode(QString::number(1));
                e.appendChild(value);
                filt.appendChild(e);
                for (int j = 0; j < attrs.count(); ++j) {
                    QDomAttr a = attrs.item(j).toAttr();
                    if (!a.isNull()) {
                        //qDebug() << " FILTER; adding :" << a.name() << ':' << a.value();
                        QDomElement e = m_doc.createElement("property");
                        e.setAttribute("name", a.name());
                        QDomText value = m_doc.createTextNode(a.value());
                        e.appendChild(value);
                        filt.appendChild(e);
                    }
                }
            }*/

        // fix slowmotion
        QDomNodeList producers = westley.toElement().elementsByTagName("producer");
        max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (prod.attribute("mlt_service") == "framebuffer") {
                QString slowmotionprod = prod.attribute("resource");
                slowmotionprod.replace(':', '?');
                //qDebug() << "// FOUND WRONG SLOWMO, new: " << slowmotionprod;
                prod.setAttribute("resource", slowmotionprod);
            }
        }
        // move producers to correct place, markers to a global list, fix clip descriptions
        QDomElement markers = m_doc.createElement("markers");
        // This will get the xml producers:
        producers = m_doc.elementsByTagName("producer");
        max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(0).toElement();
            // add resource also as a property (to allow path correction in setNewResource())
            // TODO: will it work with slowmotion? needs testing
            /*if (!prod.attribute("resource").isEmpty()) {
                QDomElement prop_resource = m_doc.createElement("property");
                prop_resource.setAttribute("name", "resource");
                QDomText resource = m_doc.createTextNode(prod.attribute("resource"));
                prop_resource.appendChild(resource);
                prod.appendChild(prop_resource);
            }*/
            QDomNode m = prod.firstChild();
            if (!m.isNull()) {
                if (m.toElement().tagName() == "markers") {
                    QDomNodeList prodchilds = m.childNodes();
                    int maxchild = prodchilds.count();
                    for (int k = 0; k < maxchild; ++k) {
                        QDomElement mark = prodchilds.at(0).toElement();
                        mark.setAttribute("id", prod.attribute("id"));
                        markers.insertAfter(mark, QDomNode());
                    }
                    prod.removeChild(m);
                } else if (prod.attribute("type").toInt() == Text) {
                    // convert title clip
                    if (m.toElement().tagName() == "textclip") {
                        QDomDocument tdoc;
                        QDomElement titleclip = m.toElement();
                        QDomElement title = tdoc.createElement("kdenlivetitle");
                        tdoc.appendChild(title);
                        QDomNodeList objects = titleclip.childNodes();
                        int maxchild = objects.count();
                        for (int k = 0; k < maxchild; ++k) {
                            QString objectxml;
                            QDomElement ob = objects.at(k).toElement();
                            if (ob.attribute("type") == "3") {
                                // text object - all of this goes into "xmldata"...
                                QDomElement item = tdoc.createElement("item");
                                item.setAttribute("z-index", ob.attribute("z"));
                                item.setAttribute("type", "QGraphicsTextItem");
                                QDomElement position = tdoc.createElement("position");
                                position.setAttribute("x", ob.attribute("x"));
                                position.setAttribute("y", ob.attribute("y"));
                                QDomElement content = tdoc.createElement("content");
                                content.setAttribute("font", ob.attribute("font_family"));
                                content.setAttribute("font-size", ob.attribute("font_size"));
                                content.setAttribute("font-bold", ob.attribute("bold"));
                                content.setAttribute("font-italic", ob.attribute("italic"));
                                content.setAttribute("font-underline", ob.attribute("underline"));
                                QString col = ob.attribute("color");
                                QColor c(col);
                                content.setAttribute("font-color", colorToString(c));
                                // todo: These fields are missing from the newly generated xmldata:
                                // transform, startviewport, endviewport, background

                                QDomText conttxt = tdoc.createTextNode(ob.attribute("text"));
                                content.appendChild(conttxt);
                                item.appendChild(position);
                                item.appendChild(content);
                                title.appendChild(item);
                            } else if (ob.attribute("type") == "5") {
                                // rectangle object
                                QDomElement item = tdoc.createElement("item");
                                item.setAttribute("z-index", ob.attribute("z"));
                                item.setAttribute("type", "QGraphicsRectItem");
                                QDomElement position = tdoc.createElement("position");
                                position.setAttribute("x", ob.attribute("x"));
                                position.setAttribute("y", ob.attribute("y"));
                                QDomElement content = tdoc.createElement("content");
                                QString col = ob.attribute("color");
                                QColor c(col);
                                content.setAttribute("brushcolor", colorToString(c));
                                QString rect = "0,0,";
                                rect.append(ob.attribute("width"));
                                rect.append(",");
                                rect.append(ob.attribute("height"));
                                content.setAttribute("rect", rect);
                                item.appendChild(position);
                                item.appendChild(content);
                                title.appendChild(item);
                            }
                        }
                        prod.setAttribute("xmldata", tdoc.toString());
                        // mbd todo: This clearly does not work, as every title gets the same name - trying to leave it empty
                        // QStringList titleInfo = TitleWidget::getFreeTitleInfo(projectFolder());
                        // prod.setAttribute("titlename", titleInfo.at(0));
                        // prod.setAttribute("resource", titleInfo.at(1));
                        ////qDebug()<<"TITLE DATA:\n"<<tdoc.toString();
                        prod.removeChild(m);
                    } // End conversion of title clips.

                } else if (m.isText()) {
                    QString comment = m.nodeValue();
                    if (!comment.isEmpty()) {
                        prod.setAttribute("description", comment);
                    }
                    prod.removeChild(m);
                }
            }
            int duration = prod.attribute("duration").toInt();
            if (duration > 0) prod.setAttribute("out", QString::number(duration));
            // The clip goes back in, but text clips should not go back in, at least not modified
            westley.insertBefore(prod, QDomNode());
        }

        QDomNode westley0 = m_doc.elementsByTagName("westley").at(0);
        if (!markers.firstChild().isNull()) westley0.appendChild(markers);

        /*
         * Convert as much of the kdenlivedoc as possible. Use the producer in
         * westley. First, remove the old stuff from westley, and add a new
         * empty one. Also, track the max id in order to use it for the adding
         * of groups/folders
         */
        int max_kproducer_id = 0;
        westley0.removeChild(infoXmlNode);
        QDomElement infoXml_new = m_doc.createElement("kdenlivedoc");
        infoXml_new.setAttribute("profile", profile);
        infoXml.setAttribute("position", startPos);

        // Add all the producers that has a resource in westley
        QDomElement westley_element = westley0.toElement();
        if (westley_element.isNull()) {
            qWarning() << "westley0 element in document was not a QDomElement - unable to add producers to new kdenlivedoc";
        } else {
            QDomNodeList wproducers = westley_element.elementsByTagName("producer");
            int kmax = wproducers.count();
            for (int i = 0; i < kmax; ++i) {
                QDomElement wproducer = wproducers.at(i).toElement();
                if (wproducer.isNull()) {
                    qWarning() << "Found producer in westley0, that was not a QDomElement";
                    continue;
                }
                if (wproducer.attribute("id") == "black") continue;
                // We have to do slightly different things, depending on the type
                //qDebug() << "Converting producer element with type" << wproducer.attribute("type");
                if (wproducer.attribute("type").toInt() == Text) {
                    //qDebug() << "Found TEXT element in producer" << endl;
                    QDomElement kproducer = wproducer.cloneNode(true).toElement();
                    kproducer.setTagName("kdenlive_producer");
                    infoXml_new.appendChild(kproducer);
                    /*
                     * TODO: Perhaps needs some more changes here to
                     * "frequency", aspect ratio as a float, frame_size,
                     * channels, and later, resource and title name
                     */
                } else {
                    QDomElement kproducer = m_doc.createElement("kdenlive_producer");
                    kproducer.setAttribute("id", wproducer.attribute("id"));
                    if (!wproducer.attribute("description").isEmpty())
                        kproducer.setAttribute("description", wproducer.attribute("description"));
                    kproducer.setAttribute("resource", wproducer.attribute("resource"));
                    kproducer.setAttribute("type", wproducer.attribute("type"));
                    // Testing fix for 358
                    if (!wproducer.attribute("aspect_ratio").isEmpty()) {
                        kproducer.setAttribute("aspect_ratio", wproducer.attribute("aspect_ratio"));
                    }
                    if (!wproducer.attribute("source_fps").isEmpty()) {
                        kproducer.setAttribute("fps", wproducer.attribute("source_fps"));
                    }
                    if (!wproducer.attribute("length").isEmpty()) {
                        kproducer.setAttribute("duration", wproducer.attribute("length"));
                    }
                    infoXml_new.appendChild(kproducer);
                }
                if (wproducer.attribute("id").toInt() > max_kproducer_id) {
                    max_kproducer_id = wproducer.attribute("id").toInt();
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
        //QDomElement infoXml_old = infoXmlNode.toElement();
        if (!infoXml_old.isNull()) {
            QDomNodeList folders = infoXml_old.elementsByTagName("folder");
            int fsize = folders.size();
            int groupId = max_kproducer_id + 1; // Start at +1 of max id of the kdenlive_producers
            for (int i = 0; i < fsize; ++i) {
                QDomElement folder = folders.at(i).toElement();
                if (!folder.isNull()) {
                    QString groupName = folder.attribute("name");
                    //qDebug() << "groupName: " << groupName << " with groupId: " << groupId;
                    QDomNodeList fproducers = folder.elementsByTagName("producer");
                    int psize = fproducers.size();
                    for (int j = 0; j < psize; ++j) {
                        QDomElement fproducer = fproducers.at(j).toElement();
                        if (!fproducer.isNull()) {
                            QString id = fproducer.attribute("id");
                            // This is not very effective, but compared to loading the clips, its a breeze
                            QDomNodeList kdenlive_producers = infoXml_new.elementsByTagName("kdenlive_producer");
                            int kpsize = kdenlive_producers.size();
                            for (int k = 0; k < kpsize; ++k) {
                                QDomElement kproducer = kdenlive_producers.at(k).toElement(); // Its an element for sure
                                if (id == kproducer.attribute("id")) {
                                    // We do not check that it already is part of a folder
                                    kproducer.setAttribute("groupid", groupId);
                                    kproducer.setAttribute("groupname", groupName);
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
        QDomNodeList kproducers = m_doc.elementsByTagName("kdenlive_producer");
        QDomNodeList avfiles = infoXml_old.elementsByTagName("avfile");
        //qDebug() << "found" << avfiles.count() << "<avfile />s and" << kproducers.count() << "<kdenlive_producer />s";
        for (int i = 0; i < avfiles.count(); ++i) {
            QDomElement avfile = avfiles.at(i).toElement();
            QDomElement kproducer;
            if (avfile.isNull())
                qWarning() << "found an <avfile /> that is not a QDomElement";
            else {
                QString id = avfile.attribute("id");
                // this is horrible, must be rewritten, it's just for test
                for (int j = 0; j < kproducers.count(); ++j) {
                    ////qDebug() << "checking <kdenlive_producer /> with id" << kproducers.at(j).toElement().attribute("id");
                    if (kproducers.at(j).toElement().attribute("id") == id) {
                        kproducer = kproducers.at(j).toElement();
                        break;
                    }
                }
                if (kproducer == QDomElement())
                    qWarning() << "no match for <avfile /> with id =" << id;
                else {
                    ////qDebug() << "ready to set additional <avfile />'s attributes (id =" << id << ')';
                    kproducer.setAttribute("channels", avfile.attribute("channels"));
                    kproducer.setAttribute("duration", avfile.attribute("duration"));
                    kproducer.setAttribute("frame_size", avfile.attribute("width") + 'x' + avfile.attribute("height"));
                    kproducer.setAttribute("frequency", avfile.attribute("frequency"));
                    if (kproducer.attribute("description").isEmpty() && !avfile.attribute("description").isEmpty())
                        kproducer.setAttribute("description", avfile.attribute("description"));
                }
            }
        }
        infoXml = infoXml_new;
    }

    if (version <= 0.81) {
        // Add the tracks information
        QString tracksOrder = infoXml.attribute("tracks");
        if (tracksOrder.isEmpty()) {
            QDomNodeList tracks = m_doc.elementsByTagName("track");
            for (int i = 0; i < tracks.count(); ++i) {
                QDomElement track = tracks.at(i).toElement();
                if (track.attribute("producer") != "black_track") {
                    if (track.attribute("hide") == "video")
                        tracksOrder.append('a');
                    else
                        tracksOrder.append('v');
                }
            }
        }
        QDomElement tracksinfo = m_doc.createElement("tracksinfo");
        for (int i = 0; i < tracksOrder.size(); ++i) {
            QDomElement trackinfo = m_doc.createElement("trackinfo");
            if (tracksOrder.data()[i] == 'a') {
                trackinfo.setAttribute("type", "audio");
                trackinfo.setAttribute("blind", true);
            } else
                trackinfo.setAttribute("blind", false);
            trackinfo.setAttribute("mute", false);
            trackinfo.setAttribute("locked", false);
            tracksinfo.appendChild(trackinfo);
        }
        infoXml.appendChild(tracksinfo);
    }

    if (version <= 0.82) {
        // Convert <westley />s in <mlt />s (MLT extreme makeover)
        QDomNodeList westleyNodes = m_doc.elementsByTagName("westley");
        for (int i = 0; i < westleyNodes.count(); ++i) {
            QDomElement westley = westleyNodes.at(i).toElement();
            westley.setTagName("mlt");
        }
    }

    if (version <= 0.83) {
        // Replace point size with pixel size in text titles
        if (m_doc.toString().contains("font-size")) {
            KMessageBox::ButtonCode convert = KMessageBox::Continue;
            QDomNodeList kproducerNodes = m_doc.elementsByTagName("kdenlive_producer");
            for (int i = 0; i < kproducerNodes.count() && convert != KMessageBox::No; ++i) {
                QDomElement kproducer = kproducerNodes.at(i).toElement();
                if (kproducer.attribute("type").toInt() == Text) {
                    QDomDocument data;
                    data.setContent(kproducer.attribute("xmldata"));
                    QDomNodeList items = data.firstChild().childNodes();
                    for (int j = 0; j < items.count() && convert != KMessageBox::No; ++j) {
                        if (items.at(j).attributes().namedItem("type").nodeValue() == "QGraphicsTextItem") {
                            QDomNamedNodeMap textProperties = items.at(j).namedItem("content").attributes();
                            if (textProperties.namedItem("font-pixel-size").isNull() && !textProperties.namedItem("font-size").isNull()) {
                                // Ask the user if he wants to convert
                                if (convert != KMessageBox::Yes && convert != KMessageBox::No)
                                    convert = (KMessageBox::ButtonCode)KMessageBox::warningYesNo(QApplication::activeWindow(), i18n("Some of your text clips were saved with size in points, which means different sizes on different displays. Do you want to convert them to pixel size, making them portable? It is recommended you do this on the computer they were first created on, or you could have to adjust their size."), i18n("Update Text Clips"));
                                if (convert == KMessageBox::Yes) {
                                    QFont font;
                                    font.setPointSize(textProperties.namedItem("font-size").nodeValue().toInt());
                                    QDomElement content = items.at(j).namedItem("content").toElement();
                                    content.setAttribute("font-pixel-size", QFontInfo(font).pixelSize());
                                    content.removeAttribute("font-size");
                                    kproducer.setAttribute("xmldata", data.toString());
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
        QDomElement docProperties = infoXml.firstChildElement("documentproperties");
        if (docProperties.isNull()) {
            docProperties = m_doc.createElement("documentproperties");
            docProperties.setAttribute("zonein", infoXml.attribute("zonein"));
            docProperties.setAttribute("zoneout", infoXml.attribute("zoneout"));
            docProperties.setAttribute("zoom", infoXml.attribute("zoom"));
            docProperties.setAttribute("position", infoXml.attribute("position"));
            infoXml.appendChild(docProperties);
        }
    }

    if (version <= 0.84) {
        // update the title clips to use the new MLT kdenlivetitle producer
        QDomNodeList kproducerNodes = m_doc.elementsByTagName("kdenlive_producer");
        for (int i = 0; i < kproducerNodes.count(); ++i) {
            QDomElement kproducer = kproducerNodes.at(i).toElement();
            if (kproducer.attribute("type").toInt() == Text) {
                QString data = kproducer.attribute("xmldata");
                QString datafile = kproducer.attribute("resource");
                if (!datafile.endsWith(QLatin1String(".kdenlivetitle"))) {
                    datafile = QString();
                    kproducer.setAttribute("resource", QString());
                }
                QString id = kproducer.attribute("id");
                QDomNodeList mltproducers = m_doc.elementsByTagName("producer");
                bool foundData = false;
                bool foundResource = false;
                bool foundService = false;
                for (int j = 0; j < mltproducers.count(); ++j) {
                    QDomElement wproducer = mltproducers.at(j).toElement();
                    if (wproducer.attribute("id") == id) {
                        QDomNodeList props = wproducer.childNodes();
                        for (int k = 0; k < props.count(); ++k) {
                            if (props.at(k).toElement().attribute("name") == "xmldata") {
                                props.at(k).firstChild().setNodeValue(data);
                                foundData = true;
                            } else if (props.at(k).toElement().attribute("name") == "mlt_service") {
                                props.at(k).firstChild().setNodeValue("kdenlivetitle");
                                foundService = true;
                            } else if (props.at(k).toElement().attribute("name") == "resource") {
                                props.at(k).firstChild().setNodeValue(datafile);
                                foundResource = true;
                            }
                        }
                        if (!foundData) {
                            QDomElement e = m_doc.createElement("property");
                            e.setAttribute("name", "xmldata");
                            QDomText value = m_doc.createTextNode(data);
                            e.appendChild(value);
                            wproducer.appendChild(e);
                        }
                        if (!foundService) {
                            QDomElement e = m_doc.createElement("property");
                            e.setAttribute("name", "mlt_service");
                            QDomText value = m_doc.createTextNode("kdenlivetitle");
                            e.appendChild(value);
                            wproducer.appendChild(e);
                        }
                        if (!foundResource) {
                            QDomElement e = m_doc.createElement("property");
                            e.setAttribute("name", "resource");
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
        QDomNodeList effectNodes = m_doc.elementsByTagName("filter");
        for (int i = 0; i < effectNodes.count(); ++i) {
            QDomElement effect = effectNodes.at(i).toElement();
            if (EffectsList::property(effect, "mlt_service") == "ladspa") {
                // Needs to be converted
                QStringList info = getInfoFromEffectName(EffectsList::property(effect, "kdenlive_id"));
                if (info.isEmpty()) continue;
                // info contains the correct ladspa.id from kdenlive effect name, and a list of parameter's old and new names
                EffectsList::setProperty(effect, "kdenlive_id", info.at(0));
                EffectsList::setProperty(effect, "tag", info.at(0));
                EffectsList::setProperty(effect, "mlt_service", info.at(0));
                EffectsList::removeProperty(effect, "src");
                for (int j = 1; j < info.size(); ++j) {
                    QString value = EffectsList::property(effect, info.at(j).section('=', 0, 0));
                    if (!value.isEmpty()) {
                        // update parameter name
                        EffectsList::renameProperty(effect, info.at(j).section('=', 0, 0), info.at(j).section('=', 1, 1));
                    }
                }
            }
        }
    }

    if (version <= 0.86) {
        // Make sure we don't have avformat-novalidate producers, since it caused crashes
        QDomNodeList producers = m_doc.elementsByTagName("producer");
        int max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (EffectsList::property(prod, "mlt_service") == "avformat-novalidate")
                EffectsList::setProperty(prod, "mlt_service", "avformat");
        }

        // There was a mistake in Geometry transitions where the last keyframe was created one frame after the end of transition, so fix it and move last keyframe to real end of transition

        // Get profile info (width / height)
        int profileWidth;
        int profileHeight;
        QDomElement profile = m_doc.firstChildElement("profile");
        if (profile.isNull()) profile = infoXml.firstChildElement("profileinfo");
        if (profile.isNull()) {
            // could not find profile info, set PAL
            profileWidth = 720;
            profileHeight = 576;
        }
        else {
            profileWidth = profile.attribute("width").toInt();
            profileHeight = profile.attribute("height").toInt();
        }
        QDomNodeList transitions = m_doc.elementsByTagName("transition");
        max = transitions.count();
        for (int i = 0; i < max; ++i) {
            QDomElement trans = transitions.at(i).toElement();
            int out = trans.attribute("out").toInt() - trans.attribute("in").toInt();
            QString geom = EffectsList::property(trans, "geometry");
            Mlt::Geometry *g = new Mlt::Geometry(geom.toUtf8().data(), out, profileWidth, profileHeight);
            Mlt::GeometryItem item;
            if (g->next_key(&item, out) == 0) {
                // We have a keyframe just after last frame, try to move it to last frame
                if (item.frame() == out + 1) {
                    item.frame(out);
                    g->insert(item);
                    g->remove(out + 1);
                    EffectsList::setProperty(trans, "geometry", g->serialise());
                }
            }
            delete g;
        }
    }

    if (version <= 0.87) {
        if (!m_doc.firstChildElement("mlt").hasAttribute("LC_NUMERIC")) {
            m_doc.firstChildElement("mlt").setAttribute("LC_NUMERIC", "C");
        }
    }
    
    if (version <= 0.88) {
        // convert to new MLT-only format
        QDomNodeList producers = m_doc.elementsByTagName("producer");
        QDomDocumentFragment frag = m_doc.createDocumentFragment();
        
        // Create Bin Playlist
        QDomElement main_playlist = m_doc.createElement("playlist");
        QDomElement prop = m_doc.createElement("property");
        prop.setAttribute("name", "xml_retain");
        QDomText val = m_doc.createTextNode("1");
        prop.appendChild(val);
        main_playlist.appendChild(prop);

        // Move markers
        QDomNodeList markers = m_doc.elementsByTagName("marker");
        for (int i = 0; i < markers.count(); ++i) {
            QDomElement marker = markers.at(i).toElement();
            QDomElement prop = m_doc.createElement("property");
            prop.setAttribute("name", "kdenlive:marker." + marker.attribute("id") + ":" + marker.attribute("time"));
            QDomText val = m_doc.createTextNode(marker.attribute("type") + ":" + marker.attribute("comment"));
            prop.appendChild(val);
            main_playlist.appendChild(prop);
        }

        // Move guides
        QDomNodeList guides = m_doc.elementsByTagName("guide");
        for (int i = 0; i < guides.count(); ++i) {
            QDomElement guide = guides.at(i).toElement();
            QDomElement prop = m_doc.createElement("property");
            prop.setAttribute("name", "kdenlive:guide." + guide.attribute("time"));
            QDomText val = m_doc.createTextNode(guide.attribute("comment"));
            prop.appendChild(val);
            main_playlist.appendChild(prop);
        }
        
        // Move folders
        QDomNodeList folders = m_doc.elementsByTagName("folder");
        for (int i = 0; i < folders.count(); ++i) {
            QDomElement folder = folders.at(i).toElement();
            QDomElement prop = m_doc.createElement("property");
            prop.setAttribute("name", "kdenlive:folder.-1." + folder.attribute("id"));
            QDomText val = m_doc.createTextNode(folder.attribute("name"));
            prop.appendChild(val);
            main_playlist.appendChild(prop);
        }

        QDomNode mlt = m_doc.firstChildElement("mlt");
        main_playlist.setAttribute("id", pCore->binController()->binPlaylistId());
        mlt.toElement().setAttribute("producer", pCore->binController()->binPlaylistId());
        QStringList ids;
        QStringList slowmotionIds;
        QDomNode firstProd = m_doc.firstChildElement("producer");
        for (int i = 0; i < producers.count(); ++i) {
            QDomElement prod = producers.at(i).toElement();
            QString id = prod.attribute("id");
            if (id.startsWith("slowmotion")) {
                // No need to process slowmotion producers
                QString slowmo = id.section(":", 1, 1).section("_", 0, 0);
                if (!slowmotionIds.contains(slowmo)) {
                    slowmotionIds << slowmo;
                }
                continue;
            }
            QString prodId = id.section("_", 0, 0);
            if (ids.contains(prodId)) {
                // Already processed, continue
                continue;
            }
            if (id == prodId) {
                // This is an original producer, move it to the main playlist
                QDomElement entry = m_doc.createElement("entry");
                entry.setAttribute("in", prod.attribute("in"));
                entry.setAttribute("out", prod.attribute("out"));
                entry.setAttribute("producer", id);
                main_playlist.appendChild(entry);
                frag.appendChild(prod);
            }
            else {
                QDomElement originalProd = prod.cloneNode().toElement();
                originalProd.setAttribute("id", prodId);
                frag.appendChild(originalProd);
                QDomElement entry = m_doc.createElement("entry");
                entry.setAttribute("in", prod.attribute("in"));
                entry.setAttribute("out", prod.attribute("out"));
                entry.setAttribute("producer", prodId);
                main_playlist.appendChild(entry);
            }
            ids.append(prodId);
        }

        // Set clip folders
        QDomNodeList kdenlive_producers = m_doc.elementsByTagName("kdenlive_producer");
        for (int j = 0; j < kdenlive_producers.count(); j++) {
            QDomElement prod = kdenlive_producers.at(j).toElement();
            QString prodName = prod.attribute("name");
            QString prodHash = prod.attribute("file_hash");
            QString prodSize = prod.attribute("file_size");
            QString id = prod.attribute("id");
            QString folder = prod.attribute("groupid");
            QDomNodeList mlt_producers = frag.childNodes();
            for (int k = 0; k < mlt_producers.count(); k++) {
                QDomElement mltprod = mlt_producers.at(k).toElement();
                if (mltprod.tagName() != "producer") continue;
                if (mltprod.attribute("id") == id) {
                    if (!folder.isEmpty()) {
                        // We have found our producer, set folder info
                        QDomElement prop = m_doc.createElement("property");
                        prop.setAttribute("name", "kdenlive:folderid");
                        QDomText val = m_doc.createTextNode(folder);
                        prop.appendChild(val);
                        mltprod.appendChild(prop);
                    }
                    if (!prodName.isEmpty()) {
                        // Set clip name
                        QDomElement prop = m_doc.createElement("property");
                        prop.setAttribute("name", "kdenlive:clipname");
                        QDomText val = m_doc.createTextNode(prodName);
                        prop.appendChild(val);
                        mltprod.appendChild(prop);
                    }
                    if (!prodHash.isEmpty()) {
                        // Set clip name
                        QDomElement prop = m_doc.createElement("property");
                        prop.setAttribute("name", "kdenlive:file_hash");
                        QDomText val = m_doc.createTextNode(prodHash);
                        prop.appendChild(val);
                        mltprod.appendChild(prop);
                    }
                    if (!prodSize.isEmpty()) {
                        // Set clip name
                        QDomElement prop = m_doc.createElement("property");
                        prop.setAttribute("name", "kdenlive:file_size");
                        QDomText val = m_doc.createTextNode(prodSize);
                        prop.appendChild(val);
                        mltprod.appendChild(prop);
                    }
                    break;
                }
            }
        }

        // Make sure all slowmotion producers have a master clip
        for (int i = 0; i < slowmotionIds.count(); i++) {
            QString slo = slowmotionIds.at(i);
            if (!ids.contains(slo)) {
                // rebuild producer from Kdenlive's old xml format
                for (int j = 0; j < kdenlive_producers.count(); j++) {
                    QDomElement prod = kdenlive_producers.at(j).toElement();
                    QString id = prod.attribute("id");
                    if (id == slo) {
                        // We found the kdenlive_producer, build MLT producer
                        QDomElement original = m_doc.createElement("producer");
                        original.setAttribute("in", 0);
                        original.setAttribute("out", prod.attribute("duration").toInt() - 1);
                        original.setAttribute("id", id);
                        QDomElement prop = m_doc.createElement("property");
                        prop.setAttribute("name", "resource");
                        QDomText val = m_doc.createTextNode(prod.attribute("resource"));
                        prop.appendChild(val);
                        original.appendChild(prop);
                        QDomElement prop2 = m_doc.createElement("property");
                        prop2.setAttribute("name", "mlt_service");
                        QDomText val2 = m_doc.createTextNode("avformat");
                        prop2.appendChild(val2);
                        original.appendChild(prop2);
                        QDomElement prop3 = m_doc.createElement("property");
                        prop3.setAttribute("name", "length");
                        QDomText val3 = m_doc.createTextNode(prod.attribute("duration"));
                        prop3.appendChild(val3);
                        original.appendChild(prop3);
                        QDomElement entry = m_doc.createElement("entry");
                        entry.setAttribute("in", original.attribute("in"));
                        entry.setAttribute("out", original.attribute("out"));
                        entry.setAttribute("producer", id);
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
        QDomNodeList old_tracks = m_doc.elementsByTagName("trackinfo");
        QDomNodeList tracks = m_doc.elementsByTagName("track");
        QDomNodeList playlists = m_doc.elementsByTagName("playlist");
        for (int i = 0; i < old_tracks.count(); i++) {
            QString playlistName = tracks.at(i + 1).toElement().attribute("producer");
            // find playlist for track
            QDomElement trackPlaylist;
            for (int j = 0; j < playlists.count(); j++) {
                if (playlists.at(j).toElement().attribute("id") == playlistName) {
                    trackPlaylist = playlists.at(j).toElement();
                    break;
                }
            }
            if (!trackPlaylist.isNull()) {
                QDomElement kdenliveTrack = old_tracks.at(i).toElement();
                if (kdenliveTrack.attribute("type") == "audio") {
                    qDebug()<<"Found AUDIO TK";
                    EffectsList::setProperty(trackPlaylist, "kdenlive:audio_track", "1");
                }
                if (kdenliveTrack.attribute("locked") == "1") {
                    EffectsList::setProperty(trackPlaylist, "kdenlive:locked_track", "1");
                }
                EffectsList::setProperty(trackPlaylist, "kdenlive:track_name", kdenliveTrack.attribute("trackname"));
            }
        }
        // Find bin playlist
        playlists = m_doc.elementsByTagName("playlist");
        QDomElement playlist;
        for (int i = 0; i < playlists.count(); i++) {
            if (playlists.at(i).toElement().attribute("id") == pCore->binController()->binPlaylistId()) {
                playlist = playlists.at(i).toElement();
                break;
            }
        }
        // Migrate document notes
        QDomNodeList notesList = m_doc.elementsByTagName("documentnotes");
        if (!notesList.isEmpty()) {
            QDomElement notes_elem = notesList.at(0).toElement();
            QString notes = notes_elem.firstChild().nodeValue();
            EffectsList::setProperty(playlist, "kdenlive:documentnotes", notes);
        }
        // Migrate clip groups
        QDomNodeList groupElement = m_doc.elementsByTagName("groups");
        if (!groupElement.isEmpty()) {
            QDomElement groups = groupElement.at(0).toElement();
            QDomDocument d2;
            d2.importNode(groups, true);
            EffectsList::setProperty(playlist, "kdenlive:clipgroups", d2.toString());
        }
        // Migrate custom effects
        QDomNodeList effectsElement = m_doc.elementsByTagName("customeffects");
        if (!effectsElement.isEmpty()) {
            QDomElement effects = effectsElement.at(0).toElement();
            QDomDocument d2;
            d2.importNode(effects, true);
            EffectsList::setProperty(playlist, "kdenlive:customeffects", d2.toString());
        }
        EffectsList::setProperty(playlist, "kdenlive:docproperties.version", QString::number(currentVersion));
        if (!infoXml.isNull()) EffectsList::setProperty(playlist, "kdenlive:docproperties.projectfolder", infoXml.attribute("projectfolder"));
    }
    m_modified = true;
    return true;
}

QStringList DocumentValidator::getInfoFromEffectName(const QString &oldName)
{
    QStringList info;
    // Returns a list to convert old Kdenlive ladspa effects
    if (oldName == "pitch_shift") {
        info << "ladspa.1433";
        info << "pitch=0";
    }
    else if (oldName == "vinyl") {
        info << "ladspa.1905";
        info << "year=0";
        info << "rpm=1";
        info << "warping=2";
        info << "crackle=3";
        info << "wear=4";
    }
    else if (oldName == "room_reverb") {
        info << "ladspa.1216";
        info << "room=0";
        info << "delay=1";
        info << "damp=2";
    }
    else if (oldName == "reverb") {
        info << "ladspa.1423";
        info << "room=0";
        info << "damp=1";
    }
    else if (oldName == "rate_scale") {
        info << "ladspa.1417";
        info << "rate=0";
    }
    else if (oldName == "pitch_scale") {
        info << "ladspa.1193";
        info << "coef=0";
    }
    else if (oldName == "phaser") {
        info << "ladspa.1217";
        info << "rate=0";
        info << "depth=1";
        info << "feedback=2";
        info << "spread=3";
    }
    else if (oldName == "limiter") {
        info << "ladspa.1913";
        info << "gain=0";
        info << "limit=1";
        info << "release=2";
    }
    else if (oldName == "equalizer_15") {
        info << "ladspa.1197";
        info << "1=0";
        info << "2=1";
        info << "3=2";
        info << "4=3";
        info << "5=4";
        info << "6=5";
        info << "7=6";
        info << "8=7";
        info << "9=8";
        info << "10=9";
        info << "11=10";
        info << "12=11";
        info << "13=12";
        info << "14=13";
        info << "15=14";
    }
    else if (oldName == "equalizer") {
        info << "ladspa.1901";
        info << "logain=0";
        info << "midgain=1";
        info << "higain=2";
    }
    else if (oldName == "declipper") {
        info << "ladspa.1195";
    }
    return info;
}

QString DocumentValidator::colorToString(const QColor& c)
{
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

bool DocumentValidator::isProject() const
{
    return m_doc.documentElement().tagName() == "mlt";
    /*QDomNode infoXmlNode = m_doc.elementsByTagName("kdenlivedoc").at(0);
    return !infoXmlNode.isNull();*/
}

bool DocumentValidator::isModified() const
{
    return m_modified;
}

/*
void DocumentValidator::updateEffects()
{
    // WARNING: order by findDirs will determine which js file to use (in case multiple scripts for the same filter exist)
    QMap <QString, QUrl> paths;
    QMap <QString, QScriptProgram> scripts;
    QStringList directories = QStandardPaths::locateAll(QStandardPaths::DataLocation, "effects/update");
    foreach (const QString &directoryName, directories) {
        QDir directory(directoryName);
        QStringList fileList = directory.entryList(QStringList() << "*.js", QDir::Files);
        foreach (const QString &fileName, fileList) {
            QString identifier = fileName;
            // remove extension (".js")
            identifier.chop(3);
            paths.insert(identifier, QUrl(directoryName + fileName));
        }
    }

    QDomNodeList effects = m_doc.elementsByTagName("filter");
    int max = effects.count();
    QStringList safeEffects;
    for(int i = 0; i < max; ++i) {
        QDomElement effect = effects.at(i).toElement();
        QString effectId = EffectsList::property(effect, "kdenlive_id");
        if (safeEffects.contains(effectId)) {
            // Do not check the same effect twice if it is at the correct version
            // (assume we don't have different versions of the same effect in a project file)
            continue;
        }
        QString effectTag = EffectsList::property(effect, "tag");
        QString effectVersionStr = EffectsList::property(effect, "version");
        double effectVersion = effectVersionStr.isNull() ? -1 : effectVersionStr.toDouble();

        QDomElement effectDescr = MainWindow::customEffects.getEffectByTag(QString(), effectId);
        if (effectDescr.isNull()) {
            effectDescr = MainWindow::videoEffects.getEffectByTag(effectTag, effectId);
        }
        if (effectDescr.isNull()) {
            effectDescr = MainWindow::audioEffects.getEffectByTag(effectTag, effectId);
        }
        if (!effectDescr.isNull()) {
            double serviceVersion = -1;
            QDomElement serviceVersionElem = effectDescr.firstChildElement("version");
            if (!serviceVersionElem.isNull()) {
                serviceVersion = serviceVersionElem.text().toDouble();
            }
            if (serviceVersion != effectVersion && paths.contains(effectId)) {
                if (!scripts.contains(effectId)) {
                    QFile scriptFile(paths.value(effectId).path());
                    if (!scriptFile.open(QIODevice::ReadOnly)) {
                        continue;
                    }
                    QScriptProgram scriptProgram(scriptFile.readAll());
                    scriptFile.close();
                    scripts.insert(effectId, scriptProgram);
                }

                QScriptEngine scriptEngine;
                scriptEngine.importExtension("qt.core");
                scriptEngine.importExtension("qt.xml");
                scriptEngine.evaluate(scripts.value(effectId));
                QScriptValue updateRules = scriptEngine.globalObject().property("update");
                if (!updateRules.isValid())
                    continue;
                if (updateRules.isFunction()) {
                    QDomDocument scriptDoc;
                    scriptDoc.appendChild(scriptDoc.importNode(effect, true));

                    QString effectString = updateRules.call(QScriptValue(), QScriptValueList()  << serviceVersion << effectVersion << scriptDoc.toString()).toString();

                    if (!effectString.isEmpty() && !scriptEngine.hasUncaughtException()) {
                        scriptDoc.setContent(effectString);
                        QDomNode updatedEffect = effect.ownerDocument().importNode(scriptDoc.documentElement(), true);
                        effect.parentNode().replaceChild(updatedEffect, effect);
                        m_modified = true;
                    }
                } else {
                    m_modified = updateEffectParameters(effect.childNodes(), &updateRules, serviceVersion, effectVersion);
                }

                // set version number since MLT won't change it (only initially set it)
                QDomElement versionElem = effect.firstChildElement("version");
                if (EffectsList::property(effect, "version").isNull()) {
                    versionElem = effect.ownerDocument().createTextNode(QLocale().toString(serviceVersion)).toElement();
                    versionElem.setTagName("property");
                    versionElem.setAttribute("name", "version");
                    effect.appendChild(versionElem);
                } else {
                    EffectsList::setProperty(effect, "version", QLocale().toString(serviceVersion));
                }
            }
            else safeEffects.append(effectId);
        }
    }
}

bool DocumentValidator::updateEffectParameters(const QDomNodeList &parameters, const QScriptValue* updateRules, const double serviceVersion, const double effectVersion)
{
    bool updated = false;
    bool isDowngrade = serviceVersion < effectVersion;
    for (int i = 0; i < parameters.count(); ++i) {
        QDomElement parameter = parameters.at(i).toElement();
        QScriptValue rules = updateRules->property(parameter.attribute("name"));
        if (rules.isValid() && rules.isArray()) {
            int rulesCount = rules.property("length").toInt32();
            if (isDowngrade) {
                // start with the highest version and downgrade step by step
                for (int j = rulesCount - 1; j >= 0; --j) {
                    double version = rules.property(j).property(0).toNumber();
                    if (version <= effectVersion && version > serviceVersion) {
                        parameter.firstChild().setNodeValue(rules.property(j).property(1).call(QScriptValue(), QScriptValueList() << parameter.text() << isDowngrade).toString());
                        updated = true;
                    }
                }
            } else {
                for (int j = 0; j < rulesCount; ++j) {
                    double version = rules.property(j).property(0).toNumber();
                    if (version > effectVersion && version <= serviceVersion) {
                        parameter.firstChild().setNodeValue(rules.property(j).property(1).call(QScriptValue(), QScriptValueList() << parameter.text() << isDowngrade).toString());
                        updated = true;
                    }
                }
            }
        }
    }
    return updated;
}
*/

bool DocumentValidator::checkMovit()
{
    QString playlist = m_doc.toString();
    if (!playlist.contains("movit.")) {
        // Project does not use Movit GLSL effects, we can load it
        return true;
    }
    if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("The project file uses some GPU effects. GPU acceleration is not currently enabled.\nDo you want to convert the project to a non-GPU version ?\nThis might result in data loss.")) != KMessageBox::Yes) {
        return false;
    }
    // Try to convert Movit filters to their non GPU equivalent
    
    QStringList convertedFilters;
    QStringList discardedFilters;
    int ix = MainWindow::videoEffects.hasEffect("frei0r.colgate", "frei0r.colgate");
    bool hasWB = ix > -1;
    ix = MainWindow::videoEffects.hasEffect("frei0r.IIRblur", "frei0r.IIRblur");
    bool hasBlur = ix > -1;
    ix = MainWindow::transitions.hasTransition("frei0r.cairoblend");
    bool hasCairo = ix > -1;
    
    // Parse all effects in document
    QDomNodeList filters = m_doc.elementsByTagName("filter");
    int max = filters.count();
    QLocale locale;
    for (int i = 0; i < max; ++i) {
        QDomElement filt = filters.at(i).toElement();
        QString filterId = filt.attribute("id");
        if (!filterId.startsWith("movit.")) {
            continue;
        }
        if (filterId == "movit.white_balance" && hasWB) {
            // Convert to frei0r.colgate
            filt.setAttribute("id", "frei0r.colgate");
            EffectsList::setProperty(filt, "kdenlive_id", "frei0r.colgate");
            EffectsList::setProperty(filt, "tag", "frei0r.colgate");
            EffectsList::setProperty(filt, "mlt_service", "frei0r.colgate");
            EffectsList::renameProperty(filt, "neutral_color", "Neutral Color");
            QString value = EffectsList::property(filt, "color_temperature");
            value = factorizeGeomValue(value, 15000.0);
            EffectsList::setProperty(filt, "color_temperature", value);
            EffectsList::renameProperty(filt, "color_temperature", "Color Temperature");
            convertedFilters << filterId;
            continue;
        }
        if (filterId == "movit.blur" && hasBlur) {
            // Convert to frei0r.IIRblur
            filt.setAttribute("id", "frei0r.IIRblur");
            EffectsList::setProperty(filt, "kdenlive_id", "frei0r.IIRblur");
            EffectsList::setProperty(filt, "tag", "frei0r.IIRblur");
            EffectsList::setProperty(filt, "mlt_service", "frei0r.IIRblur");
            EffectsList::renameProperty(filt, "radius", "Amount");
            QString value = EffectsList::property(filt, "Amount");
            value = factorizeGeomValue(value, 14.0);
            EffectsList::setProperty(filt, "Amount", value);
            convertedFilters << filterId;
            continue;
        }
        if (filterId == "movit.mirror") {
            // Convert to MLT's mirror
            filt.setAttribute("id", "mirror");
            EffectsList::setProperty(filt, "kdenlive_id", "mirror");
            EffectsList::setProperty(filt, "tag", "mirror");
            EffectsList::setProperty(filt, "mlt_service", "mirror");
            EffectsList::setProperty(filt, "mirror", "flip");
            convertedFilters << filterId;
            continue;
        }
        if (filterId.startsWith("movit.")) {
            //TODO: implement conversion for more filters
            discardedFilters << filterId;
        }
    }
    
    // Parse all transitions in document
    QDomNodeList transitions = m_doc.elementsByTagName("transition");
    max = transitions.count();
    for (int i = 0; i < max; ++i) {
        QDomElement t = transitions.at(i).toElement();
        QString transId = EffectsList::property(t, "mlt_service");
        if (!transId.startsWith("movit.")) {
            continue;
        }
        if (transId == "movit.overlay" && hasCairo) {
            // Convert to frei0r.colgate
            EffectsList::setProperty(t, "mlt_service", "frei0r.cairoblend");
            convertedFilters << transId;
            continue;
        }
        if (transId.startsWith("movit.")) {
            //TODO: implement conversion for more filters
            discardedFilters << transId;
        }
    }

    convertedFilters.removeDuplicates();
    discardedFilters.removeDuplicates();
    if (discardedFilters.isEmpty()) {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following filters/transitions were converted to non GPU versions:"), convertedFilters);
    }
    else {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following filters/transitions were deleted from the project:"), discardedFilters);
    }
    m_modified = true;
    QString scene = m_doc.toString();
    scene.replace("movit.", "");
    m_doc.setContent(scene);
    return true;
}

QString DocumentValidator::factorizeGeomValue(QString value, double factor)
{
    QStringList vals = value.split(";");
    QString result;
    QLocale locale;
    for (int i = 0; i < vals.count(); i++) {
        QString s = vals.at(i);
        QString key = s.section("=", 0, 0);
        QString val = s.section("=", 1, 1);
        double v = locale.toDouble(val) / factor;
        result.append(key + "=" + locale.toString(v));
        if ( i + 1 < vals.count()) result.append(";");
    }
    return result;
}

void DocumentValidator::checkOrphanedProducers()
{
    QDomElement mlt = m_doc.firstChildElement("mlt");
    QDomElement main = mlt.firstChildElement("playlist");
    QDomNodeList bin_producers = main.childNodes();
    QStringList binProducers;
    for (int k = 0; k < bin_producers.count(); k++) {
        QDomElement mltprod = bin_producers.at(k).toElement();
        if (mltprod.tagName() != "entry") continue;
        binProducers << mltprod.attribute("producer");
    }

    QDomNodeList producers = m_doc.elementsByTagName("producer");
    int max = producers.count();
    QStringList allProducers;
    for (int i = 0; i < max; ++i) {
        QDomElement prod = producers.at(i).toElement();
        if (prod.isNull()) continue;
        allProducers << prod.attribute("id");
    }

    QDomDocumentFragment frag = m_doc.createDocumentFragment();
    QDomDocumentFragment trackProds = m_doc.createDocumentFragment();
    for (int i = 0; i < max; ++i) {
        QDomElement prod = producers.at(i).toElement();
        if (prod.isNull()) continue;
        QString id = prod.attribute("id").section("_", 0, 0);
        if (id.startsWith("slowmotion") || id == "black") continue;
        if (!binProducers.contains(id)) {
            qWarning()<<" ///////// WARNING, FOUND UNKNOWN PRODUDER: "<<id<<" ----------------";
            // This producer is unknown to Bin
            QString resource = EffectsList::property(prod, "resource");
            QString service = EffectsList::property(prod, "mlt_service");
            QString distinctiveTag("resource");
            if (service == "kdenlivetitle") {
                distinctiveTag = "xmldata";
            }
            QString orphanValue = EffectsList::property(prod, distinctiveTag);
            for (int j = 0; j< max; j++) {
                // Search for a similar producer
                QDomElement binProd = producers.at(j).toElement();
                QString binId = binProd.attribute("id").section("_", 0, 0);
                if (binId.startsWith("slowmotion") || !binProducers.contains(binId)) continue;
                QString binService = EffectsList::property(binProd, "mlt_service");
                qDebug()<<" / /LKNG FOR: "<<service<<" / "<<orphanValue;
                if (service != binService) continue;
                QString binValue = EffectsList::property(binProd, distinctiveTag);
                if (binValue == orphanValue) {
                    // Found probable source producer, replace
                    qDebug()<<" / / /FOUND SOURCE: "<< binId;
                    frag.appendChild(prod);
                    QDomNodeList entries = m_doc.elementsByTagName("entry");
                    for (int k = 0; k < entries.count(); k++) {
                        QDomElement entry = entries.at(k).toElement();
                        if (entry.attribute("producer") == id) {
                            QString entryId = binId;
                            if (service.contains("avformat") || service == "xml" || service == "consumer") {
                                // We must use track producer, find track for this entry
                                QString trackPlaylist = entry.parentNode().toElement().attribute("id");
                                entryId.append("_" + trackPlaylist);
                            }
                            if (!allProducers.contains(entryId)) {
                                // The track producer does not exist, create a clone for it
                                QDomElement cloned = binProd.cloneNode(true).toElement();
                                cloned.setAttribute("id", entryId);
                                trackProds.appendChild(cloned);
                                allProducers << entryId;
                            }
                            entry.setAttribute("producer", entryId);
                            m_modified = true;
                        }
                    }
                    continue;
                }
            }
        }
    }
    if (!trackProds.isNull()) {
        QDomNode firstProd = m_doc.firstChildElement("producer");
        mlt.insertBefore(trackProds, firstProd);
    }
}

