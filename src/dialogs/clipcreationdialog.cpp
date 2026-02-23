/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipcreationdialog.h"
#include "bin/bin.h"
#include "bin/bincommands.h"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "glaxnimatelauncher.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "project/dialogs/slideshowclip.h"
#include "titler/titlewidget.h"
#include "titletemplatedialog.h"
#include "ui_colorclip_ui.h"
#include "ui_qtextclip_ui.h"
#include "utils/devices.hpp"
#include "utils/qcolorutils.h"
#include "widgets/timecodedisplay.h"
#include "xml/xml.hpp"
#include "filefilter.h"

#include <KDirOperator>
#include <KFileWidget>
#include <KIO/RenameDialog>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentDirs>
#include <KWindowConfig>

#include <QDialog>
#include <QDir>
#include <QMimeDatabase>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QUndoCommand>
#include <QWindow>

#include <unordered_map>
#include <utility>


// static
void ClipCreationDialog::createColorClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model,
                                         const std::function<void(const QString &)> &readyCallBack, int suggestedDuration)
{
    QScopedPointer<QDialog> dia(new QDialog(qApp->activeWindow()));
    Ui::ColorClip_UI dia_ui;
    dia_ui.setupUi(dia.data());
    dia->setAccessibleName(i18n("Color Clip Dialog"));
    dia->setWindowTitle(i18nc("@title:window", "Color Clip"));
    dia_ui.clip_name->setText(i18n("Color Clip"));
    dia_ui.clip_name->selectAll();
    dia_ui.clip_name->setAccessibleName(i18n("Clip Name"));

    int defaultDuration = pCore->getDurationFromString(KdenliveSettings::color_duration());
    int duration = suggestedDuration > 0 ? qMin(suggestedDuration, defaultDuration) : defaultDuration;
    dia_ui.clip_duration->setValue(duration);
    dia_ui.clip_color->setColor(KdenliveSettings::colorclipcolor());

    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui.clip_color->color().name();
        KdenliveSettings::setColorclipcolor(color);
        color = color.replace(0, 1, QStringLiteral("0x")) + "ff";
        int finalDuration = doc->getFramePos(doc->timecode().getTimecode(dia_ui.clip_duration->gentime()));
        QString name = dia_ui.clip_name->text();

        ClipCreator::createColorClip(color, finalDuration, name, parentFolder, std::move(model), readyCallBack);
    }
}

