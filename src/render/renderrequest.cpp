/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderrequest.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"
#include "utils/qstringutils.h"
#include "xml/xml.hpp"

#include "utils/KMessageBox_KdenliveCompat.h"

#include <QTemporaryFile>

// TODO: remove, see generatePlaylistFile()
#include <KMessageBox>
#include <QInputDialog>

// TODO:
#include "doc/docundostack.hpp"
#include <QUndoGroup>

RenderRequest::RenderRequest()
{
    setBounds(-1, -1);
}

void RenderRequest::setBounds(int in, int out)
{
    m_boundingIn = qMax(0, in);
    if (out < 0 || out > pCore->projectDuration() - 1) {
        // Remove last black frame
        out = pCore->projectDuration() - 1;
    }
    m_boundingOut = out;
}

void RenderRequest::setOutputFile(const QString &filename)
{
    m_outputFile = filename;
}

void RenderRequest::setPresetParams(const RenderPresetParams &params)
{
    m_presetParams = params;
}

void RenderRequest::setDelayedRendering(bool enabled)
{
    m_delayedRendering = enabled;
}

void RenderRequest::setProxyRendering(bool enabled)
{
    m_proxyRendering = enabled;
}

void RenderRequest::setEmbedSubtitles(bool enabled)
{
    m_embedSubtitles = enabled;
}

void RenderRequest::setTwoPass(bool enabled)
{
    m_twoPass = enabled;
}

void RenderRequest::setAudioFilePerTrack(bool enabled)
{
    m_audioFilePerTrack = enabled;
}

void RenderRequest::setGuideParams(std::weak_ptr<MarkerListModel> model, bool enableMultiExport, int filterCategory)
{
    m_guidesModel = std::move(model);
    m_guideMultiExport = enableMultiExport;
    m_guideCategory = filterCategory;
}

void RenderRequest::setOverlayData(const QString &data)
{
    m_overlayData = data;
}

std::vector<RenderRequest::RenderJob> RenderRequest::process(const QUrl &openUrl)
{
    m_errors.clear();

    QString playlistPath = generatePlaylistFile();
    if (playlistPath.isEmpty()) {
        return {};
    }

    bool fromUrl = !openUrl.isEmpty();
    KdenliveDoc *project;

    QDomDocument doc;
    if (fromUrl) {
        QUndoGroup *undoGroup = new QUndoGroup();
        std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openUrl, QDir::temp().path(), undoGroup, false, nullptr);
        if (openResults.isAborted()) {
            return {};
        }
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        doc.setContent(openedDoc.get()->getAndClearProjectXml());
        project = openedDoc.release();
    } else {
        project = pCore->currentDoc();
    }

    // On delayed rendering, make a copy of all assets
    if (m_delayedRendering) {
        QDir dir = QFileInfo(playlistPath).absoluteDir();
        if (!dir.mkpath(QFileInfo(playlistPath).baseName())) {
            addErrorMessage(i18n("Could not create assets folder:\n %1", dir.absoluteFilePath(QFileInfo(playlistPath).baseName())));
            return {};
        }
        dir.cd(QFileInfo(playlistPath).baseName());
        project->prepareRenderAssets(dir);
    }
    if (!fromUrl) {
        QString playlistContent =
            pCore->projectManager()->projectSceneList(project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile(), m_overlayData);

        doc.setContent(playlistContent);
    }

    if (m_delayedRendering) {
        project->restoreRenderAssets();
    }

    // Add autoclose to playlists
    KdenliveDoc::setAutoclosePlaylists(doc);

    // Do we want proxy rendering
    if (!m_proxyRendering && project->useProxy()) {
        KdenliveDoc::useOriginals(doc);
    }

    if (m_embedSubtitles && project->hasSubtitles()) {
        // disable subtitle filter(s) as they will be embeded in a second step of rendering
        KdenliveDoc::disableSubtitles(doc);
    }

    // If we use a pix_fmt with alpha channel (ie. transparent),
    // we need to remove the black background track
    if (m_presetParams.hasAlpha()) {
        KdenliveDoc::makeBackgroundTrackTransparent(doc);
    }

    std::vector<RenderSection> sections;

    if (m_guideMultiExport) {
        sections = getGuideSections();
    }

    if (sections.empty()) {
        RenderSection section;
        section.in = m_boundingIn;
        section.out = m_boundingOut;
        sections.push_back(section);
    }

    const QUuid currentUuid = pCore->currentTimelineId();

    int i = 0;

    std::vector<RenderJob> jobs;
    for (const auto &section : sections) {
        i++;
        QString outputPath = m_outputFile;
        if (!section.name.isEmpty()) {
            outputPath = QStringUtils::appendToFilename(outputPath, QStringLiteral("-%1.").arg(section.name));
        }
        QDomDocument sectionDoc = doc.cloneNode(true).toDocument();

        QString subtitleFile;
        if (m_embedSubtitles) {
            subtitleFile = createEmptyTempFile(QStringLiteral("srt"));
            if (subtitleFile.isEmpty()) {
                addErrorMessage(i18n("Could not create temporary subtitle file"));
                return {};
            }
            project->generateRenderSubtitleFile(currentUuid, section.in, section.out, subtitleFile);
        }

        QString newPlaylistPath = playlistPath;
        newPlaylistPath = newPlaylistPath.replace(QStringLiteral(".mlt"), QString("-%1.mlt").arg(i));
        // QString newPlaylistPath = createEmptyTempFile(QStringLiteral("mlt")); // !!! This does not take the delayed rendering logic of generatePlaylistFile()
        // in to account

        // QFile::copy(job.playlistPath, newJob.playlistPath); // TODO: if we have a single item in sections this is unnesessary and produces just unused files

        // set parameters
        setDocGeneralParams(sectionDoc, section.in, section.out);

        createRenderJobs(jobs, sectionDoc, newPlaylistPath, outputPath, subtitleFile);
    }

    return jobs;
}

