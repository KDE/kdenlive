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

#include <KUrlRequester>
#include <KIntSpinBox>
#include <KDebug>
#include <KMessageBox>
#include <KApplication>

#include <QDialog>
#include <QDomDocument>
#include <QInputDialog>
#include <QProcess>

QStringList SamplePlugin::generators(const QStringList producers) const
{
    QStringList result;
    if (producers.contains("pango")) result << i18n("Countdown");
    if (producers.contains("noise")) result << i18n("Noise");
    return result;
}


KUrl SamplePlugin::generatedClip(const QString &renderer, const QString &generator, const KUrl &projectFolder, const QStringList &/*lumaNames*/, const QStringList &/*lumaFiles*/, const double fps, const int /*width*/, const int height)
{
    QString prePath;
    if (generator == i18n("Noise")) {
        prePath = projectFolder.path() + "/noise";
    } else prePath = projectFolder.path() + "/counter";
    int ct = 0;
    QString counter = QString::number(ct).rightJustified(5, '0', false);
    while (QFile::exists(prePath + counter + ".mlt")) {
        ct++;
        counter = QString::number(ct).rightJustified(5, '0', false);
    }
    QPointer<QDialog> d = new QDialog;
    Ui::CountDown_UI view;
    view.setupUi(d);
    if (generator == i18n("Noise")) {
        d->setWindowTitle(tr("Create Noise Clip"));
        view.font_label->setHidden(true);
        view.font->setHidden(true);
    } else {
        d->setWindowTitle(tr("Create Countdown Clip"));
        view.font->setValue(height);
    }

    // Set single file mode. Default seems to be File::ExistingOnly.
    view.path->setMode(KFile::File);

    QString clipFile = prePath + counter + ".mlt";
    view.path->setUrl(KUrl(clipFile));
    KUrl result;
    
    if (d->exec() == QDialog::Accepted) {
	QProcess generatorProcess;

	// Disable VDPAU so that rendering will work even if there is a Kdenlive instance using VDPAU
#if QT_VERSION >= 0x040600
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert("MLT_NO_VDPAU", "1");
	generatorProcess.setProcessEnvironment(env);
#else
	QStringList env = QProcess::systemEnvironment();
	env << "MLT_NO_VDPAU=1";
	generatorProcess.setEnvironment(env);
#endif
	QStringList args;
	if (generator == i18n("Noise")) {
	    args << "noise:" << "in=0" << QString("out=" + QString::number((int) fps * view.duration->value()));
	}
	else {
	    // Countdown producer
	    for (int i = 0; i < view.duration->value(); i++) {
                // Create the producers
                args << "pango:" << "in=0" << QString("out=" + QString::number((int) fps * view.duration->value()));
		args << QString("text=" + QString::number(view.duration->value() - i));
		args << QString("font=" + QString::number(view.font->value()) + "px");
	    }
	}
	
	args << "-consumer"<<QString("xml:%1").arg(view.path->url().path());
	generatorProcess.start(renderer, args);
        if (generatorProcess.waitForFinished()) {
	    if (generatorProcess.exitStatus() == QProcess::CrashExit) {
                kDebug() << "/// Generator failed: ";
		QString error = generatorProcess.readAllStandardError();
		KMessageBox::sorry(kapp->activeWindow(), i18n("Failed to generate clip:\n%1", error, i18n("Generator Failed")));
	    }
	    else {
		result = view.path->url();
	    }
        } else {
	    kDebug() << "/// Generator failed: ";
	    QString error = generatorProcess.readAllStandardError();
	    KMessageBox::sorry(kapp->activeWindow(), i18n("Failed to generate clip:\n%1", error, i18n("Generator Failed")));
        }
    }
    delete d;
    return result;
}

Q_EXPORT_PLUGIN2(kdenlive_sampleplugin, SamplePlugin)
