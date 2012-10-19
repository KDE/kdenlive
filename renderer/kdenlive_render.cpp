/***************************************************************************
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

#include <stdio.h>
#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include "renderjob.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    QStringList preargs;
    QString locale;
    int pid = 0;
    int in = -1;
    int out = -1;
    if (args.count() >= 7) {
        // Remove program name
        args.removeFirst();

        bool erase = false;
        if (args.at(0) == "-erase") {
            erase = true;
            args.removeFirst();
        }
        bool usekuiserver = false;
        if (args.at(0) == "-kuiserver") {
            usekuiserver = true;
            args.removeFirst();
        }
        if (QString(args.at(0)).startsWith("-pid:")) {
            pid = QString(args.at(0)).section(':', 1).toInt();
            args.removeFirst();
        }

        if (QString(args.at(0)).startsWith("-locale:")) {
            locale = QString(args.at(0)).section(':', 1);
            args.removeFirst();
        }
        if (args.at(0).startsWith("in="))
            in = args.takeFirst().section('=', -1).toInt();
        if (args.at(0).startsWith("out="))
            out = args.takeFirst().section('=', -1).toInt();
        if (args.at(0).startsWith("preargs="))
            preargs = args.takeFirst().section('=', 1).split(' ', QString::SkipEmptyParts);

        QString render = args.takeFirst();
        QString profile = args.takeFirst();
        QString rendermodule = args.takeFirst();
        QString player = args.takeFirst();
	QByteArray srcString = args.takeFirst().toUtf8();
	QUrl srcurl = QUrl::fromEncoded(srcString);
        QString src = srcurl.path();
	// The QUrl path() strips the consumer: protocol, so re-add it if necessary
	if (srcString.startsWith("consumer:")) src.prepend("consumer:");
        QUrl desturl = QUrl::fromEncoded(args.takeFirst().toUtf8());
        QString dest = desturl.path();
        bool dualpass = false;
        bool doerase;
        QString vpre;

        int vprepos = args.indexOf(QRegExp("vpre=.*"));
        if (vprepos >= 0) {
            vpre=args.at(vprepos);
        }
        QStringList vprelist = vpre.replace("vpre=", "").split(',');
        if (vprelist.size() > 0) {
            args.replaceInStrings(QRegExp("^vpre=.*"), QString("vpre=").append(vprelist.at(0)));
        }

        if (args.contains("pass=2")) {
            // dual pass encoding
            dualpass = true;
            doerase = false;
            args.replace(args.indexOf("pass=2"), "pass=1");
            if (args.contains("vcodec=libx264")) args << QString("passlogfile=%1").arg(dest + ".log");
        } else {
            args.removeAll("pass=1");
            doerase = erase;
        }

        qDebug() << "//STARTING RENDERING: " << erase << "," << usekuiserver << "," << render << "," << profile << "," << rendermodule << "," << player << "," << src << "," << dest << "," << preargs << "," << args << "," << in << "," << out ;
        RenderJob *job = new RenderJob(doerase, usekuiserver, pid, render, profile, rendermodule, player, src, dest, preargs, args, in, out);
        if (!locale.isEmpty()) job->setLocale(locale);
        job->start();
        if (dualpass) {
            if (vprelist.size()>1)
                args.replaceInStrings(QRegExp("^vpre=.*"),QString("vpre=").append(vprelist.at(1)));
            args.replace(args.indexOf("pass=1"), "pass=2");
            RenderJob *dualjob = new RenderJob(erase, usekuiserver, pid, render, profile, rendermodule, player, src, dest, preargs, args, in, out);
            QObject::connect(job, SIGNAL(renderingFinished()), dualjob, SLOT(start()));
        }
        app.exec();
    } else {
        fprintf(stderr, "Kdenlive video renderer for MLT.\nUsage: "
                "kdenlive_render [-erase] [-kuiserver] [-locale:LOCALE] [in=pos] [out=pos] [render] [profile] [rendermodule] [player] [src] [dest] [[arg1] [arg2] ...]\n"
                "  -erase: if that parameter is present, src file will be erased at the end\n"
                "  -kuiserver: if that parameter is present, use KDE job tracker\n"
                "  -locale:LOCALE : set a locale for rendering. For example, -locale:fr_FR.UTF-8 will use a french locale (comma as numeric separator)\n"
                "  in=pos: start rendering at frame pos\n"
                "  out=pos: end rendering at frame pos\n"
                "  render: path to MLT melt renderer\n"
                "  profile: the MLT video profile\n"
                "  rendermodule: the MLT consumer used for rendering, usually it is avformat\n"
                "  player: path to video player to play when rendering is over, use '-' to disable playing\n"
                "  src: source file (usually MLT XML)\n"
                "  dest: destination file\n"
                "  args: space separated libavformat arguments\n");
    }
}

