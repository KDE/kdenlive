/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
#include "doc/documentchecker.h"
#include "doc/kdenlivedoc.h"
#include "src/assets/keyframes/model/keyframemodel.hpp"
#include "src/renderpresets/renderpresetrepository.hpp"
#include "src/utils/thumbnailcache.hpp"

QString KdenliveTests::createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length, bool limited)
{
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "color", color.c_str());
    producer->set("length", length);
    producer->set("out", length - 1);

    REQUIRE(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    if (limited) {
        binClip->forceLimitedDuration();
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}

QString KdenliveTests::createProducerWithSound(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, int length)
{
    // std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, QFileInfo(QCoreApplication::applicationDirPath()  +
    // "/../../tests/small.mkv").absoluteFilePath().toStdString().c_str()); In case the test system does not have avformat support, we can switch to the
    // integrated blipflash producer
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "blipflash");
    REQUIRE(producer->is_valid());

    producer->set("length", length);
    producer->set_in_and_out(0, length - 1);
    producer->set("kdenlive:duration", length);

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    binClip->forceLimitedDuration();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}

QString KdenliveTests::createAVProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel)
{
    std::shared_ptr<Mlt::Producer> producer =
        std::make_shared<Mlt::Producer>(prof, QFileInfo(sourcesPath + "/small.mkv").absoluteFilePath().toStdString().c_str());

    // In case the test system does not have avformat support, we can switch to the integrated blipflash producer
    int length = -1;
    if (!producer || !producer->is_valid()) {
        length = 40;
        producer.reset(new Mlt::Producer(prof, "blipflash"));
        producer->set("length", length);
        producer->set_in_and_out(0, length - 1);
        producer->set("kdenlive:duration", length);
    }
    REQUIRE(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    if (length > -1) {
        binClip->forceLimitedDuration();
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));
    return binId;
}

QString KdenliveTests::createTextProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, const QString &xmldata, const QString &clipname,
                                          int length)
{
    std::shared_ptr<Mlt::Producer> producer =
        std::make_shared<Mlt::Producer>(prof, "kdenlivetitle");

    REQUIRE(producer->is_valid());

    producer->set("length", length);
    producer->set_in_and_out(0, length - 1);
    producer->set("kdenlive:duration", length);
    producer->set_string("kdenlive:clipname", clipname.toLocal8Bit().data());
    producer->set_string("xmldata", xmldata.toLocal8Bit().data());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    binClip->forceLimitedDuration();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    REQUIRE(binClip->clipType() == ClipType::Text);
    return binId;
}

std::unique_ptr<QDomElement> KdenliveTests::getProperty(const QDomElement &doc, const QString &name)
{
    QDomNodeList list = doc.elementsByTagName("property");
    for (int i = 0; i < list.count(); i++) {
        QDomElement e = list.at(i).toElement();
        if (e.attribute("name") == name) {
            return std::make_unique<QDomElement>(e);
        }
    }
    return nullptr;
}

QStringList KdenliveTests::getAssetsServiceIds(const QDomDocument &doc, const QString &tagName)
{
    return DocumentChecker::getAssetsServiceIds(doc, tagName).keys();
}

void KdenliveTests::removeAssetsById(QDomDocument &doc, const QString &tagName, const QStringList &idsToDelete)
{
    return DocumentChecker::removeAssetsById(doc, tagName, idsToDelete);
}

bool KdenliveTests::updateTimeline(bool createNewTab, const QString &chunks, const QString &dirty, const QDateTime &documentDate, bool enablePreview)
{
    return pCore->projectManager()->updateTimeline(createNewTab, chunks, dirty, documentDate, enablePreview);
}

bool KdenliveTests::isMltBuildInLuma(const QString &lumaName)
{
    return DocumentChecker::isMltBuildInLuma(lumaName);
}

void KdenliveTests::setAudioTargets(std::shared_ptr<TimelineItemModel> timeline, QMap<int, QString> audioInfo)
{
    timeline->m_binAudioTargets = audioInfo;
}

void KdenliveTests::setAudioTimelineTargets(std::shared_ptr<TimelineItemModel> timeline, QMap<int, int> audioInfo)
{
    timeline->m_audioTarget = audioInfo;
}

void KdenliveTests::setVideoTargets(std::shared_ptr<TimelineItemModel> timeline, int tid)
{
    timeline->m_videoTarget = tid;
}

QStringList KdenliveTests::getAudioKey(const QString &binId, bool *ok)
{
    return ThumbnailCache::getAudioKey(binId, ok);
}

void KdenliveTests::resetNextId(int id)
{
    KdenliveDoc::next_id = id;
}

int KdenliveTests::getNextId()
{
    return TimelineModel::getNextId();
}

GroupsModel *KdenliveTests::groupsModel(std::shared_ptr<TimelineItemModel> timeline)
{
    return timeline->m_groups.get();
}

