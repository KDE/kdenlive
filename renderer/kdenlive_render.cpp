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
#include <QtDebug>

#include "renderjob.h"

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    QStringList preargs;
    int in = -1;
    int out = -1;
    if (!args.isEmpty()) args.takeFirst();
    if (args.count() >= 4) {
        bool erase = false;
        if (args.at(0) == "-erase") {
            erase = true;
            args.takeFirst();
        }
        bool usekuiserver = false;
        if (args.at(0) == "-kuiserver") {
            usekuiserver = true;
            args.takeFirst();
        }
        if (args.at(0).startsWith("in=")) {
            in = args.at(0).section('=', -1).toInt();
            args.takeFirst();
        }
        if (args.at(0).startsWith("out=")) {
            out = args.at(0).section('=', -1).toInt();
            args.takeFirst();
        }
        if (args.at(0).startsWith("preargs=")) {
            QString a = args.at(0).section('=', 1);
            preargs = a.split(" ", QString::SkipEmptyParts);
            args.takeFirst();
        }
        QString render = args.at(0);
        args.takeFirst();
        QString profile = args.at(0);
        args.takeFirst();
        QString rendermodule = args.at(0);
        args.takeFirst();
        QString player = args.at(0);
        args.takeFirst();
        QString src = args.at(0);
        args.takeFirst();
        QString dest = args.at(0);
        args.takeFirst();
        bool dualpass = false;
        bool doerase;
        if (args.contains("pass=2")) {
            // dual pass encoding
            dualpass = true;
            doerase = false;
            args.replace(args.indexOf("pass=2"), "pass=1");
        } else doerase = erase;
        qDebug() << "//STARTING RENDERING: " << erase << "," << usekuiserver << "," << render << "," << profile << "," << rendermodule << "," << player << "," << src << "," << dest << "," << preargs << "," << args << "," << in << "," << out ;
        RenderJob *job = new RenderJob(doerase, usekuiserver, render, profile, rendermodule, player, src, dest, preargs, args, in, out);
        job->start();
        if (dualpass) {
            args.replace(args.indexOf("pass=1"), "pass=2");
            RenderJob *dualjob = new RenderJob(erase, usekuiserver, render, profile, rendermodule, player, src, dest, preargs, args, in, out);
            QObject::connect(job, SIGNAL(renderingFinished()), dualjob, SLOT(start()));
        }
        app.exec();
    } else {
        fprintf(stderr, "Kdenlive video renderer for MLT.\nUsage: "
                "kdenlive_render [-erase] [-kuiserver] [in=pos] [out=pos] [render] [profile] [rendermodule] [player] [src] [dest] [[arg1] [arg2] ...]\n"
                "  -erase: if that parameter is present, src file will be erased at the end\n"
                "  -kuiserver: if that parameter is present, use KDE job tracker\n"
                "  in=pos: start rendering at frame pos\n"
                "  out=pos: end rendering at frame pos\n"
                "  render: path to inigo render\n"
                "  profile: the MLT video profile\n"
                "  rendermodule: the MLT consumer used for rendering, usually it is avformat\n"
                "  player: path to video player to play when rendering is over, use '-' to disable playing\n"
                "  src: source file (usually westley playlist)\n"
                "  dest: destination file\n"
                "  args: space separated libavformat arguments\n");
    }
}

