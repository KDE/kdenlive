/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "../src/lib/localeHandling.h"
#include "mlt++/Mlt.h"
#include "renderjob.h"
#include <../config-kdenlive.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDomDocument>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("kdenlive_render");
    QCoreApplication::setApplicationVersion(KDENLIVE_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Kdenlive video renderer for MLT");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("mode", "Render mode. Either \"delivery\" or \"preview-chunks\".");
    parser.parse(QCoreApplication::arguments());
    QStringList args = parser.positionalArguments();
    const QString mode = args.isEmpty() ? QString() : args.first();

    if (mode == "preview-chunks") {

        parser.clearPositionalArguments();
        parser.addPositionalArgument("preview-chunks", "Render splited in to multiple files for timeline preview.");
        parser.addPositionalArgument("source", "Source file (usually MLT XML).");
        parser.addPositionalArgument("destination", "Destination directory.");
        parser.addPositionalArgument("chunks", "Chunks to render.");
        parser.addPositionalArgument("chunk_size", "Chunks to render.");
        parser.addPositionalArgument("profile_path", "Path to profile.");
        parser.addPositionalArgument("file_extension", "Rendered file extension.");
        parser.addPositionalArgument("args", "Space separated libavformat arguments.", "[arg1 arg2 ...]");

        parser.process(app);
        args = parser.positionalArguments();
        if (args.count() < 7) {
            qCritical() << "Error: not enough arguments specified\n";
            parser.showHelp(1);
            // the command above will quit the app with return 1;
        }
        // After initialising the MLT factory, set the locale back from user default to C
        // to ensure numbers are always serialised with . as decimal point.
        Mlt::Factory::init();
        LocaleHandling::resetAllLocale();

        // mode
        args.removeFirst();
        // Source playlist path
        QString playlist = args.at(0);
        args.removeFirst();
        // destination - where to save result
        QDir baseFolder(args.at(0));
        args.removeFirst();
        // chunks to render
        QStringList chunks = args.at(0).split(QLatin1Char(','), Qt::SkipEmptyParts);
        args.removeFirst();
        // chunk size in frames
        int chunkSize = args.at(0).toInt();
        args.removeFirst();
        // path to profile
        Mlt::Profile profile(args.at(0).toUtf8().constData());

        args.removeFirst();
        // rendered file extension
        QString extension = args.at(0);
        args.removeFirst();
        // avformat consumer params
        QStringList consumerParams = args.at(0).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        args.removeFirst();

        profile.set_explicit(1);
        Mlt::Producer prod(profile, nullptr, playlist.toUtf8().constData());
        if (!prod.is_valid()) {
            fprintf(stderr, "INVALID playlist: %s \n", playlist.toUtf8().constData());
            return 1;
        }
        const char *localename = prod.get_lcnumeric();
        QLocale::setDefault(QLocale(localename));

        int currentFrame = 0;
        int rangeStart = 0;
        int rangeEnd = 0;
        QString frame;
        while (!chunks.isEmpty()) {
            if (rangeEnd == 0) {
                // We are not processing a range
                frame = chunks.first();
            }
            if (rangeEnd > 0) {
                // We are processing a range
                currentFrame += chunkSize + 1;
                frame = QString::number(currentFrame);
                if (currentFrame >= rangeEnd) {
                    // End of range
                    rangeStart = 0;
                    rangeEnd = 0;
                    // Range is processed, remove from stack
                    chunks.removeFirst();
                }
            } else if (frame.contains(QLatin1Char('-'))) {
                rangeStart = frame.section(QLatin1Char('-'), 0, 0).toInt();
                rangeEnd = frame.section(QLatin1Char('-'), 1, 1).toInt();
                currentFrame = rangeStart;
                frame = QString::number(currentFrame);
            } else {
                // Frame will be processed, remove from stack
                chunks.removeFirst();
            }
            fprintf(stderr, "START:%d \n", frame.toInt());
            QString fileName = QStringLiteral("%1.%2").arg(frame, extension);
            if (baseFolder.exists(fileName)) {
                // Don't overwrite an existing file
                fprintf(stderr, "DONE:%d \n", frame.toInt());
                continue;
            }
            QScopedPointer<Mlt::Producer> playlst(prod.cut(frame.toInt(), frame.toInt() + chunkSize));
            QScopedPointer<Mlt::Consumer> cons(
                new Mlt::Consumer(profile, QString("avformat:%1").arg(baseFolder.absoluteFilePath(fileName)).toUtf8().constData()));
            for (const QString &param : qAsConst(consumerParams)) {
                if (param.contains(QLatin1Char('='))) {
                    cons->set(param.section(QLatin1Char('='), 0, 0).toUtf8().constData(), param.section(QLatin1Char('='), 1).toUtf8().constData());
                }
            }
            if (!cons->is_valid()) {
                fprintf(stderr, " = =  = INVALID CONSUMER\n\n");
                return 1;
            }
            cons->set("terminate_on_pause", 1);
            cons->connect(*playlst);
            playlst.reset();
            cons->run();
            cons->stop();
            cons->purge();
            fprintf(stderr, "DONE:%d \n", frame.toInt());
        }
        // Mlt::Factory::close();
        fprintf(stderr, "+ + + RENDERING FINISHED + + + \n");
        return 0;
    }

    if (mode == "delivery") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument("delivery", "Render to a final output file.");
        parser.addPositionalArgument("renderer", "Path to MLT melt renderer.");
        parser.addPositionalArgument("source", "Source file (usually MLT XML).");
        parser.addPositionalArgument("destination", "Destination file.");

        QCommandLineOption pidOption("pid", "Process ID to send back progress.", "pid");
        parser.addOption(pidOption);

        QCommandLineOption inOption("in", "Start rendering at frame pos", "frame");
        parser.addOption(inOption);

        QCommandLineOption outOption("out", "End rendering at frame pos.", "frame");
        parser.addOption(outOption);

        QCommandLineOption subtitleOption("subtitle", "Subtitle file.", "file");
        parser.addOption(subtitleOption);

        parser.process(app);
        args = parser.positionalArguments();

        if (args.count() < 4) {
            qCritical() << "Error: not enough arguments specified\n";
            parser.showHelp(1);
            // the command above will quit the app with return 1;
        }

        // mode
        args.removeFirst();
        // renderer path (melt)
        QString render = args.at(0);
        args.removeFirst();
        // Source playlist path
        QString playlist = args.at(0);
        args.removeFirst();
        // target - where to save result
        QString target = args.at(0);
        args.removeFirst();

        int pid = parser.value(pidOption).toInt();
        int out = parser.value(outOption).toInt();
        if (out == 0) {
            out = -1;
        }
        QString subtitleFile = parser.value(subtitleOption);

        // older MLT version, does not support embedded consumer in/out in xml, and current
        // MLT (6.16) does not pass it onto the multi / movit consumer, so read it manually and enforce
        LocaleHandling::resetAllLocale();
        QFile f(playlist);
        QDomDocument doc;
        doc.setContent(&f, false);
        f.close();
        QDomElement consumer = doc.documentElement().firstChildElement(QStringLiteral("consumer"));
        if (!consumer.isNull()) {
            if (consumer.hasAttribute(QLatin1String("s")) || consumer.hasAttribute(QLatin1String("r"))) {
                // Workaround MLT embedded consumer resize (MLT issue #453)
                playlist.prepend(QStringLiteral("xml:"));
                playlist.append(QStringLiteral("?multi=1"));
            }
        }
        int in = -1;
        auto *rJob = new RenderJob(render, playlist, target, pid, in, out, subtitleFile, &app);
        QObject::connect(rJob, &RenderJob::renderingFinished, rJob, [&]() {
            rJob->deleteLater();
            app.quit();
        });
        // app.setQuitOnLastWindowClosed(false);
        QMetaObject::invokeMethod(rJob, "start", Qt::QueuedConnection);
        return app.exec();
    }

    qCritical() << "Error: unknown mode" << mode << "\n";
    parser.showHelp(1);
    // the command above will quit the app with return 1;
}
