/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clipcreationdialog.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"
#include "bin/bin.h"
#include "ui_colorclip_ui.h"
#include "timecodedisplay.h"
#include "doc/doccommands.h"
#include "titler/titlewidget.h"
#include "project/dialogs/slideshowclip.h"

#include <KMessageBox>
#include <KRecentDirs>
#include "klocalizedstring.h"

#include <QDir>
#include <QUndoStack>
#include <QUndoCommand>
#include <QStandardPaths>
#include <QDebug>
#include <QDialog>
#include <QPointer>
#include <QMimeDatabase>

//static
void ClipCreationDialogDialog::createColorClip(KdenliveDoc *doc, QStringList groupInfo, Bin *bin)
{
    QPointer<QDialog> dia = new QDialog(bin);
    Ui::ColorClip_UI dia_ui;
    dia_ui.setupUi(dia);
    dia->setWindowTitle(i18n("Color Clip"));
    dia_ui.clip_name->setText(i18n("Color Clip"));

    TimecodeDisplay *t = new TimecodeDisplay(doc->timecode());
    t->setValue(KdenliveSettings::color_duration());
    dia_ui.clip_durationBox->addWidget(t);
    dia_ui.clip_color->setColor(KdenliveSettings::colorclipcolor());

    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui.clip_color->color().name();
        KdenliveSettings::setColorclipcolor(color);
        color = color.replace(0, 1, "0x") + "ff";
        // Everything is ready. create clip xml
        QDomDocument xml;
        QDomElement prod = xml.createElement("producer");
        xml.appendChild(prod);
        prod.setAttribute("type", (int) Color);
        uint id = bin->getFreeClipId();
        prod.setAttribute("id", QString::number(id));
        prod.setAttribute("in", "0");
        prod.setAttribute("out", doc->getFramePos(doc->timecode().getTimecode(t->gentime())) - 1);
        QMap <QString, QString> properties;
        properties.insert("resource", color);
        properties.insert("kdenlive:clipname", dia_ui.clip_name->text());
        properties.insert("mlt_service", "color");
        if (!groupInfo.isEmpty()) {
            properties.insert("kdenlive:folderid", groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        AddClipCommand *command = new AddClipCommand(doc, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete t;
    delete dia;
}

//static
void ClipCreationDialogDialog::createSlideshowClip(KdenliveDoc *doc, QStringList groupInfo, Bin *bin)
{
    QPointer<SlideshowClip> dia = new SlideshowClip(doc->timecode(), bin);

    if (dia->exec() == QDialog::Accepted) {
        // Ready, create xml
        QDomDocument xml;
        QDomElement prod = xml.createElement("producer");
        xml.appendChild(prod);
        prod.setAttribute("in", "0");
        prod.setAttribute("out", QString::number(doc->getFramePos(dia->clipDuration()) * dia->imageCount() - 1));
        prod.setAttribute("type", (int) SlideShow);
        QMap <QString, QString> properties;
        properties.insert("kdenlive:clipname", dia->clipName());
        properties.insert("resource", dia->selectedPath());
        properties.insert("ttl", QString::number(doc->getFramePos(dia->clipDuration())));
        properties.insert("loop", QString::number(dia->loop()));
        properties.insert("crop", QString::number(dia->crop()));
        properties.insert("fade", QString::number(dia->fade()));
        properties.insert("luma_duration", dia->lumaDuration());
        properties.insert("luma_file", dia->lumaFile());
        properties.insert("softness", QString::number(dia->softness()));
        properties.insert("animation", dia->animation());
        if (!groupInfo.isEmpty()) {
            properties.insert("kdenlive:folderid", groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        uint id = bin->getFreeClipId();
        AddClipCommand *command = new AddClipCommand(doc, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete dia;
}

void ClipCreationDialogDialog::createTitleClip(KdenliveDoc *doc, QStringList groupInfo, QString templatePath, Bin *bin)
{
    // Make sure the titles folder exists
    QDir dir(doc->projectFolder().path());
    dir.mkdir("titles");
    dir.cd("titles");
    QPointer<TitleWidget> dia_ui = new TitleWidget(QUrl::fromLocalFile(templatePath), doc->timecode(), dir.path(), doc->renderer(), bin);
    if (dia_ui->exec() == QDialog::Accepted) {
        // Ready, create clip xml
        QDomDocument xml;
        QDomElement prod = xml.createElement("producer");
        xml.appendChild(prod);
        //prod.setAttribute("resource", imagePath);
        uint id = bin->getFreeClipId();
        prod.setAttribute("id", QString::number(id));

        QMap <QString, QString> properties;
        properties.insert("xmldata", dia_ui->xml().toString());
        properties.insert("kdenlive:clipname", i18n("Title clip"));
        if (!groupInfo.isEmpty()) {
            properties.insert("kdenlive:folderid", groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        prod.setAttribute("type", (int) Text);
        prod.setAttribute("transparency", "1");
        prod.setAttribute("in", "0");
        prod.setAttribute("out", dia_ui->duration() - 1);
        AddClipCommand *command = new AddClipCommand(doc, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete dia_ui;
}

void ClipCreationDialogDialog::addXmlProperties(QDomElement &producer, QMap <QString, QString> &properties)
{
    QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        QDomElement prop = producer.ownerDocument().createElement("property");
        prop.setAttribute("name", i.key());
        QDomText value = producer.ownerDocument().createTextNode(i.value());
        prop.appendChild(value);
        producer.appendChild(prop);
    }
}

void ClipCreationDialogDialog::createClipsCommand(KdenliveDoc *doc, const QList<QUrl> &urls, QStringList groupInfo, Bin *bin)
{
    QUndoCommand *addClips = new QUndoCommand();
    
    //TODO check folders 
    /*QList < QList<QUrl> > foldersList;
    QMimeDatabase db;
    foreach(const QUrl & file, list) {
        // Check there is no folder here
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.inherits("inode/directory")) {
            // user dropped a folder, import its files
            list.removeAll(file);
            QDir dir(file.path());
            QStringList result = dir.entryList(QDir::Files);
            QList <QUrl> folderFiles;
            folderFiles << file;
            foreach(const QString & path, result) {
                folderFiles.append(QUrl::fromLocalFile(dir.absoluteFilePath(path)));
            }
            if (folderFiles.count() > 1) foldersList.append(folderFiles);
        }
    }*/
    
    foreach(const QUrl &file, urls) {
        QDomDocument xml;
        QDomElement prod = xml.createElement("producer");
        xml.appendChild(prod);
        QMap <QString, QString> properties;
        properties.insert("resource", file.path());
        if (!groupInfo.isEmpty()) {
            properties.insert("kdenlive:folderid", groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        //prod.setAttribute("resource", file.path());
        uint id = bin->getFreeClipId();
        prod.setAttribute("id", QString::number(id));
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.name().startsWith(QLatin1String("image/"))) {
            prod.setAttribute("type", (int) Image);
            prod.setAttribute("in", 0);
            prod.setAttribute("out", doc->getFramePos(KdenliveSettings::image_duration()) - 1);
            if (KdenliveSettings::autoimagetransparency()) prod.setAttribute("transparency", 1);
                // Read EXIF metadata for JPEG
                if (type.inherits("image/jpeg")) {
                    //TODO KF5 how to read metadata?
                    /*
                    KFileMetaInfo metaInfo(file.path(), QString("image/jpeg"), KFileMetaInfo::TechnicalInfo);
                    const QHash<QString, KFileMetaInfoItem> metaInfoItems = metaInfo.items();
                    foreach(const KFileMetaInfoItem & metaInfoItem, metaInfoItems) {
                        QDomElement meta = xml.createElement("metaproperty");
                        meta.setAttribute("name", "meta.attr." + metaInfoItem.name().section('#', 1));
                        QDomText value = xml.createTextNode(metaInfoItem.value().toString());
                        meta.setAttribute("tool", "KDE Metadata");
                        meta.appendChild(value);
                        prod.appendChild(meta);
                    }*/
                }
        } else if (type.inherits("application/x-kdenlivetitle")) {
            // opening a title file
            QDomDocument txtdoc("titledocument");
            QFile txtfile(file.path());
            if (txtfile.open(QIODevice::ReadOnly) && txtdoc.setContent(&txtfile)) {
                txtfile.close();
                prod.setAttribute("type", (int) Text);
                // extract embeded images
                QDomNodeList items = txtdoc.elementsByTagName("content");
                for (int i = 0; i < items.count() ; ++i) {
                    QDomElement content = items.item(i).toElement();
                    if (content.hasAttribute("base64")) {
                        QString titlesFolder = doc->projectFolder().path() + QDir::separator() + "titles/";
                        QString path = TitleDocument::extractBase64Image(titlesFolder, content.attribute("base64"));
                        if (!path.isEmpty()) {
                            content.setAttribute("url", path);
                            content.removeAttribute("base64");
                        }
                    }
                }
                prod.setAttribute("transparency", 1);
                prod.setAttribute("in", 0);
                int duration = 0;
                if (txtdoc.documentElement().hasAttribute("duration")) {
                    duration = txtdoc.documentElement().attribute("duration").toInt();
                } else if (txtdoc.documentElement().hasAttribute("out")) {
                    duration = txtdoc.documentElement().attribute("out").toInt();
                }
                if (duration <= 0)
                    duration = doc->getFramePos(KdenliveSettings::title_duration()) - 1;
                prod.setAttribute("duration", duration);
                prod.setAttribute("out", duration);
                txtdoc.documentElement().setAttribute("duration", duration);
                txtdoc.documentElement().setAttribute("out", duration);
                QString titleData = txtdoc.toString();
                prod.setAttribute("xmldata", titleData);
            } else {
                txtfile.close();
            }
        }
        new AddClipCommand(doc, xml.documentElement(), QString::number(id), true, addClips);
    }
    if (addClips->childCount() > 0) {
        addClips->setText(i18np("Add clip", "Add clips", addClips->childCount()));
        doc->commandStack()->push(addClips);
    }
}

void ClipCreationDialogDialog::createClipsCommand(KdenliveDoc *doc, QStringList groupInfo, Bin *bin)
{
    QList <QUrl> list;
    QString allExtensions = "*"; //getExtensions().join(" ");
    const QString dialogFilter =  i18n("All Supported Files") + "(" + allExtensions + ");;" + i18n("All Files") + "(*)";
    QCheckBox *b = new QCheckBox(i18n("Import image sequence"));
    b->setChecked(KdenliveSettings::autoimagesequence());
    QCheckBox *c = new QCheckBox(i18n("Transparent background for images"));
    c->setChecked(KdenliveSettings::autoimagetransparency());
    QFrame *f = new QFrame();
    f->setFrameShape(QFrame::NoFrame);
    QHBoxLayout *l = new QHBoxLayout;
    l->addWidget(b);
    l->addWidget(c);
    l->addStretch(5);
    f->setLayout(l);
    QString clipFolder = KRecentDirs::dir(":KdenliveClipFolder");
    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    QPointer<QFileDialog> d = new QFileDialog(QApplication::activeWindow(), i18n("Open Clips"), clipFolder, dialogFilter);
    //TODO: KF5, how to add a custom widget to file dialog
    /*QGridLayout *layout = (QGridLayout*)d->layout();
    layout->addWidget(f, 0, 0);*/
    d->setFileMode(QFileDialog::ExistingFiles);
    if (d->exec() == QDialog::Accepted) {
        KdenliveSettings::setAutoimagetransparency(c->isChecked());
        list = d->selectedUrls();
        if (!list.isEmpty()) {
            KRecentDirs::add(":KdenliveClipFolder", list.first().adjusted(QUrl::RemoveFilename).path());
        }
        if (b->isChecked() && list.count() == 1) {
            // Check for image sequence
            QUrl url = list.at(0);
            QString fileName = url.fileName().section('.', 0, -2);
            if (fileName.at(fileName.size() - 1).isDigit()) {
                KFileItem item(url);
                if (item.mimetype().startsWith(QLatin1String("image"))) {
                    // import as sequence if we found more than one image in the sequence
                    QStringList list;
                    QString pattern = SlideshowClip::selectedPath(url, false, QString(), &list);
                    int count = list.count();
                    if (count > 1) {
                        delete d;
                        /*QStringList groupInfo = getGroup(); */
                        // get image sequence base name
                        while (fileName.at(fileName.size() - 1).isDigit()) {
                            fileName.chop(1);
                        }
                        QDomDocument xml;
                        QDomElement prod = xml.createElement("producer");
                        xml.appendChild(prod);
                        prod.setAttribute("in", "0");
                        QString duration = doc->timecode().reformatSeparators(KdenliveSettings::sequence_duration());
                        prod.setAttribute("out", QString::number(doc->getFramePos(duration) * count));
                        QMap <QString, QString> properties;
                        properties.insert("resource", pattern);
                        properties.insert("kdenlive:clipname", fileName);
                        properties.insert("ttl", QString::number(doc->getFramePos(duration)));
                        properties.insert("loop", QString::number(false));
                        properties.insert("crop", QString::number(false));
                        properties.insert("fade", QString::number(false));
                        properties.insert("luma_duration", QString::number(doc->getFramePos(doc->timecode().getTimecodeFromFrames(int(ceil(doc->timecode().fps()))))));
                        if (!groupInfo.isEmpty()) {
                            properties.insert("kdenlive:folderid", groupInfo.at(0));
                        }
                        addXmlProperties(prod, properties);
                        uint id = bin->getFreeClipId();
                        AddClipCommand *command = new AddClipCommand(doc, xml.documentElement(), QString::number(id), true);
                        doc->commandStack()->push(command);
                        return;
                    }
                }
            }
        }
    }
    delete d;
    if (!list.isEmpty()) {
        ClipCreationDialogDialog::createClipsCommand(doc, list, groupInfo, bin);
    }
}