void ClipCreationDialog::createAnimationClip(KdenliveDoc *doc, const QString &parentId, const std::function<void(const QString &)> &readyCallBack,
                                             int suggestedDuration)
{
    if (!GlaxnimateLauncher::instance().checkInstalled()) {
        return;
    }
    QDir dir;
    QString path = KRecentDirs::dir(QStringLiteral(":KdenliveAnimationFolder"));
    if (path.isEmpty()) {
        dir = QDir(doc->projectDataFolder());
        if (!dir.cd("animations")) {
            dir.mkpath("animations");
            dir.cd("animations");
        }
    } else {
        dir = QDir(path);
    }
    QString fileName("animation-0001.rawr");
    int ix = 2;
    while (dir.exists(fileName) && ix < 9999) {
        QString number = QString::number(ix).rightJustified(4, '0');
        number.prepend(QStringLiteral("animation-"));
        number.append(QStringLiteral(".rawr"));
        fileName = number;
        ix++;
    }
    QDialog d(QApplication::activeWindow());
    d.setWindowTitle(i18n("Create animation"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    KUrlRequester fileUrl(&d);
    fileUrl.setAcceptMode(QFileDialog::AcceptSave);
    fileUrl.setMode(KFile::File);
    fileUrl.setUrl(QUrl::fromLocalFile(dir.absoluteFilePath(fileName)));
    l->addWidget(new QLabel(i18n("Save animation as"), &d));
    l->addWidget(&fileUrl);
    QHBoxLayout *lay = new QHBoxLayout;
    lay->addWidget(new QLabel(i18n("Animation duration"), &d));
    TimecodeDisplay tCode(&d);
    int defaultDuration = doc->getFramePos(doc->timecode().getTimecode(GenTime(5, doc->timecode().fps()))); // Default 5s
    int duration = suggestedDuration > 0 ? qMin(suggestedDuration, defaultDuration) : defaultDuration;
    tCode.setValue(doc->timecode().getTimecode(GenTime(duration, doc->timecode().fps())));
    lay->addWidget(&tCode);
    l->addLayout(lay);
    l->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    fileName = fileUrl.url().toLocalFile();
    if (QFile::exists(fileName)) {
        KIO::RenameDialog renameDialog(QApplication::activeWindow(), i18n("File already exists"), fileUrl.url(), fileUrl.url(),
                                       KIO::RenameDialog_Option::RenameDialog_Overwrite);
        if (renameDialog.exec() != QDialog::Rejected) {
            QUrl final = renameDialog.newDestUrl();
            if (final.isValid()) {
                fileName = final.toLocalFile();
            }
        } else {
            return;
        }
    }
    KRecentDirs::add(QStringLiteral(":KdenliveAnimationFolder"), QFileInfo(fileName).absolutePath());
    int frameLength = tCode.getValue() - 1;
    // Params: duration, framerate, width, height
    const QString templateRawr =
        QString(
            "{ \"animation\": { \"__type__\": \"MainComposition\", \"animation\": { \"__type__\": \"AnimationContainer\", \"first_frame\": 0, \"last_frame\": "
            "%1 }, \"fps\": %2, \"group_color\": \"#00000000\", \"height\": %4, \"locked\": false, \"name\": \"Animation\", \"shapes\": [ { \"__type__\": "
            "\"Layer\", \"animation\": { \"__type__\": \"AnimationContainer\", \"first_frame\": 0, \"last_frame\": %1 }, \"group_color\": \"#00000000\", "
            "\"locked\": false, \"mask\": { \"__type__\": \"MaskSettings\", \"inverted\": false, \"mask\": \"NoMask\" }, \"name\": \"Layer\", \"opacity\": { "
            "\"value\": 1 }, \"parent\": null, \"render\": true, \"shapes\": [ ], \"transform\": { \"__type__\": \"Transform\", \"anchor_point\": { \"value\": "
            "{ \"x\": 160, \"y\": 160 } }, \"position\": { \"value\": { \"x\": 160, \"y\": 160 } }, \"rotation\": { \"value\": 0 }, \"scale\": { \"value\": { "
            "\"x\": 1, \"y\": 1 } } }, \"uuid\": \"39cd3d4c-2a87-4af9-b52c-704f2c320aa3\", \"visible\": true } ], \"uuid\": "
            "\"b85ac6be-7935-45b9-9a62-7347d1cdf972\", \"visible\": true, \"width\": %3 }, \"assets\": { \"__type__\": \"Assets\", \"colors\": { \"__type__\": "
            "\"NamedColorList\", \"name\": \"\", \"uuid\": \"af0a82b1-daff-404d-9c91-aecdf1a7b1ba\", \"values\": [ ] }, \"fonts\": { \"__type__\": "
            "\"FontList\", \"name\": \"\", \"uuid\": \"af90956d-af34-4a81-9ab5-fa6d0520f96c\", \"values\": [ ] }, \"gradient_colors\": { \"__type__\": "
            "\"GradientColorsList\", \"name\": \"\", \"uuid\": \"eb84adf9-92de-44c5-a7c6-52e9390473c1\", \"values\": [ ] }, \"gradients\": { \"__type__\": "
            "\"GradientList\", \"name\": \"\", \"uuid\": \"096ffcf6-60a0-46e5-964e-752cb64a7607\", \"values\": [ ] }, \"images\": { \"__type__\": "
            "\"BitmapList\", \"name\": \"\", \"uuid\": \"333b9e0c-4825-4108-9de4-2a64f2dbb523\", \"values\": [ ] }, \"name\": \"\", \"precompositions\": { "
            "\"__type__\": \"PrecompositionList\", \"name\": \"\", \"uuid\": \"cca2c63d-5295-4ab2-ae32-72e58af6f3e0\", \"values\": [ ] }, \"uuid\": "
            "\"d7ac670a-6dc6-4ad9-8e3a-26348903a7e1\" }, \"format\": { \"format_version\": 7, \"generator\": \"Glaxnimate\", \"generator_version\": "
            "\"0.5.3-51-g110a1d77\" }, \"info\": { \"author\": \"\", \"description\": \"\", \"keywords\": [ ] }, \"metadata\": { } }")
            .arg(frameLength)
            .arg(QString::number(doc->timecode().fps()))
            .arg(pCore->getCurrentFrameSize().width())
            .arg(pCore->getCurrentFrameSize().height());
    /*const QString templateJson =
        QStringLiteral("{\"v\":\"5.7.1\",\"ip\":0,\"op\":%1,\"nm\":\"Animation\",\"mn\":\"{c9eac49f-b1f0-482f-a8d8-302293bd1e46}\",\"fr\":%2,\"w\":%3,\"h\":%4,"
                "\"assets\":[],\"layers\":[{\"ddd\":0,\"ty\":3,\"ind\":0,\"st\":0,\"ip\":0,\"op\":90,\"nm\":\"Layer\",\"mn\":\"{4d7c9721-b5ef-4075-a89c-"
                "c4c5629423db}\",\"ks\":{\"a\":{\"a\":0,\"k\":[960,540]},\"p\":{\"a\":0,\"k\":[960,540]},\"s\":{\"a\":0,\"k\":[100,100]},\"r\":{\"a\":0,\"k\":"
                "0},\"o\":{\"a\":0,\"k\":100}}}],\"meta\":{\"g\":\"Glaxnimate 0.5.0-52-g36d6269d\"}}")
            .arg(frameLength)
            .arg(QString::number(doc->timecode().fps()))
            .arg(pCore->getCurrentFrameSize().width())
            .arg(pCore->getCurrentFrameSize().height());*/
    QFile file(fileName);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << templateRawr;
    file.close();
    GlaxnimateLauncher::instance().openFile(fileName);
    // Add clip to project
    QDomDocument xml;
    QDomElement prod = xml.createElement(QStringLiteral("producer"));
    xml.appendChild(prod);
    prod.setAttribute(QStringLiteral("type"), int(ClipType::Animation));
    int id = pCore->projectItemModel()->getFreeClipId();
    prod.setAttribute(QStringLiteral("id"), QString::number(id));
    QMap<QString, QString> properties;
    if (!parentId.isEmpty()) {
        properties.insert(QStringLiteral("kdenlive:folderid"), parentId);
    }
    properties.insert(QStringLiteral("mlt_service"), QStringLiteral("glaxnimate"));
    properties.insert(QStringLiteral("resource"), fileName);
    Xml::addXmlProperties(prod, properties);
    QString clipId = QString::number(id);
    pCore->projectItemModel()->requestAddBinClip(clipId, xml.documentElement(), parentId, i18n("Create Animation clip"), readyCallBack);
}

const QString ClipCreationDialog::createPlaylistClip(const QString &name, std::pair<int, int> tracks, const QString &parentFolder,
                                                     std::shared_ptr<ProjectItemModel> model, const std::function<void(const QString &)> &readyCallBack,
                                                     int suggestedDuration)
{
    return ClipCreator::createPlaylistClip(name, tracks, parentFolder, model, readyCallBack);
}

void ClipCreationDialog::createQTextClip(const QString &parentId, Bin *bin, ProjectClip *clip, const std::function<void(const QString &)> &readyCallBack,
                                         int suggestedDuration)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");
    QScopedPointer<QDialog> dia(new QDialog(bin));
    Ui::QTextClip_UI dia_ui;
    dia_ui.setupUi(dia.data());
    dia->setWindowTitle(i18nc("@title:window", "Text Clip"));
    dia_ui.fgColor->setAlphaChannelEnabled(true);
    dia_ui.lineColor->setAlphaChannelEnabled(true);
    dia_ui.bgColor->setAlphaChannelEnabled(true);
    if (clip) {
        dia_ui.name->setText(clip->clipName());
        dia_ui.text->setPlainText(clip->getProducerProperty(QStringLiteral("text")));
        dia_ui.fgColor->setColor(QColorUtils::stringToColor(clip->getProducerProperty(QStringLiteral("fgcolour"))));
        dia_ui.bgColor->setColor(QColorUtils::stringToColor(clip->getProducerProperty(QStringLiteral("bgcolour"))));
        dia_ui.pad->setValue(clip->getProducerProperty(QStringLiteral("pad")).toInt());
        dia_ui.lineColor->setColor(QColorUtils::stringToColor(clip->getProducerProperty(QStringLiteral("olcolour"))));
        dia_ui.lineWidth->setValue(clip->getProducerProperty(QStringLiteral("outline")).toInt());
        dia_ui.font->setCurrentFont(QFont(clip->getProducerProperty(QStringLiteral("family"))));
        dia_ui.fontSize->setValue(clip->getProducerProperty(QStringLiteral("size")).toInt());
        dia_ui.weight->setValue(clip->getProducerProperty(QStringLiteral("weight")).toInt());
        dia_ui.italic->setChecked(clip->getProducerProperty(QStringLiteral("style")) == QStringLiteral("italic"));
        dia_ui.duration->setValue(clip->frameDuration());
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
        int defaultDuration =
            pCore->currentDoc()->getFramePos(pCore->currentDoc()->timecode().getTimecode(GenTime(5, pCore->currentDoc()->timecode().fps()))); // Default 5s
        int duration = suggestedDuration > 0 ? qMin(suggestedDuration, defaultDuration) : defaultDuration;
        dia_ui.duration->setValue(duration);
    }
    if (dia->exec() == QDialog::Accepted) {
        // KdenliveSettings::setColorclipcolor(color);
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("kdenlive:clipname"), dia_ui.name->text());
        if (!parentId.isEmpty()) {
            properties.insert(QStringLiteral("kdenlive:folderid"), parentId);
        }
        int newDuration = dia_ui.duration->getValue();
        if (clip && (newDuration != clip->getFramePlaytime())) {
            // duration changed, we need to update duration
            properties.insert(QStringLiteral("out"), clip->framesToTime(newDuration - 1));
            int currentLength = clip->getProducerDuration();
            if (currentLength != newDuration) {
                properties.insert(QStringLiteral("kdenlive:duration"), clip->framesToTime(newDuration));
                properties.insert(QStringLiteral("length"), QString::number(newDuration));
            }
        }

        properties.insert(QStringLiteral("mlt_service"), QStringLiteral("qtext"));
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
            bin->slotEditClipCommand(clip->AbstractProjectItem::clipId(), clip->currentProperties(properties), properties);
        } else {
            QDomDocument xml;
            QDomElement prod = xml.createElement(QStringLiteral("producer"));
            xml.appendChild(prod);
            prod.setAttribute(QStringLiteral("type"), int(ClipType::QText));
            int id = pCore->projectItemModel()->getFreeClipId();
            prod.setAttribute(QStringLiteral("id"), QString::number(id));

            prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
            prod.setAttribute(QStringLiteral("out"), newDuration);
            Xml::addXmlProperties(prod, properties);
            QString clipId = QString::number(id);
            pCore->projectItemModel()->requestAddBinClip(clipId, xml.documentElement(), parentId, i18n("Create Text clip"), readyCallBack);
        }
    }
}

