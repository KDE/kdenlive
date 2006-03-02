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
#include <kstandarddirs.h>

#include "kdenlive.h"
#include "kdenlivesplash.h"

static const char *description = I18N_NOOP("Kdenlive"
    "\n\nA non-linear video editor for KDE."
    "\n\nKdenlive is a frontend for Piave video renderer."
    "\nYou can download the Piave program on this url:"
    "\nhttp://modesto.sourceforge.net/piave");

static KCmdLineOptions options[] = {
    {"+[File]", I18N_NOOP("file to open"), 0},
    {0, 0, 0}
    // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{
    KAboutData aboutData("kdenlive",
	I18N_NOOP("Kdenlive"),
	VERSION,
	description,
	KAboutData::License_GPL,
	"(c) 2002-2003, Jason Wood",
	0,
	"http://www.uchian.pwp.blueyonder.co.uk/kdenlive.html",
	"jasonwood@blueyonder.co.uk");

    aboutData.addAuthor("Jason Wood",
	I18N_NOOP("Kdenlive author and Maintainer"),
	"jasonwood@blueyonder.co.uk",
	"http://www.uchian.pwp.blueyonder.co.uk/kdenlive.html");

    aboutData.addAuthor("Rolf Dubitzky",
	I18N_NOOP("Piave renderer author and maintainer"),
	"dubitzky@pktw06.phy.tu-dresden.de",
	"http://modesto.sourceforge.net/piave");

    aboutData.addAuthor("Gilles Caulier",
	I18N_NOOP
	("Piave and Kdenlive internationalization, French translations, splashscreen"),
	"caulier.gilles@free.fr", "http://caulier.gilles.free.fr");

    aboutData.addAuthor("Danny Allen",
	I18N_NOOP
	("Artist, Kdenlive logo, splashscreen and application icon"),
	"dannya40uk@yahoo.co.uk", "");
    aboutData.addAuthor("Rob Hare", I18N_NOOP("Kdenlive programmer"),
	"rob@nocturnalatl.com", "");

    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineArgs::addCmdLineOptions(options);	// Add our own options.

    KApplication app;

    if (app.isRestored()) {
	RESTORE(Gui::KdenliveApp);
    } else {

        QPixmap pixmap(locate("appdata", "graphics/kdenlive-splash.png"));

	KdenliveSplash *splash = new KdenliveSplash(pixmap);
	splash->show();

	Gui::KdenliveApp * kdenlive = new Gui::KdenliveApp();
	kdenlive->show();
        
	splash->finish(kdenlive);
	delete splash;

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->count()) {
	    kdenlive->openDocumentFile(args->arg(0));
	} else {
	    kdenlive->openDocumentFile();
	}
	args->clear();
    }

    return app.exec();
}
