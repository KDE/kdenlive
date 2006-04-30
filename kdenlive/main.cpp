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

static const char *description = I18N_NOOP("Kdenlive"
    "\n\nA non-linear video editor for KDE."
    "\n\nKdenlive is a frontend for the Mlt Framework."
    "\nYou can download the Mlt Framework on this url:"
    "\nhttp://mlt.sourceforge.net/");

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
        "http://kdenlive.sourceforge.net",
	"jasonwood@blueyonder.co.uk");

    aboutData.addAuthor("Jason Wood",
	I18N_NOOP("Kdenlive original author"),
	"jasonwood@blueyonder.co.uk",
	"http://www.uchian.pwp.blueyonder.co.uk/kdenlive.html");

    aboutData.addAuthor("Rolf Dubitzky",
	I18N_NOOP("Piave renderer author"),
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
    
    aboutData.addAuthor("Marco Gittler", I18N_NOOP("Kdenlive programmer, Trackdecorators"),
	"g.marco@freenet.de", "");
    
    aboutData.addAuthor("Jean-Baptiste Mardelle", I18N_NOOP("Kdenlive programmer, MLT porting, Effects, Transitions"), "jb@ader.ch", "");

    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineArgs::addCmdLineOptions(options);	// Add our own options.

    KApplication app;

    if (app.isRestored()) {
	RESTORE(Gui::KdenliveApp);
    } else {

	Gui::KdenliveApp * kdenlive = new Gui::KdenliveApp();
	kdenlive->show();

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