// static
void ClipCreationDialog::createSlideshowClip(KdenliveDoc *doc, const QString &parentId, std::shared_ptr<ProjectItemModel> model,
                                             const std::function<void(const QString &)> &readyCallBack, int suggestedDuration)
{
    QScopedPointer<SlideshowClip> dia(
        new SlideshowClip(doc->timecode(), KRecentDirs::dir(QStringLiteral(":KdenliveSlideShowFolder")), nullptr, QApplication::activeWindow()));

    if (dia->exec() == QDialog::Accepted) {
        // Ready, create xml
        KRecentDirs::add(QStringLiteral(":KdenliveSlideShowFolder"), QUrl::fromLocalFile(dia->selectedPath()).adjusted(QUrl::RemoveFilename).toLocalFile());
        KdenliveSettings::setSlideshowmimeextension(dia->extension());
        std::unordered_map<QString, QString> properties;
        properties[QStringLiteral("ttl")] = QString::number(doc->getFramePos(dia->clipDuration()));
        properties[QStringLiteral("loop")] = QString::number(static_cast<int>(dia->loop()));
        properties[QStringLiteral("crop")] = QString::number(static_cast<int>(dia->crop()));
        properties[QStringLiteral("fade")] = QString::number(static_cast<int>(dia->fade()));
        properties[QStringLiteral("luma_duration")] = QString::number(doc->getFramePos(dia->lumaDuration()));
        properties[QStringLiteral("luma_file")] = dia->lumaFile();
        properties[QStringLiteral("softness")] = QString::number(dia->softness());
        properties[QStringLiteral("animation")] = dia->animation();
        properties[QStringLiteral("low-pass")] = QString::number(dia->lowPass());
        int duration = doc->getFramePos(dia->clipDuration()) * dia->imageCount();
        ClipCreator::createSlideshowClip(dia->selectedPath(), duration, dia->clipName(), parentId, properties, model, readyCallBack);
    }
}

