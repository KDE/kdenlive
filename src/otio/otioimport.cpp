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
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/generatorReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/marker.h>

OtioImport::OtioImport(QObject *parent)
    : QObject(parent)
{
}

void OtioImport::slotImport()
{
    // Get the file name.
    std::shared_ptr<OtioImportData> data = std::make_shared<OtioImportData>();
    data->otioFile = QFileInfo(QFileDialog::getOpenFileName(pCore->window(), i18n("OpenTimelineIO Import"), pCore->currentDoc()->projectDataFolder(),
                                                            QStringLiteral("%1 (*.otio)").arg(i18n("OpenTimelineIO Project"))));
    if (!data->otioFile.exists()) {
        // TODO: Error handling?
        return;
    }

    // Open the OTIO timeline.
    OTIO_NS::ErrorStatus otioError;
    data->otioTimeline = OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>(
        dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(data->otioFile.filePath().toStdString(), &otioError)));
    if (!data->otioTimeline || OTIO_NS::is_error(otioError)) {
        // TODO: Error handling?
        return;
    }

    // Find a profile with a frame rate that matches the OTIO timeline.
    // Note that OTIO files do not store information about the render
    // resolution, so we will pick an arbitrary one. An alternative might
    // be to check the media files for the resolution.
    const double otioFps = data->otioTimeline->duration().rate();
    QVector<QString> profileCandidates;
    auto &profileRepository = ProfileRepository::get();
    auto profiles = profileRepository->getAllProfiles();
    for (const auto &i : profiles) {
        const auto &profileModel = profileRepository->getProfile(i.second);
        if (profileModel->fps() == otioFps) {
            profileCandidates.push_back(i.second);
        }
    }
    QString profile;
    if (!profileCandidates.empty()) {
        // Try and find a profile with 1920x1080 resolution, otherwise
        // just use the first one.
        for (const auto &i : profileCandidates) {
            const auto &profileModel = profileRepository->getProfile(i);
            const bool progressive = profileModel->progressive();
            const int width = profileModel->width();
            const int height = profileModel->height();
            if (progressive && 1920 == width && 1080 == height) {
                profile = i;
                break;
            }
        }
        if (profile.isEmpty()) {
            profile = profileCandidates.first();
        }
    } else {
        // No profiles match the frame rate, create a new one.
        QDomDocument doc;
        QDomElement elem = doc.createElement("profile");
        elem.setAttribute("description", QString("HD 1080p %1FPS").arg(otioFps));
        bool adjusted = false;
        const std::pair<int, int> fraction = KdenliveDoc::getFpsFraction(otioFps, &adjusted);
        elem.setAttribute("frame_rate_num", fraction.first);
        elem.setAttribute("frame_rate_den", fraction.second);
        elem.setAttribute("progressive", 1);
        elem.setAttribute("sample_aspect_num", 1.0);
        elem.setAttribute("sample_aspect_den", 1.0);
        elem.setAttribute("display_aspect_num", 16.0);
        elem.setAttribute("display_asepct_den", 9.0);
        elem.setAttribute("colorspace", 0);
        elem.setAttribute("width", 1920);
        elem.setAttribute("height", 1080);
        ProfileParam profileParam(elem);
        profile = ProfileRepository::get()->saveProfile(&profileParam);
    }

    // Create a new document.
    pCore->projectManager()->newFile(profile, false);
    data->timeline = pCore->currentDoc()->getTimeline(pCore->currentTimelineId());
    data->oldTracks = data->timeline->getAllTracksIds();

    // Map color names to marker types.
    for (const auto &i : pCore->markerTypes.keys()) {
        const QString name = pCore->markerTypes[i].color.name();
        m_colorNameToMarkerType[name] = i;
    }

    // Find all of the OTIO media references and add them to the bin. When
    // the bin clips are ready, import the timeline.
    //
    // TODO: Is the callback passed to ClipCreator::createClipFromFile
    // guaranteed to be called? Can the callback be called and the bin
    // clip still not able to be added to the timeline (like a missing
    // file)?
    //
    // TODO: Add a progress dialog?
    for (const auto &otioClip : data->otioTimeline->find_clips()) {
        if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
            const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), data->otioFile);
            const auto i = data->otioExternalReferencesToBinIds.find(file);
            if (i == data->otioExternalReferencesToBinIds.end()) {
                Fun undo = []() { return true; };
                Fun redo = []() { return true; };
                ++data->waitingBinIds;
                std::function<void(const QString &)> callback = [this, data](const QString &) {
                    --data->waitingBinIds;
                    if (0 == data->waitingBinIds) {
                        importTimeline(data);
                    }
                };
                const QString binId = ClipCreator::createClipFromFile(file, pCore->projectItemModel()->getRootFolder()->clipId(), pCore->projectItemModel(),
                                                                      undo, redo, callback);
                data->otioExternalReferencesToBinIds[file] = binId;
            }
        }
    }
}

