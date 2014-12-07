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

#include <config-kdenlive.h>
#include "mainwindow.h"


#include <KAboutData>
#include <QDebug>

#include <QUrl> //new
#include <QApplication>
#include <klocalizedstring.h>
#include <KDBusService>
#include <QCommandLineParser>
#include <QCommandLineOption>


int main(int argc, char *argv[])
{
    KAboutData aboutData(QByteArray("kdenlive"), 
                         i18n("Kdenlive"), KDENLIVE_VERSION,
                         i18n("An open source video editor."),
                         KAboutLicense::GPL,
                         i18n("Copyright © 2007–2014 Kdenlive authors"),
                         i18n("Please report bugs to http://kdenlive.org/mantis"),
                         "http://kdenlive.org",
                         "http://bugs.kdenlive.org");
    aboutData.addAuthor(i18n("Jean-Baptiste Mardelle"), i18n("MLT and KDE SC 4 porting, main developer and maintainer"), "jb@kdenlive.org");
    aboutData.addAuthor(i18n("Vincent Pinon"), i18n("Interim maintainer, bugs fixing, minor functions, profiles updates, etc."), "vpinon@april.org");
    aboutData.addAuthor(i18n("Laurent Montel"), i18n("Bugs fixing, clean up code, optimization etc."), "montel@kde.org");
    aboutData.addAuthor(i18n("Marco Gittler"), i18n("MLT transitions and effects, timeline, audio thumbs"), "g.marco@freenet.de");
    aboutData.addAuthor(i18n("Dan Dennedy"), i18n("Bug fixing, etc."), "dan@dennedy.org");
    aboutData.addAuthor(i18n("Simon A. Eugster"), i18n("Color scopes, bug fixing, etc."), "simon.eu@gmail.com");
    aboutData.addAuthor(i18n("Till Theato"), i18n("Bug fixing, etc."), "root@ttill.de");
    aboutData.addAuthor(i18n("Alberto Villa"), i18n("Bug fixing, logo, etc."), "avilla@FreeBSD.org");
    aboutData.addAuthor(i18n("Jean-Michel Poure"), i18n("Rendering profiles customization"), "jm@poure.com");
    aboutData.addAuthor(i18n("Ray Lehtiniemi"), i18n("Bug fixing, etc."), "rayl@mail.com");
    aboutData.addAuthor(i18n("Steve Guilford"), i18n("Bug fixing, etc."), "s.guilford@dbplugins.com");
    aboutData.addAuthor(i18n("Jason Wood"), i18n("Original KDE 3 version author (not active anymore)"), "jasonwood@blueyonder.co.uk");
    aboutData.setTranslator(i18n("NAME OF TRANSLATORS"), i18n("EMAIL OF TRANSLATORS"));

    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    QApplication app(argc, argv);
    app.setOrganizationDomain("kde.org");
    KDBusService programDBusService;
    
    //PORTING SCRIPT: adapt aboutdata variable if necessary
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    parser.addOption(QCommandLineOption(QStringList() <<  QLatin1String("mlt-path"), i18n("Set the path for MLT environment"), QLatin1String("mlt-path")));
    parser.addOption(QCommandLineOption(QStringList() <<  QLatin1String("+[file]"), i18n("Document to open")));
    parser.addOption(QCommandLineOption(QStringList() <<  QLatin1String("i"), i18n("Comma separated list of clips to add"), QLatin1String("clips")));

    MainWindow* window = 0;

    // see if we are starting with session management
    if (app.isSessionRestored()) {
        int n = 1;
        while (KMainWindow::canBeRestored(n)) {
            const QString className = KXmlGuiWindow::classNameOfToplevel(n);
            if (className == QLatin1String("MainWindow")) {
                window = new MainWindow();
                window->restore(n);
            } else {
                qWarning() << "Unknown class " << className << " in session saved data!";
            }
            ++n;
        }
    } else {
        QString clipsToLoad = parser.value("i");
        QString mltPath = parser.value("mlt-path");
        QUrl url;
        if (parser.positionalArguments().count()) {
            url = parser.positionalArguments().first();
        }
        window = new MainWindow(mltPath, url, clipsToLoad);
        window->show();

        
    }
    int result = app.exec();
    return result;
}
