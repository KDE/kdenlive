/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
#include "timelinepreviewmonitor.h"

#include "bin/clipcreator.hpp"
#include "bin/projectfolder.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "timeline2/view/previewmanager.h"
#include "timeline2/view/timelinecontroller.h"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>

namespace {
// Process renderer/clip-loading callbacks, but fail rather than hang on a broken job.
template <typename Predicate> void waitUntil(Predicate done)
{
    QElapsedTimer timeout;
    timeout.start();
    while (!done() && timeout.elapsed() < 30000) {
        qApp->processEvents();
        QThread::msleep(10);
    }
    REQUIRE(done());
}

bool isRed(const QColor &color)
{
    return color.red() > 230 && color.green() < 20 && color.blue() < 20;
}

bool isBlack(const QColor &color)
{
    return color.red() < 20 && color.green() < 20 && color.blue() < 20;
}

struct PreviewProject
{
    const int previousChunkSize = KdenliveSettings::timelinechunks();
    const bool previousAutoPreview = KdenliveSettings::autopreview();
    std::unique_ptr<KdenliveDoc> document;
    std::shared_ptr<TimelineItemModel> timeline;
    QDir cache;

    ~PreviewProject()
    {
        if (timeline) {
            timeline->resetPreviewManager();
        }
        if (document) {
            pCore->projectManager()->closeCurrentDocument(false, false);
        }
        KdenliveSettings::setTimelinechunks(previousChunkSize);
        KdenliveSettings::setAutopreview(previousAutoPreview);
    }

    void createProject()
    {
        KdenliveSettings::setTimelinechunks(25);
        KdenliveSettings::setAutopreview(false);
        pCore->setCurrentProfile("atsc_1080p_25");
        document = std::make_unique<KdenliveDoc>(std::make_shared<DocUndoStack>(nullptr));
        pCore->projectManager()->testSetDocument(document.get());
        REQUIRE(KdenliveTests::updateTimeline(false, QString(), QString(), QDateTime::currentDateTime(), false));
        timeline = document->getTimeline(document->uuid());
        pCore->projectManager()->testSetActiveTimeline(timeline);
        document->setDocumentProperty(QStringLiteral("documentid"), QString::number(QDateTime::currentMSecsSinceEpoch()));
        document->setDocumentProperty(QStringLiteral("previewextension"), QStringLiteral("avi"));
        document->setDocumentProperty(QStringLiteral("previewparameters"), QStringLiteral("vcodec=mjpeg progressive=1 qscale=10"));
        bool ok = false;
        const QDir base = document->getCacheDir(CacheBase, &ok);
        REQUIRE(ok);
        REQUIRE(base.mkpath(QStringLiteral("preview")));
    }

    std::shared_ptr<PreviewManager> preview() const { return timeline->previewManager(); }

    void renderPreview()
    {
        timeline->initializePreviewManager();
        REQUIRE(timeline->hasTimelinePreview());
        timeline->buildPreviewTrack();
        cache = preview()->getCacheDir();
        preview()->addPreviewRange({0, 50}, true);
        preview()->startPreviewRender();
        waitUntil([&]() { return !preview()->isRunning(); });
        REQUIRE(preview()->previewChunks().first == QStringList{"0-50"});
        REQUIRE(cache.entryList({QStringLiteral("*.avi")}, QDir::Files).size() == 3);
    }

    void insertRedClip(int position, int duration = 75)
    {
        // Use the same asynchronous clip-creation API as the bin's Color Clip dialog.
        const auto ready = std::make_shared<bool>(false);
        auto bin = pCore->projectItemModel();
        const QString binId = ClipCreator::createColorClip(QStringLiteral("0xff0000ff"), duration, QStringLiteral("Red"), bin->getRootFolder()->clipId(), bin,
                                                           [ready](const QString &) { *ready = true; });
        REQUIRE(binId != QStringLiteral("-1"));
        waitUntil([&]() { return *ready; });
        int clipId = -1;
        REQUIRE(timeline->requestClipInsertion(binId, timeline->getTrackIndexFromPosition(2), position, clipId, true, true, false));
    }

