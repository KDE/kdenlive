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
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"
#include "bin/bin.h"
#include "bin/bincommands.h"
#include "bin/projectclip.h"
#include "ui_colorclip_ui.h"
#include "ui_qtextclip_ui.h"
#include "timecodedisplay.h"
#include "titler/titlewidget.h"
#include "titletemplatedialog.h"
#include "project/dialogs/slideshowclip.h"

#include <KMessageBox>
#include <KRecentDirs>
#include <KFileWidget>
#include <KWindowConfig>
#include "klocalizedstring.h"

#include <QDir>
#include <QWindow>
#include <QUndoCommand>
#include <QStandardPaths>
#include <QPushButton>
#include "kdenlive_debug.h"
#include <QDialog>
#include <QPointer>
#include <QMimeDatabase>

// static
QStringList ClipCreationDialog::getExtensions()
{
    // Build list of mime types
    QStringList mimeTypes = QStringList() << QStringLiteral("application/x-kdenlive") << QStringLiteral("application/x-kdenlivetitle") << QStringLiteral("video/mlt-playlist") << QStringLiteral("text/plain");

    // Video mimes
    mimeTypes <<  QStringLiteral("video/x-flv") << QStringLiteral("application/vnd.rn-realmedia") << QStringLiteral("video/x-dv") << QStringLiteral("video/dv") << QStringLiteral("video/x-msvideo") << QStringLiteral("video/x-matroska") << QStringLiteral("video/mpeg") << QStringLiteral("video/ogg") << QStringLiteral("video/x-ms-wmv") << QStringLiteral("video/mp4") << QStringLiteral("video/quicktime") << QStringLiteral("video/webm") << QStringLiteral("video/3gpp") << QStringLiteral("video/mp2t");

    // Audio mimes
    mimeTypes << QStringLiteral("audio/x-flac") << QStringLiteral("audio/x-matroska") << QStringLiteral("audio/mp4") << QStringLiteral("audio/mpeg") << QStringLiteral("audio/x-mp3") << QStringLiteral("audio/ogg") << QStringLiteral("audio/x-wav") << QStringLiteral("audio/x-aiff") << QStringLiteral("audio/aiff") << QStringLiteral("application/ogg") << QStringLiteral("application/mxf") << QStringLiteral("application/x-shockwave-flash") << QStringLiteral("audio/ac3");

    // Image mimes
    mimeTypes << QStringLiteral("image/gif") << QStringLiteral("image/jpeg") << QStringLiteral("image/png") << QStringLiteral("image/x-tga") << QStringLiteral("image/x-bmp") << QStringLiteral("image/svg+xml") << QStringLiteral("image/tiff") << QStringLiteral("image/x-xcf") << QStringLiteral("image/x-xcf-gimp") << QStringLiteral("image/x-vnd.adobe.photoshop") << QStringLiteral("image/x-pcx") << QStringLiteral("image/x-exr") << QStringLiteral("image/x-portable-pixmap") << QStringLiteral("application/x-krita");

    QMimeDatabase db;
    QStringList allExtensions;
    foreach (const QString &mimeType, mimeTypes) {
        QMimeType mime = db.mimeTypeForName(mimeType);
        if (mime.isValid()) {
            allExtensions.append(mime.globPatterns());
        }
    }
    // process custom user extensions
    const QStringList customs = KdenliveSettings::addedExtensions().split(' ', QString::SkipEmptyParts);
    if (!customs.isEmpty()) {
        for (const QString &ext : customs) {
            if (ext.startsWith(QLatin1String("*."))) {
                allExtensions << ext;
            } else if (ext.startsWith(QLatin1String("."))) {
                allExtensions << QStringLiteral("*") + ext;
            } else if (!ext.contains(QLatin1Char('.'))) {
                allExtensions << QStringLiteral("*.") + ext;
            } else {
                //Unrecognized format
                qCDebug(KDENLIVE_LOG)<<"Unrecognized custom format: "<<ext;
            }
        }
    }
    allExtensions.removeDuplicates();
    return allExtensions;
}

