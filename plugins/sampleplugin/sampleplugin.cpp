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

#include <QDialog>
#include <QDomDocument>
#include <QInputDialog>

QStringList SamplePlugin::generators(const QStringList producers) const
{
    QStringList result;
    if (producers.contains("pango")) result << i18n("Countdown");
    if (producers.contains("noise")) result << i18n("Noise");
    return result;
}


KUrl SamplePlugin::generatedClip(const QString &generator, const KUrl &projectFolder, const QStringList &/*lumaNames*/, const QStringList &/*lumaFiles*/, const double fps, const int /*width*/, const int height)
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
    if (d->exec() == QDialog::Accepted) {
        QDomDocument doc;
        QDomElement mlt = doc.createElement("mlt");
        QDomElement playlist = doc.createElement("playlist");
        if (generator == i18n("Noise")) {
            QDomElement prod = doc.createElement("producer");
            prod.setAttribute("mlt_service", "noise");
            prod.setAttribute("in", "0");
            prod.setAttribute("out", QString::number((int) fps * view.duration->value()));
            playlist.appendChild(prod);
        } else {
            for (int i = 0; i < view.duration->value(); i++) {
                // Create the producers
                QDomElement prod = doc.createElement("producer");
                prod.setAttribute("mlt_service", "pango");
                prod.setAttribute("in", "0");
                prod.setAttribute("out", QString::number((int) fps));
                prod.setAttribute("text", QString::number(view.duration->value() - i));
                //FIXME: the font and pad values are approximate, the pango producer seems unable
                // to produce a predictable frame size.
                prod.setAttribute("font", QString::number(view.font->value()) + "px");
                //prod.setAttribute("pad", 50);
                playlist.appendChild(prod);
            }
        }
        mlt.appendChild(playlist);
        doc.appendChild(mlt);
        QFile file(view.path->url().path());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            kWarning() << "//////  ERROR writing to file: " << view.path->url().path();
            KMessageBox::error(0, i18n("Cannot write to file %1", view.path->url().path()));
            delete d;
            return KUrl();
        }
        QTextStream out(&file);
        out << doc.toString();
        if (file.error() != QFile::NoError) {
            KMessageBox::error(0, i18n("Cannot write to file %1", view.path->url().path()));
            file.close();
            delete d;
            return KUrl();
        }
        file.close();
       delete d;
        return view.path->url();
    }
    return KUrl();
}

Q_EXPORT_PLUGIN2(kdenlive_sampleplugin, SamplePlugin)