void RenderRequest::createRenderJobs(std::vector<RenderJob> &jobs, const QDomDocument &doc, const QString &playlistPath, QString outputPath,
                                     const QString &subtitlePath)
{
    if (m_audioFilePerTrack) {
        if (m_delayedRendering) {
            addErrorMessage(i18n("Script rendering and multi track audio export can not be used together. Script will be saved without multi track export."));
        } else {
            prepareMultiAudioFiles(jobs, doc, playlistPath, outputPath);
        }
    }

    if (m_presetParams.isImageSequence()) {
        // Image sequence, ensure we have a %0xd (format string for counter) at output filename end
        static const QRegularExpression rx(QRegularExpression::anchoredPattern(QStringLiteral(".*%[0-9]*d.*")));
        if (!rx.match(outputPath).hasMatch()) {
            outputPath = QStringUtils::appendToFilename(outputPath, QStringLiteral("_%05d"));
        }
    }

    int passes = m_twoPass ? 2 : 1;

    for (int i = 0; i < passes; i++) {
        // clone the dom if this is not the first iteration (happens with two pass)
        QDomDocument final = i > 0 ? doc.cloneNode(true).toDocument() : doc;

        int pass = m_twoPass ? i + 1 : 0;
        RenderJob job;
        job.playlistPath = playlistPath;
        job.outputPath = outputPath;
        job.subtitlePath = subtitlePath;
        if (pass == 2) {
            job.playlistPath = QStringUtils::appendToFilename(job.playlistPath, QStringLiteral("-pass%1").arg(2));
        }
        jobs.push_back(job);

        // get the consumer element
        QDomNodeList consumers = final.elementsByTagName(QStringLiteral("consumer"));
        QDomElement consumer = consumers.at(0).toElement();

        consumer.setAttribute(QStringLiteral("target"), job.outputPath);

        // Set two pass parameters. In case pass is 0 the function does nothing.
        setDocTwoPassParams(pass, final, job.outputPath);

        if (!Xml::docContentToFile(final, job.playlistPath)) {
            addErrorMessage(i18n("Cannot write to file %1", job.playlistPath));
            return;
        }
    }
}

