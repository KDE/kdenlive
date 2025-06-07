/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioimport.h"

#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "otioutil.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/view/timelinewidget.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QFileDialog>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/generatorReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/transition.h>

OtioImport::OtioImport(QObject *parent)
    : QObject(parent)
{
    // Create importing dialog, unless the tests are running.
    if (auto window = pCore->window()) {
        m_importingDialog = new QProgressDialog(window);
        m_importingDialog->setWindowFlags((m_importingDialog->windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowCloseButtonHint &
                                          ~Qt::WindowSystemMenuHint);
        m_importingDialog->setMinimumDuration(0);
        m_importingDialog->setMaximum(0);
        m_importingDialog->setWindowTitle(i18nc("@title:window", "Importing OpenTimelineIO"));
        m_importingDialog->setCancelButton(nullptr);
        m_importingDialog->setModal(true);
        m_importingDialog->close();
    }
}

void OtioImport::importFile(const QString &fileName, bool newDocument)
{
    // Show the importing dialog.
    if (m_importingDialog) {
        m_importingDialog->reset();
        m_importingDialog->show();
    }

    // Create the temporary data for importing.
    std::shared_ptr<OtioImportData> importData = std::make_shared<OtioImportData>();
    importData->otioFile = QFileInfo(fileName);

    // Open the OTIO timeline.
    OTIO_NS::ErrorStatus otioError;
    importData->otioTimeline = OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>(
        dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(importData->otioFile.filePath().toStdString(), &otioError)));
    if (!importData->otioTimeline || OTIO_NS::is_error(otioError)) {
        if (m_importingDialog) {
            m_importingDialog->close();
        }
        if (pCore->window()) {
            KMessageBox::error(qApp->activeWindow(), QString::fromStdString(otioError.details), i18n("Error importing OpenTimelineIO file"));
        } else {
            qWarning() << "Error importing OpenTimelineIO file:" << QString::fromStdString(otioError.details);
        }
        return;
    }
    const double otioFps = importData->otioTimeline->duration().rate();

    // Find the OTIO media references that will be added to the bin. This count
    // will be used to drive the progress dialog.
    for (const auto &otioClip : importData->otioTimeline->find_clips()) {
        if (const auto &otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {

            // Found an external reference.
            const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), importData->otioFile);
            importData->otioExternalRefs.insert(file);
        } else if (const auto &otioGeneratorReference = dynamic_cast<OTIO_NS::GeneratorReference *>(otioClip->media_reference())) {
            if (otioGeneratorReference->generator_kind() == "kdenlive:SolidColor") {

                // Found a solid color generator.
                const QString name = QString::fromStdString(otioGeneratorReference->name());
                QString color;
                const auto &otioParameters = otioGeneratorReference->parameters();
                auto i = otioParameters.find("kdenlive");
                if (i != otioParameters.end() && i->second.has_value()) {
                    auto j = std::any_cast<OTIO_NS::AnyDictionary>(i->second);
                    auto k = j.find("color");
                    if (k != j.end() && k->second.has_value()) {
                        color = QString::fromStdString(std::any_cast<std::string>(k->second));
                    }
                }
                OTIO_NS::TimeRange timeRange(OTIO_NS::RationalTime(0.0, otioFps), OTIO_NS::RationalTime(1.0, otioFps));
                if (otioGeneratorReference->available_range().has_value()) {
                    timeRange = otioGeneratorReference->available_range().value();
                }
                importData->otioColorGeneratorRefs[name] = QPair<QString, int>(color, timeRange.duration().value());
            }
        }
    }
    importData->binClipCount = importData->otioExternalRefs.size() + importData->otioColorGeneratorRefs.size();
    if (m_importingDialog) {
        m_importingDialog->setMaximum(importData->binClipCount);
    }

    // Try to find an existing profile that matches the frame rate
    // and resolution, otherwise create a custom profile.
    //
    // Note that OTIO files do not contain information about rendering,
    // so we get the resolution from the first video clip.
    QVector<QString> profileCandidates;
    auto &profileRepository = ProfileRepository::get();
    auto profiles = profileRepository->getAllProfiles();
    for (const auto &i : profiles) {
        const auto &profileModel = profileRepository->getProfile(i.second);
        if (profileModel->fps() == otioFps) {
            profileCandidates.push_back(i.second);
        }
    }
    QSize videoSize(1920, 1080);
    for (const auto &otioItem : importData->otioTimeline->tracks()->children()) {
        if (auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(otioItem)) {
            if (OTIO_NS::Track::Kind::video == otioTrack->kind()) {
                for (const auto &otioClip : otioTrack->find_clips()) {
                    if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
                        const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), importData->otioFile);
                        QSize fileVideoSize = getVideoSize(file);
                        if (fileVideoSize.isValid()) {
                            videoSize = fileVideoSize;
                            break;
                        }
                    }
                }
            }
        }
    }
    QString profile;
    if (!profileCandidates.empty()) {
        for (const auto &i : profileCandidates) {
            const auto &profileModel = profileRepository->getProfile(i);
            const bool progressive = profileModel->progressive();
            const int width = profileModel->width();
            const int height = profileModel->height();
            if (progressive && videoSize.width() == width && videoSize.height() == height) {
                profile = i;
                break;
            }
        }
    }
    if (profile.isEmpty()) {
        QDomDocument doc;
        QDomElement elem = doc.createElement("profile");
        elem.setAttribute("description", QString("%1x%2 %3FPS").arg(videoSize.width()).arg(videoSize.height()).arg(otioFps));
        bool adjusted = false;
        std::pair<int, int> fraction = KdenliveDoc::getFpsFraction(otioFps, &adjusted);
        elem.setAttribute("frame_rate_num", fraction.first);
        elem.setAttribute("frame_rate_den", fraction.second);
        elem.setAttribute("progressive", 1);
        elem.setAttribute("sample_aspect_num", 1.0);
        elem.setAttribute("sample_aspect_den", 1.0);
        elem.setAttribute("display_aspect_num", videoSize.width());
        elem.setAttribute("display_aspect_den", videoSize.height());
        elem.setAttribute("colorspace", 0);
        elem.setAttribute("width", videoSize.width());
        elem.setAttribute("height", videoSize.height());
        ProfileParam profileParam(elem);
        profile = ProfileRepository::get()->saveProfile(&profileParam);
    }
    pCore->setCurrentProfile(profile);

    if (newDocument) {
        // Create a new document.
        pCore->projectManager()->newFile(profile, false);
    }
    pCore->currentDoc()->loading = true;

    // Get the timeline model. We save the default tracks so we can delete
    // them after the import is finished.
    importData->timeline = pCore->currentDoc()->getTimeline(pCore->currentTimelineId());
    importData->defaultTracks = importData->timeline->getAllTracksIds();

    // Add the OTIO media references to the bin. When the bin clips have
    // completed loading, import the timeline.
    std::function<void(const QString &)> callback = [this, importData](const QString &) {
        ++importData->completedBinClips;
        if (importData->completedBinClips >= importData->binClipCount) {
            importTimeline(importData);
        }
        if (m_importingDialog) {
            m_importingDialog->setValue(importData->completedBinClips);
            if (importData->completedBinClips >= importData->binClipCount) {
                m_importingDialog->close();
            }
        }
    };
    for (const auto &file : importData->otioExternalRefs) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        const QString binId =
            ClipCreator::createClipFromFile(file, pCore->projectItemModel()->getRootFolder()->clipId(), pCore->projectItemModel(), undo, redo, callback);
        importData->otioExternalRefToBinId[file] = binId;

        // Get the start timecode from the media.
        const QString timecode = getTimecode(file);
        if (!timecode.isEmpty()) {
            importData->binIdToTimecode[binId] = timecode;
        }
    }
    for (const auto &key : importData->otioColorGeneratorRefs.keys()) {
        const auto &value = importData->otioColorGeneratorRefs[key];
        const QString binId = ClipCreator::createColorClip(value.first, value.second, key, pCore->projectItemModel()->getRootFolder()->clipId(),
                                                           pCore->projectItemModel(), callback);
        importData->otioColorGeneratorRefToBinId[key] = binId;
    }

    // Manually call the callback if there were no clips to import.
    if (0 == importData->binClipCount) {
        callback(QString());
    }
}