    void distinguishPreviewFromTimeline()
    {
        // Deliberately retain an older (black) render over a live red clip. Different
        // pixels let us test which source is displayed, not how tracks are wired.
        QTemporaryDir saved;
        REQUIRE(saved.isValid());
        const QStringList files = cache.entryList({QStringLiteral("*.avi")}, QDir::Files);
        for (const auto &file : files) {
            REQUIRE(QFile::copy(cache.filePath(file), saved.filePath(file)));
        }
        insertRedClip(0);
        preview()->invalidatePreviews();
        for (const auto &file : files) {
            QFile::remove(cache.filePath(file));
            REQUIRE(QFile::copy(saved.filePath(file), cache.filePath(file)));
            preview()->gotPreviewRender(QFileInfo(file).baseName().toInt(), cache.filePath(file), 1000);
        }
    }
};
} // namespace

TEST_CASE_METHOD(PreviewProject, "Timeline preview action availability", "[TimelinePreview]")
{
    createProject();
    TimelineController controller(nullptr);
    QAction disablePreview;
    disablePreview.setEnabled(false);
    QObject::connect(&controller, &TimelineController::previewDisabledStateChanged, &disablePreview,
                     [&]() { disablePreview.setEnabled(timeline->hasTimelinePreview()); });

    SECTION("Creating the preview manager enables the action")
    {
        controller.setModel(timeline);
        timeline->initializePreviewManager();
        CHECK(disablePreview.isEnabled());
    }
    SECTION("Reopening a rendered timeline and reusing its cache keeps the action enabled")
    {
        renderPreview();
        const auto chunks = preview()->previewChunks();
        controller.setModel(timeline);
        REQUIRE(disablePreview.isEnabled());
        controller.startPreviewRender();
        CHECK(disablePreview.isEnabled());
        CHECK(preview()->previewChunks() == chunks);
    }
}

TEST_CASE_METHOD(PreviewProject, "Timeline preview switching preserves rendered chunks", "[TimelinePreview]")
{
    createProject();
    renderPreview();
    distinguishPreviewFromTimeline();
    TimelineController controller(nullptr);
    controller.setModel(timeline);
    TimelinePreviewTests::PreviewMonitor monitor(timeline, controller);
    const auto chunks = preview()->previewChunks();
    REQUIRE(isBlack(monitor.color()));

    SECTION("Paused switching redraws without seeking or losing the cache")
    {
        controller.setPreviewEnabled(false);
        CHECK(isRed(monitor.color()));
        CHECK(monitor.position() == 0);
        CHECK(monitor.speed() == 0);
        CHECK(preview()->previewChunks() == chunks);
        controller.setPreviewEnabled(true);
        CHECK(isBlack(monitor.color()));
        CHECK(monitor.position() == 0);
        CHECK(monitor.speed() == 0);
        CHECK(preview()->previewChunks() == chunks);
        monitor.seek(50);
        CHECK(isBlack(monitor.color()));
    }
    SECTION("Updating the effect comparison overlay does not enable disabled previews")
    {
        controller.setPreviewEnabled(false);
        monitor.setOverlay(true);
        monitor.seek(0);
        CHECK(isRed(monitor.color()));
        monitor.setOverlay(false);
        monitor.seek(0);
        CHECK(isRed(monitor.color()));
        controller.setPreviewEnabled(true);
        CHECK(isBlack(monitor.color()));
        CHECK(preview()->previewChunks() == chunks);
    }
    SECTION("Switching during buffered playback changes the displayed source")
    {
        monitor.play();
        REQUIRE(isBlack(monitor.nextFrame()));
        controller.setPreviewEnabled(false);
        // Allow already-delivered/read-ahead frames to drain, with a bounded
        // frame budget rather than relying on wall-clock sleeps or magic indices.
        bool sawLive = false;
        for (int frame = 0; frame < 12 && !sawLive; ++frame) {
            sawLive = isRed(monitor.nextFrame());
        }
        REQUIRE(sawLive);
        controller.setPreviewEnabled(true);
        bool sawPreview = false;
        for (int frame = 0; frame < 12 && !sawPreview; ++frame) {
            sawPreview = isBlack(monitor.nextFrame());
        }
        CHECK(sawPreview);
        CHECK(monitor.refreshCount() == 2);
        CHECK(preview()->previewChunks() == chunks);
    }
}

