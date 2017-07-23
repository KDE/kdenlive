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

#include "kdenlive_debug.h"
#include <KMessageBox>
#include <klocalizedstring.h>

#include <QFile>
#include <QColor>
#include <QString>
#include <QDir>

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
    QDomElement mlt = m_doc.firstChildElement(QStringLiteral("mlt"));
    // At least the root element must be there
    if (mlt.isNull()) {
        return false;
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

    // Previous MLT / Kdenlive versions used C locale by default
    QLocale documentLocale = QLocale::c();

    if (mlt.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
        // Check document numeric separator (added in Kdenlive 16.12.1
        QDomElement main_playlist = mlt.firstChildElement(QStringLiteral("playlist"));
        QDomNodeList props = main_playlist.elementsByTagName(QStringLiteral("property"));
        QChar numericalSeparator;
        for (int i = 0; i < props.count(); ++i) {
            QDomNode n = props.at(i);
            if (n.toElement().attribute(QStringLiteral("name")) == QLatin1String("kdenlive:docproperties.decimalPoint")) {
                QString sep = n.firstChild().nodeValue();
                if (!sep.isEmpty()) {
                    numericalSeparator = sep.at(0);
                }
                break;
            }
        }
        bool error = false;
        if (!numericalSeparator.isNull() && numericalSeparator != QLocale().decimalPoint()) {
            qCDebug(KDENLIVE_LOG)<<" * ** LOCALE CHANGE REQUIRED: "<<numericalSeparator <<"!="<< QLocale().decimalPoint()<<" / "<<QLocale::system().decimalPoint();
            // Change locale to match document
            QString requestedLocale = mlt.attribute(QStringLiteral("LC_NUMERIC"));
            documentLocale = QLocale(requestedLocale);
#ifdef Q_OS_WIN
            // Most locales don't work on windows, so use C whenever possible
            if (numericalSeparator == QLatin1Char('.')) {
#else
            if (numericalSeparator != documentLocale.decimalPoint() && numericalSeparator == QLatin1Char('.')) {
#endif
                requestedLocale = QStringLiteral("C");
                documentLocale = QLocale::c();
            }
#ifdef Q_OS_MAC
            setlocale(LC_NUMERIC_MASK, requestedLocale.toUtf8().constData());
#elif defined(Q_OS_WIN)
            std::locale::global(std::locale(requestedLocale.toUtf8().constData()));
#else
            setlocale(LC_NUMERIC, requestedLocale.toUtf8().constData());
#endif
            if (numericalSeparator != documentLocale.decimalPoint()) {
                // Parse installed locales to find one matching
                const QList<QLocale> list = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale().script(), QLocale::AnyCountry);
                QLocale matching;
                for (const QLocale &loc : list) {
                    if (loc.decimalPoint() == numericalSeparator) {
                        matching = loc;
                        qCDebug(KDENLIVE_LOG)<<"Warning, document locale: "<<mlt.attribute(QStringLiteral("LC_NUMERIC"))<<" is not available, using: "<<loc.name();
#ifndef Q_OS_MAC
                        setlocale(LC_NUMERIC, loc.name().toUtf8().constData());
#else
                        setlocale(LC_NUMERIC_MASK, loc.name().toUtf8().constData());
#endif
                        documentLocale = matching;
                        break;
                    }
                }
                error = numericalSeparator != documentLocale.decimalPoint();
            }
        } else if (numericalSeparator.isNull()) {
            // Change locale to match document
#ifndef Q_OS_MAC
            const QString newloc = QString::fromLatin1(setlocale(LC_NUMERIC, mlt.attribute(QStringLiteral("LC_NUMERIC")).toUtf8().constData()));
#else
            const QString newloc = setlocale(LC_NUMERIC_MASK, mlt.attribute("LC_NUMERIC").toUtf8().constData());
#endif
            documentLocale = QLocale(mlt.attribute(QStringLiteral("LC_NUMERIC")));
            error = newloc.isEmpty();
        } else {
            // Document separator matching system separator
            documentLocale = QLocale();
        }
        if (error) {
            // Requested locale not available, ask for install
            KMessageBox::sorry(QApplication::activeWindow(), i18n("The document was created in \"%1\" locale, which is not installed on your system. Please install that language pack. Until then, Kdenlive might not be able to correctly open the document.", mlt.attribute(QStringLiteral("LC_NUMERIC"))));
        }

        // Make sure Qt locale and C++ locale have the same numeric separator, might not be the case
        // With some locales since C++ and Qt use a different database for locales
        // localeconv()->decimal_point does not give reliable results on Windows
#ifndef Q_OS_WIN
        char *separator = localeconv()->decimal_point;
        if (QString::fromUtf8(separator) != QString(documentLocale.decimalPoint())) {
        KMessageBox::sorry(QApplication::activeWindow(), i18n("There is a locale conflict on your system. The document uses locale %1 which uses a \"%2\" as numeric separator (in system libraries) but Qt expects \"%3\". You might not be able to correctly open the project.", mlt.attribute(QStringLiteral("LC_NUMERIC")), documentLocale.decimalPoint(), separator));
            //qDebug()<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
            // HACK: There is a locale conflict, so set locale to at least have correct decimal point
            if (strncmp(separator, ".", 1) == 0) {
                documentLocale = QLocale::c();
            } else if (strncmp(separator, ",", 1) == 0) {
                documentLocale = QLocale(QStringLiteral("fr_FR.UTF-8"));
            }
        }
#endif
    }
    documentLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    if (documentLocale.decimalPoint() != QLocale().decimalPoint()) {
        // If loading an older MLT file without LC_NUMERIC, set locale to C which was previously the default
        if (!mlt.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
#ifndef Q_OS_MAC
            setlocale(LC_NUMERIC, "C");
#else
            setlocale(LC_NUMERIC_MASK, "C");
#endif
	}
        QLocale::setDefault(documentLocale);
        if (documentLocale.decimalPoint() != QLocale().decimalPoint()) {
            KMessageBox::sorry(QApplication::activeWindow(), i18n("There is a locale conflict. The document uses a \"%1\" as numeric separator, but your computer is configured to use \"%2\". Change your computer settings or you might not be able to correctly open the project.", documentLocale.decimalPoint(), QLocale().decimalPoint()));
        }
        // locale conversion might need to be redone
#ifndef Q_OS_MAC
        initEffects::parseEffectFiles(pCore->getMltRepository(), QString::fromLatin1(setlocale(LC_NUMERIC, nullptr)));
#else
        initEffects::parseEffectFiles(pCore->getMltRepository(), QString::fromLatin1(setlocale(LC_NUMERIC_MASK, nullptr)));
#endif
    }
    double version = -1;
    if (kdenliveDoc.isNull() || !kdenliveDoc.hasAttribute(QStringLiteral("version"))) {
        // Newer Kdenlive document version
        QDomElement main = mlt.firstChildElement(QStringLiteral("playlist"));
        version = EffectsList::property(main, QStringLiteral("kdenlive:docproperties.version")).toDouble();
    } else {
        bool ok;
        version = documentLocale.toDouble(kdenliveDoc.attribute(QStringLiteral("version")), &ok);
        if (!ok) {
            // Could not parse version number, there is probably a conflict in decimal separator
            QLocale tempLocale = QLocale(mlt.attribute(QStringLiteral("LC_NUMERIC")));
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
        return false;
    }

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
            qCDebug(KDENLIVE_LOG) << "//// WARNING, PROJECT IS CORRUPTED, MISSING TRACK";
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
    qCDebug(KDENLIVE_LOG) << "Opening a document with version " << version << " / " << currentVersion;

    // No conversion needed
    if (qAbs(version - currentVersion) < 0.001) {
        return true;
    }

    // The document is too new
    if (version > currentVersion) {
        //qCDebug(KDENLIVE_LOG) << "Unable to open document with version " << version;
        KMessageBox::sorry(QApplication::activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.\nPlease consider upgrading your Kdenlive version.", version), i18n("Unable to open project"));
        return false;
    }

    // Unsupported document versions
    if (version <= 0.7) {
        //qCDebug(KDENLIVE_LOG) << "Unable to open document with version " << version;
        KMessageBox::sorry(QApplication::activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.", version), i18n("Unable to open project"));
        return false;
    }

    // <kdenlivedoc />
    QDomNode infoXmlNode;
    QDomElement infoXml;
    QDomNodeList docs = m_doc.elementsByTagName(QStringLiteral("kdenlivedoc"));
    if (!docs.isEmpty()) {
        infoXmlNode = m_doc.elementsByTagName(QStringLiteral("kdenlivedoc")).at(0);
        infoXml = infoXmlNode.toElement();
        infoXml.setAttribute(QStringLiteral("upgraded"), QStringLiteral("1"));
    }
    m_doc.documentElement().setAttribute(QStringLiteral("upgraded"), QStringLiteral("1"));

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
                trackinfo.setAttribute(QStringLiteral("blind"), true);
            } else {
                trackinfo.setAttribute(QStringLiteral("blind"), false);
            }
            trackinfo.setAttribute(QStringLiteral("mute"), false);
            trackinfo.setAttribute(QStringLiteral("locked"), false);
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
                if (kproducer.attribute(QStringLiteral("type")).toInt() == Text) {
                    QDomDocument data;
                    data.setContent(kproducer.attribute(QStringLiteral("xmldata")));
                    QDomNodeList items = data.firstChild().childNodes();
                    for (int j = 0; j < items.count() && convert != KMessageBox::No; ++j) {
                        if (items.at(j).attributes().namedItem(QStringLiteral("type")).nodeValue() == QLatin1String("QGraphicsTextItem")) {
                            QDomNamedNodeMap textProperties = items.at(j).namedItem(QStringLiteral("content")).attributes();
                            if (textProperties.namedItem(QStringLiteral("font-pixel-size")).isNull() && !textProperties.namedItem(QStringLiteral("font-size")).isNull()) {
                                // Ask the user if he wants to convert
                                if (convert != KMessageBox::Yes && convert != KMessageBox::No) {
                                    convert = (KMessageBox::ButtonCode)KMessageBox::warningYesNo(QApplication::activeWindow(), i18n("Some of your text clips were saved with size in points, which means different sizes on different displays. Do you want to convert them to pixel size, making them portable? It is recommended you do this on the computer they were first created on, or you could have to adjust their size."), i18n("Update Text Clips"));
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
            if (kproducer.attribute(QStringLiteral("type")).toInt() == Text) {
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
            if (EffectsList::property(effect, QStringLiteral("mlt_service")) == QLatin1String("ladspa")) {
                // Needs to be converted
                QStringList info = getInfoFromEffectName(EffectsList::property(effect, QStringLiteral("kdenlive_id")));
                if (info.isEmpty()) {
                    continue;
                }
                // info contains the correct ladspa.id from kdenlive effect name, and a list of parameter's old and new names
                EffectsList::setProperty(effect, QStringLiteral("kdenlive_id"), info.at(0));
                EffectsList::setProperty(effect, QStringLiteral("tag"), info.at(0));
                EffectsList::setProperty(effect, QStringLiteral("mlt_service"), info.at(0));
                EffectsList::removeProperty(effect, QStringLiteral("src"));
                for (int j = 1; j < info.size(); ++j) {
                    QString value = EffectsList::property(effect, info.at(j).section(QLatin1Char('='), 0, 0));
                    if (!value.isEmpty()) {
                        // update parameter name
                        EffectsList::renameProperty(effect, info.at(j).section(QLatin1Char('='), 0, 0), info.at(j).section(QLatin1Char('='), 1, 1));
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
            if (EffectsList::property(prod, QStringLiteral("mlt_service")) == QLatin1String("avformat-novalidate")) {
                EffectsList::setProperty(prod, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
            }
        }

        // There was a mistake in Geometry transitions where the last keyframe was created one frame after the end of transition, so fix it and move last keyframe to real end of transition

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
            QString geom = EffectsList::property(trans, QStringLiteral("geometry"));
            Mlt::Geometry *g = new Mlt::Geometry(geom.toUtf8().data(), out, profileWidth, profileHeight);
            Mlt::GeometryItem item;
            if (g->next_key(&item, out) == 0) {
                // We have a keyframe just after last frame, try to move it to last frame
                if (item.frame() == out + 1) {
                    item.frame(out);
                    g->insert(item);
                    g->remove(out + 1);
                    EffectsList::setProperty(trans, QStringLiteral("geometry"), QString::fromLatin1(g->serialise()));
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
            property.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive:marker.") + marker.attribute(QStringLiteral("id")) + QLatin1Char(':') + marker.attribute(QStringLiteral("time")));
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
        main_playlist.setAttribute(QStringLiteral("id"), pCore->binController()->binPlaylistId());
        mlt.toElement().setAttribute(QStringLiteral("producer"), pCore->binController()->binPlaylistId());
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
                QString newId = QStringLiteral("slowmotion:") + entryId.section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0) + QLatin1Char(':') + entryId.section(QLatin1Char(':'), 2);
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
                    QString service = EffectsList::property(prod, QStringLiteral("mlt_service"));
                    int a_ix = EffectsList::property(prod, QStringLiteral("audio_index")).toInt();
                    if (service == QLatin1String("xml") || service == QLatin1String("consumer") || (service.contains(QStringLiteral("avformat")) && a_ix != -1)) {
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
                QString service = EffectsList::property(prod, QStringLiteral("mlt_service"));
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
                    EffectsList::removeProperty(originalProd, QStringLiteral("video_index"));
                } else if (id.endsWith(QLatin1String("_video"))) {
                    EffectsList::removeProperty(originalProd, QStringLiteral("audio_index"));
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
                EffectsList::setProperty(originalProd, QStringLiteral("resource"), originalProd.attribute(QStringLiteral("resource")));
                updateProducerInfo(originalProd, prod);
                originalProd.removeAttribute(QStringLiteral("proxy"));
                originalProd.removeAttribute(QStringLiteral("type"));
                originalProd.removeAttribute(QStringLiteral("file_hash"));
                originalProd.removeAttribute(QStringLiteral("file_size"));
                originalProd.removeAttribute(QStringLiteral("frame_size"));
                originalProd.removeAttribute(QStringLiteral("proxy_out"));
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
                EffectsList::setProperty(originalProd, QStringLiteral("mlt_service"), mltService);
                EffectsList::setProperty(originalProd, QStringLiteral("mlt_type"), QStringLiteral("producer"));
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
            QString slo = slowmotionIds.at(i);
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
                    EffectsList::setProperty(trackPlaylist, QStringLiteral("kdenlive:audio_track"), QStringLiteral("1"));
                }
                if (kdenliveTrack.attribute(QStringLiteral("locked")) == QLatin1String("1")) {
                    EffectsList::setProperty(trackPlaylist, QStringLiteral("kdenlive:locked_track"), QStringLiteral("1"));
                }
                EffectsList::setProperty(trackPlaylist, QStringLiteral("kdenlive:track_name"), kdenliveTrack.attribute(QStringLiteral("trackname")));
            }
        }
        // Find bin playlist
        playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
        QDomElement playlist;
        for (int i = 0; i < playlists.count(); i++) {
            if (playlists.at(i).toElement().attribute(QStringLiteral("id")) == pCore->binController()->binPlaylistId()) {
                playlist = playlists.at(i).toElement();
                break;
            }
        }
        // Migrate document notes
        QDomNodeList notesList = m_doc.elementsByTagName(QStringLiteral("documentnotes"));
        if (!notesList.isEmpty()) {
            QDomElement notes_elem = notesList.at(0).toElement();
            QString notes = notes_elem.firstChild().nodeValue();
            EffectsList::setProperty(playlist, QStringLiteral("kdenlive:documentnotes"), notes);
        }
        // Migrate clip groups
        QDomNodeList groupElement = m_doc.elementsByTagName(QStringLiteral("groups"));
        if (!groupElement.isEmpty()) {
            QDomElement groups = groupElement.at(0).toElement();
            QDomDocument d2;
            d2.importNode(groups, true);
            EffectsList::setProperty(playlist, QStringLiteral("kdenlive:clipgroups"), d2.toString());
        }
        // Migrate custom effects
        QDomNodeList effectsElement = m_doc.elementsByTagName(QStringLiteral("customeffects"));
        if (!effectsElement.isEmpty()) {
            QDomElement effects = effectsElement.at(0).toElement();
            QDomDocument d2;
            d2.importNode(effects, true);
            EffectsList::setProperty(playlist, QStringLiteral("kdenlive:customeffects"), d2.toString());
        }
        EffectsList::setProperty(playlist, QStringLiteral("kdenlive:docproperties.version"), QString::number(currentVersion));
        if (!infoXml.isNull()) {
            EffectsList::setProperty(playlist, QStringLiteral("kdenlive:docproperties.projectfolder"), infoXml.attribute(QStringLiteral("projectfolder")));
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
            QString id = EffectsList::property(trans, QStringLiteral("kdenlive_id"));
            if (id == QLatin1String("luma")) {
                EffectsList::setProperty(trans, QStringLiteral("kdenlive_id"), QStringLiteral("wipe"));
                EffectsList::setProperty(trans, QStringLiteral("mlt_service"), QStringLiteral("composite"));
                bool reverse = EffectsList::property(trans, QStringLiteral("reverse")).toInt();
                EffectsList::setProperty(trans, QStringLiteral("luma_invert"), EffectsList::property(trans, QStringLiteral("invert")));
                EffectsList::setProperty(trans, QStringLiteral("luma"), EffectsList::property(trans, QStringLiteral("resource")));
                EffectsList::removeProperty(trans, QStringLiteral("invert"));
                EffectsList::removeProperty(trans, QStringLiteral("reverse"));
                EffectsList::removeProperty(trans, QStringLiteral("resource"));
                if (reverse) {
                    EffectsList::setProperty(trans, QStringLiteral("geometry"), QStringLiteral("0%/0%:100%x100%:100;-1=0%/0%:100%x100%:0"));
                } else {
                    EffectsList::setProperty(trans, QStringLiteral("geometry"), QStringLiteral("0%/0%:100%x100%:0;-1=0%/0%:100%x100%:100"));
                }
                EffectsList::setProperty(trans, QStringLiteral("aligned"), QStringLiteral("0"));
                EffectsList::setProperty(trans, QStringLiteral("fill"), QStringLiteral("1"));
            }
        }
    }

    if (version < 0.93) {
        // convert old keyframe filters to animated
        // these filters were "animated" by adding several instance of the filter, each one having a start and end tag.
        // We convert by parsing the start and end tags vor values and adding all to the new animated parameter
        QMap<QString, QStringList> keyframeFilterToConvert;
        keyframeFilterToConvert.insert(QStringLiteral("volume"), QStringList() << QStringLiteral("gain") << QStringLiteral("end") << QStringLiteral("level"));
        keyframeFilterToConvert.insert(QStringLiteral("brightness"), QStringList() << QStringLiteral("start") << QStringLiteral("end") << QStringLiteral("level"));

        QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));
        for (int i = 0; i < entries.count(); i++) {
            QDomNode entry = entries.at(i);
            QDomNodeList effects = entry.toElement().elementsByTagName(QStringLiteral("filter"));
            QStringList parsedIds;
            for (int j = 0; j < effects.count(); j++) {
                QDomElement eff = effects.at(j).toElement();
                QString id = EffectsList::property(eff, QStringLiteral("kdenlive_id"));
                if (keyframeFilterToConvert.contains(id) && !parsedIds.contains(id)) {
                    parsedIds << id;
                    QMap<int, double> values;
                    QStringList conversionParams = keyframeFilterToConvert.value(id);
                    int offset = eff.attribute(QStringLiteral("in")).toInt();
                    int out = eff.attribute(QStringLiteral("out")).toInt();
                    convertKeyframeEffect(eff, conversionParams, values, offset);
                    EffectsList::removeProperty(eff, conversionParams.at(0));
                    EffectsList::removeProperty(eff, conversionParams.at(1));
                    for (int k = j + 1; k < effects.count(); k++) {
                        QDomElement subEffect = effects.at(k).toElement();
                        QString subId = EffectsList::property(subEffect, QStringLiteral("kdenlive_id"));
                        if (subId == id) {
                            convertKeyframeEffect(subEffect, conversionParams, values, offset);
                            out = subEffect.attribute(QStringLiteral("out")).toInt();
                            entry.removeChild(subEffect);
                            k--;
                        }
                    }
                    QStringList parsedValues;
                    QLocale locale;
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
                    EffectsList::setProperty(eff, conversionParams.at(2), parsedValues.join(QLatin1Char(';')));
                    //EffectsList::setProperty(eff, QStringLiteral("kdenlive:sync_in_out"), QStringLiteral("1"));
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
                QString service = EffectsList::property(prod, QStringLiteral("mlt_service"));
                if (service == QLatin1String("framebuffer")) {
                    // convert to new timewarp producer
                    prod.setAttribute(QStringLiteral("id"), id + QStringLiteral(":1"));
                    slowmoIds << id;
                    EffectsList::setProperty(prod, QStringLiteral("mlt_service"), QStringLiteral("timewarp"));
                    QString resource = EffectsList::property(prod, QStringLiteral("resource"));
                    EffectsList::setProperty(prod, QStringLiteral("warp_resource"), resource.section(QLatin1Char('?'), 0, 0));
                    EffectsList::setProperty(prod, QStringLiteral("warp_speed"), resource.section(QLatin1Char('?'), 1).section(QLatin1Char(':'), 0, 0));
                    EffectsList::setProperty(prod, QStringLiteral("resource"), resource.section(QLatin1Char('?'), 1) + QLatin1Char(':') + resource.section(QLatin1Char('?'), 0, 0));
                    EffectsList::setProperty(prod, QStringLiteral("audio_index"), QStringLiteral("-1"));
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
        //qCDebug(KDENLIVE_LOG)<<"------------------------\n"<<m_doc.toString();
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
                EffectsList::setProperty(prod, QStringLiteral("set.test_audio"), QStringLiteral("0"));
                break;
            }
        }
    }
    if (version < 0.96) {
        // Check image sequences with buggy begin frame number
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        int max = producers.count();
        for (int i = 0; i < max; ++i) {
            QDomElement prod = producers.at(i).toElement();
            if (prod.isNull()) continue;
            const QString service = EffectsList::property(prod, QStringLiteral("mlt_service"));
            if (service == QLatin1String("pixbuf") || service == QLatin1String("qimage")) {
                QString resource = EffectsList::property(prod, QStringLiteral("resource"));
                if (resource.contains(QStringLiteral("?begin:"))) {
                    resource.replace(QStringLiteral("?begin:"), QStringLiteral("?begin="));
                    EffectsList::setProperty(prod, QStringLiteral("resource"), resource);
                }
            }
        }
    }

    m_modified = true;
    return true;
}

void DocumentValidator::convertKeyframeEffect(const QDomElement &effect, const QStringList &params, QMap<int, double> &values, int offset)
{
    QLocale locale;
    int in = effect.attribute(QStringLiteral("in")).toInt() - offset;
    values.insert(in, locale.toDouble(EffectsList::property(effect, params.at(0))));
    QString endValue = EffectsList::property(effect, params.at(1));
    if (!endValue.isEmpty()) {
        int out = effect.attribute(QStringLiteral("out")).toInt() - offset;
        values.insert(out, locale.toDouble(endValue));
    }
}

void DocumentValidator::updateProducerInfo(const QDomElement &prod, const QDomElement &source)
{
    QString pxy = source.attribute(QStringLiteral("proxy"));
    if (pxy.length() > 1) {
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:proxy"), pxy);
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:originalurl"), source.attribute(QStringLiteral("resource")));
    }
    if (source.hasAttribute(QStringLiteral("file_hash"))) {
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:file_hash"), source.attribute(QStringLiteral("file_hash")));
    }
    if (source.hasAttribute(QStringLiteral("file_size"))) {
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:file_size"), source.attribute(QStringLiteral("file_size")));
    }
    if (source.hasAttribute(QStringLiteral("name"))) {
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:clipname"), source.attribute(QStringLiteral("name")));
    }
    if (source.hasAttribute(QStringLiteral("zone_out"))) {
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:zone_out"), source.attribute(QStringLiteral("zone_out")));
    }
    if (source.hasAttribute(QStringLiteral("zone_in"))) {
        EffectsList::setProperty(prod, QStringLiteral("kdenlive:zone_in"), source.attribute(QStringLiteral("zone_in")));
    }
    if (source.hasAttribute(QStringLiteral("cutzones"))) {
        QString zoneData = source.attribute(QStringLiteral("cutzones"));
        QStringList zoneList = zoneData.split(QLatin1Char(';'));
        int ct = 1;
        foreach (const QString &data, zoneList) {
            QString zoneName = data.section(QLatin1Char('-'), 2);
            if (zoneName.isEmpty()) {
                zoneName = i18n("Zone %1", ct++);
            }
            EffectsList::setProperty(prod, QStringLiteral("kdenlive:clipzone.") + zoneName, data.section(QLatin1Char('-'), 0, 0) + QLatin1Char(';') + data.section(QLatin1Char('-'), 1, 1));
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
    if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("The project file uses some GPU effects. GPU acceleration is not currently enabled.\nDo you want to convert the project to a non-GPU version ?\nThis might result in data loss.")) != KMessageBox::Yes) {
        return false;
    }
    // Try to convert Movit filters to their non GPU equivalent
    QStringList convertedFilters;
    QStringList discardedFilters;
    int ix = MainWindow::videoEffects.hasEffect(QStringLiteral("frei0r.colgate"), QStringLiteral("frei0r.colgate"));
    bool hasWB = ix > -1;
    ix = MainWindow::videoEffects.hasEffect(QStringLiteral("frei0r.IIRblur"), QStringLiteral("frei0r.IIRblur"));
    bool hasBlur = ix > -1;
    QString compositeTrans;
    if (MainWindow::transitions.hasTransition(QStringLiteral("qtblend"))) {
        compositeTrans = QStringLiteral("qtblend");
    } else if (MainWindow::transitions.hasTransition(QStringLiteral("frei0r.cairoblend"))) {
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
            EffectsList::setProperty(filt, QStringLiteral("kdenlive_id"), QStringLiteral("frei0r.colgate"));
            EffectsList::setProperty(filt, QStringLiteral("tag"), QStringLiteral("frei0r.colgate"));
            EffectsList::setProperty(filt, QStringLiteral("mlt_service"), QStringLiteral("frei0r.colgate"));
            EffectsList::renameProperty(filt, QStringLiteral("neutral_color"), QStringLiteral("Neutral Color"));
            QString value = EffectsList::property(filt, QStringLiteral("color_temperature"));
            value = factorizeGeomValue(value, 15000.0);
            EffectsList::setProperty(filt, QStringLiteral("color_temperature"), value);
            EffectsList::renameProperty(filt, QStringLiteral("color_temperature"), QStringLiteral("Color Temperature"));
            convertedFilters << filterId;
            continue;
        }
        if (filterId == QLatin1String("movit.blur") && hasBlur) {
            // Convert to frei0r.IIRblur
            filt.setAttribute(QStringLiteral("id"), QStringLiteral("frei0r.IIRblur"));
            EffectsList::setProperty(filt, QStringLiteral("kdenlive_id"), QStringLiteral("frei0r.IIRblur"));
            EffectsList::setProperty(filt, QStringLiteral("tag"), QStringLiteral("frei0r.IIRblur"));
            EffectsList::setProperty(filt, QStringLiteral("mlt_service"), QStringLiteral("frei0r.IIRblur"));
            EffectsList::renameProperty(filt, QStringLiteral("radius"), QStringLiteral("Amount"));
            QString value = EffectsList::property(filt, QStringLiteral("Amount"));
            value = factorizeGeomValue(value, 14.0);
            EffectsList::setProperty(filt, QStringLiteral("Amount"), value);
            convertedFilters << filterId;
            continue;
        }
        if (filterId == QLatin1String("movit.mirror")) {
            // Convert to MLT's mirror
            filt.setAttribute(QStringLiteral("id"), QStringLiteral("mirror"));
            EffectsList::setProperty(filt, QStringLiteral("kdenlive_id"), QStringLiteral("mirror"));
            EffectsList::setProperty(filt, QStringLiteral("tag"), QStringLiteral("mirror"));
            EffectsList::setProperty(filt, QStringLiteral("mlt_service"), QStringLiteral("mirror"));
            EffectsList::setProperty(filt, QStringLiteral("mirror"), QStringLiteral("flip"));
            convertedFilters << filterId;
            continue;
        }
        if (filterId.startsWith(QLatin1String("movit."))) {
            //TODO: implement conversion for more filters
            discardedFilters << filterId;
        }
    }

    // Parse all transitions in document
    QDomNodeList transitions = m_doc.elementsByTagName(QStringLiteral("transition"));
    max = transitions.count();
    for (int i = 0; i < max; ++i) {
        QDomElement t = transitions.at(i).toElement();
        QString transId = EffectsList::property(t, QStringLiteral("mlt_service"));
        if (!transId.startsWith(QLatin1String("movit."))) {
            continue;
        }
        if (transId == QLatin1String("movit.overlay") && !compositeTrans.isEmpty()) {
            // Convert to frei0r.cairoblend
            EffectsList::setProperty(t, QStringLiteral("mlt_service"), compositeTrans);
            convertedFilters << transId;
            continue;
        }
        if (transId.startsWith(QLatin1String("movit."))) {
            //TODO: implement conversion for more filters
            discardedFilters << transId;
        }
    }

    convertedFilters.removeDuplicates();
    discardedFilters.removeDuplicates();
    if (discardedFilters.isEmpty()) {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following filters/transitions were converted to non GPU versions:"), convertedFilters);
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
    QStringList vals = value.split(QLatin1Char(';'));
    QString result;
    QLocale locale;
    for (int i = 0; i < vals.count(); i++) {
        QString s = vals.at(i);
        QString key = s.section(QLatin1Char('='), 0, 0);
        QString val = s.section(QLatin1Char('='), 1, 1);
        double v = locale.toDouble(val) / factor;
        result.append(key + QLatin1Char('=') + locale.toString(v));
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
        QDomElement prod = producers.at(i).toElement();
        if (prod.isNull()) {
            continue;
        }
        allProducers << prod.attribute(QStringLiteral("id"));
    }

    QDomDocumentFragment frag = m_doc.createDocumentFragment();
    QDomDocumentFragment trackProds = m_doc.createDocumentFragment();
    for (int i = 0; i < max; ++i) {
        QDomElement prod = producers.at(i).toElement();
        if (prod.isNull()) {
            continue;
        }
        QString id = prod.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
        if (id.startsWith(QLatin1String("slowmotion")) || id == QLatin1String("black")) {
            continue;
        }
        if (!binProducers.contains(id)) {
            QString binId = EffectsList::property(prod, QStringLiteral("kdenlive:binid"));
            if (!binId.isEmpty() && binProducers.contains(binId)) {
                continue;
            }
            qCWarning(KDENLIVE_LOG) << " ///////// WARNING, FOUND UNKNOWN PRODUDER: " << id << " ----------------";
            // This producer is unknown to Bin
            QString service = EffectsList::property(prod, QStringLiteral("mlt_service"));
            QString distinctiveTag(QStringLiteral("resource"));
            if (service == QLatin1String("kdenlivetitle")) {
                distinctiveTag = QStringLiteral("xmldata");
            }
            QString orphanValue = EffectsList::property(prod, distinctiveTag);
            for (int j = 0; j < max; j++) {
                // Search for a similar producer
                QDomElement binProd = producers.at(j).toElement();
                binId = binProd.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
                if (service != QLatin1String("timewarp") && (binId.startsWith(QLatin1String("slowmotion")) || !binProducers.contains(binId))) {
                    continue;
                }
                QString binService = EffectsList::property(binProd, QStringLiteral("mlt_service"));
                qCDebug(KDENLIVE_LOG) << " / /LKNG FOR: " << service << " / " << orphanValue << ", checking: " << binProd.attribute(QStringLiteral("id"));
                if (service != binService) {
                    continue;
                }
                QString binValue = EffectsList::property(binProd, distinctiveTag);
                if (binValue == orphanValue) {
                    // Found probable source producer, replace
                    frag.appendChild(prod);
                    i--;
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
    QString data = EffectsList::property(producer, QStringLiteral("xmldata"));
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
        EffectsList::setProperty(producer, QStringLiteral("xmldata"), doc.toString());
    }
}