void OtioImport::slotImport()
{
    const QString &fileName = QFileDialog::getOpenFileName(pCore->window(), i18n("OpenTimelineIO Import"), pCore->currentDoc()->projectDataFolder(),
                                                           QStringLiteral("%1 (*.otio)").arg(i18n("OpenTimelineIO Project")));
    if (fileName.isNull()) {
        return;
    }
    importFile(fileName);
}

void OtioImport::importTimeline(const std::shared_ptr<OtioImportData> &importData)
{

    auto orderedTracks = getOrderedOtioTracksForTimelineInsertion(importData->otioTimeline);
    for (const auto &otioTrack : orderedTracks) {
        int trackId = 0;
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        bool isAudio = (OTIO_NS::Track::Kind::audio == otioTrack->kind());
        importData->timeline->requestTrackInsertion(0, trackId, QString::fromStdString(otioTrack->name()), isAudio, undo, redo);
        importTrack(importData, otioTrack, trackId);
    }

    importData->timeline->updateDuration();

    // Import the OTIO markers as guides.
    for (const auto &otioMarker : importData->otioTimeline->tracks()->markers()) {
        const GenTime pos(otioMarker->marked_range().start_time().value(), importData->otioTimeline->duration().rate());
        importMarker(otioMarker, pos, importData->timeline->getGuideModel());
    }

    // Delete the default tracks.
    for (int id : importData->defaultTracks) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        importData->timeline->requestTrackDeletion(id, undo, redo);
    }
    pCore->currentDoc()->loading = false;
}

