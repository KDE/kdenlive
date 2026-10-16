/*
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "bin/projectfolder.h"
#include "doc/kdenlivedoc.h"
#include "test_utils.hpp"

TEST_CASE("Bin items resolve to unique source clips", "[Bin]")
{
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    KdenliveTests::updateTimeline(false, QString(), QString(), QDateTime::currentDateTime(), 0);
    pCore->projectManager()->testSetActiveTimeline(document.getTimeline(document.uuid()));

    const QString masterId = KdenliveTests::createAVProducer(pCore->getProjectProfile(), binModel);
    const QString otherId = KdenliveTests::createAVProducer(pCore->getProjectProfile(), binModel);
    QString zoneId;
    REQUIRE(binModel->requestAddBinSubClip(zoneId, 1, 5, {}, masterId));
    QString siblingId;
    REQUIRE(binModel->requestAddBinSubClip(siblingId, 6, 10, {}, masterId));
    QString otherZoneId;
    REQUIRE(binModel->requestAddBinSubClip(otherZoneId, 1, 5, {}, otherId));
    QString folderId;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->requestAddFolder(folderId, QStringLiteral("Folder"), binModel->getRootFolder()->clipId(), undo, redo));

    const auto master = binModel->getClipByBinID(masterId);
    const auto other = binModel->getClipByBinID(otherId);
    const auto item = [&](const QString &id) { return binModel->getItemByBinId(id); };
    using Sources = QList<std::shared_ptr<ProjectClip>>;

    CHECK(ProjectItemModel::sourceClips({}).isEmpty());
    CHECK(ProjectItemModel::sourceClips({item(folderId), nullptr}).isEmpty());
    // A selected zone is not a clip itself: it must resolve to its master.
    CHECK(ProjectItemModel::sourceClips({item(zoneId)}) == Sources{master});
    // A master and its zones are one source.
    CHECK(ProjectItemModel::sourceClips({item(zoneId), item(masterId), item(siblingId), item(folderId)}) == Sources{master});
    // Distinct masters keep first-selection order.
    CHECK(ProjectItemModel::sourceClips({item(otherZoneId), item(zoneId), item(otherId)}) == Sources{other, master});
    pCore->projectManager()->closeCurrentDocument(false, false);
}