TEST_CASE_METHOD(PreviewProject, "Timeline preview chunk changes refresh only the paused playhead", "[TimelinePreview]")
{
    createProject();
    renderPreview();
    distinguishPreviewFromTimeline();
    TimelineController controller(nullptr);
    controller.setModel(timeline);
    TimelinePreviewTests::PreviewMonitor monitor(timeline, controller);

    SECTION("Invalidating and completing the current chunk redraws immediately")
    {
        preview()->invalidatePreview(0, 24);
        CHECK(isRed(monitor.color()));
        CHECK(monitor.refreshCount() == 1);
        preview()->gotPreviewRender(0, cache.filePath(QStringLiteral("0.avi")), 1000);
        CHECK(isBlack(monitor.color()));
        CHECK(monitor.refreshCount() == 2);
        CHECK(monitor.position() == 0);
        CHECK(monitor.speed() == 0);
    }
    SECTION("The next chunk does not refresh a playhead at the previous chunk's last frame")
    {
        monitor.seek(24);
        preview()->invalidatePreview(25, 49);
        preview()->gotPreviewRender(25, cache.filePath(QStringLiteral("25.avi")), 1000);
        CHECK(monitor.refreshCount() == 0);
        CHECK(isBlack(monitor.color()));
    }
    SECTION("A disabled preview does not refresh when a chunk completes")
    {
        controller.setPreviewEnabled(false);
        monitor.resetRefreshCount();
        preview()->invalidatePreview(0, 24);
        preview()->gotPreviewRender(0, cache.filePath(QStringLiteral("0.avi")), 1000);
        CHECK(monitor.refreshCount() == 0);
        CHECK(isRed(monitor.color()));
    }
    SECTION("Completing a preview chunk during playback does not request an explicit monitor refresh")
    {
        monitor.play();
        preview()->invalidatePreview(0, 24);
        preview()->gotPreviewRender(0, cache.filePath(QStringLiteral("0.avi")), 1000);
        CHECK(monitor.refreshCount() == 0);
    }
    SECTION("Removing the current preview range redraws immediately")
    {
        preview()->addPreviewRange({0, 24}, false);
        CHECK(monitor.refreshCount() == 1);
        CHECK(isRed(monitor.color()));
        CHECK(monitor.position() == 0);
    }
    SECTION("Clearing all preview ranges redraws the current chunk")
    {
        monitor.seek(25);
        preview()->clearPreviewRange(true);
        CHECK(monitor.refreshCount() == 1);
        CHECK(isRed(monitor.color()));
        CHECK(monitor.position() == 25);
    }
}

TEST_CASE_METHOD(PreviewProject, "Timeline edits invalidate disabled previews", "[TimelinePreview]")
{
    createProject();
    renderPreview();
    TimelineController controller(nullptr);
    controller.setModel(timeline);
    TimelinePreviewTests::PreviewMonitor monitor(timeline, controller);
    controller.setPreviewEnabled(false);
    insertRedClip(50, 20);
    preview()->invalidatePreviews();
    CHECK(controller.renderedChunks() == QVariantList{0, 25});
    CHECK_FALSE(cache.exists(QStringLiteral("50.avi")));
    CHECK(cache.exists(QStringLiteral("0.avi")));
    CHECK(cache.exists(QStringLiteral("25.avi")));
    controller.setPreviewEnabled(true);
    monitor.seek(0);
    CHECK(isBlack(monitor.color()));
    monitor.seek(50);
    CHECK(isRed(monitor.color()));
}

TEST_CASE_METHOD(PreviewProject, "Timeline preview insert-remove", "[TimelinePreview]")
{
    createProject();
    renderPreview();
    REQUIRE(cache.entryList(QDir::Files).size() == 3);

    // Preserve the original enabled-preview insertion and cache-cleanup coverage.
    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    KdenliveTests::setAudioTargets(timeline, audioInfo);
    int clipId = -1;
    REQUIRE(timeline->requestClipInsertion(binId, timeline->getTrackIndexFromPosition(2), 50, clipId, true, true, false));
    REQUIRE(timeline->getClipsCount() == 1);
    preview()->invalidatePreviews();
    CHECK(cache.entryList(QDir::Files) == QStringList{QStringLiteral("0.avi"), QStringLiteral("25.avi")});

    timeline->resetPreviewManager();
    CHECK_FALSE(cache.exists());
}

TEST_CASE_METHOD(PreviewProject, "Closing an unsaved timeline removes its preview cache", "[TimelinePreview]")
{
    createProject();
    renderPreview();
    timeline->resetPreviewManager();
    CHECK_FALSE(cache.exists());
}