void OtioImport::importTrack(const std::shared_ptr<OtioImportData> &importData, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack,
                             int trackId)
{
    // Import the clips and transitions.
    int clipIdPrev = -1;
    const OTIO_NS::RationalTime otioTimelineDuration = importData->otioTimeline->duration();
    const auto otioChildren = otioTrack->children();
    for (size_t i = 0; i < otioChildren.size(); ++i) {
        if (const auto &otioClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(otioChildren[i])) {
            const int clipId = importClip(importData, otioClip, trackId);

            // Check if there is a transition before this clip, and convert
            // it to a mix.
            if (i > 0 && clipId != -1 && clipIdPrev != -1) {
                if (const auto &otioTransition = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Transition>(otioChildren[i - 1])) {
                    Fun undo = []() { return true; };
                    Fun redo = []() { return true; };
                    const int in = otioTransition->in_offset().rescaled_to(otioTimelineDuration).round().value();
                    const int out = otioTransition->out_offset().rescaled_to(otioTimelineDuration).round().value();
                    const int pos = importData->timeline->getClipPosition(clipId);
                    importData->timeline->requestClipMix(QString("luma"), {clipIdPrev, clipId}, {out, in}, trackId, pos, true, true, true, undo, redo, false);
                }
            }

            clipIdPrev = clipId;
        }
    }
}

