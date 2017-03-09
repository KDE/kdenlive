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
#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QDebug>
#include "renderjob.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    QStringList preargs;
    QString locale;
    if (args.count() >= 7) {
        int pid = 0;
        int in = -1;
        int out = -1;
        // Remove program name
        args.removeFirst();

        bool erase = false;
        if (args.at(0) == QLatin1String("-erase")) {
            erase = true;
            args.removeFirst();
        }
        bool usekuiserver = false;
        if (args.at(0) == QLatin1String("-kuiserver")) {
            usekuiserver = true;
            args.removeFirst();
        }
        if (args.at(0).startsWith(QLatin1String("-pid:"))) {
            pid = args.at(0).section(QLatin1Char(':'), 1).toInt();
            args.removeFirst();
        }

        if (args.at(0).startsWith(QLatin1String("-locale:"))) {
            locale = args.at(0).section(QLatin1Char(':'), 1);
            args.removeFirst();
        }
        if (args.at(0).startsWith(QLatin1String("in="))) {
            in = args.takeFirst().section(QLatin1Char('='), -1).toInt();
        }
        if (args.at(0).startsWith(QLatin1String("out="))) {
            out = args.takeFirst().section(QLatin1Char('='), -1).toInt();
        }
        if (args.at(0).startsWith(QLatin1String("preargs="))) {
            preargs = args.takeFirst().section(QLatin1Char('='), 1).split(QLatin1Char(' '), QString::SkipEmptyParts);
        }

        QString render = args.takeFirst();
        QString profile = args.takeFirst();
        QString rendermodule = args.takeFirst();
        QString player = args.takeFirst();
        QString srcString = args.takeFirst();
        QUrl srcurl;
        if (srcString.startsWith(QLatin1String("consumer:"))) {
            srcurl = QUrl::fromEncoded(srcString.section(QLatin1Char(':'), 1).toUtf8().constData());
        } else {
            srcurl = QUrl::fromEncoded(srcString.toUtf8().constData());
        }
        QString src = srcurl.toLocalFile();
        // The QUrl path() strips the consumer: protocol, so re-add it if necessary
        if (srcString.startsWith(QStringLiteral("consumer:"))) {
            src.prepend(QLatin1String("consumer:"));
        }
        QString dest = QFileInfo(QUrl::fromEncoded(args.takeFirst().toUtf8()).toLocalFile()).absoluteFilePath();
        bool dualpass = false;
        bool doerase;
        QString vpre;

        int vprepos = args.indexOf(QRegExp(QLatin1String("vpre=.*")));
        if (vprepos >= 0) {
            vpre = args.at(vprepos);
        }
        QStringList vprelist = vpre.remove(QStringLiteral("vpre=")).split(QLatin1Char(','));
        if (!vprelist.isEmpty()) {
            args.replaceInStrings(QRegExp(QLatin1String("^vpre=.*")), QStringLiteral("vpre=%1").arg(vprelist.at(0)));
        }

        if (args.contains(QStringLiteral("pass=2"))) {
            // dual pass encoding
            dualpass = true;
            doerase = false;
            args.replace(args.indexOf(QStringLiteral("pass=2")), QStringLiteral("pass=1"));
            if (args.contains(QStringLiteral("vcodec=libx264"))) {
                args << QStringLiteral("passlogfile=%1").arg(dest + QStringLiteral(".log"));
            }
        } else {
            args.removeAll(QStringLiteral("pass=1"));
            doerase = erase;
        }

        // Decode metadata
        for (int i = 0; i < args.count(); ++i) {
            if (args.at(i).startsWith(QLatin1String("meta.attr"))) {
                QString data = args.at(i);
                args.replace(i, data.section(QLatin1Char('='), 0, 0) + QStringLiteral("=\"") + QUrl::fromPercentEncoding(data.section(QLatin1Char('='), 1).toUtf8()) + QLatin1Char('\"'));
            }
        }

        qDebug() << "//STARTING RENDERING: " << erase << ',' << usekuiserver << ',' << render << ',' << profile << ',' << rendermodule << ',' << player << ',' << src << ',' << dest << ',' << preargs << ',' << args << ',' << in << ',' << out;
        RenderJob *job = new RenderJob(doerase, usekuiserver, pid, render, profile, rendermodule, player, src, dest, preargs, args, in, out);
        if (!locale.isEmpty()) {
            job->setLocale(locale);
        }
        job->start();
        RenderJob *dualjob = nullptr;
        if (dualpass) {
            if (vprelist.size() > 1) {
                args.replaceInStrings(QRegExp(QLatin1String("^vpre=.*")), QStringLiteral("vpre=%1").arg(vprelist.at(1)));
            }
            args.replace(args.indexOf(QStringLiteral("pass=1")), QStringLiteral("pass=2"));
            dualjob = new RenderJob(erase, usekuiserver, pid, render, profile, rendermodule, player, src, dest, preargs, args, in, out);
            QObject::connect(job, &RenderJob::renderingFinished, dualjob, &RenderJob::start);
        }
        app.exec();
        delete dualjob;
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

