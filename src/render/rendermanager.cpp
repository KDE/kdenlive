#include "rendermanager.h"

#include "doc/kdenlivedoc.h"
#include "renderpresets/renderpresetmodel.hpp"
#include "utils/qstringutils.h"
#include "xml/xml.hpp"

#include <KLocalizedString>
#include <KNotification>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QUndoGroup>
#include <QUrl>

// TODO: remove!!!
#include "core.h"
#include "doc/docundostack.hpp"
#include "kdenlivesettings.h"

RenderManager::RenderManager()
    : m_useProxies(false)
    , m_in(0)
    , m_out()
    , m_renderer("./kdenlive_render")
{
}

#if 0
void RenderManager::prepareRendering()
{
    KdenliveDoc *project = pCore->currentDoc();

    QString playlistContent =
        pCore->projectManager()->projectSceneList(project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile(), overlayData);
    QDomDocument doc;
    doc.setContent(playlistContent);

    prepareRendering(project);
}
#endif

void RenderManager::prepareRendering(const QUrl &openUrl)
{
    QUndoGroup *undoGroup = new QUndoGroup();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    undoGroup->addStack(undoStack.get());
    DocOpenResult openResults = KdenliveDoc::Open(openUrl, QDir::temp().path(), undoGroup, false, nullptr);
    std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

    QDomDocument doc;
    doc.setContent(openedDoc.get()->getAndClearProjectXml());

    prepareRendering(openedDoc.get(), doc);
}

void RenderManager::prepareRendering(KdenliveDoc *project, const QDomDocument &document)
{
    QDomDocument doc = document.cloneNode(true).toDocument();
#if 1
    // QString overlayData = m_view.tc_type->currentData().toString();
    RenderJob job;
    job.playlistPath = createEmptyTempFile(QStringLiteral("mlt")); // generatePlaylistFile(job.delayedRendering);
    qDebug() << "playlistPath:" << job.playlistPath;

    if (job.playlistPath.isEmpty()) {
        return;
    }
    // On delayed rendering, make a copy of all assets
    if (job.delayedRendering) {
        createAssetBundle(job, project);
    }

    // Add autoclose to playlists.
    setAutoClosePlaylists(doc);

    // Do we want proxy rendering?
    if (project->useProxy() && !m_useProxies) {
        KdenliveDoc::useOriginals(doc);
    }

    if (job.embedSubtitle && project->hasSubtitles()) {
        // disable subtitle filter(s) as they will be embeded in a second step of rendering
        disableSubtitles(doc);
    }
    const QUuid currentUuid = pCore->currentTimelineId();

    /*
     double fps = pCore->getCurrentProfile()->fps();
    // in/out points
    int in = 0;
    // Remove last black frame
    int out = pCore->projectDuration() - 1;

    Monitor *pMon = pCore->getMonitor(Kdenlive::ProjectMonitor);
    if (m_view.render_zone->isChecked()) {
        in = pMon->getZoneStart();
        out = pMon->getZoneEnd() - 1;
    } else if (m_view.render_guide->isChecked()) {
        double guideStart = m_view.guide_start->itemData(m_view.guide_start->currentIndex()).toDouble();
        double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
        in = int(GenTime(qMin(guideStart, guideEnd)).frames(fps));
        // End rendering at frame before last guide
        out = int(GenTime(qMax(guideStart, guideEnd)).frames(fps)) - 1;

    }*/ // else: full project

    std::vector<RenderSection> sections;

    if (m_guideMultiExport) {
        int guideCategory = -1;
        sections = getGuideSections(guideCategory, m_in, m_out);
    }

    if (sections.empty()) {
        RenderSection section;
        section.in = m_in;
        section.out = m_out;
        sections.push_back(section);
    }

    job.outputPath = QStringLiteral("/home/julius/Videos/automated.mp4");

    for (auto section : sections) {
        RenderJob newJob = job;
        if (!section.name.isEmpty()) {
            newJob.outputPath = QStringUtils::appendToFilename(newJob.outputPath, QStringLiteral("-%1.").arg(section.name));
        }

        QDomDocument docCopy = doc.cloneNode(true).toDocument();
        if (!newJob.subtitlePath.isEmpty()) {
            if (!QFileInfo(newJob.subtitlePath).exists()) {
                newJob.subtitlePath = createEmptyTempFile(QStringLiteral("srt"));
            }
            project->generateRenderSubtitleFile(currentUuid, section.in, section.out, newJob.subtitlePath);
        }

        newJob.playlistPath = createEmptyTempFile(QStringLiteral("mlt")); // newJob.playlistPath.replace(QStringLiteral(".mlt"), QString("-%1.mlt").arg(i));
        QFile::copy(job.playlistPath, newJob.playlistPath); // TODO: if we have a single item in sections this is unnesessary and produces just unused files

        generateRenderFiles(newJob, docCopy, section.in, section.out);
    }
#endif
}

