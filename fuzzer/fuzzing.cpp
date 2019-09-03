/***************************************************************************
 *   Copyright (C) 2019 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "fuzzing.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "doc/docundostack.hpp"
#include "fakeit_standalone.hpp"
#include "logger.hpp"
#include <mlt++/MltFactory.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>
#include <sstream>
#define private public
#define protected public
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "mltconnection.h"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/trackmodel.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>
#pragma GCC diagnostic pop

using namespace fakeit;

namespace {
QString createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length, bool limited)
{
    Logger::log_create_producer("test_producer", {color, binModel, length, limited});
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "color", color.c_str());
    producer->set("length", length);
    producer->set("out", length - 1);

    Q_ASSERT(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    if (limited) {
        binClip->forceLimitedDuration();
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    Q_ASSERT(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}

QString createProducerWithSound(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel)
{
    Logger::log_create_producer("test_producer_sound", {binModel});
    // std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof,
    // QFileInfo("../tests/small.mkv").absoluteFilePath().toStdString().c_str());

    // In case the test system does not have avformat support, we can switch to the integrated blipflash producer
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "blipflash");
    producer->set_in_and_out(0, 1);
    producer->set("kdenlive:duration", 2);

    Q_ASSERT(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    Q_ASSERT(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}
inline int modulo(int a, int b)
{
    const int result = a % b;
    return result >= 0 ? result : result + b;
}
namespace {
bool isIthParamARef(const rttr::method &method, size_t i)
{
    QString sig = QString::fromStdString(method.get_signature().to_string());
    int deb = sig.indexOf("(");
    int end = sig.lastIndexOf(")");
    sig = sig.mid(deb + 1, deb - end - 1);
    QStringList args = sig.split(QStringLiteral(","));
    return args[(int)i].contains("&") && !args[(int)i].contains("const &");
}
} // namespace
} // namespace

void fuzz(const std::string &input)
{
    Logger::init();
    Logger::clear();
    std::stringstream ss;
    ss << input;

    Mlt::Profile profile;
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    std::vector<std::shared_ptr<TimelineModel>> all_timelines;

    std::unordered_map<std::shared_ptr<TimelineModel>, std::vector<int>> all_clips, all_tracks, all_compositions, all_groups;
    auto update_elems = [&]() {
        all_clips.clear();
        all_tracks.clear();
        all_compositions.clear();
        for (const auto &timeline : all_timelines) {
            all_clips[timeline] = {};
            all_tracks[timeline] = {};
            all_compositions[timeline] = {};
            all_groups[timeline] = {};
            auto &clips = all_clips[timeline];
            clips.clear();
            for (const auto &c : timeline->m_allClips) {
                clips.push_back(c.first);
            }
            std::sort(clips.begin(), clips.end());

            auto &compositions = all_compositions[timeline];
            compositions.clear();
            for (const auto &c : timeline->m_allCompositions) {
                compositions.push_back(c.first);
            }
            std::sort(compositions.begin(), compositions.end());

            auto &tracks = all_tracks[timeline];
            tracks.clear();
            for (const auto &c : timeline->m_iteratorTable) {
                tracks.push_back(c.first);
            }
            std::sort(tracks.begin(), tracks.end());

            auto &groups = all_groups[timeline];
            groups.clear();
            for (int c : timeline->m_allGroups) {
                groups.push_back(c);
            }
            std::sort(groups.begin(), groups.end());
        }
    };
    auto get_timeline = [&]() -> std::shared_ptr<TimelineModel> {
        int id = 0;
        ss >> id;
        if (all_timelines.size() == 0) return nullptr;
        id = modulo(id, (int)all_timelines.size());
        return all_timelines[size_t(id)];
    };
    auto get_group = [&](std::shared_ptr<TimelineModel> timeline) {
        int id = 0;
        ss >> id;
        if (!timeline) return -1;
        if (timeline->isGroup(id)) return id;
        if (all_timelines.size() == 0) return -1;
        if (all_groups.count(timeline) == 0) return -1;
        if (all_groups[timeline].size() == 0) return -1;
        id = modulo(id, (int)all_groups[timeline].size());
        return all_groups[timeline][id];
    };
    auto get_clip = [&](std::shared_ptr<TimelineModel> timeline) {
        int id = 0;
        ss >> id;
        if (!timeline) return -1;
        if (timeline->isClip(id)) return id;
        if (all_timelines.size() == 0) return -1;
        if (all_clips.count(timeline) == 0) return -1;
        if (all_clips[timeline].size() == 0) return -1;
        id = modulo(id, (int)all_clips[timeline].size());
        return all_clips[timeline][id];
    };
    auto get_compo = [&](std::shared_ptr<TimelineModel> timeline) {
        int id = 0;
        ss >> id;
        if (!timeline) return -1;
        if (timeline->isComposition(id)) return id;
        if (all_timelines.size() == 0) return -1;
        if (all_compositions.count(timeline) == 0) return -1;
        if (all_compositions[timeline].size() == 0) return -1;
        id = modulo(id, (int)all_compositions[timeline].size());
        return all_compositions[timeline][id];
    };
    auto get_item = [&](std::shared_ptr<TimelineModel> timeline) {
        int id = 0;
        ss >> id;
        if (!timeline) return -1;
        if (timeline->isClip(id)) return id;
        if (timeline->isComposition(id)) return id;
        if (all_timelines.size() == 0) return -1;
        int clip_count = 0;
        if (all_clips.count(timeline) > 0) {
            clip_count = all_clips[timeline].size();
        }
        int compo_count = 0;
        if (all_compositions.count(timeline) > 0) {
            compo_count = all_compositions[timeline].size();
        }
        if (clip_count + compo_count == 0) return -1;
        id = modulo(id, clip_count + compo_count);
        if (id < clip_count) {
            return all_clips[timeline][id];
        }
        return all_compositions[timeline][id - clip_count];
    };
    auto get_track = [&](std::shared_ptr<TimelineModel> timeline) {
        int id = 0;
        ss >> id;
        if (!timeline) return -1;
        if (timeline->isTrack(id)) return id;
        if (all_timelines.size() == 0) return -1;
        if (all_tracks.count(timeline) == 0) return -1;
        if (all_tracks[timeline].size() == 0) return -1;
        id = modulo(id, (int)all_tracks[timeline].size());
        return all_tracks[timeline][id];
    };
    std::string c;

    while (ss >> c) {
        if (c == "u") {
            std::cout << "UNDOING" << std::endl;
            undoStack->undo();
        } else if (c == "r") {
            std::cout << "REDOING" << std::endl;
            undoStack->redo();
        } else if (Logger::back_translation_table.count(c) > 0) {
            // std::cout << "found=" << c;
            c = Logger::back_translation_table[c];
            // std::cout << " translated=" << c << std::endl;
            if (c == "constr_TimelineModel") {
                all_timelines.emplace_back(TimelineItemModel::construct(&profile, guideModel, undoStack));
            } else if (c == "constr_ClipModel") {
                auto timeline = get_timeline();
                int id = 0, state_id;
                double speed = 1;
                PlaylistState::ClipState state = PlaylistState::VideoOnly;
                std::string binId;
                ss >> binId >> id >> state_id >> speed;
                QString binClip = QString::fromStdString(binId);
                bool valid = true;
                if (!pCore->projectItemModel()->hasClip(binClip)) {
                    if (pCore->projectItemModel()->getAllClipIds().size() == 0) {
                        valid = false;
                    } else {
                        binClip = pCore->projectItemModel()->getAllClipIds()[0];
                    }
                }
                state = static_cast<PlaylistState::ClipState>(state_id);
                if (timeline && valid) {
                    ClipModel::construct(timeline, binClip, -1, state, speed);
                }
            } else if (c == "constr_TrackModel") {
                auto timeline = get_timeline();
                int id, pos = 0;
                std::string name;
                bool audio = false;
                ss >> id >> pos >> name >> audio;
                if (name == "$$") {
                    name = "";
                }
                if (pos < -1) pos = 0;
                pos = std::min((int)all_tracks[timeline].size(), pos);
                if (timeline) {
                    TrackModel::construct(timeline, -1, pos, QString::fromStdString(name), audio);
                }
            } else if (c == "constr_test_producer") {
                std::string color;
                int length = 0;
                bool limited = false;
                ss >> color >> length >> limited;
                createProducer(profile, color, binModel, length, limited);
            } else if (c == "constr_test_producer_sound") {
                createProducerWithSound(profile, binModel);
            } else {
                // std::cout << "executing " << c << std::endl;
                rttr::type target_type = rttr::type::get<int>();
                bool found = false;
                for (const std::string &t : {"TimelineModel", "TimelineFunctions"}) {
                    rttr::type current_type = rttr::type::get_by_name(t);
                    // std::cout << "type " << t << " has methods count=" << current_type.get_methods().size() << std::endl;
                    if (current_type.get_method(c).is_valid()) {
                        found = true;
                        target_type = current_type;
                        break;
                    }
                }
                if (found) {
                    // std::cout << "found!" << std::endl;
                    bool valid = true;
                    rttr::method target_method = target_type.get_method(c);
                    std::vector<rttr::variant> arguments;
                    rttr::variant ptr;
                    if (target_type == rttr::type::get<TimelineModel>()) {
                        if (all_timelines.size() == 0) {
                            valid = false;
                        }
                        ptr = get_timeline();
                    }
                    int i = -1;
                    for (const auto &p : target_method.get_parameter_infos()) {
                        ++i;
                        std::string arg_name = p.get_name().to_string();
                        // std::cout << arg_name << std::endl;
                        if (arg_name == "compoId") {
                            std::shared_ptr<TimelineModel> tim =
                                (ptr.can_convert<std::shared_ptr<TimelineModel>>() ? ptr.convert<std::shared_ptr<TimelineModel>>() : nullptr);
                            int compoId = get_compo(tim);
                            valid = valid && (compoId >= 0);
                            // std::cout << "got compo" << compoId << std::endl;
                            arguments.emplace_back(compoId);
                        } else if (arg_name == "clipId") {
                            std::shared_ptr<TimelineModel> tim =
                                (ptr.can_convert<std::shared_ptr<TimelineModel>>() ? ptr.convert<std::shared_ptr<TimelineModel>>() : nullptr);
                            int clipId = get_clip(tim);
                            valid = valid && (clipId >= 0);
                            arguments.emplace_back(clipId);
                            // std::cout << "got clipId" << clipId << std::endl;
                        } else if (arg_name == "trackId") {
                            std::shared_ptr<TimelineModel> tim =
                                (ptr.can_convert<std::shared_ptr<TimelineModel>>() ? ptr.convert<std::shared_ptr<TimelineModel>>() : nullptr);
                            int trackId = get_track(tim);
                            valid = valid && (trackId >= 0);
                            arguments.emplace_back(trackId);
                            // std::cout << "got trackId" << trackId << std::endl;
                        } else if (arg_name == "itemId") {
                            std::shared_ptr<TimelineModel> tim =
                                (ptr.can_convert<std::shared_ptr<TimelineModel>>() ? ptr.convert<std::shared_ptr<TimelineModel>>() : nullptr);
                            int itemId = get_item(tim);
                            valid = valid && (itemId >= 0);
                            arguments.emplace_back(itemId);
                            // std::cout << "got itemId" << itemId << std::endl;
                        } else if (arg_name == "groupId") {
                            std::shared_ptr<TimelineModel> tim =
                                (ptr.can_convert<std::shared_ptr<TimelineModel>>() ? ptr.convert<std::shared_ptr<TimelineModel>>() : nullptr);
                            int groupId = get_group(tim);
                            valid = valid && (groupId >= 0);
                            arguments.emplace_back(groupId);
                            // std::cout << "got clipId" << clipId << std::endl;
                        } else if (arg_name == "logUndo") {
                            bool a = false;
                            ss >> a;
                            // we enforce undo logging
                            a = true;
                            arguments.emplace_back(a);
                        } else if (arg_name == "itemIds") {
                            int count = 0;
                            ss >> count;
                            // std::cout << "got ids. going to read count=" << count << std::endl;
                            if (count > 0) {
                                std::shared_ptr<TimelineModel> tim =
                                    (ptr.can_convert<std::shared_ptr<TimelineModel>>() ? ptr.convert<std::shared_ptr<TimelineModel>>() : nullptr);
                                std::unordered_set<int> ids;
                                for (int i = 0; i < count; ++i) {
                                    int itemId = get_item(tim);
                                    // std::cout << "\t read" << itemId << std::endl;
                                    valid = valid && (itemId >= 0);
                                    ids.insert(itemId);
                                }
                                arguments.emplace_back(ids);
                            } else {
                                valid = false;
                            }
                        } else if (!isIthParamARef(target_method, i)) {
                            rttr::type arg_type = p.get_type();
                            if (arg_type == rttr::type::get<int>()) {
                                int a = 0;
                                ss >> a;
                                // std::cout << "read int " << a << std::endl;
                                arguments.emplace_back(a);
                            } else if (arg_type == rttr::type::get<size_t>()) {
                                size_t a = 0;
                                ss >> a;
                                arguments.emplace_back(a);
                            } else if (arg_type == rttr::type::get<double>()) {
                                double a = 0;
                                ss >> a;
                                arguments.emplace_back(a);
                            } else if (arg_type == rttr::type::get<float>()) {
                                float a = 0;
                                ss >> a;
                                arguments.emplace_back(a);
                            } else if (arg_type == rttr::type::get<bool>()) {
                                bool a = false;
                                ss >> a;
                                // std::cout << "read bool " << a << std::endl;
                                arguments.emplace_back(a);
                            } else if (arg_type == rttr::type::get<QString>()) {
                                std::string str = "";
                                ss >> str;
                                // std::cout << "read str " << str << std::endl;
                                if (str == "$$") {
                                    str = "";
                                }
                                arguments.emplace_back(QString::fromStdString(str));
                            } else if (arg_type == rttr::type::get<std::shared_ptr<TimelineItemModel>>()) {
                                auto timeline = get_timeline();
                                if (timeline) {
                                    // std::cout << "got timeline" << std::endl;
                                    auto timeline2 = std::dynamic_pointer_cast<TimelineItemModel>(timeline);
                                    arguments.emplace_back(timeline2);
                                    ptr = timeline;
                                } else {
                                    // std::cout << "didn't get timeline" << std::endl;
                                    valid = false;
                                }
                            } else if (arg_type.is_enumeration()) {
                                int a = 0;
                                ss >> a;
                                rttr::variant var_a = a;
                                var_a.convert((const rttr::type &)arg_type);
                                // std::cout << "read enum " << arg_type.get_enumeration().value_to_name(var_a).to_string() << std::endl;
                                arguments.push_back(var_a);
                            } else {
                                std::cout << "ERROR: unsupported arg type " << arg_type.get_name().to_string() << std::endl;
                                assert(false);
                            }
                        } else {
                            if (p.get_type() == rttr::type::get<int>()) {
                                arguments.emplace_back(-1);
                            } else {
                                assert(false);
                            }
                        }
                    }
                    if (valid) {
                        std::cout << "VALID!!! " << target_method.get_name().to_string() << std::endl;
                        std::vector<rttr::argument> args;
                        args.reserve(arguments.size());
                        for (auto &a : arguments) {
                            args.emplace_back(a);
                            // std::cout << "argument=" << a.get_type().get_name().to_string() << std::endl;
                        }
                        for (const auto &p : target_method.get_parameter_infos()) {
                            // std::cout << "expected=" << p.get_type().get_name().to_string() << std::endl;
                        }
                        rttr::variant res = target_method.invoke_variadic(ptr, args);
                        if (res.is_valid()) {
                            std::cout << "SUCCESS!!!" << std::endl;
                        } else {
                            std::cout << "!!!FAILLLLLL!!!" << std::endl;
                        }
                    }
                }
            }
        }
        update_elems();
        for (const auto &t : all_timelines) {
            assert(t->checkConsistency());
        }
    }
    undoStack->clear();
    all_clips.clear();
    all_tracks.clear();
    all_compositions.clear();
    all_groups.clear();
    for (auto &all_timeline : all_timelines) {
        all_timeline.reset();
    }

    pCore->m_projectManager = nullptr;
    Core::m_self.reset();
    MltConnection::m_self.reset();
    std::cout << "---------------------------------------------------------------------------------------------------------------------------------------------"
                 "---------------"
              << std::endl;
}