//static
void ClipCreationDialog::createClipFromXml(KdenliveDoc *doc, QDomElement &xml, const QStringList &groupInfo, Bin *bin)
{
    //FIXME?
    Q_UNUSED(groupInfo)

    uint id = bin->getFreeClipId();
    xml.setAttribute(QStringLiteral("id"), QString::number(id));
    AddClipCommand *command = new AddClipCommand(bin, xml, QString::number(id), true);
    doc->commandStack()->push(command);
}

//static
void ClipCreationDialog::createColorClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin)
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
        color = color.replace(0, 1, QStringLiteral("0x")) + "ff";
        // Everything is ready. create clip xml
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        prod.setAttribute(QStringLiteral("type"), (int) Color);
        uint id = bin->getFreeClipId();
        prod.setAttribute(QStringLiteral("id"), QString::number(id));
        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
        prod.setAttribute(QStringLiteral("length"), doc->getFramePos(doc->timecode().getTimecode(t->gentime())));
        QMap <QString, QString> properties;
        properties.insert(QStringLiteral("resource"), color);
        properties.insert(QStringLiteral("kdenlive:clipname"), dia_ui.clip_name->text());
        properties.insert(QStringLiteral("mlt_service"), QStringLiteral("color"));
        if (!groupInfo.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        AddClipCommand *command = new AddClipCommand(bin, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete t;
    delete dia;
}

void ClipCreationDialog::createQTextClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin, ProjectClip *clip)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");
    QPointer<QDialog> dia = new QDialog(bin);
    Ui::QTextClip_UI dia_ui;
    dia_ui.setupUi(dia);
    dia->setWindowTitle(i18n("Text Clip"));
    dia_ui.fgColor->setAlphaChannelEnabled(true);
    dia_ui.lineColor->setAlphaChannelEnabled(true);
    dia_ui.bgColor->setAlphaChannelEnabled(true);
    if (clip) {
        dia_ui.name->setText(clip->getProducerProperty(QStringLiteral("kdenlive:clipname")));
        dia_ui.text->setPlainText(clip->getProducerProperty(QStringLiteral("text")));
        dia_ui.fgColor->setColor(clip->getProducerProperty(QStringLiteral("fgcolour")));
        dia_ui.bgColor->setColor(clip->getProducerProperty(QStringLiteral("bgcolour")));
        dia_ui.pad->setValue(clip->getProducerProperty(QStringLiteral("pad")).toInt());
        dia_ui.lineColor->setColor(clip->getProducerProperty(QStringLiteral("olcolour")));
        dia_ui.lineWidth->setValue(clip->getProducerProperty(QStringLiteral("outline")).toInt());
        dia_ui.font->setCurrentFont(QFont(clip->getProducerProperty(QStringLiteral("family"))));
        dia_ui.fontSize->setValue(clip->getProducerProperty(QStringLiteral("size")).toInt());
        dia_ui.weight->setValue(clip->getProducerProperty(QStringLiteral("weight")).toInt());
        dia_ui.italic->setChecked(clip->getProducerProperty(QStringLiteral("style")) == QStringLiteral("italic"));
        dia_ui.duration->setText(doc->timecode().getTimecodeFromFrames(clip->getProducerProperty(QStringLiteral("out")).toInt()));
    } else {
        dia_ui.name->setText(i18n("Text Clip"));
        dia_ui.fgColor->setColor(titleConfig.readEntry(QStringLiteral("font_color")));
        dia_ui.bgColor->setColor(titleConfig.readEntry(QStringLiteral("background_color")));
        dia_ui.lineColor->setColor(titleConfig.readEntry(QStringLiteral("font_outline_color")));
        dia_ui.lineWidth->setValue(titleConfig.readEntry(QStringLiteral("font_outline")).toInt());
        dia_ui.font->setCurrentFont(QFont(titleConfig.readEntry(QStringLiteral("font_family"))));
        dia_ui.fontSize->setValue(titleConfig.readEntry(QStringLiteral("font_pixel_size")).toInt());
        dia_ui.weight->setValue(titleConfig.readEntry(QStringLiteral("font_weight")).toInt());
        dia_ui.italic->setChecked(titleConfig.readEntry(QStringLiteral("font_italic")).toInt());
        dia_ui.duration->setText(titleConfig.readEntry(QStringLiteral("title_duration")));
    }
    if (dia->exec() == QDialog::Accepted) {
        //KdenliveSettings::setColorclipcolor(color);
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        prod.setAttribute(QStringLiteral("type"), (int) QText);
        uint id = bin->getFreeClipId();
        prod.setAttribute(QStringLiteral("id"), QString::number(id));

        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
        prod.setAttribute(QStringLiteral("out"), doc->timecode().getFrameCount(dia_ui.duration->text()));

        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("kdenlive:clipname"), dia_ui.name->text());
        if (!groupInfo.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
        }

        properties.insert(QStringLiteral("mlt_service"), QStringLiteral("qtext"));
        properties.insert(QStringLiteral("out"), QString::number(doc->timecode().getFrameCount(dia_ui.duration->text())));
        properties.insert(QStringLiteral("length"), dia_ui.duration->text());
        //properties.insert(QStringLiteral("scale"), QStringLiteral("off"));
        //properties.insert(QStringLiteral("fill"), QStringLiteral("0"));
        properties.insert(QStringLiteral("text"), dia_ui.text->document()->toPlainText());
        properties.insert(QStringLiteral("fgcolour"), dia_ui.fgColor->color().name(QColor::HexArgb));
        properties.insert(QStringLiteral("bgcolour"), dia_ui.bgColor->color().name(QColor::HexArgb));
        properties.insert(QStringLiteral("olcolour"), dia_ui.lineColor->color().name(QColor::HexArgb));
        properties.insert(QStringLiteral("outline"), QString::number(dia_ui.lineWidth->value()));
        properties.insert(QStringLiteral("pad"), QString::number(dia_ui.pad->value()));
        properties.insert(QStringLiteral("family"), dia_ui.font->currentFont().family());
        properties.insert(QStringLiteral("size"), QString::number(dia_ui.fontSize->value()));
        properties.insert(QStringLiteral("style"), dia_ui.italic->isChecked() ? QStringLiteral("italic") : QStringLiteral("normal"));
        properties.insert(QStringLiteral("weight"), QString::number(dia_ui.weight->value()));
        if (clip) {
            QMap<QString, QString> oldProperties;
            oldProperties.insert(QStringLiteral("out"), clip->getProducerProperty(QStringLiteral("out")));
            oldProperties.insert(QStringLiteral("length"), clip->getProducerProperty(QStringLiteral("length")));
            oldProperties.insert(QStringLiteral("kdenlive:clipname"), clip->name());
            oldProperties.insert(QStringLiteral("ttl"), clip->getProducerProperty(QStringLiteral("ttl")));
            oldProperties.insert(QStringLiteral("loop"), clip->getProducerProperty(QStringLiteral("loop")));
            oldProperties.insert(QStringLiteral("crop"), clip->getProducerProperty(QStringLiteral("crop")));
            oldProperties.insert(QStringLiteral("fade"), clip->getProducerProperty(QStringLiteral("fade")));
            oldProperties.insert(QStringLiteral("luma_duration"), clip->getProducerProperty(QStringLiteral("luma_duration")));
            oldProperties.insert(QStringLiteral("luma_file"), clip->getProducerProperty(QStringLiteral("luma_file")));
            oldProperties.insert(QStringLiteral("softness"), clip->getProducerProperty(QStringLiteral("softness")));
            oldProperties.insert(QStringLiteral("animation"), clip->getProducerProperty(QStringLiteral("animation")));
            bin->slotEditClipCommand(clip->clipId(), oldProperties, properties);
        } else {
            addXmlProperties(prod, properties);
            AddClipCommand *command = new AddClipCommand(bin, xml.documentElement(), QString::number(id), true);
            doc->commandStack()->push(command);
        }
    }
    delete dia;
}