void KdenliveTests::destructGroupItem(std::shared_ptr<TimelineItemModel> timeline, int id, bool update, Fun &undo, Fun &redo)
{
    timeline->m_groups->destructGroupItem(id, update, undo, redo);
}

std::shared_ptr<KeyframeModel> KdenliveTests::cloneModel(std::shared_ptr<KeyframeModel> original)
{
    std::shared_ptr<KeyframeModel> clone = std::make_shared<KeyframeModel>(original->m_model, original->m_index, original->m_undoStack);
    clone->parseAnimProperty(original->getAnimProperty());
    return clone;
}

const std::shared_ptr<TrackModel> KdenliveTests::getTrackById_const(std::shared_ptr<TimelineItemModel> timeline, int tid)
{
    return timeline->getTrackById_const(tid);
}

QDomDocument KdenliveTests::getDocument(KdenliveDoc *openedDoc)
{
    return openedDoc->m_document;
}

int KdenliveTests::downLinks(std::shared_ptr<TimelineItemModel> timeline, int gid)
{
    return timeline->m_groups->m_downLink.count(gid);
}

int KdenliveTests::downLinksSize(std::shared_ptr<TimelineItemModel> timeline)
{
    return timeline->m_groups->m_downLink.size();
}

std::shared_ptr<ClipModel> KdenliveTests::getClipPtr(std::shared_ptr<TimelineItemModel> timeline, int clipId)
{
    return timeline->getClipPtr(clipId);
}

bool KdenliveTests::addKeyframe(std::shared_ptr<KeyframeModel> model, GenTime pos, KeyframeType::KeyframeEnum type, QVariant value)
{
    return model->addKeyframe(pos, type, value);
}

bool KdenliveTests::removeKeyframe(std::shared_ptr<KeyframeModel> model, GenTime pos)
{
    return model->removeKeyframe(pos);
}

bool KdenliveTests::removeAllKeyframes(std::shared_ptr<KeyframeModel> model)
{
    return model->removeAllKeyframes();
}

void KdenliveTests::forceClipAudio(std::shared_ptr<TimelineItemModel> timeline, int clipId)
{
    timeline->getClipPtr(clipId)->m_canBeAudio = true;
}

int KdenliveTests::groupsCount(std::shared_ptr<TimelineItemModel> timeline)
{
    return timeline->m_allGroups.size();
}

std::unordered_map<int, int> KdenliveTests::groupUpLink(std::shared_ptr<TimelineItemModel> timeline)
{
    return timeline->m_groups->m_upLink;
}

void KdenliveTests::setGroupType(std::shared_ptr<TimelineItemModel> timeline, int gid, GroupType type)
{
    timeline->m_groups->setType(gid, type);
}

std::shared_ptr<TimelineItemModel> KdenliveTests::createTimelineModel(const QUuid uuid, std::shared_ptr<DocUndoStack> undoStack)
{
    std::shared_ptr<TimelineItemModel> ptr(new TimelineItemModel(uuid, undoStack));
    return ptr;
}

void KdenliveTests::finishTimelineConstruct(std::shared_ptr<TimelineItemModel> timeline)
{
    TimelineItemModel::finishConstruct(timeline);
}

bool KdenliveTests::requestClipCreation(std::shared_ptr<TimelineItemModel> timeline, const QString &binClipId, int &id, PlaylistState::ClipState state,
                                        int audioStream, double speed, bool warp_pitch, Fun &undo, Fun &redo)
{
    return timeline->requestClipCreation(binClipId, id, state, audioStream, speed, warp_pitch, undo, redo);
}

void KdenliveTests::makeFiniteClipEnd(std::shared_ptr<TimelineItemModel> timeline, int cid)
{
    timeline->m_allClips[cid]->m_endlessResize = false;
}

void KdenliveTests::setRenderRequestBounds(RenderRequest *r, int in, int out)
{
    r->m_boundingIn = in;
    r->m_boundingOut = out;
}

void KdenliveTests::initRenderRepository()
{
    RenderPresetRepository::m_acodecsList = QStringList(QStringLiteral("libvorbis"));
    RenderPresetRepository::m_vcodecsList = QStringList(QStringLiteral("libvpx"));
    RenderPresetRepository::m_supportedFormats = QStringList(QStringLiteral("mp4"));
}

bool KdenliveTests::checkModelConsistency(std::shared_ptr<AbstractTreeModel> model)
{
    return model->checkConsistency();
}

int KdenliveTests::modelSize(std::shared_ptr<AbstractTreeModel> model)
{
    return model->m_allItems.size();
}

bool KdenliveTests::effectFilterName(EffectFilter &filter, std::shared_ptr<TreeItem> item)
{
    return filter.filterName(item);
}
