/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include "abortutil.hpp"
#include "catch.hpp"
#include "tests_definitions.h"
#include <QString>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <mlt++/MltFactory.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>
// #define private public
// #define protected public
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effects/effectlist/model/effectfilter.hpp"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "project/projectmanager.h"
#include "src/render/renderrequest.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "transitions/transitionsrepository.hpp"

using namespace fakeit;
#define RESET(mock)                                                                                                                                            \
    mock.Reset();                                                                                                                                              \
    Fake(Method(mock, adjustAssetRange));                                                                                                                      \
    Spy(Method(mock, _beginInsertRows));                                                                                                                       \
    Spy(Method(mock, _beginRemoveRows));                                                                                                                       \
    Spy(Method(mock, _endInsertRows));                                                                                                                         \
    Spy(Method(mock, _endRemoveRows));                                                                                                                         \
    Spy(OverloadedMethod(mock, notifyChange, void(const QModelIndex &, const QModelIndex &, bool, bool, bool)));                                               \
    Spy(OverloadedMethod(mock, notifyChange, void(const QModelIndex &, const QModelIndex &, const QVector<int> &)));                                           \
    Spy(OverloadedMethod(mock, notifyChange, void(const QModelIndex &, const QModelIndex &, int)));

#define NO_OTHERS()                                                                                                                                            \
    VerifyNoOtherInvocations(Method(timMock, _beginRemoveRows));                                                                                               \
    VerifyNoOtherInvocations(Method(timMock, _beginInsertRows));                                                                                               \
    VerifyNoOtherInvocations(Method(timMock, _endRemoveRows));                                                                                                 \
    VerifyNoOtherInvocations(Method(timMock, _endInsertRows));                                                                                                 \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, bool, bool, bool)));                       \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, const QVector<int> &)));                   \
    RESET(timMock);

#define CHECK_MOVE(times)                                                                                                                                      \
    Verify(Method(timMock, _beginRemoveRows) + Method(timMock, _endRemoveRows) + Method(timMock, _beginInsertRows) + Method(timMock, _endInsertRows))          \
        .Exactly(times);                                                                                                                                       \
    NO_OTHERS();

#define CHECK_INSERT(times)                                                                                                                                    \
    Verify(Method(timMock, _beginInsertRows) + Method(timMock, _endInsertRows)).Exactly(times);                                                                \
    NO_OTHERS();

#define CHECK_REMOVE(times)                                                                                                                                    \
    Verify(Method(timMock, _beginRemoveRows) + Method(timMock, _endRemoveRows)).Exactly(times);                                                                \
    NO_OTHERS();

#define CHECK_RESIZE(times)                                                                                                                                    \
    Verify(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, const QVector<int> &))).Exactly(times);                      \
    NO_OTHERS();

#define CHECK_UPDATE(role)                                                                                                                                     \
    Verify(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, int))                                                        \
               .Matching([](const QModelIndex &, const QModelIndex &, int c) { return c == role; }))                                                           \
        .Exactly(1);                                                                                                                                           \
    NO_OTHERS();

class KdenliveTests
{
public:
    KdenliveTests(){};
    static QString createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length = 20, bool limited = true);

    static QString createProducerWithSound(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, int length = 10);

    static QString createTextProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, const QString &xmldata, const QString &clipname,
                                      int length = 10);

    static QString createAVProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel);

    static std::unique_ptr<QDomElement> getProperty(const QDomElement &element, const QString &name);

    static QStringList getAssetsServiceIds(const QDomDocument &doc, const QString &tagName);

    static void removeAssetsById(QDomDocument &doc, const QString &tagName, const QStringList &idsToDelete);
    static bool isMltBuildInLuma(const QString &lumaName);

    static bool updateTimeline(bool createNewTab, const QString &chunks, const QString &dirty, const QDateTime &documentDate, bool enablePreview);
    static void setAudioTargets(std::shared_ptr<TimelineItemModel> timeline, QMap<int, QString> audioInfo);
    static void setAudioTimelineTargets(std::shared_ptr<TimelineItemModel> timeline, QMap<int, int> audioInfo);
    static void setVideoTargets(std::shared_ptr<TimelineItemModel> timeline, int tid);
    static QStringList getAudioKey(const QString &binId, bool *ok);
    static void resetNextId(int id = 0);
    static int getNextId();
    static GroupsModel *groupsModel(std::shared_ptr<TimelineItemModel> timeline);
    static void destructGroupItem(std::shared_ptr<TimelineItemModel> timeline, int id, bool update, Fun &undo, Fun &redo);
    static std::shared_ptr<KeyframeModel> cloneModel(std::shared_ptr<KeyframeModel> original);
    static const std::shared_ptr<TrackModel> getTrackById_const(std::shared_ptr<TimelineItemModel> timeline, int tid);
    static QDomDocument getDocument(KdenliveDoc *openedDoc);
    static int downLinks(std::shared_ptr<TimelineItemModel> timeline, int gid);
    static int downLinksSize(std::shared_ptr<TimelineItemModel> timeline);
    static std::shared_ptr<ClipModel> getClipPtr(std::shared_ptr<TimelineItemModel> timeline, int clipId);
    static bool addKeyframe(std::shared_ptr<KeyframeModel> model, GenTime pos, KeyframeType type, QVariant value);
    static bool removeKeyframe(std::shared_ptr<KeyframeModel> model, GenTime pos);
    static bool removeAllKeyframes(std::shared_ptr<KeyframeModel> model);
    static void forceClipAudio(std::shared_ptr<TimelineItemModel> timeline, int clipId);
    static int groupsCount(std::shared_ptr<TimelineItemModel> timeline);
    static std::unordered_map<int, int> groupUpLink(std::shared_ptr<TimelineItemModel> timeline);
    static void setGroupType(std::shared_ptr<TimelineItemModel> timeline, int gid, GroupType type);
    static std::shared_ptr<TimelineItemModel> createTimelineModel(const QUuid uuid, std::shared_ptr<DocUndoStack> undoStack);
    static void finishTimelineConstruct(std::shared_ptr<TimelineItemModel> timeline);
    static bool requestClipCreation(std::shared_ptr<TimelineItemModel> timeline, const QString &binClipId, int &id, PlaylistState::ClipState state,
                                    int audioStream, double speed, bool warp_pitch, Fun &undo, Fun &redo);
    static void makeFiniteClipEnd(std::shared_ptr<TimelineItemModel> timeline, int cid);
    static void setRenderRequestBounds(RenderRequest *r, int in, int out);
    static void initRenderRepository();
    static bool checkModelConsistency(std::shared_ptr<AbstractTreeModel> model);
    static int modelSize(std::shared_ptr<AbstractTreeModel> model);
    static bool effectFilterName(EffectFilter &filter, std::shared_ptr<TreeItem> item);
};
