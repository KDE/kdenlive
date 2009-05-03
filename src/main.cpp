/***************************************************************************
 *   Copyright (C) 2007 by Marco Gittler (g.marco@freenet.de)              *
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "kdenlive-config.h"
#include "mainwindow.h"

#include <KApplication>
#include <KAboutData>
#include <KDebug>
#include <KCmdLineArgs>
#include <KUrl> //new


#if defined(__APPLE_KDE__) || defined(__DARWIN__)
#include <SDL/SDL.h>
#endif

int main(int argc, char *argv[])
{
    KAboutData aboutData(QByteArray("kdenlive"), QByteArray("kdenlive"),
                         ki18n("Kdenlive"), VERSION,
                         ki18n("An open source video editor."),
                         KAboutData::License_GPL,
                         ki18n("Copyright (c) 2009 Development team"));
    aboutData.addAuthor(ki18n("Jean-Baptiste Mardelle"), ki18n("MLT porting, KDE 4 porting, Main developer"), "jb@kdenlive.org");
    aboutData.addAuthor(ki18n("Marco Gittler"), ki18n("MLT Connection, Transition, Effect, Timeline Developer"), "g.marco@freenet.de");
    aboutData.addAuthor(ki18n("Jean-Michel Poure"), ki18n("Rendering profiles customisation"), "jm@poure.com");
    aboutData.addAuthor(ki18n("Ray Lehtiniemi"), ki18n("Bug fixing etc."), "rayl@mail.com");
    aboutData.addAuthor(ki18n("Jason Wood"), ki18n("Original KDE 3 version author (not active anymore)"), "jasonwood@blueyonder.co.uk");
    aboutData.setHomepage("http://kdenlive.org");
    aboutData.setCustomAuthorText(ki18n("Please report bugs to http://kdenlive.org/mantis"), ki18n("Please report bugs to <a href=\"http://kdenlive.org/mantis\">http://kdenlive.org/mantis</a>"));
    aboutData.setTranslator(ki18n("NAME OF TRANSLATORS"), ki18n("EMAIL OF TRANSLATORS"));

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    options.add("mlt-path <path>", ki18n("Set the path for MLT environnement"));
    options.add("+[file]", ki18n("Document to open")); //new
    KCmdLineArgs::addCmdLineOptions(options); //new

    KApplication app;

    // see if we are starting with session management
    if (app.isSessionRestored()) {
        int n = 1;
        while (KMainWindow::canBeRestored(n)) {
            const QString className = KXmlGuiWindow::classNameOfToplevel(n);
            if (className == QLatin1String("MainWindow")) {
                MainWindow* win = new MainWindow();
                win->restore(n);
            } else {
                kWarning() << "Unknown class " << className << " in session saved data!";
            }
            ++n;
        }
    } else {
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs(); //new

        QString mltPath = args->getOption("mlt-path");
        KUrl url;
        if (args->count()) {
            url = args->url(0);
        }
        MainWindow* window = new MainWindow(mltPath, url);
        window->show();

        args->clear();
    }

    return app.exec();
}
