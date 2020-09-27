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
#include "bin/bin.h"
#include "bin/bincommands.h"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "definitions.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "project/dialogs/slideshowclip.h"
#include "timecodedisplay.h"
#include "titler/titlewidget.h"
#include "titletemplatedialog.h"
#include "ui_colorclip_ui.h"
#include "ui_qtextclip_ui.h"
#include "utils/devices.hpp"
#include "xml/xml.hpp"

#include "klocalizedstring.h"
#include <KFileWidget>
#include <KMessageBox>
#include <KRecentDirs>
#include <KWindowConfig>

#include <QDialog>
#include <QDir>
#include <QMimeDatabase>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QUndoCommand>
#include <QWindow>
#include <unordered_map>
#include <utility>
// static
QStringList ClipCreationDialog::getExtensions()
{
    // Build list of MIME types
    QStringList mimeTypes = QStringList() << QLatin1String("") << QStringLiteral("application/x-kdenlivetitle") << QStringLiteral("video/mlt-playlist")
                                          << QStringLiteral("text/plain") << QStringLiteral("application/x-kdenlive");

    // Video MIMEs
    mimeTypes << QStringLiteral("video/x-flv") << QStringLiteral("application/vnd.rn-realmedia") << QStringLiteral("video/x-dv") << QStringLiteral("video/dv")
              << QStringLiteral("video/x-msvideo") << QStringLiteral("video/x-matroska") << QStringLiteral("video/mpeg") << QStringLiteral("video/ogg")
              << QStringLiteral("video/x-ms-wmv") << QStringLiteral("video/mp4") << QStringLiteral("video/quicktime") << QStringLiteral("video/webm")
              << QStringLiteral("video/3gpp") << QStringLiteral("video/mp2t");

    // Audio MIMEs
    mimeTypes << QStringLiteral("audio/AMR") << QStringLiteral("audio/x-flac") << QStringLiteral("audio/x-matroska") << QStringLiteral("audio/mp4")
              << QStringLiteral("audio/mpeg") << QStringLiteral("audio/x-mp3") << QStringLiteral("audio/ogg") << QStringLiteral("audio/x-wav")
              << QStringLiteral("audio/x-aiff") << QStringLiteral("audio/aiff") << QStringLiteral("application/ogg") << QStringLiteral("application/mxf")
              << QStringLiteral("application/x-shockwave-flash") << QStringLiteral("audio/ac3");

    // Image MIMEs
    mimeTypes << QStringLiteral("image/gif") << QStringLiteral("image/jpeg") << QStringLiteral("image/png") << QStringLiteral("image/x-tga")
              << QStringLiteral("image/x-bmp") << QStringLiteral("image/svg+xml") << QStringLiteral("image/tiff") << QStringLiteral("image/x-xcf")
              << QStringLiteral("image/x-xcf-gimp") << QStringLiteral("image/x-vnd.adobe.photoshop") << QStringLiteral("image/x-pcx")
              << QStringLiteral("image/x-exr") << QStringLiteral("image/x-portable-pixmap") << QStringLiteral("application/x-krita");

    QMimeDatabase db;
    QStringList allExtensions;
    for (const QString &mimeType : qAsConst(mimeTypes)) {
        QMimeType mime = db.mimeTypeForName(mimeType);
        if (mime.isValid()) {
            allExtensions.append(mime.globPatterns());
        }
    }
    // process custom user extensions
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    const QStringList customs = KdenliveSettings::addedExtensions().split(' ', QString::SkipEmptyParts);
#else
    const QStringList customs = KdenliveSettings::addedExtensions().split(' ', Qt::SkipEmptyParts);
#endif
    if (!customs.isEmpty()) {
        for (const QString &ext : customs) {
            if (ext.startsWith(QLatin1String("*."))) {
                allExtensions << ext;
            } else if (ext.startsWith(QLatin1String("."))) {
                allExtensions << QStringLiteral("*") + ext;
            } else if (!ext.contains(QLatin1Char('.'))) {
                allExtensions << QStringLiteral("*.") + ext;
            } else {
                // Unrecognized format
                qCDebug(KDENLIVE_LOG) << "Unrecognized custom format: " << ext;
            }
        }
    }
    allExtensions.removeDuplicates();
    return allExtensions;
}

QString ClipCreationDialog::getExtensionsFilter(const QStringList& additionalFilters)
{
    const QString allExtensions = ClipCreationDialog::getExtensions().join(QLatin1Char(' '));
    QString filter = i18n("All Supported Files") + " (" + allExtensions + ')';
    if (!additionalFilters.isEmpty()) {
        filter += ";;";
        filter.append(additionalFilters.join(";;"));
    }
    return filter;
}

