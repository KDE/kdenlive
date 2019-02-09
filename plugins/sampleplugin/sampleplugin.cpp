/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "sampleplugin.h"
#include "ui_countdown_ui.h"

#include <KMessageBox>
#include <QDebug>

#include <QDialog>
#include <QProcess>

QStringList SamplePlugin::generators(const QStringList &producers) const
{
    QStringList result;
    if (producers.contains(QLatin1String("pango"))) {
        result << i18n("Countdown");
    }
    if (producers.contains(QLatin1String("noise"))) {
        result << i18n("Noise");
    }
    return result;
}

QUrl SamplePlugin::generatedClip(const QString &renderer, const QString &generator, const QUrl &projectFolder, const QStringList & /*lumaNames*/,
                                 const QStringList & /*lumaFiles*/, const double fps, const int /*width*/, const int height)
{
    QString prePath;
    if (generator == i18n("Noise")) {
        prePath = projectFolder.path() + QLatin1String("/noise");
    } else {
        prePath = projectFolder.path() + QLatin1String("/counter");
    }
    int ct = 0;
    QString counter = QString::number(ct).rightJustified(5, QLatin1Char('0'), false);
    while (QFile::exists(prePath + counter + QLatin1String(".mlt"))) {
        ct++;
        counter = QString::number(ct).rightJustified(5, QLatin1Char('0'), false);
    }
    QPointer<QDialog> d = new QDialog;
    Ui::CountDown_UI view;
    view.setupUi(d);
    if (generator == i18n("Noise")) {
        d->setWindowTitle(i18n("Create Noise Clip"));
        view.font_label->setHidden(true);
        view.font->setHidden(true);
    } else {
        d->setWindowTitle(i18n("Create Countdown Clip"));
        view.font->setValue(height);
    }

    // Set single file mode. Default seems to be File::ExistingOnly.
    view.path->setMode(KFile::File);

    QString clipFile = prePath + counter + QLatin1String(".mlt");
    view.path->setUrl(QUrl(clipFile));
    QUrl result;

    if (d->exec() == QDialog::Accepted) {
        QProcess generatorProcess;

        // Disable VDPAU so that rendering will work even if there is a Kdenlive instance using VDPAU
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QLatin1String("MLT_NO_VDPAU"), QLatin1String("1"));
        generatorProcess.setProcessEnvironment(env);
        QStringList args;
        if (generator == i18n("Noise")) {
            args << QLatin1String("noise:") << QLatin1String("in=0") << QLatin1String("out=") + QString::number((int)fps * view.duration->value());
        } else {
            // Countdown producer
            for (int i = 0; i < view.duration->value(); ++i) {
                // Create the producers
                args << QLatin1String("pango:") << QLatin1String("in=0") << QLatin1String("out=") + QString::number((int)fps * view.duration->value());
                args << QLatin1String("text=") + QString::number(view.duration->value() - i);
                args << QLatin1String("font=") + QString::number(view.font->value()) + QLatin1String("px");
            }
        }

        args << QLatin1String("-consumer") << QString::fromLatin1("xml:%1").arg(view.path->url().path());
        generatorProcess.start(renderer, args);
        if (generatorProcess.waitForFinished()) {
            if (generatorProcess.exitStatus() == QProcess::CrashExit) {
                // qDebug() << "/// Generator failed: ";
                QString error = QString::fromLocal8Bit(generatorProcess.readAllStandardError());
                KMessageBox::sorry(QApplication::activeWindow(), i18n("Failed to generate clip:\n%1", error), i18n("Generator Failed"));
            } else {
                result = view.path->url();
            }
        } else {
            // qDebug() << "/// Generator failed: ";
            QString error = QString::fromLocal8Bit(generatorProcess.readAllStandardError());
            KMessageBox::sorry(QApplication::activeWindow(), i18n("Failed to generate clip:\n%1", error), i18n("Generator Failed"));
        }
    }
    delete d;
    return result;
}

Q_EXPORT_PLUGIN2(kdenlive_sampleplugin, SamplePlugin)
