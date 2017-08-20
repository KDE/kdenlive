#pragma once
#include "catch.hpp"
#include "doc/docundostack.hpp"
#include "bin/model/markerlistmodel.hpp"
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
#define private public
#define protected public
#include "timeline2/model/timelinemodel.hpp"
#include "project/projectmanager.h"
#include "core.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "bin/projectitemmodel.h"
#include "bin/projectfolder.h"
#include "bin/projectclip.h"
#include "bin/clipcreator.hpp"

using namespace fakeit;
#define RESET()                                                         \
    timMock.Reset();                                                    \
    Fake(Method(timMock, adjustAssetRange));                            \
    Spy(Method(timMock, _resetView));                                   \
    Spy(Method(timMock, _beginInsertRows));                             \
    Spy(Method(timMock, _beginRemoveRows));                             \
    Spy(Method(timMock, _endInsertRows));                               \
    Spy(Method(timMock, _endRemoveRows));                               \
    Spy(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, bool, bool, bool))); \
    Spy(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, QVector<int>)));

#define NO_OTHERS() \
    VerifyNoOtherInvocations(Method(timMock, _beginRemoveRows));  \
    VerifyNoOtherInvocations(Method(timMock, _beginInsertRows));  \
    VerifyNoOtherInvocations(Method(timMock, _endRemoveRows));    \
    VerifyNoOtherInvocations(Method(timMock, _endInsertRows));    \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, bool, bool, bool))); \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, QVector<int>))); \
    RESET();

#define CHECK_MOVE(times)                                         \
    Verify(Method(timMock, _beginRemoveRows) +                    \
           Method(timMock, _endRemoveRows) +                      \
           Method(timMock, _beginInsertRows) +                    \
           Method(timMock, _endInsertRows)                        \
        ).Exactly(times);                                         \
    NO_OTHERS();

#define CHECK_INSERT(times)                       \
    Verify(Method(timMock, _beginInsertRows) +    \
           Method(timMock, _endInsertRows)        \
        ).Exactly(times);                         \
    NO_OTHERS();

#define CHECK_REMOVE(times)                       \
    Verify(Method(timMock, _beginRemoveRows) +    \
           Method(timMock, _endRemoveRows)        \
        ).Exactly(times);                         \
    NO_OTHERS();

#define CHECK_RESIZE(times) \
    Verify(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, bool, bool, bool))).Exactly(times); \
    NO_OTHERS();

QString createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length = 20);
