/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "../src/lib/localeHandling.h"
#include "mlt++/Mlt.h"
#include "renderjob.h"
#include <../config-kdenlive.h>
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QTemporaryFile>
#include <QtGlobal>

int main(int argc, char **argv)
{
    // kdenlive_render needs to be a full QApplication since some MLT modules
    // like kdenlive_title require a full App to launch a QGraphicsView.
    // If you need to render on a headless server, you must start rendering using a
    // virtual server, like xvfb-run...
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("kdenlive_render");
    QCoreApplication::setApplicationVersion(KDENLIVE_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Kdenlive video renderer for MLT");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("mode", "Render mode: \"delivery\", \"preview-chunks\" or \"synchronous-delivery\".");
    parser.parse(QCoreApplication::arguments());
    QStringList args = parser.positionalArguments();
    const QString mode = args.isEmpty() ? QString() : args.first();

    if (mode == "preview-chunks") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument("preview-chunks", "Mode: Render splited in to multiple files for timeline preview.");
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
        QString playlist = args.takeFirst();
        // destination - where to save result
        QDir baseFolder(args.takeFirst());
        // chunks to render
        QStringList chunks = args.takeFirst().split(QLatin1Char(','), Qt::SkipEmptyParts);
        // chunk size in frames
        int chunkSize = args.takeFirst().toInt();
        // path to profile
        Mlt::Profile profile(args.takeFirst().toUtf8().constData());
        // rendered file extension
        QString extension = args.takeFirst();
        // avformat consumer params
        QStringList consumerParams = args.takeFirst().split(QLatin1Char(' '), Qt::SkipEmptyParts);

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
            for (const QString &param : std::as_const(consumerParams)) {
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

    if (mode.contains(QLatin1String("delivery"))) {
        parser.clearPositionalArguments();
        parser.addPositionalArgument("delivery", "Mode: Render to a final output file.");
        parser.addPositionalArgument("renderer", "Path to MLT melt renderer.");
        parser.addPositionalArgument("source", "Source file (usually MLT XML).");

        QCommandLineOption outputOption({"o", "output"},
                                        "The destination file, optional. If no set the destination will be retrieved from the \"target\" property of the "
                                        "consumer in the source file. If set it overrides the consumers \"taget\" property.",
                                        "file");
        parser.addOption(outputOption);

        QCommandLineOption pidOption("pid", "Process ID to send back progress.", "pid", QString::number(-1));
        parser.addOption(pidOption);

        QCommandLineOption subtitleOption("subtitle", "Subtitle file.", "file");
        parser.addOption(subtitleOption);

        parser.process(app);
        args = parser.positionalArguments();

        if (args.count() != 3) {
            qCritical() << "Error: wrong number of arguments specified\n";
            parser.showHelp(1);
            // the command above will quit the app with return 1;
        }

        // mode
        args.removeFirst();
        // renderer path (melt)
        QString render = args.takeFirst();
        // Source playlist path
        QString playlist = args.takeFirst();

        LocaleHandling::resetAllLocale();
        QFile f(playlist);
        QDomDocument doc;
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open file" << f.fileName() << "for reading";
            return 1;
        }
        if (!doc.setContent(&f, false)) {
            qWarning() << "Failed to parse file" << f.fileName() << "to QDomDocument";
            f.close();
            return 1;
        }
        f.close();
        QDomElement consumer = doc.documentElement().firstChildElement(QStringLiteral("consumer"));
        int in = -1;
        int out = -1;
        QString target;
        // get in and out point, we need them to calculate the progress in some cases
        if (!consumer.isNull()) {
            in = consumer.attribute(QStringLiteral("in"), QString::number(-1)).toInt();
            out = consumer.attribute(QStringLiteral("out"), QString::number(-1)).toInt();
            target = consumer.attribute(QStringLiteral("target"));
            QString output = parser.value(outputOption);
            if (!output.isEmpty()) {
                // A custom output target was set.
                // To apply it we store a copy of the source file with the modified target
                // in a temporary file and use this file instead of the original source file.
                consumer.setAttribute(QStringLiteral("target"), output);
                QTemporaryFile tmp(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
                tmp.setAutoRemove(false);
                if (tmp.open()) {
                    tmp.close();
                    QFile file(tmp.fileName());
                    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        qDebug() << "Failed to set custom output destination, falling back to target set in source file: " << target;
                    } else {
                        playlist = tmp.fileName();
                        target = output;
                        QTextStream outStream(&file);
                        outStream << doc.toString();
                    }
                    file.close();
                } else {
                    qDebug() << "Failed to set custom output destination, falling back to target set in source file: " << target;
                }
                tmp.close();
            }
        }
        int pid = parser.value(pidOption).toInt();
        QString subtitleFile = parser.value(subtitleOption);

        // Synchronous delivery
        if (mode == QLatin1String("synchronous-delivery")) {
            QProcess renderProcess;
            QProcessEnvironment env = renderProcess.processEnvironment();
            env.insert("MLT_NO_VDPAU", "1");
            env.insert("DISPLAY", "1");
            renderProcess.setProcessEnvironment(env);
            QStringList args = {"-progress2", playlist};
            renderProcess.execute(render, args);
            return 0;
        }

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
