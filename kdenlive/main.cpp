/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 01:46:16 GMT 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "kdenlive.h"
#include "kdenlivesplash.h"

static const char *description = I18N_NOOP("Kdenlive"
                    "\nA non-linear video editor for KDE");

static KCmdLineOptions options[] =
{
  { "+[File]", I18N_NOOP("file to open"), 0 },
  { 0, 0, 0 }
  // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{
	KAboutData aboutData(
           "kdenlive",
	         I18N_NOOP("Kdenlive"),
		       VERSION,
			     description,
			     KAboutData::License_GPL,
		       "(c) 2002-2003, Jason Wood",
			     0,
			     "http://www.uchian.pwp.blueyonder.co.uk/kdenlive.html",
			     "jasonwood@blueyonder.co.uk");

	aboutData.addAuthor(
           "Jason Wood",
			     I18N_NOOP("Author and Maintainer"),
			     "jasonwood@blueyonder.co.uk");

	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication app;

  KdenliveSplash *splash = new KdenliveSplash("kdenlive-splash.png");
  splash->show();
        
  if (app.isRestored())
    {
    RESTORE(KdenliveApp);
    }
  else 
    {
    KdenliveApp *kdenlive = new KdenliveApp();
    kdenlive->show();

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
		
		if (args->count())
		  {
      kdenlive->openDocumentFile(args->arg(0));
		  }
		else
		  {
		  kdenlive->openDocumentFile();
		  }
		args->clear();
  }

  return app.exec();
}
