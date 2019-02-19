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
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/trackmodel.hpp"
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

} // namespace

void fuzz(const std::string &input)
{
    Logger::init();
    std::stringstream ss;
    ss << input;

    auto repo = Mlt::Factory::init(NULL);
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

    std::unordered_map<std::shared_ptr<TimelineModel>, std::vector<int>> all_clips, all_tracks, all_compositions;
    auto update_elems = [&]() {
        all_clips.clear();
        all_tracks.clear();
        all_compositions.clear();
        for (const auto &timeline : all_timelines) {
            all_clips[timeline] = {};
            all_tracks[timeline] = {};
            all_compositions[timeline] = {};
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
        }
    };
    auto get_timeline = [&]() -> std::shared_ptr<TimelineModel> {
        if (all_timelines.size() == 0) return nullptr;
        int id = 0;
        ss >> id;
        id %= all_timelines.size();
        if (id < 0) {
            id += all_timelines.size();
        }
        return all_timelines[id];
    };
    std::string c;

    while (ss >> c) {
        if (Logger::back_translation_table.count(c) > 0) {
            c = Logger::back_translation_table[c];
            // std::cout << "found=" << c << std::endl;
            if (c == "constr_TimelineModel") {
                all_timelines.emplace_back(TimelineItemModel::construct(&profile, guideModel, undoStack));
            } else if (c == "constr_TrackModel") {
                auto timeline = get_timeline();
                int pos = 0;
                std::string name;
                bool audio = false;
                ss >> pos >> name >> audio;
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
            }
            update_elems();
        }
    }
}
