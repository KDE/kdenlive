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
        pCore->setCurrentProfile("quarter_pal");
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
