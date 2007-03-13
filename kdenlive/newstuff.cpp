/***************************************************************************
                          newstuff.cpp  -  description
                             -------------------
    begin                :  Mar 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qdir.h>

#include <kdebug.h>
#include <kio/netaccess.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <knewstuff/entry.h>
#include <ktar.h>
#include <karchive.h>
#include <ktempdir.h>

#include "newstuff.h"


namespace Gui {


//virtual 
bool newLumaStuff::install(const QString &fileName)
{
	bool result = false;
	kdDebug()<<"//// GOT: "<<fileName<<endl;
	QPixmap pix(fileName);
	if (pix.isNull()) {
	    // maybe it is a tar.gz file, try uncompressing
	    KTar archive(fileName, "application/x-gzip");
	    archive.open(IO_ReadOnly);
	    KTempDir tmpDirectory;
	    const KArchiveDirectory *directory = archive.directory();

	    if (directory) {
		// extract data to tmp directory
	    	directory->copyTo(tmpDirectory.name());
		// pars files
 		QDir dir( tmpDirectory.name() );

		QStringList more;
    		QStringList::Iterator it;
        	more = dir.entryList( QDir::Files );
        	for ( it = more.begin() ; it != more.end() ; ++it ){
		    //kdDebug()<<"//  NEW STUFF, reading: "<< KURL(dir.path() + "/" + (*it)).fileName()<<endl;
		    QPixmap p(dir.path() + "/" + (*it));
		    if (!p.isNull()) {
	    		if (p.width() == 720) {
				if (p.height() == 576) 
					KIO::NetAccess::move(KURL(dir.path() + "/" + (*it)), KURL(locateLocal("data", "kdenlive/pgm/PAL/") + (*it)), parentWidget());
			}
			else if (p.height() == 480) KIO::NetAccess::move(KURL(dir.path() + "/" + (*it)), KURL(locateLocal("data", "kdenlive/pgm/NTSC/") + (*it)), parentWidget());
		    }
		}
		m_transDlg->refreshLumas();
		tmpDirectory.unlink();
		result = true;
	    }
	}
	else {
	    if (pix.width() == 720) {
		if (pix.height() == 576 && KIO::NetAccess::move(KURL(fileName), KURL(locateLocal("data", "kdenlive/pgm/PAL/") + m_originalName), parentWidget())) {
			m_transDlg->refreshLumas();
			result = true;
		}
		else if (pix.height() == 480 && KIO::NetAccess::move(KURL(fileName), KURL(locateLocal("data", "kdenlive/pgm/NTSC/") + m_originalName), parentWidget())) {
			m_transDlg->refreshLumas();
			result = true;
		}
	    }
	}
	KIO::NetAccess::del(KURL(fileName, 0));
	return result;
}


//virtual 
bool newLumaStuff::createUploadFile (const QString &fileName)
{
	return false;
}

QString newLumaStuff::downloadDestination( KNS::Entry *entry )
{
   m_originalName = entry->fullName() + ".pgm";
   kdDebug()<<"//*/  ORIG NAME: "<<m_originalName<<endl;
/*   if ( KStandardDirs::exists( file ) ) {
     int result = KMessageBox::questionYesNo( parentWidget(),
         i18n("The file '%1' already exists. Do you want to override it?")
         .arg( file ),
         QString::null, i18n("Overwrite") );
     if ( result == KMessageBox::No ) return QString::null;
   }*/
 
   return KGlobal::dirs()->saveLocation( "tmp" ) + KApplication::randomString( 10 );
}



} // namespace Gui


