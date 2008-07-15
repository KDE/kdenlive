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


#include <KApplication>
#include <KAboutData>
#include <KDebug>
#include <KCmdLineArgs>
#include <KUrl> //new

#include "mainwindow.h"

int main(int argc, char *argv[]) {
    KAboutData aboutData("kdenlive", "kdenlive",
                         ki18n("Kdenlive"), "0.7",
                         ki18n("An open source video editor."),
                         KAboutData::License_GPL,
                         ki18n("Copyright (c) 2008 Development team"));
    aboutData.addAuthor(ki18n("Jean-Baptiste Mardelle"), ki18n("Mlt porting, KDE4 porting, Main developer"), "jb@kdenlive.org");
    aboutData.addAuthor(ki18n("Marco Gittler"), ki18n("MltConnection, Transition, Effect, Timeline Developer"), "g.marco@freenet.de");
    aboutData.setHomepage("http://kdenlive.org");
    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options; //new
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
        MainWindow* window = new MainWindow();
        window->show();

        KCmdLineArgs *args = KCmdLineArgs::parsedArgs(); //new
        if (args->count()) { //new
            window->openFile(args->url(0)); //new
        }

        args->clear();
    }

    return app.exec();
}