//static
void ClipCreationDialog::createSlideshowClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin)
{
    QPointer<SlideshowClip> dia = new SlideshowClip(doc->timecode(), KRecentDirs::dir(QStringLiteral(":KdenliveSlideShowFolder")), nullptr, bin);

    if (dia->exec() == QDialog::Accepted) {
        // Ready, create xml
        KRecentDirs::add(QStringLiteral(":KdenliveSlideShowFolder"), QUrl::fromLocalFile(dia->selectedPath()).adjusted(QUrl::RemoveFilename).toLocalFile());
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
        prod.setAttribute(QStringLiteral("out"), QString::number(doc->getFramePos(dia->clipDuration()) * dia->imageCount() - 1));
        prod.setAttribute(QStringLiteral("type"), (int) SlideShow);
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("kdenlive:clipname"), dia->clipName());
        properties.insert(QStringLiteral("resource"), dia->selectedPath());
        properties.insert(QStringLiteral("ttl"), QString::number(doc->getFramePos(dia->clipDuration())));
        properties.insert(QStringLiteral("loop"), QString::number(dia->loop()));
        properties.insert(QStringLiteral("crop"), QString::number(dia->crop()));
        properties.insert(QStringLiteral("fade"), QString::number(dia->fade()));
        properties.insert(QStringLiteral("luma_duration"), QString::number(doc->getFramePos(dia->lumaDuration())));
        properties.insert(QStringLiteral("luma_file"), dia->lumaFile());
        properties.insert(QStringLiteral("softness"), QString::number(dia->softness()));
        properties.insert(QStringLiteral("animation"), dia->animation());
        if (!groupInfo.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        uint id = bin->getFreeClipId();
        AddClipCommand *command = new AddClipCommand(bin, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete dia;
}

void ClipCreationDialog::createTitleClip(KdenliveDoc *doc, const QStringList &groupInfo, const QString &templatePath, Bin *bin)
{
    // Make sure the titles folder exists
    QDir dir(doc->projectDataFolder() + QStringLiteral("/titles"));
    dir.mkpath(QStringLiteral("."));
    QPointer<TitleWidget> dia_ui = new TitleWidget(QUrl::fromLocalFile(templatePath), doc->timecode(), dir.absolutePath(), doc->renderer(), bin);
    QObject::connect(dia_ui.data(), &TitleWidget::requestBackgroundFrame, bin, &Bin::slotGetCurrentProjectImage);
    if (dia_ui->exec() == QDialog::Accepted) {
        // Ready, create clip xml
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        //prod.setAttribute("resource", imagePath);
        uint id = bin->getFreeClipId();
        prod.setAttribute(QStringLiteral("id"), QString::number(id));

        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("xmldata"), dia_ui->xml().toString());
        properties.insert(QStringLiteral("kdenlive:clipname"), i18n("Title clip"));
        if (!groupInfo.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        prod.setAttribute(QStringLiteral("type"), (int) Text);
        prod.setAttribute(QStringLiteral("transparency"), QStringLiteral("1"));
        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
        prod.setAttribute(QStringLiteral("out"), dia_ui->duration() - 1);
        AddClipCommand *command = new AddClipCommand(bin, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete dia_ui;
}

void ClipCreationDialog::createTitleTemplateClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin)
{

    QPointer<TitleTemplateDialog> dia = new TitleTemplateDialog(doc->projectDataFolder(), QApplication::activeWindow());

    if (dia->exec() == QDialog::Accepted) {
        QString textTemplate = dia->selectedTemplate();
        // Create a cloned template clip
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);

        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("resource"), textTemplate);
        properties.insert(QStringLiteral("kdenlive:clipname"), i18n("Template title clip"));
        if (!groupInfo.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
        }
        addXmlProperties(prod, properties);
        uint id = bin->getFreeClipId();
        prod.setAttribute(QStringLiteral("id"), QString::number(id));
        prod.setAttribute(QStringLiteral("type"), (int) TextTemplate);
        prod.setAttribute(QStringLiteral("transparency"), QStringLiteral("1"));
        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
        prod.setAttribute(QStringLiteral("templatetext"), dia->selectedText());

        int duration = 0;
        QDomDocument titledoc;
        QFile txtfile(textTemplate);
        if (txtfile.open(QIODevice::ReadOnly) && titledoc.setContent(&txtfile)) {
            if (titledoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
                duration = titledoc.documentElement().attribute(QStringLiteral("duration")).toInt();
            } else {
                // keep some time for backwards compatibility - 26/12/12
                duration = titledoc.documentElement().attribute(QStringLiteral("out")).toInt();
            }
        }
        txtfile.close();

        if (duration == 0) {
            duration = doc->getFramePos(KdenliveSettings::title_duration());
        }
        prod.setAttribute(QStringLiteral("duration"), duration - 1);
        prod.setAttribute(QStringLiteral("out"), duration - 1);

        AddClipCommand *command = new AddClipCommand(bin, xml.documentElement(), QString::number(id), true);
        doc->commandStack()->push(command);
    }
    delete dia;
}

void ClipCreationDialog::addXmlProperties(QDomElement &producer, QMap<QString, QString> &properties)
{
    QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        QDomElement prop = producer.ownerDocument().createElement(QStringLiteral("property"));
        prop.setAttribute(QStringLiteral("name"), i.key());
        QDomText value = producer.ownerDocument().createTextNode(i.value());
        prop.appendChild(value);
        producer.appendChild(prop);
    }
}