void OtioImport::importTimeline(const std::shared_ptr<OtioImportData> &data)
{
    // Import the tracks.
    auto otioChildren = data->otioTimeline->tracks()->children();
    for (auto i = otioChildren.rbegin(); i != otioChildren.rend(); ++i) {
        if (auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*i)) {
            int trackId = 0;
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            data->timeline->requestTrackInsertion(-1, trackId, QString::fromStdString(otioTrack->name()), OTIO_NS::Track::Kind::audio == otioTrack->kind(),
                                                  undo, redo);
            importTrack(data, otioTrack, trackId);
        }
    }
    data->timeline->updateDuration();

    // Clean up.
    for (int id : data->oldTracks) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        data->timeline->requestTrackDeletion(id, undo, redo);
    }
}

void OtioImport::importTrack(const std::shared_ptr<OtioImportData> &data, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack, int trackId)
{
    auto otioChildren = otioTrack->children();
    for (auto i = otioChildren.begin(); i != otioChildren.end(); ++i) {
        if (auto otioClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*i)) {
            importClip(data, otioClip, trackId);
        }
    }
}

void OtioImport::importClip(const std::shared_ptr<OtioImportData> &data, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &otioClip, int trackId)
{
    const auto otioTrimmedRange = otioClip->trimmed_range_in_parent();
    if (otioTrimmedRange.has_value()) {
        if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {

            // Find the bin clip that corresponds to the OTIO media reference.
            const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), data->otioFile);
            const auto i = data->otioExternalReferencesToBinIds.find(file);
            if (i != data->otioExternalReferencesToBinIds.end()) {

                // Insert the clip into the track.
                const OTIO_NS::RationalTime otioTimelineDuration = data->otioTimeline->duration();
                const int position = otioTrimmedRange.value().start_time().rescaled_to(otioTimelineDuration).round().value();
                int clipId = -1;
                Fun undo = []() { return true; };
                Fun redo = []() { return true; };
                data->timeline->requestClipInsertion(i.value(), trackId, position, clipId, false, true, true, undo, redo);
                if (clipId != -1) {

                    // Find the start timecode of the media.
                    //
                    // TODO: Is there a better way to get the start timecode?
                    // * ProjectClip::getRecordTime() returns a value of 3590003 for the
                    //   timecode 00:59:50:00 @ 23.97?
                    // * Unfortunately MLT does not provide the start timecode:
                    //   https://github.com/mltframework/mlt/pull/1011
                    // * Could we use the FFmpeg libraries to avoid relying on an external
                    //   application?
                    QString timecode;
                    QProcess mediainfo;
                    mediainfo.start(KdenliveSettings::mediainfopath(), {QStringLiteral("--Output=XML"), file});
                    mediainfo.waitForFinished();
                    if (mediainfo.exitStatus() == QProcess::NormalExit && mediainfo.exitCode() == 0) {
                        QDomDocument doc;
                        doc.setContent(mediainfo.readAllStandardOutput());
                        QDomNodeList nodes = doc.documentElement().elementsByTagName(QStringLiteral("TimeCode_FirstFrame"));
                        if (!nodes.isEmpty()) {
                            timecode = nodes.at(0).toElement().text();
                        }
                    }

                    // Resize the clip.
                    const int duration = otioTrimmedRange.value().duration().rescaled_to(otioTimelineDuration).round().value();
                    data->timeline->requestItemResize(clipId, duration, true, false, -1, true);

                    // Slip the clip.
                    const int start = otioClip->trimmed_range().start_time().rescaled_to(otioTimelineDuration).round().value();
                    int slip = start;
                    if (!timecode.isEmpty()) {
                        const OTIO_NS::RationalTime otioTimecode = OTIO_NS::RationalTime::from_timecode(timecode.toStdString(), otioTimelineDuration.rate());
                        slip -= otioTimecode.value();
                    }
                    data->timeline->requestClipSlip(clipId, -slip, false, true);

                    // Add markers.
                    for (const auto &otioMarker : otioClip->markers()) {
                        if (std::shared_ptr<ClipModel> clipModel = data->timeline->getClipPtr(clipId)) {
                            if (std::shared_ptr<MarkerListModel> markerModel = clipModel->getMarkerModel()) {
                                const OTIO_NS::RationalTime otioMarkerStart = otioMarker->marked_range().start_time().rescaled_to(otioTimelineDuration).round();
                                GenTime pos(start + otioMarkerStart.value(), otioTimelineDuration.rate());
                                int type = -1;
                                auto j = m_colorNameToMarkerType.find(QString::fromStdString(otioMarker->color()));
                                if (j != m_colorNameToMarkerType.end()) {
                                    type = *j;
                                }
                                markerModel->addMarker(pos, QString::fromStdString(otioMarker->comment()), type);
                            }
                        }
                    }
                }
            }
        } else if (auto otioImagSequenceReference = dynamic_cast<OTIO_NS::ImageSequenceReference *>(otioClip->media_reference())) {
            // TODO: Image sequence references.
        } else if (auto otioGeneratorReference = dynamic_cast<OTIO_NS::GeneratorReference *>(otioClip->media_reference())) {
            // TODO: Generator references.
        }
    }
}

QString OtioImport::resolveFile(const QString &file, const QFileInfo &timelineFile)
{
    // Check whether it is a URL or file name, and if it is
    // relative to the timeline file.
    const QUrl url(file);
    QFileInfo out(url.isValid() ? url.path() : file);
    if (out.isRelative()) {
        out = QFileInfo(timelineFile.path(), out.filePath());
    }
    return out.filePath();
}
