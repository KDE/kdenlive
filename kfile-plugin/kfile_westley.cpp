/***************************************************************************
                          kfile_westley  -  description
                             -------------------
    begin                : Sat Aug 4 2007
    copyright            : (C) 2007 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


    #include <config.h>

    #include <qdom.h>
    #include <qfile.h>

    #include <kgenericfactory.h>

    #include "kfile_westley.h"

    typedef KGenericFactory<westleyPlugin> westleyFactory;
   

    K_EXPORT_COMPONENT_FACTORY(kfile_westley, westleyFactory( "kfile_westley" ))
   

    westleyPlugin::westleyPlugin(QObject *parent, const char *name,
                           const QStringList &args)
        : KFilePlugin(parent, name, args)
    {
        KFileMimeTypeInfo* info = addMimeTypeInfo( "application/vnd.westley.scenelist" );

        // our new group
        KFileMimeTypeInfo::GroupInfo* group = 0L;
        group = addGroupInfo(info, "westleyInfo", i18n("Westley Information"));
   

        KFileMimeTypeInfo::ItemInfo* item;
   

        // our new items in the group
        item = addItemInfo(group, "Title", i18n("Title"), QVariant::String);
        item = addItemInfo(group, "Clips", i18n("Clips"), QVariant::Int);
        item = addItemInfo(group, "Tracks", i18n("Tracks"), QVariant::Int);
        //setUnit(item, KFileMimeTypeInfo::KiloBytes);
   
        KFileMimeTypeInfo* info2 = addMimeTypeInfo( "application/vnd.kde.kdenlive" );
        group = addGroupInfo(info2, "westleyInfo", i18n("Westley Information"));
        item = addItemInfo(group, "Title", i18n("Title"), QVariant::String);
        item = addItemInfo(group, "Clips", i18n("Clips"), QVariant::Int);
        item = addItemInfo(group, "Tracks", i18n("Tracks"), QVariant::Int);

        // strings are possible, too:
        //addItemInfo(group, "Text", i18n("Document type"), QVariant::String);
    }
   

    bool westleyPlugin::readInfo( KFileMetaInfo& info, uint /*what*/)
    {
        KFileMetaInfoGroup group = appendGroup(info, "westleyInfo");

	QDomDocument doc;
	QFile file(info.path());
	doc.setContent(&file, false);
	file.close();

	QDomElement documentElement = doc.documentElement();
	if (documentElement.tagName() != "westley") {
	    kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
	    return false;
	}
	appendItem(group, "Title", documentElement.attribute("title", QString::null));
	int clipCount = 0;
	int tracksCount = 0;
	int duration = 0;

    	QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
	QDomNode n;
	if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	else n = documentElement.firstChild();
	QDomElement e;
	while (!n.isNull()) {
	    e = n.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "producer") clipCount++;
		else if (e.tagName() == "playlist") tracksCount++;
	    }
	    n = n.nextSibling();
	}
	if (!kdenlivedoc.isNull() || tracksCount == 0) {
	    // it is a kdenlive document, parse for tracks
	    kdenlivedoc = documentElement.elementsByTagName("multitrack").item(0);
	    if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
	    while (!n.isNull()) {
	    	e = n.toElement();
	    	if (!e.isNull()) {
		    if (e.tagName() == "playlist") tracksCount++;
		}
		n = n.nextSibling();
	    }
	}

	appendItem(group, "Clips", clipCount);
	appendItem(group, "Tracks", tracksCount);

	return true;

}