void ClipCreationDialog::createTitleClip(KdenliveDoc *doc, const QString &parentFolder, const QString &templatePath, std::shared_ptr<ProjectItemModel> model,
                                         const std::function<void(const QString &)> &readyCallBack, int suggestedDuration)
{
    // Make sure the titles folder exists
    QDir dir(doc->projectDataFolder() + QStringLiteral("/titles"));
    dir.mkpath(QStringLiteral("."));
    QPointer<TitleWidget> dia_ui =
        new TitleWidget(QUrl::fromLocalFile(templatePath), dir.absolutePath(), pCore->getMonitor(Kdenlive::ProjectMonitor), pCore->bin());
    if (suggestedDuration > 0) {
        int duration = qMin(suggestedDuration, doc->getFramePos(KdenliveSettings::title_duration()));
        dia_ui->setDuration(duration);
    }
    if (dia_ui->exec() == QDialog::Accepted) {
        // Ready, create clip xml
        std::unordered_map<QString, QString> properties;
        properties[QStringLiteral("xmldata")] = dia_ui->xml().toString();
        QString titleSuggestion = dia_ui->titleSuggest();

        ClipCreator::createTitleClip(properties, dia_ui->duration(), titleSuggestion.isEmpty() ? i18n("Title clip") : titleSuggestion, parentFolder,
                                     std::move(model), readyCallBack);
    }
    delete dia_ui;
}