// static
void ClipCreationDialog::createColorClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model)
{
    QScopedPointer<QDialog> dia(new QDialog(qApp->activeWindow()));
    Ui::ColorClip_UI dia_ui;
    dia_ui.setupUi(dia.data());
    dia->setWindowTitle(i18n("Color Clip"));
    dia_ui.clip_name->setText(i18n("Color Clip"));

    QScopedPointer<TimecodeDisplay> t(new TimecodeDisplay(doc->timecode()));
    t->setValue(KdenliveSettings::color_duration());
    dia_ui.clip_durationBox->addWidget(t.data());
    dia_ui.clip_color->setColor(KdenliveSettings::colorclipcolor());

    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui.clip_color->color().name();
        KdenliveSettings::setColorclipcolor(color);
        color = color.replace(0, 1, QStringLiteral("0x")) + "ff";
        int duration = doc->getFramePos(doc->timecode().getTimecode(t->gentime()));
        QString name = dia_ui.clip_name->text();

        ClipCreator::createColorClip(color, duration, name, parentFolder, std::move(model));
    }
}

void ClipCreationDialog::createQTextClip(KdenliveDoc *doc, const QString &parentId, Bin *bin, ProjectClip *clip)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");
    QScopedPointer<QDialog> dia(new QDialog(bin));
    Ui::QTextClip_UI dia_ui;
    dia_ui.setupUi(dia.data());
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
        dia_ui.italic->setChecked(titleConfig.readEntry(QStringLiteral("font_italic")).toInt() != 0);
        dia_ui.duration->setText(titleConfig.readEntry(QStringLiteral("title_duration")));
    }
    if (dia->exec() == QDialog::Accepted) {
        // KdenliveSettings::setColorclipcolor(color);
        QDomDocument xml;
        QDomElement prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        prod.setAttribute(QStringLiteral("type"), (int)ClipType::QText);
        int id = pCore->projectItemModel()->getFreeClipId();
        prod.setAttribute(QStringLiteral("id"), QString::number(id));

        prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
        prod.setAttribute(QStringLiteral("out"), doc->timecode().getFrameCount(dia_ui.duration->text()));

        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("kdenlive:clipname"), dia_ui.name->text());
        if (!parentId.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), parentId);
        }

        properties.insert(QStringLiteral("mlt_service"), QStringLiteral("qtext"));
        properties.insert(QStringLiteral("out"), QString::number(doc->timecode().getFrameCount(dia_ui.duration->text())));
        properties.insert(QStringLiteral("length"), dia_ui.duration->text());
        // properties.insert(QStringLiteral("scale"), QStringLiteral("off"));
        // properties.insert(QStringLiteral("fill"), QStringLiteral("0"));
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
            bin->slotEditClipCommand(clip->AbstractProjectItem::clipId(), oldProperties, properties);
        } else {
            Xml::addXmlProperties(prod, properties);
            QString clipId = QString::number(id);
            pCore->projectItemModel()->requestAddBinClip(clipId, xml.documentElement(), parentId, i18n("Create Title clip"));
        }
    }
}

// static
void ClipCreationDialog::createSlideshowClip(KdenliveDoc *doc, const QString &parentId, std::shared_ptr<ProjectItemModel> model)
{
    QScopedPointer<SlideshowClip> dia(
        new SlideshowClip(doc->timecode(), KRecentDirs::dir(QStringLiteral(":KdenliveSlideShowFolder")), nullptr, QApplication::activeWindow()));

    if (dia->exec() == QDialog::Accepted) {
        // Ready, create xml
        KRecentDirs::add(QStringLiteral(":KdenliveSlideShowFolder"), QUrl::fromLocalFile(dia->selectedPath()).adjusted(QUrl::RemoveFilename).toLocalFile());
        std::unordered_map<QString, QString> properties;
        properties[QStringLiteral("ttl")] = QString::number(doc->getFramePos(dia->clipDuration()));
        properties[QStringLiteral("loop")] = QString::number(static_cast<int>(dia->loop()));
        properties[QStringLiteral("crop")] = QString::number(static_cast<int>(dia->crop()));
        properties[QStringLiteral("fade")] = QString::number(static_cast<int>(dia->fade()));
        properties[QStringLiteral("luma_duration")] = QString::number(doc->getFramePos(dia->lumaDuration()));
        properties[QStringLiteral("luma_file")] = dia->lumaFile();
        properties[QStringLiteral("softness")] = QString::number(dia->softness());
        properties[QStringLiteral("animation")] = dia->animation();

        int duration = doc->getFramePos(dia->clipDuration()) * dia->imageCount();
        ClipCreator::createSlideshowClip(dia->selectedPath(), duration, dia->clipName(), parentId, properties, std::move(model));
    }
}

