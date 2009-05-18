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


#include "documentconvert.h"
#include "definitions.h"

#include <KDebug>
#include <KMessageBox>
#include <KApplication>
#include <KLocale>

#include <QColor>

DocumentConvert::DocumentConvert(QDomDocument doc):
        m_doc(doc),
        m_modified(false)
{}


bool DocumentConvert::doConvert(double version, const double current_version)
{
    kDebug() << "Opening a document with version " << version;

    if (version == current_version) return true;

    if (version > current_version) {
        kDebug() << "Unable to open document with version " << version;
        KMessageBox::sorry(kapp->activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.\nPlease consider upgrading you Kdenlive version.", version), i18n("Unable to open project"));
        return false;
    }

    // Opening a old Kdenlive document
    if (version == 0.5 || version == 0.7) {
        KMessageBox::sorry(kapp->activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.", version), i18n("Unable to open project"));
        kDebug() << "Unable to open document with version " << version;
        // TODO: convert 0.7 (0.5?) files to the new document format.
        return false;
    }

    m_modified = true;

    if (version == 0.82) {
        convertToMelt();
        return true;
    }

    if (version == 0.81) {
        // Add correct tracks info
        QDomNode kdenlivedoc = m_doc.elementsByTagName("kdenlivedoc").at(0);
        QDomElement infoXml = kdenlivedoc.toElement();
        infoXml.setAttribute("version", current_version);
        QString currentTrackOrder = infoXml.attribute("tracks");
        QDomElement tracksinfo = m_doc.createElement("tracksinfo");
        for (int i = 0; i < currentTrackOrder.size(); i++) {
            QDomElement trackinfo = m_doc.createElement("trackinfo");
            if (currentTrackOrder.data()[i] == 'a') {
                trackinfo.setAttribute("type", "audio");
                trackinfo.setAttribute("blind", true);
            } else trackinfo.setAttribute("blind", false);
            trackinfo.setAttribute("mute", false);
            trackinfo.setAttribute("locked", false);
            tracksinfo.appendChild(trackinfo);
        }
        infoXml.appendChild(tracksinfo);
        convertToMelt();
        return true;
    }

    if (version == 0.8) {
        // Add the tracks information
        QDomNodeList tracks = m_doc.elementsByTagName("track");
        int max = tracks.count();

        QDomNode kdenlivedoc = m_doc.elementsByTagName("kdenlivedoc").at(0);
        QDomElement infoXml = kdenlivedoc.toElement();
        infoXml.setAttribute("version", current_version);
        QDomElement tracksinfo = m_doc.createElement("tracksinfo");

        for (int i = 0; i < max; i++) {
            QDomElement trackinfo = m_doc.createElement("trackinfo");
            QDomElement t = tracks.at(i).toElement();
            if (t.attribute("hide") == "video") {
                trackinfo.setAttribute("type", "audio");
                trackinfo.setAttribute("blind", true);
            } else trackinfo.setAttribute("blind", false);
            trackinfo.setAttribute("mute", false);
            trackinfo.setAttribute("locked", false);
            if (t.attribute("producer") != "black_track") tracksinfo.appendChild(trackinfo);
        }
        infoXml.appendChild(tracksinfo);
        convertToMelt();
        return true;
    }

    QDomNode westley = m_doc.elementsByTagName("westley").at(1);
    QDomNode tractor = m_doc.elementsByTagName("tractor").at(0);
    QDomNode kdenlivedoc = m_doc.elementsByTagName("kdenlivedoc").at(0);
    QDomElement kdenlivedoc_old = kdenlivedoc.cloneNode(true).toElement(); // Needed for folders
    QDomElement infoXml = kdenlivedoc.toElement();
    infoXml.setAttribute("version", current_version);
    QDomNode multitrack = m_doc.elementsByTagName("multitrack").at(0);
    QDomNodeList playlists = m_doc.elementsByTagName("playlist");

    QDomNode props = m_doc.elementsByTagName("properties").at(0).toElement();
    QString profile = props.toElement().attribute("videoprofile");
    int startPos = props.toElement().attribute("timeline_position").toInt();
    infoXml.setAttribute("position", startPos);
    if (profile == "dv_wide") profile = "dv_pal_wide";

    // move playlists outside of tractor and add the tracks instead
    int max = playlists.count();
    if (westley.isNull())
        westley = m_doc.elementsByTagName("westley").at(1);
    if (westley.isNull()) {
        westley = m_doc.createElement("mlt");
        m_doc.documentElement().appendChild(westley);
    }
    if (tractor.isNull()) {
        kDebug() << "// NO MLT PLAYLIST, building empty one";
        QDomElement blank_tractor = m_doc.createElement("tractor");
        westley.appendChild(blank_tractor);
        QDomElement blank_playlist = m_doc.createElement("playlist");
        blank_playlist.setAttribute("id", "black_track");
        westley.insertBefore(blank_playlist, QDomNode());
        QDomElement blank_track = m_doc.createElement("track");
        blank_track.setAttribute("producer", "black_track");
        blank_tractor.appendChild(blank_track);

        QDomNodeList kdenlivetracks = m_doc.elementsByTagName("kdenlivetrack");
        for (int i = 0; i < kdenlivetracks.count(); i++) {
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
    } else for (int i = 0; i < max; i++) {
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
                            kDebug() << "playlist_id: " << playlist_id << " producer:" << track_elem.attribute("producer");
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
                kWarning() << "tractor was not a QDomElement";
                tractor.insertAfter(track, QDomNode());
            }
#endif
        }
    tractor.removeChild(multitrack);

    // audio track mixing transitions should not be added to track view, so add required attribute
    QDomNodeList transitions = m_doc.elementsByTagName("transition");
    max = transitions.count();
    for (int i = 0; i < max; i++) {
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
            for (int j = 0; j < attrs.count(); j++) {
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
    for (int i = 0; i < max; i++) {
        tractor.insertAfter(transitions.at(0), QDomNode());
    }

    // Fix filters format
    QDomNodeList entries = m_doc.elementsByTagName("entry");
    max = entries.count();
    for (int i = 0; i < max; i++) {
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
                for (int j = 0; j < attrs.count(); j++) {
                    QDomAttr a = attrs.item(j).toAttr();
                    if (!a.isNull()) {
                        kDebug() << " FILTER; adding :" << a.name() << ":" << a.value();
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
        for (int i = 0; i < max; i++) {
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
            for (int j = 0; j < attrs.count(); j++) {
                QDomAttr a = attrs.item(j).toAttr();
                if (!a.isNull()) {
                    kDebug() << " FILTER; adding :" << a.name() << ":" << a.value();
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
    for (int i = 0; i < max; i++) {
        QDomElement prod = producers.at(i).toElement();
        if (prod.attribute("mlt_service") == "framebuffer") {
            QString slowmotionprod = prod.attribute("resource");
            slowmotionprod.replace(':', '?');
            kDebug() << "// FOUND WRONG SLOWMO, new: " << slowmotionprod;
            prod.setAttribute("resource", slowmotionprod);
        }
    }
    // move producers to correct place, markers to a global list, fix clip descriptions
    QDomElement markers = m_doc.createElement("markers");
    // This will get the xml producers:
    producers = m_doc.elementsByTagName("producer");
    max = producers.count();
    for (int i = 0; i < max; i++) {
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
                for (int k = 0; k < maxchild; k++) {
                    QDomElement mark = prodchilds.at(0).toElement();
                    mark.setAttribute("id", prod.attribute("id"));
                    markers.insertAfter(mark, QDomNode());
                }
                prod.removeChild(m);
            } else if (prod.attribute("type").toInt() == TEXT) {
                // convert title clip
                if (m.toElement().tagName() == "textclip") {
                    QDomDocument tdoc;
                    QDomElement titleclip = m.toElement();
                    QDomElement title = tdoc.createElement("kdenlivetitle");
                    tdoc.appendChild(title);
                    QDomNodeList objects = titleclip.childNodes();
                    int maxchild = objects.count();
                    for (int k = 0; k < maxchild; k++) {
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
                    //kDebug()<<"TITLE DATA:\n"<<tdoc.toString();
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


    // Convert as much of the kdenlivedoc as possible. Use the producer in westley
    // First, remove the old stuff from westley, and add a new empty one
    // Also, track the max id in order to use it for the adding of groups/folders
    int max_kproducer_id = 0;
    westley0.removeChild(kdenlivedoc);
    QDomElement kdenlivedoc_new = m_doc.createElement("kdenlivedoc");
    kdenlivedoc_new.setAttribute("profile", profile);

    // Add tracks info
    QDomNodeList tracks = m_doc.elementsByTagName("track");
    max = tracks.count();
    QDomElement tracksinfo = m_doc.createElement("tracksinfo");
    for (int i = 0; i < max; i++) {
        QDomElement trackinfo = m_doc.createElement("trackinfo");
        QDomElement t = tracks.at(i).toElement();
        if (t.attribute("hide") == "video") {
            trackinfo.setAttribute("type", "audio");
            trackinfo.setAttribute("blind", true);
        } else trackinfo.setAttribute("blind", false);
        trackinfo.setAttribute("mute", false);
        trackinfo.setAttribute("locked", false);
        if (t.attribute("producer") != "black_track") tracksinfo.appendChild(trackinfo);
    }
    kdenlivedoc_new.appendChild(tracksinfo);

    // Add all the producers that has a resource in westley
    QDomElement westley_element = westley0.toElement();
    if (westley_element.isNull()) {
        kWarning() << "westley0 element in document was not a QDomElement - unable to add producers to new kdenlivedoc";
    } else {
        QDomNodeList wproducers = westley_element.elementsByTagName("producer");
        int kmax = wproducers.count();
        for (int i = 0; i < kmax; i++) {
            QDomElement wproducer = wproducers.at(i).toElement();
            if (wproducer.isNull()) {
                kWarning() << "Found producer in westley0, that was not a QDomElement";
                continue;
            }
            if (wproducer.attribute("id") == "black") continue;
            // We have to do slightly different things, depending on the type
            kDebug() << "Converting producer element with type" << wproducer.attribute("type");
            if (wproducer.attribute("type").toInt() == TEXT) {
                kDebug() << "Found TEXT element in producer" << endl;
                QDomElement kproducer = wproducer.cloneNode(true).toElement();
                kproducer.setTagName("kdenlive_producer");
                kdenlivedoc_new.appendChild(kproducer);
                // TODO: Perhaps needs some more changes here to "frequency", aspect ratio as a float, frame_size, channels, and later, ressource and title name
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
                kdenlivedoc_new.appendChild(kproducer);
            }
            if (wproducer.attribute("id").toInt() > max_kproducer_id) {
                max_kproducer_id = wproducer.attribute("id").toInt();
            }
        }
    }
#define LOOKUP_FOLDER 1
#ifdef LOOKUP_FOLDER
    // Look through all the folder elements of the old doc, for each folder, for each producer,
    // get the id, look it up in the new doc, set the groupname and groupid
    // Note, this does not work at the moment - at least one folders shows up missing, and clips with no folder
    // does not show up.
    //    QDomElement kdenlivedoc_old = kdenlivedoc.toElement();
    if (!kdenlivedoc_old.isNull()) {
        QDomNodeList folders = kdenlivedoc_old.elementsByTagName("folder");
        int fsize = folders.size();
        int groupId = max_kproducer_id + 1; // Start at +1 of max id of the kdenlive_producers
        for (int i = 0; i < fsize; ++i) {
            QDomElement folder = folders.at(i).toElement();
            if (!folder.isNull()) {
                QString groupName = folder.attribute("name");
                kDebug() << "groupName: " << groupName << " with groupId: " << groupId;
                QDomNodeList fproducers = folder.elementsByTagName("producer");
                int psize = fproducers.size();
                for (int j = 0; j < psize; ++j) {
                    QDomElement fproducer = fproducers.at(j).toElement();
                    if (!fproducer.isNull()) {
                        QString id = fproducer.attribute("id");
                        // This is not very effective, but compared to loading the clips, its a breeze
                        QDomNodeList kdenlive_producers = kdenlivedoc_new.elementsByTagName("kdenlive_producer");
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
    westley0.appendChild(kdenlivedoc_new);

    QDomNodeList elements = westley.childNodes();
    max = elements.count();
    for (int i = 0; i < max; i++) {
        QDomElement prod = elements.at(0).toElement();
        westley0.insertAfter(prod, QDomNode());
    }

    westley0.removeChild(westley);

    // experimental and probably slow
    // adds <avfile /> information to <kdenlive_producer />
    QDomNodeList kproducers = m_doc.elementsByTagName("kdenlive_producer");
    QDomNodeList avfiles = kdenlivedoc_old.elementsByTagName("avfile");
    kDebug() << "found" << avfiles.count() << "<avfile />s and" << kproducers.count() << "<kdenlive_producer />s";
    for (int i = 0; i < avfiles.count(); ++i) {
        QDomElement avfile = avfiles.at(i).toElement();
        QDomElement kproducer;
        if (avfile.isNull())
            kWarning() << "found an <avfile /> that is not a QDomElement";
        else {
            QString id = avfile.attribute("id");
            // this is horrible, must be rewritten, it's just for test
            for (int j = 0; j < kproducers.count(); ++j) {
                //kDebug() << "checking <kdenlive_producer /> with id" << kproducers.at(j).toElement().attribute("id");
                if (kproducers.at(j).toElement().attribute("id") == id) {
                    kproducer = kproducers.at(j).toElement();
                    break;
                }
            }
            if (kproducer == QDomElement())
                kWarning() << "no match for <avfile /> with id =" << id;
            else {
                //kDebug() << "ready to set additional <avfile />'s attributes (id =" << id << ")";
                kproducer.setAttribute("channels", avfile.attribute("channels"));
                kproducer.setAttribute("duration", avfile.attribute("duration"));
                kproducer.setAttribute("frame_size", avfile.attribute("width") + 'x' + avfile.attribute("height"));
                kproducer.setAttribute("frequency", avfile.attribute("frequency"));
                if (kproducer.attribute("description").isEmpty() && !avfile.attribute("description").isEmpty())
                    kproducer.setAttribute("description", avfile.attribute("description"));
            }
        }
    }

    /*kDebug() << "/////////////////  CONVERTED DOC:";
    kDebug() << m_doc.toString();
    kDebug() << "/////////////////  END CONVERTED DOC:";

    QFile file("converted.kdenlive");
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << m_doc.toString().toUtf8();
        file.close();
    } else {
        kDebug() << "Unable to dump file to converted.kdenlive";
    }*/

    //kDebug() << "/////////////////  END CONVERTED DOC:";
    convertToMelt();
    return true;
}

QString DocumentConvert::colorToString(const QColor& c)
{
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

bool DocumentConvert::isModified() const
{
    return m_modified;
}

void DocumentConvert::convertToMelt()
{
    QDomNodeList list = m_doc.elementsByTagName("westley");
    int max = list.count();
    for (int i = 0; i < max; i++) {
        QDomElement elem = list.at(i).toElement();
        elem.setTagName("mlt");
    }
}