void ClipCreationDialog::createTitleTemplateClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model,
                                                 const std::function<void(const QString &)> &readyCallBack, int suggestedDuration)
{

    QScopedPointer<TitleTemplateDialog> dia(new TitleTemplateDialog(doc->projectDataFolder(), QApplication::activeWindow()));

    if (dia->exec() == QDialog::Accepted) {
        QString templateClipName = dia->selectedText().simplified();
        while (templateClipName.length() > 28 && templateClipName.contains(QLatin1Char(' '))) {
            templateClipName = templateClipName.section(QLatin1Char(' '), 0, -2);
        }
        if (templateClipName.length() > 28) {
            templateClipName.resize(25);
            templateClipName.append(QStringLiteral("…"));
        }
        if (templateClipName.isEmpty()) {
            templateClipName = i18n("Template title clip");
        }
        ClipCreator::createTitleTemplate(dia->selectedTemplate(), dia->selectedText(), templateClipName, parentFolder, std::move(model), readyCallBack,
                                         suggestedDuration);
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
KGuiItem(i18n("Continue")), KStandardGuiItem::cancel(), QStringLiteral("copyFilesToProjectFolder"));

            if (answer == KMessageBox::Cancel) continue;
            else if (answer == KMessageBox::Yes) {
                // Copy files to project folder
                QDir sourcesFolder(m_doc->projectFolder().toLocalFile());
                sourcesFolder.cd("clips");
                KIO::MkdirJob *mkdirJob = KIO::mkdir(QUrl::fromLocalFile(sourcesFolder.absolutePath()));
                KJobWidgets::setWindow(mkdirJob, QApplication::activeWindow());
                if (!mkdirJob->exec()) {
                    KMessageBox::error(QApplication::activeWindow(), i18n("Cannot create directory %1", sourcesFolder.absolutePath()));
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

void ClipCreationDialog::createClipsCommand(KdenliveDoc *doc, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model,
                                            const std::function<void(const QString &)> &readyCallBack, int suggestedDuration)
{
    qDebug() << "/////////// starting to add bin clips";
    QList<QUrl> list;
    QCheckBox *b = new QCheckBox(i18n("Import image sequence"));
    b->setToolTip(i18n("Try to import an image sequence"));
    b->setWhatsThis(
        xi18nc("@info:whatsthis", "When enabled, Kdenlive will look for other images with the same name pattern and import them as an image sequence."));
    b->setChecked(KdenliveSettings::autoimagesequence());
    QCheckBox *bf = new QCheckBox(i18n("Ignore subfolder structure"));
    bf->setChecked(KdenliveSettings::ignoresubdirstructure());
    bf->setToolTip(i18n("Do not create subfolders in Project Bin"));
    bf->setWhatsThis(
        xi18nc("@info:whatsthis",
               "When enabled, Kdenlive will import all clips contained in the folder and its subfolders without creating the subfolders in Project Bin."));
    QFrame *f = new QFrame();
    f->setFrameShape(QFrame::NoFrame);
    auto *l = new QHBoxLayout;
    l->addWidget(b);
    l->addWidget(bf);
    l->addStretch(5);
    f->setLayout(l);
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    if (!QFileInfo::exists(clipFolder)) {
        clipFolder = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    }
    QScopedPointer<QDialog> dlg(new QDialog(static_cast<QWidget *>(doc->parent())));
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

    auto dialogFilter = FileFilter::Builder().defaultCategories().toKFilter();
    fileWidget->setFilters(dialogFilter);
    fileWidget->setMode(KFile::Files | KFile::ExistingOnly | KFile::LocalOnly | KFile::Directory);
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    dlg->winId(); // Make sure window gets created before getting the handle
    QWindow *handle = dlg->windowHandle();
    if ((handle != nullptr) && conf->hasGroup("FileDialogSize")) {
        KWindowConfig::restoreWindowSize(handle, conf->group("FileDialogSize"));
        dlg->resize(handle->size());
    }
    int result = dlg->exec();
    if (result == QDialog::Accepted) {
        KdenliveSettings::setAutoimagesequence(b->isChecked());
        KdenliveSettings::setIgnoresubdirstructure(bf->isChecked());
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
                        ClipCreator::createSlideshowClip(pattern, frame_duration, fileName, parentFolder, properties, model, readyCallBack);
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
    const QString id = ClipCreator::createClipsFromList(list, true, parentFolder, model, undo, redo, readyCallBack);

    // We reset the state of the "don't ask again" for the question about removable devices
    KMessageBox::enableMessage(QStringLiteral("removable"));
    if (!id.isEmpty()) {
        pCore->pushUndo(undo, redo, i18np("Add clip", "Add clips", list.size()));
    }
}