void ClipCreationDialog::createClipsCommand(KdenliveDoc *doc, const QList<QUrl> &urls, const QStringList &groupInfo, Bin *bin, const QMap<QString, QString> &data)
{
    QUndoCommand *addClips = new QUndoCommand();

    //TODO: check files on removable volume
    /*listRemovableVolumes();
    foreach(const QUrl &file, urls) {
        if (QFile::exists(file.path())) {
            //TODO check for duplicates
            if (!data.contains("bypassDuplicate") && !getClipByResource(file.path()).empty()) {
                if (KMessageBox::warningContinueCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br />already exists in project, what do you want to do?", file.path()), i18n("Clip already exists")) == KMessageBox::Cancel)
                    continue;
            }
            if (isOnRemovableDevice(file) && !isOnRemovableDevice(m_doc->projectFolder())) {
                int answer = KMessageBox::warningYesNoCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br /> is on a removable device, will not be available when device is unplugged", file.path()), i18n("File on a Removable Device"), KGuiItem(i18n("Copy file to project folder")), KGuiItem(i18n("Continue")), KStandardGuiItem::cancel(), QString("copyFilesToProjectFolder"));

                if (answer == KMessageBox::Cancel) continue;
                else if (answer == KMessageBox::Yes) {
                    // Copy files to project folder
                    QDir sourcesFolder(m_doc->projectFolder().toLocalFile());
                    sourcesFolder.cd("clips");
                    KIO::MkdirJob *mkdirJob = KIO::mkdir(QUrl::fromLocalFile(sourcesFolder.absolutePath()));
                    KJobWidgets::setWindow(mkdirJob, QApplication::activeWindow());
                    if (!mkdirJob->exec()) {
                        KMessageBox::sorry(QApplication::activeWindow(), i18n("Cannot create directory %1", sourcesFolder.absolutePath()));
                        continue;
                    }
                    //KIO::filesize_t m_requestedSize;
                    KIO::CopyJob *copyjob = KIO::copy(file, QUrl::fromLocalFile(sourcesFolder.absolutePath()));
                    //TODO: for some reason, passing metadata does not work...
                    copyjob->addMetaData("group", data.value("group"));
                    copyjob->addMetaData("groupId", data.value("groupId"));
                    copyjob->addMetaData("comment", data.value("comment"));
                    KJobWidgets::setWindow(copyjob, QApplication::activeWindow());
                    connect(copyjob, &KIO::CopyJob::copyingDone, this, &ClipManager::slotAddCopiedClip);
                    continue;
                }
            }*/

    //TODO check folders
    /*QList< QList<QUrl> > foldersList;
    QMimeDatabase db;
    foreach(const QUrl & file, list) {
        // Check there is no folder here
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.inherits("inode/directory")) {
            // user dropped a folder, import its files
            list.removeAll(file);
            QDir dir(file.path());
            QStringList result = dir.entryList(QDir::Files);
            QList<QUrl> folderFiles;
            folderFiles << file;
            foreach(const QString & path, result) {
                // TODO: create folder command
                folderFiles.append(QUrl::fromLocalFile(dir.absoluteFilePath(path)));
            }
            if (folderFiles.count() > 1) foldersList.append(folderFiles);
        }
    }*/

    foreach (const QUrl &file, urls) {
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("resource"), file.toLocalFile());
        if (!groupInfo.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
        }
        // Merge data
        QMapIterator<QString, QString> i(data);
        while (i.hasNext()) {
            i.next();
            properties.insert(i.key(), i.value());
        }
        uint id = bin->getFreeClipId();
        prod.setAttribute(QStringLiteral("id"), QString::number(id));
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.name().startsWith(QLatin1String("image/"))) {
            prod.setAttribute(QStringLiteral("type"), (int) Image);
            prod.setAttribute(QStringLiteral("in"), 0);
            prod.setAttribute(QStringLiteral("length"), doc->getFramePos(KdenliveSettings::image_duration()));
            if (KdenliveSettings::autoimagetransparency()) {
                properties.insert(QStringLiteral("kdenlive:transparency"), QStringLiteral("1"));
            }
        } else if (type.inherits(QStringLiteral("application/x-kdenlivetitle"))) {
            // opening a title file
            QDomDocument txtdoc(QStringLiteral("titledocument"));
            QFile txtfile(file.toLocalFile());
            if (txtfile.open(QIODevice::ReadOnly) && txtdoc.setContent(&txtfile)) {
                txtfile.close();
                prod.setAttribute(QStringLiteral("type"), (int) Text);
                // extract embedded images
                QDomNodeList items = txtdoc.elementsByTagName(QStringLiteral("content"));
                for (int i = 0; i < items.count(); ++i) {
                    QDomElement content = items.item(i).toElement();
                    if (content.hasAttribute(QStringLiteral("base64"))) {
                        QString titlesFolder = doc->projectDataFolder() + QStringLiteral("/titles/");
                        QString path = TitleDocument::extractBase64Image(titlesFolder, content.attribute(QStringLiteral("base64")));
                        if (!path.isEmpty()) {
                            content.setAttribute(QStringLiteral("url"), path);
                            content.removeAttribute(QStringLiteral("base64"));
                        }
                    }
                }
                prod.setAttribute(QStringLiteral("in"), 0);
                int duration = 0;
                if (txtdoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
                    duration = txtdoc.documentElement().attribute(QStringLiteral("duration")).toInt();
                } else if (txtdoc.documentElement().hasAttribute(QStringLiteral("out"))) {
                    duration = txtdoc.documentElement().attribute(QStringLiteral("out")).toInt();
                }
                if (duration <= 0) {
                    duration = doc->getFramePos(KdenliveSettings::title_duration()) - 1;
                }
                prod.setAttribute(QStringLiteral("duration"), duration);
                prod.setAttribute(QStringLiteral("out"), duration);
                txtdoc.documentElement().setAttribute(QStringLiteral("duration"), duration);
                txtdoc.documentElement().setAttribute(QStringLiteral("out"), duration);
                QString titleData = txtdoc.toString();
                prod.setAttribute(QStringLiteral("xmldata"), titleData);
            } else {
                txtfile.close();
            }
        }
        addXmlProperties(prod, properties);
        new AddClipCommand(bin, xml.documentElement(), QString::number(id), true, addClips);
    }
    if (addClips->childCount() > 0) {
        addClips->setText(i18np("Add clip", "Add clips", addClips->childCount()));
        doc->commandStack()->push(addClips);
    }
}