std::vector<RenderManager::RenderSection> RenderManager::getGuideSections(int guideCategory, int boundingIn, int boundingOut)
{
    std::vector<RenderSection> sections;
    if (auto ptr = m_guidesModel.lock()) {
        QList<CommentedTime> markers;
        double fps = pCore->getCurrentFps();
        for (auto marker : ptr->getAllMarkers(guideCategory)) {
            int pos = marker.time().frames(fps);
            if (pos < boundingIn || (boundingOut > 0 && pos > boundingOut)) continue;
            markers << marker;
        }
        if (!markers.isEmpty()) {
            bool beginParsed = false;
            QStringList names;
            for (int i = 0; i < markers.count(); i++) {
                RenderSection section;
                if (!beginParsed && i == 0 && markers.at(i).time().frames(fps) != boundingIn) {
                    i -= 1;
                    beginParsed = true;
                    section.name = i18n("begin");
                }
                section.in = boundingIn;
                if (i >= 0) {
                    section.name = markers.at(i).comment();
                    section.in = markers.at(i).time().frames(fps);
                }
                section.name = QStringUtils::getUniqueName(names, section.name);
                names << section.name;

                section.out = boundingOut;
                if (i + 1 < markers.count()) {
                    section.out = qMin(markers.at(i + 1).time().frames(fps) - 1, boundingOut);
                }

                sections.push_back(section);
            }
        }
    }
    return sections;
}

void RenderManager::createAssetBundle(const RenderJob &job, KdenliveDoc *project)
{
    // On delayed rendering, make a copy of all assets
    QDir dir = QFileInfo(job.playlistPath).absoluteDir();
    if (!dir.mkpath(QFileInfo(job.playlistPath).baseName())) {
        // Q_EMIT showMessage(i18n("Could not create assets folder:\n %1", dir.absoluteFilePath(QFileInfo(playlistPath).baseName())));
        // KMessageBox::error(this, );
        return;
    }
    dir.cd(QFileInfo(job.playlistPath).baseName());
    project->prepareRenderAssets(dir);
    project->restoreRenderAssets();
}