void ClipCreationDialog::createTitleClip(KdenliveDoc *doc, const QString &parentFolder, const QString &templatePath, std::shared_ptr<ProjectItemModel> model)
{
    // Make sure the titles folder exists
    QDir dir(doc->projectDataFolder() + QStringLiteral("/titles"));
    dir.mkpath(QStringLiteral("."));
    QPointer<TitleWidget> dia_ui =
        new TitleWidget(QUrl::fromLocalFile(templatePath), doc->timecode(), dir.absolutePath(), pCore->getMonitor(Kdenlive::ProjectMonitor), pCore->bin());
    if (dia_ui->exec() == QDialog::Accepted) {
        // Ready, create clip xml
        std::unordered_map<QString, QString> properties;
        properties[QStringLiteral("xmldata")] = dia_ui->xml().toString();
        QString titleSuggestion = dia_ui->titleSuggest();

        ClipCreator::createTitleClip(properties, dia_ui->duration(), titleSuggestion.isEmpty() ? i18n("Title clip") : titleSuggestion, parentFolder,
                                     std::move(model));
    }
    delete dia_ui;
}

void ClipCreationDialog::createTitleTemplateClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model)
{

    QScopedPointer<TitleTemplateDialog> dia(new TitleTemplateDialog(doc->projectDataFolder(), QApplication::activeWindow()));

    if (dia->exec() == QDialog::Accepted) {
        ClipCreator::createTitleTemplate(dia->selectedTemplate(), dia->selectedText(), i18n("Template title clip"), parentFolder, std::move(model));
    }
}

// void ClipCreationDialog::createClipsCommand(KdenliveDoc *doc, const QList<QUrl> &urls, const QStringList &groupInfo, Bin *bin,
//                                             const QMap<QString, QString> &data)
// {
//     auto *addClips = new QUndoCommand();

// TODO: check files on removable volume
/*listRemovableVolumes();
for (const QUrl &file :  urls) {
    if (QFile::exists(file.path())) {
        //TODO check for duplicates
        if (!data.contains("bypassDuplicate") && !getClipByResource(file.path()).empty()) {
            if (KMessageBox::warningContinueCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br />already exists in project, what do you want to
do?", file.path()), i18n("Clip already exists")) == KMessageBox::Cancel)
                continue;
        }
        if (isOnRemovableDevice(file) && !isOnRemovableDevice(m_doc->projectFolder())) {
            int answer = KMessageBox::warningYesNoCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br /> is on a removable device, will not be
available when device is unplugged", file.path()), i18n("File on a Removable Device"), KGuiItem(i18n("Copy file to project folder")),
KGuiItem(i18n("Continue")), KStandardGuiItem::cancel(), QString("copyFilesToProjectFolder"));

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

// TODO check folders
/*QList< QList<QUrl> > foldersList;
QMimeDatabase db;
for (const QUrl & file :  list) {
    // Check there is no folder here
    QMimeType type = db.mimeTypeForUrl(file);
    if (type.inherits("inode/directory")) {
        // user dropped a folder, import its files
        list.removeAll(file);
        QDir dir(file.path());
        QStringList result = dir.entryList(QDir::Files);
        QList<QUrl> folderFiles;
        folderFiles << file;
        for (const QString & path :  result) {
            // TODO: create folder command
            folderFiles.append(QUrl::fromLocalFile(dir.absoluteFilePath(path)));
        }
        if (folderFiles.count() > 1) foldersList.append(folderFiles);
    }
}*/
//}