void ClipCreationDialog::createClipsCommand(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin)
{
    QList<QUrl> list;
    QString allExtensions = getExtensions().join(QLatin1Char(' '));
    QString dialogFilter = allExtensions + QLatin1Char('|') + i18n("All Supported Files") + QStringLiteral("\n*|") + i18n("All Files");
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
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    QDialog *dlg = new QDialog((QWidget *) doc->parent());
    KFileWidget *fileWidget = new KFileWidget(QUrl::fromLocalFile(clipFolder), dlg);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(fileWidget);
    fileWidget->setCustomWidget(f);
    fileWidget->okButton()->show();
    fileWidget->cancelButton()->show();
    QObject::connect(fileWidget->okButton(), &QPushButton::clicked, fileWidget, &KFileWidget::slotOk);
    QObject::connect(fileWidget, &KFileWidget::accepted, fileWidget, &KFileWidget::accept);
    QObject::connect(fileWidget, &KFileWidget::accepted, dlg, &QDialog::accept);
    QObject::connect(fileWidget->cancelButton(), &QPushButton::clicked, dlg, &QDialog::reject);
    dlg->setLayout(layout);
    fileWidget->setFilter(dialogFilter);
    fileWidget->setMode(KFile::Files | KFile::ExistingOnly | KFile::LocalOnly);
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    QWindow *handle = dlg->windowHandle();
    if (handle && conf->hasGroup("FileDialogSize")) {
        KWindowConfig::restoreWindowSize(handle, conf->group("FileDialogSize"));
        dlg->resize(handle->size());
    }
    if (dlg->exec() == QDialog::Accepted) {
        KdenliveSettings::setAutoimagetransparency(c->isChecked());
        list = fileWidget->selectedUrls();
        if (!list.isEmpty()) {
            KRecentDirs::add(QStringLiteral(":KdenliveClipFolder"), list.first().adjusted(QUrl::RemoveFilename).toLocalFile());
        }
        if (b->isChecked() && list.count() == 1) {
            // Check for image sequence
            QUrl url = list.at(0);
            QString fileName = url.fileName().section(QLatin1Char('.'), 0, -2);
            if (fileName.at(fileName.size() - 1).isDigit()) {
                KFileItem item(url);
                if (item.mimetype().startsWith(QLatin1String("image"))) {
                    // import as sequence if we found more than one image in the sequence
                    QStringList list;
                    QString pattern = SlideshowClip::selectedPath(url, false, QString(), &list);
                    qCDebug(KDENLIVE_LOG) << " / // IMPORT PATTERN: " << pattern << " COUNT: " << list.count();
                    int count = list.count();
                    if (count > 1) {
                        delete fileWidget;
                        delete dlg;
                        // get image sequence base name
                        while (fileName.at(fileName.size() - 1).isDigit()) {
                            fileName.chop(1);
                        }
                        QDomDocument xml;
                        QDomElement prod = xml.createElement(QStringLiteral("producer"));
                        xml.appendChild(prod);
                        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
                        QString duration = doc->timecode().reformatSeparators(KdenliveSettings::sequence_duration());
                        prod.setAttribute(QStringLiteral("out"), QString::number(doc->getFramePos(duration) * count));
                        QMap<QString, QString> properties;
                        properties.insert(QStringLiteral("resource"), pattern);
                        properties.insert(QStringLiteral("kdenlive:clipname"), fileName);
                        properties.insert(QStringLiteral("ttl"), QString::number(doc->getFramePos(duration)));
                        properties.insert(QStringLiteral("loop"), QString::number(false));
                        properties.insert(QStringLiteral("crop"), QString::number(false));
                        properties.insert(QStringLiteral("fade"), QString::number(false));
                        properties.insert(QStringLiteral("luma_duration"), QString::number(doc->getFramePos(doc->timecode().getTimecodeFromFrames(int(ceil(doc->timecode().fps()))))));
                        if (!groupInfo.isEmpty()) {
                            properties.insert(QStringLiteral("kdenlive:folderid"), groupInfo.at(0));
                        }
                        addXmlProperties(prod, properties);
                        uint id = bin->getFreeClipId();
                        AddClipCommand *command = new AddClipCommand(bin, xml.documentElement(), QString::number(id), true);
                        doc->commandStack()->push(command);
                        return;
                    }
                }
            }
        }
    }
    KConfigGroup group = conf->group("FileDialogSize");
    if (handle) {
        KWindowConfig::saveWindowSize(handle, group);
    }

    delete fileWidget;
    delete dlg;
    if (!list.isEmpty()) {
        ClipCreationDialog::createClipsCommand(doc, list, groupInfo, bin);
    }
}

void ClipCreationDialog::createClipsCommand(Bin *bin, const QDomElement &producer, const QString &id, QUndoCommand *command)
{
    new AddClipCommand(bin, producer, id, true, command);
}

