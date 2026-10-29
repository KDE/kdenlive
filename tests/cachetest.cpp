/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"

#include "core.h"
#include "utils/thumbnailcache.hpp"

#include <QStandardPaths>

TEST_CASE("Cache insert-remove", "[Cache]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    When(Method(docMock, getCacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    KdenliveDoc &mockedDoc = docMock.get();

    pCore->projectManager()->testSetDocument(&mockedDoc);
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    // Create bin clip
    QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 20, false);

    SECTION("Insert and remove thumbnail")
    {
        QImage img(100, 100, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::red);
        ThumbnailCache::get()->storeThumbnail(binId, 0, img, false);
        REQUIRE(ThumbnailCache::get()->checkIntegrity());
        ThumbnailCache::get()->storeThumbnail(binId, 0, img, false);
        REQUIRE(ThumbnailCache::get()->checkIntegrity());
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("getAudioKey() should dereference `ok` param", "ThumbnailCache") {
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    When(Method(docMock, getCacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    KdenliveDoc &mockedDoc = docMock.get();

    pCore->projectManager()->testSetDocument(&mockedDoc);
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    // Create bin clip
    QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 20, false);

    SECTION("Request invalid id")
    {
        // Catches a bug where, after setting *ok, the code checks
        //     if (ok) {
        // instead of
        //     if (*ok) {
        bool ok = true;
        KdenliveTests::getAudioKey(QStringLiteral("nonexistent-key"), &ok);
        REQUIRE(ok == false);
    }
    SECTION("Request valid id")
    {
        bool ok = false;
        KdenliveTests::getAudioKey(binId, &ok);
        REQUIRE(ok == true);
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("Thumbnail pixels outlive the MLT frame", "[Cache][Thumbnail]")
{
    const QRgb colors[] = {qRgba(255, 0, 0, 255),  qRgba(0, 255, 0, 128),     qRgba(0, 0, 255, 64), qRgba(0, 0, 0, 0),
                           qRgba(13, 57, 99, 255), qRgba(200, 150, 100, 192), qRgba(1, 2, 3, 4),    qRgba(255, 255, 255, 255)};
    int displayWidth = 0;
    SECTION("Native width") {}
    SECTION("Explicit width")
    {
        displayWidth = 4;
    }
    SECTION("Scaled width")
    {
        displayWidth = 8;
    }

    QImage thumbnail;
    {
        QByteArray rgba(4 * 2 * 4, Qt::Uninitialized);
        for (int i = 0; i < 8; ++i) {
            rgba[4 * i] = char(qRed(colors[i]));
            rgba[4 * i + 1] = char(qGreen(colors[i]));
            rgba[4 * i + 2] = char(qBlue(colors[i]));
            rgba[4 * i + 3] = char(qAlpha(colors[i]));
        }
        mlt_frame raw = mlt_frame_init(nullptr);
        Mlt::Frame frame(raw);
        mlt_frame_close(raw);
        frame.set("format", mlt_image_rgba);
        frame.set("width", 4);
        frame.set("height", 2);
        mlt_frame_set_image(frame.get_frame(), reinterpret_cast<uint8_t *>(rgba.data()), int(rgba.size()), nullptr);
        thumbnail = KThumb::getFrame(&frame, 4, 2, displayWidth);
        // The thumbnail must own its pixels, even before the source frame is destroyed.
        rgba.fill(0);
    }
    const int width = displayWidth == 0 ? 4 : displayWidth;
    REQUIRE(thumbnail.size() == QSize(width, 2));
    CHECK(thumbnail.format() == QImage::Format_ARGB32);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < width; ++x) {
            CHECK(thumbnail.pixel(x, y) == colors[y * 4 + x * 4 / width]);
        }
    }
}