int OtioImport::importClip(const std::shared_ptr<OtioImportData> &importData, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &otioClip, int trackId)
{
    const auto otioTrimmedRange = otioClip->trimmed_range_in_parent();
    if (!otioTrimmedRange.has_value()) {
        return -1;
    }
    const OTIO_NS::RationalTime otioTimelineDuration = importData->otioTimeline->duration();
    const int duration = otioTrimmedRange.value().duration().rescaled_to(otioTimelineDuration).round().value();
    if (duration <= 0) {
        return -1;
    }

    // Find the bin clip that corresponds to the OTIO media reference.
    QString binId;
    if (const auto &otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
        const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), importData->otioFile);
        const auto i = importData->otioExternalRefToBinId.find(file);
        if (i != importData->otioExternalRefToBinId.end()) {
            binId = i.value();
        }
        //} else if (const auto &otioImagSequenceReference = dynamic_cast<OTIO_NS::ImageSequenceReference *>(otioClip->media_reference())) {
        // TODO: Image sequence references.
    } else if (const auto &otioGeneratorReference = dynamic_cast<OTIO_NS::GeneratorReference *>(otioClip->media_reference())) {
        const auto i = importData->otioColorGeneratorRefToBinId.find(QString::fromStdString(otioGeneratorReference->name()));
        if (i != importData->otioColorGeneratorRefToBinId.end()) {
            binId = i.value();
        }
    }
    if (binId.isEmpty()) {
        return -1;
    }

    // Insert the clip into the track.
    const int position = otioTrimmedRange.value().start_time().rescaled_to(otioTimelineDuration).round().value();
    int clipId = -1;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    importData->timeline->requestClipInsertion(binId, trackId, position, clipId, false, true, true, undo, redo);
    if (-1 == clipId) {
        return -1;
    }

    // Resize the clip.
    importData->timeline->requestItemResize(clipId, duration, true, false, -1, true);

    // Slip the clip.
    const int start = otioClip->trimmed_range().start_time().rescaled_to(otioTimelineDuration).round().value();
    int slip = start;
    const auto i = importData->binIdToTimecode.find(binId);
    if (i != importData->binIdToTimecode.end()) {
        const OTIO_NS::RationalTime otioTimecode = OTIO_NS::RationalTime::from_timecode(i->toStdString(), otioTimelineDuration.rate());
        slip -= otioTimecode.value();
    }
    if (slip != 0) {
        importData->timeline->requestClipSlip(clipId, -slip, false, true);
    }

    // Add markers.
    if (std::shared_ptr<ClipModel> clipModel = importData->timeline->getClipPtr(clipId)) {
        for (const auto &otioMarker : otioClip->markers()) {
            const OTIO_NS::RationalTime otioMarkerStart = otioMarker->marked_range().start_time().rescaled_to(otioTimelineDuration).round();
            const GenTime pos(start + otioMarkerStart.value(), otioTimelineDuration.rate());
            importMarker(otioMarker, pos, clipModel->getMarkerModel());
        }
    }

    return clipId;
}

void OtioImport::importMarker(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Marker> &otioMarker, const GenTime &pos,
                              const std::shared_ptr<MarkerListModel> &markerModel)
{
    // Check the OTIO metadata for the marker type, otherwise find the marker
    // type closest to the OTIO marker color.
    int type = -1;
    if (otioMarker->metadata().has_key("kdenlive") && otioMarker->metadata()["kdenlive"].type() == typeid(OTIO_NS::AnyDictionary)) {
        auto otioMetadata = std::any_cast<OTIO_NS::AnyDictionary>(otioMarker->metadata()["kdenlive"]);
        if (otioMetadata.has_key("type") && otioMetadata["type"].type() == typeid(int64_t)) {
            type = std::any_cast<int64_t>(otioMetadata["type"]);
        }
    } else {
        type = fromOtioMarkerColor(otioMarker->color());
    }
    markerModel->addMarker(pos, QString::fromStdString(otioMarker->comment()), type);
}

QString OtioImport::resolveFile(const QString &file, const QFileInfo &timelineFile)
{
    // Check whether it is a URL or file name, and if it is
    // relative to the timeline file.
    const QUrl url = QUrl::fromLocalFile(file);
    QFileInfo out(url.isValid() ? url.toLocalFile() : file);
    if (out.isRelative()) {
        out = QFileInfo(timelineFile.path(), out.filePath());
    }
    return out.filePath();
}

std::vector<OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track>>
OtioImport::getOrderedOtioTracksForTimelineInsertion(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline)
{
    // Lower video tracks come before higher video tracks in OTIO.
    // The order between Audio and video tracks is not defined and they can come in interleaved A1, V1, A2, V2 or in any other order like A1, A2, V1, V2.
    // Reorder the tracks so we end up with the expected track order of Vn, Vn-1, ... V1, A1, ..., A-1, An.
    std::vector<OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track>> videoTracks, audioTracks;
    auto otioChildren = otioTimeline->tracks()->children();
    for (const auto &item : otioChildren) {
        if (auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(item)) {
            if (OTIO_NS::Track::Kind::audio == otioTrack->kind()) {
                audioTracks.push_back(otioTrack);
            } else {
                videoTracks.insert(videoTracks.begin(), otioTrack);
            }
        }
    }
    videoTracks.insert(videoTracks.end(), audioTracks.begin(), audioTracks.end());
    return videoTracks;
}