QString RenderRequest::createEmptyTempFile(const QString &extension)
{
    QTemporaryFile tmp(QDir::temp().absoluteFilePath(QString("kdenlive-XXXXXX.%1").arg(extension)));
    if (!tmp.open()) {
        // Something went wrong
        qDebug() << "Could not create temporary file";
        // TODO: some kind of warning to the UI?
        return {};
    }
    tmp.setAutoRemove(false);
    tmp.close();

    return tmp.fileName();
}

QString RenderRequest::generatePlaylistFile()
{
    if (!m_delayedRendering) {
        // No delayed rendering, we can use a temp file
        return createEmptyTempFile(QStringLiteral("mlt"));
    }

    // TODO: this is the only code part of this class that still uses UI components
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
    filename = QInputDialog::getText(nullptr, i18nc("@title:window", "Delayed Rendering"), i18n("Select a name for this rendering."), QLineEdit::Normal,
                                     newFilename, &ok);
    if (!ok) {
        return {};
    }
    if (!filename.endsWith(fileExtension)) {
        filename.append(fileExtension);
    }
    if (projectFolder.exists(newFilename)) {
        if (KMessageBox::questionTwoActions(nullptr, i18n("File %1 already exists.\nDo you want to overwrite it?", filename), {}, KStandardGuiItem::overwrite(),
                                            KStandardGuiItem::cancel()) == KMessageBox::PrimaryAction) {
            return {};
        }
    }
    return projectFolder.absoluteFilePath(filename);
}

void RenderRequest::setDocGeneralParams(QDomDocument doc, int in, int out)
{
    QDomElement consumer = doc.createElement(QStringLiteral("consumer"));
    consumer.setAttribute(QStringLiteral("in"), in);
    consumer.setAttribute(QStringLiteral("out"), out);
    consumer.setAttribute(QStringLiteral("mlt_service"), QStringLiteral("avformat"));

    QMapIterator<QString, QString> it(m_presetParams);

    while (it.hasNext()) {
        it.next();
        // insert params from preset
        consumer.setAttribute(it.key(), it.value());
    }

    // Insert consumer to document, after the profiles (if they exist)
    QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
    if (profiles.isEmpty()) {
        doc.documentElement().insertAfter(consumer, doc.documentElement());
    } else {
        doc.documentElement().insertAfter(consumer, profiles.at(profiles.length() - 1));
    }
}

void RenderRequest::setDocTwoPassParams(int pass, QDomDocument &doc, const QString &outputFile)
{
    if (pass != 1 && pass != 2) {
        return;
    }

    QDomNodeList consumers = doc.elementsByTagName(QStringLiteral("consumer"));
    QDomElement consumer = consumers.at(0).toElement();

    QString logFile = QStringLiteral("%1_2pass.log").arg(outputFile);

    if (m_presetParams.isX265()) {
        // The x265 codec is special
        QString x265params = consumer.attribute(QStringLiteral("x265-params"));
        x265params = QString("pass=%1:stats=%2:%3").arg(pass).arg(logFile.replace(":", "\\:"), x265params);
        consumer.setAttribute(QStringLiteral("x265-params"), x265params);
    } else {
        consumer.setAttribute(QStringLiteral("pass"), pass);
        consumer.setAttribute(QStringLiteral("passlogfile"), logFile);

        if (pass == 1) {
            consumer.setAttribute(QStringLiteral("fastfirstpass"), 1);
            consumer.setAttribute(QStringLiteral("an"), 1);

            consumer.removeAttribute(QStringLiteral("acodec"));
        } else { // pass == 2
            consumer.removeAttribute(QStringLiteral("fastfirstpass"));
        }
    }
}