void RenderManager::generateRenderFiles(RenderJob job, QDomDocument doc, int in, int out)
{
    qDebug() << "outputPath:" << job.outputPath;
    QString extension = job.outputPath.section(QLatin1Char('.'), -1);
    QDomElement consumer = doc.createElement(QStringLiteral("consumer"));
    consumer.setAttribute(QStringLiteral("in"), in);
    consumer.setAttribute(QStringLiteral("out"), out);
    consumer.setAttribute(QStringLiteral("mlt_service"), QStringLiteral("avformat"));

    QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
    if (profiles.isEmpty()) {
        doc.documentElement().insertAfter(consumer, doc.documentElement());
    } else {
        doc.documentElement().insertAfter(consumer, profiles.at(profiles.length() - 1));
    }

    RenderPresetParams m_params;
    QMapIterator<QString, QString> it(m_params);

    while (it.hasNext()) {
        it.next();
        // insert params from preset.
        consumer.setAttribute(it.key(), it.value());
    }

    // If we use a pix_fmt with alpha channel (ie. transparent),
    // we need to remove the black background track
    if (m_params.hasAlpha()) {
        auto prods = doc.elementsByTagName(QStringLiteral("producer"));
        for (int i = 0; i < prods.count(); ++i) {
            auto prod = prods.at(i).toElement();
            if (Xml::getXmlProperty(prod, QStringLiteral("kdenlive:playlistid")) == QStringLiteral("black_track")) {
                Xml::setXmlProperty(prod, QStringLiteral("resource"), QStringLiteral("transparent"));
                break;
            }
        }
    }

    if (m_params.isImageSequence()) {
        // Image sequence, ensure we have a %0xd at file end.
        // Format string for counter
        static const QRegularExpression rx(QRegularExpression::anchoredPattern(QStringLiteral(".*%[0-9]*d.*")));
        if (!rx.match(job.outputPath).hasMatch()) {
            job.outputPath = QStringUtils::appendToFilename(job.outputPath, QStringLiteral("_%05d."));
        }
    }

    // if (m_view.stemAudioExport->isChecked() && m_view.stemAudioExport->isEnabled()) {
    if (m_multiAudioExport) {
        if (job.delayedRendering) {
            // Q_EMIT showMessage(i18n("Script rendering and multi track audio export can not be used together.\n"
            //                         "Script will be saved without multi track export."));
            /*if (KMessageBox::warningContinueCancel(this, i18n("Script rendering and multi track audio export can not be used together.\n"
                                                              "Script will be saved without multi track export.")) == KMessageBox::Cancel) {
                return;
            };*/
        }
        prepareMultiAudioFiles(m_renderFiles, doc, job);
    }

    QDomDocument clone;
    // int passes = m_view.checkTwoPass->isChecked() ? 2 : 1;
    int passes = m_twoPassRendering ? 2 : 1;
    if (passes == 2) {
        // We will generate 2 files, one for each pass.
        clone = doc.cloneNode(true).toDocument();
    }

    for (int i = 0; i < passes; i++) {
        // Append consumer settings
        QDomDocument final = i > 0 ? clone : doc;
        QDomNodeList consList = final.elementsByTagName(QStringLiteral("consumer"));
        QDomElement finalConsumer = consList.at(0).toElement();
        finalConsumer.setAttribute(QStringLiteral("target"), job.outputPath);
        int pass = passes == 2 ? i + 1 : 0;
        QString playlistName = job.playlistPath;
        if (pass == 2) {
            playlistName = playlistName.section(QLatin1Char('.'), 0, -2) + QStringLiteral("-pass%1.").arg(2) + extension;
        }
        m_renderFiles.insert(playlistName, job.outputPath);

        // Prepare rendering args
        QString logFile = QStringLiteral("%1_2pass.log").arg(job.outputPath);
        if (m_params.value(QStringLiteral("vcodec")).toLower() == QStringLiteral("libx265")) {
            if (pass == 1 || pass == 2) {
                QString x265params = finalConsumer.attribute("x265-params");
                x265params = QString("pass=%1:stats=%2:%3").arg(pass).arg(logFile.replace(":", "\\:"), x265params);
                finalConsumer.setAttribute("x265-params", x265params);
            }
        } else {
            if (pass == 1 || pass == 2) {
                finalConsumer.setAttribute("pass", pass);
                finalConsumer.setAttribute("passlogfile", logFile);
            }
            if (pass == 1) {
                finalConsumer.setAttribute("fastfirstpass", 1);
                finalConsumer.removeAttribute("acodec");
                finalConsumer.setAttribute("an", 1);
            } else {
                finalConsumer.removeAttribute("fastfirstpass");
            }
        }
        writeToFile(final, playlistName);
    }

    // startRendering()

    // Create jobs
    /*if (delayedRendering) {
        parseScriptFiles();
        return;
    }*/
    /*QList<RenderJobItem *> jobList;
    QMap<QString, QString>::const_iterator i = renderFiles.constBegin();
    while (i != renderFiles.constEnd()) {
        RenderJobItem *renderItem = createRenderJob(i.key(), i.value(), subtitleFile);
        if (renderItem != nullptr) {
            jobList << renderItem;
        }
        ++i;
    }
    if (jobList.count() > 0) {
        m_view.running_jobs->setCurrentItem(jobList.at(0));
    }
    m_view.tabWidget->setCurrentIndex(Tabs::JobsTab);
    // check render status
    checkRenderStatus();
    */
}

bool RenderManager::startRendering(RenderJob job)
{
#if 1
    QStringList argsJob = {QStringLiteral("delivery"), KdenliveSettings::rendererpath(), job.playlistPath, QStringLiteral("--pid"),
                           QString::number(QCoreApplication::applicationPid())};
    if (job.embedSubtitle && !job.subtitlePath.isEmpty()) {
        argsJob << QStringLiteral("--subtitle") << job.subtitlePath;
    }
    qDebug() << "* CREATED JOB WITH ARGS: " << argsJob;

    qDebug() << "starting kdenlive_render process using: " << m_renderer;
    if (!QProcess::startDetached(m_renderer, argsJob)) {
        return false;
    } else {
        KNotification::event(QStringLiteral("RenderStarted"), i18n("Rendering <i>%1</i> started", job.outputPath), QPixmap());
    }
#endif
    return true;
}

void RenderManager::disableSubtitles(QDomDocument &doc)
{
    QDomNodeList filters = doc.elementsByTagName(QStringLiteral("filter"));
    for (int i = 0; i < filters.length(); ++i) {
        if (Xml::getXmlProperty(filters.item(i).toElement(), QStringLiteral("mlt_service")) == QLatin1String("avfilter.subtitles")) {
            Xml::setXmlProperty(filters.item(i).toElement(), QStringLiteral("disable"), QStringLiteral("1"));
        }
    }
}

void RenderManager::setAutoClosePlaylists(QDomDocument &doc)
{
    QDomNodeList playlists = doc.elementsByTagName(QStringLiteral("playlist"));
    for (int i = 0; i < playlists.length(); ++i) {
        playlists.item(i).toElement().setAttribute(QStringLiteral("autoclose"), 1);
    }
}