void ClipCreationDialog::createClipsCommand(KdenliveDoc *doc, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model)
{
    qDebug() << "/////////// starting to add bin clips";
    QList<QUrl> list;
    QString allExtensions = getExtensions().join(QLatin1Char(' '));
    QString dialogFilter = allExtensions + QLatin1Char('|') + i18n("All Supported Files") + QStringLiteral("\n*|") + i18n("All Files");
    QCheckBox *b = new QCheckBox(i18n("Import image sequence"));
    b->setChecked(KdenliveSettings::autoimagesequence());
    QFrame *f = new QFrame();
    f->setFrameShape(QFrame::NoFrame);
    auto *l = new QHBoxLayout;
    l->addWidget(b);
    l->addStretch(5);
    f->setLayout(l);
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    QScopedPointer<QDialog> dlg(new QDialog((QWidget *)doc->parent()));
    QScopedPointer<KFileWidget> fileWidget(new KFileWidget(QUrl::fromLocalFile(clipFolder), dlg.data()));
    auto *layout = new QVBoxLayout;
    layout->addWidget(fileWidget.data());
    fileWidget->setCustomWidget(f);
    fileWidget->okButton()->show();
    fileWidget->cancelButton()->show();
    QObject::connect(fileWidget->okButton(), &QPushButton::clicked, fileWidget.data(), &KFileWidget::slotOk);
    QObject::connect(fileWidget.data(), &KFileWidget::accepted, fileWidget.data(), &KFileWidget::accept);
    QObject::connect(fileWidget.data(), &KFileWidget::accepted, dlg.data(), &QDialog::accept);
    QObject::connect(fileWidget->cancelButton(), &QPushButton::clicked, dlg.data(), &QDialog::reject);
    dlg->setLayout(layout);
    fileWidget->setFilter(dialogFilter);
    fileWidget->setMode(KFile::Files | KFile::ExistingOnly | KFile::LocalOnly | KFile::Directory);
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    QWindow *handle = dlg->windowHandle();
    if ((handle != nullptr) && conf->hasGroup("FileDialogSize")) {
        KWindowConfig::restoreWindowSize(handle, conf->group("FileDialogSize"));
        dlg->resize(handle->size());
    }
    int result = dlg->exec();
    if (result == QDialog::Accepted) {
        KdenliveSettings::setAutoimagesequence(b->isChecked());
        list = fileWidget->selectedUrls();
        if (!list.isEmpty()) {
            KRecentDirs::add(QStringLiteral(":KdenliveClipFolder"), list.constFirst().adjusted(QUrl::RemoveFilename).toLocalFile());
        }
        if (KdenliveSettings::autoimagesequence() && list.count() >= 1) {
            // Check for image sequence
            const QUrl &url = list.at(0);
            QString fileName = url.fileName().section(QLatin1Char('.'), 0, -2);
            if (!fileName.isEmpty() && fileName.at(fileName.size() - 1).isDigit()) {
                KFileItem item(url);
                if (item.mimetype().startsWith(QLatin1String("image"))) {
                    // import as sequence if we found more than one image in the sequence
                    QStringList patternlist;
                    QString pattern = SlideshowClip::selectedPath(url, false, QString(), &patternlist);
                    qCDebug(KDENLIVE_LOG) << " / // IMPORT PATTERN: " << pattern << " COUNT: " << patternlist.count();
                    int count = patternlist.count();
                    if (count > 1) {
                        // get image sequence base name
                        while (fileName.size() > 0 && fileName.at(fileName.size() - 1).isDigit()) {
                            fileName.chop(1);
                        }
                        QString duration = doc->timecode().reformatSeparators(KdenliveSettings::sequence_duration());
                        std::unordered_map<QString, QString> properties;
                        properties[QStringLiteral("ttl")] = QString::number(doc->getFramePos(duration));
                        properties[QStringLiteral("loop")] = QString::number(0);
                        properties[QStringLiteral("crop")] = QString::number(0);
                        properties[QStringLiteral("fade")] = QString::number(0);
                        properties[QStringLiteral("luma_duration")] =
                            QString::number(doc->getFramePos(doc->timecode().getTimecodeFromFrames(int(ceil(doc->timecode().fps())))));
                        int frame_duration = doc->getFramePos(duration) * count;
                        ClipCreator::createSlideshowClip(pattern, frame_duration, fileName, parentFolder, properties, model);
                        return;
                    }
                }
            }
        }
    }
    qDebug() << "/////////// found list" << list;
    KConfigGroup group = conf->group("FileDialogSize");
    if (handle) {
        KWindowConfig::saveWindowSize(handle, group);
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    const QString id = ClipCreator::createClipsFromList(list, true, parentFolder, model, undo, redo);

    // We reset the state of the "don't ask again" for the question about removable devices
    KMessageBox::enableMessage(QStringLiteral("removable"));
    if (!id.isEmpty()) {
        pCore->pushUndo(undo, redo, i18np("Add clip", "Add clips", list.size()));
    }
}