std::vector<RenderRequest::RenderSection> RenderRequest::getGuideSections()
{
    std::vector<RenderSection> sections;
    if (auto ptr = m_guidesModel.lock()) {
        QList<CommentedTime> markers;
        double fps = pCore->getCurrentFps();

        // keep only markers that are within our bounds
        for (const auto &marker : ptr->getAllMarkers(m_guideCategory)) {

            int pos = marker.time().frames(fps);
            if (pos < m_boundingIn || (m_boundingOut > 0 && pos > m_boundingOut)) continue;
            markers << marker;
        }

        // if there are markers left, create sections based on them
        if (!markers.isEmpty()) {
            bool beginParsed = false;
            QStringList names;
            for (int i = 0; i < markers.count(); i++) {
                RenderSection section;
                if (!beginParsed && i == 0 && markers.at(i).time().frames(fps) != m_boundingIn) {
                    i -= 1;
                    beginParsed = true;
                    section.name = i18n("begin");
                }

                // in point and name of section
                section.in = m_boundingIn;
                if (i >= 0) {
                    section.name = markers.at(i).comment();
                    section.in = markers.at(i).time().frames(fps);
                }
                section.name = QStringUtils::getUniqueName(names, section.name);
                names << section.name;

                // out point of section
                section.out = m_boundingOut;
                if (i + 1 < markers.count()) {
                    section.out = qMin(markers.at(i + 1).time().frames(fps) - 1, m_boundingOut);
                }

                sections.push_back(section);
            }
        }
    }
    return sections;
}

void RenderRequest::prepareMultiAudioFiles(std::vector<RenderJob> &jobs, const QDomDocument &doc, const QString &playlistFile, const QString &targetFile)
{
    int audioCount = 0;
    QDomNodeList orginalTractors = doc.elementsByTagName(QStringLiteral("tractor"));
    // process in reversed order to make file naming fit to UI
    for (int i = orginalTractors.size(); i >= 0; i--) {
        auto originalTracktor = orginalTractors.at(i).toElement();
        QString trackName = Xml::getXmlProperty(originalTracktor, QStringLiteral("kdenlive:track_name"));

        {
            bool originalIsAudio = Xml::getXmlProperty(originalTracktor, QStringLiteral("kdenlive:audio_track")).toInt() == 1;
            if (!originalIsAudio) {
                continue;
            }
        }

        // setup filenames
        QString appendix = QString("_Audio_%1%2%3")
                               .arg(audioCount + 1)
                               .arg(trackName.isEmpty() ? QString() : QStringLiteral("-"))
                               .arg(trackName.replace(QStringLiteral(" "), QStringLiteral("_")));
        RenderJob job;
        job.playlistPath = QStringUtils::appendToFilename(playlistFile, appendix);
        job.outputPath = QStringUtils::appendToFilename(targetFile, appendix);
        jobs.push_back(job);

        // init doc copy
        QDomDocument docCopy = doc.cloneNode(true).toDocument();
        QDomElement consumer = docCopy.elementsByTagName(QStringLiteral("consumer")).at(0).toElement();
        consumer.setAttribute(QStringLiteral("target"), job.outputPath);

        QDomNodeList tracktors = docCopy.elementsByTagName(QStringLiteral("tractor"));
        Q_ASSERT(tracktors.size() == orginalTractors.size());

        for (int j = 0; j < tracktors.size(); j++) {
            auto tractor = tracktors.at(j).toElement();
            bool copyIsAudio = Xml::getXmlProperty(tractor, QStringLiteral("kdenlive:audio_track")).toInt() == 1;
            if (!copyIsAudio) {
                continue;
            }
            QDomNodeList tracks = tractor.elementsByTagName(QStringLiteral("track"));
            for (int l = 0; l < tracks.size(); l++) {
                if (i != j) {
                    tracks.at(l).toElement().setAttribute(QStringLiteral("hide"), QStringLiteral("both"));
                }
            }
        }
        Xml::docContentToFile(docCopy, job.playlistPath);
        audioCount++;
    }
}

void RenderRequest::addErrorMessage(const QString &error)
{
    m_errors.append(error);
}

QStringList RenderRequest::errorMessages()
{
    return m_errors;
}