/*QString RenderManager::generatePlaylistFile(bool delayedRendering)
{
    if (delayedRendering) {
        bool ok;
        QString filename = QFileInfo(pCore->currentDoc()->url().toLocalFile()).fileName();
        const QString fileExtension = QStringLiteral(".mlt");
        if (filename.isEmpty()) {
            filename = i18n("export");
        } else {
            filename = filename.section(QLatin1Char('.'), 0, -2);
        }

        QDir projectFolder(pCore->currentDoc()->projectDataFolder());
        projectFolder.mkpath(QStringLiteral("kdenlive-renderqueue"));
        projectFolder.cd(QStringLiteral("kdenlive-renderqueue"));
        int ix = 1;
        QString newFilename = filename;
        // if name alrady exist, add a suffix
        while (projectFolder.exists(newFilename + fileExtension)) {
            newFilename = QStringLiteral("%1-%2").arg(filename).arg(ix);
            ix++;
        }
        filename = newFilename;
        /*filename = QInputDialog::getText(this, i18nc("@title:window", "Delayed Rendering"), i18n("Select a name for this rendering."), QLineEdit::Normal,
                                         newFilename, &ok);
        if (!ok) {
            return {};
        }
        if (!filename.endsWith(fileExtension)) {
            filename.append(fileExtension);
        }
        if (projectFolder.exists(newFilename)) {
            if (KMessageBox::questionTwoActions(this, i18n("File %1 already exists.\nDo you want to overwrite it?", filename), {},
                                                KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) == KMessageBox::PrimaryAction) {
                return {};
            }
        }*//*
        return projectFolder.absoluteFilePath(filename);
    }
    // No delayed rendering, we can use a temp file
    QTemporaryFile tmp(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
    if (!tmp.open()) {
        // Something went wrong
        return {};
    }
    tmp.close();
    return tmp.fileName();
}*/

QString RenderManager::createEmptyTempFile(const QString &extension)
{
    QTemporaryFile tmp(QDir::temp().absoluteFilePath(QString("kdenlive-XXXXXX.%1").arg(extension)));
    if (!tmp.open()) {
        // Something went wrong
        // Q_EMIT showMessage(i18n("Could not create temporary file"));
        // KMessageBox::error(this, i18n("Could not create temporary file"));
        return {};
    }
    tmp.close();
    tmp.setAutoRemove(false);

    return tmp.fileName();
}

void RenderManager::prepareMultiAudioFiles(QMap<QString, QString> &renderFiles, const QDomDocument &doc, RenderJob job)
{
    int audioCount = 0;
    QDomNodeList orginalTractors = doc.elementsByTagName(QStringLiteral("tractor"));
    // process in reversed order to make file naming fit to UI
    for (int i = orginalTractors.size(); i >= 0; i--) {
        bool isAudio = Xml::getXmlProperty(orginalTractors.at(i).toElement(), QStringLiteral("kdenlive:audio_track")).toInt() == 1;
        QString trackName = Xml::getXmlProperty(orginalTractors.at(i).toElement(), QStringLiteral("kdenlive:track_name"));

        if (!isAudio) {
            continue;
        }

        // setup filenames
        QString appendix = QString("_Audio_%1%2%3.")
                               .arg(audioCount + 1)
                               .arg(trackName.isEmpty() ? QString() : QStringLiteral("-"))
                               .arg(trackName.replace(QStringLiteral(" "), QStringLiteral("_")));
        QString playlistFile = QStringUtils::appendToFilename(job.playlistPath, appendix);
        QString targetFile = QStringUtils::appendToFilename(job.playlistPath, appendix);
        renderFiles.insert(playlistFile, targetFile);

        // init doc copy
        QDomDocument docCopy = doc.cloneNode(true).toDocument();
        QDomElement consumer = docCopy.elementsByTagName(QStringLiteral("consumer")).at(0).toElement();
        consumer.setAttribute(QStringLiteral("target"), targetFile);

        QDomNodeList tracktors = docCopy.elementsByTagName(QStringLiteral("tractor"));
        // the last tractor is the main tracktor, don't process it (-1)
        for (int j = 0; j < tracktors.size() - 1; j++) {
            QDomNodeList tracks = tracktors.at(j).toElement().elementsByTagName(QStringLiteral("track"));
            for (int l = 0; l < tracks.size(); l++) {
                if (i != j) {
                    tracks.at(l).toElement().setAttribute(QStringLiteral("hide"), QStringLiteral("both"));
                }
            }
        }
        writeToFile(docCopy, playlistFile);
        audioCount++;
    }
}

bool RenderManager::writeToFile(const QDomDocument &doc, const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        pCore->displayMessage(i18n("Cannot write to file %1", filename), ErrorMessage);
        return false;
    }
    file.write(doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        pCore->displayMessage(i18n("Cannot write to file %1", filename), ErrorMessage);
        file.close();
        return false;
    }
    file.close();
    return true;
}
