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

#include "logger.hpp"
#include "bin/projectitemmodel.h"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include <QString>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>
#pragma GCC diagnostic pop

thread_local bool Logger::is_executing = false;
std::mutex Logger::mut;
std::vector<rttr::variant> Logger::operations;
std::vector<Logger::Invok> Logger::invoks;
std::unordered_map<std::string, std::vector<Logger::Constr>> Logger::constr;
std::unordered_map<std::string, std::string> Logger::translation_table;
std::unordered_map<std::string, std::string> Logger::back_translation_table;
int Logger::dump_count = 0;

thread_local size_t Logger::result_awaiting = INT_MAX;

void Logger::init()
{
    std::string cur_ind = "a";
    auto incr_ind = [&](auto &&self, size_t i = 0) {
        if (i >= cur_ind.size()) {
            cur_ind += "a";
            return;
        }
        if (cur_ind[i] == 'z') {
            cur_ind[i] = 'A';
        } else if (cur_ind[i] == 'Z') {
            cur_ind[i] = 'a';
            self(self, i + 1);
        } else {
            cur_ind[i]++;
        }
        if (cur_ind == "u" || cur_ind == "r") {
            // reserved for undo and redo
            self(self, i);
        }
    };

    for (const auto &o : {"TimelineModel", "TrackModel", "test_producer", "test_producer_sound", "ClipModel"}) {
        translation_table[std::string("constr_") + o] = cur_ind;
        incr_ind(incr_ind);
    }

    for (const auto &m : rttr::type::get<TimelineModel>().get_methods()) {
        translation_table[m.get_name().to_string()] = cur_ind;
        incr_ind(incr_ind);
    }

    for (const auto &m : rttr::type::get<TimelineFunctions>().get_methods()) {
        translation_table[m.get_name().to_string()] = cur_ind;
        incr_ind(incr_ind);
    }

    for (const auto &i : translation_table) {
        back_translation_table[i.second] = i.first;
    }
}

bool Logger::start_logging()
{
    std::unique_lock<std::mutex> lk(mut);
    if (is_executing) {
        return false;
    }
    is_executing = true;
    return true;
}
void Logger::stop_logging()
{
    std::unique_lock<std::mutex> lk(mut);
    is_executing = false;
}
std::string Logger::get_ptr_name(const rttr::variant &ptr)
{
    if (ptr.can_convert<TimelineModel *>()) {
        return "timeline_" + std::to_string(get_id_from_ptr(ptr.convert<TimelineModel *>()));
    } else if (ptr.can_convert<ProjectItemModel *>()) {
        return "binModel";
    } else if (ptr.can_convert<TimelineItemModel *>()) {
        return "timeline_" + std::to_string(get_id_from_ptr(static_cast<TimelineModel *>(ptr.convert<TimelineItemModel *>())));
    } else {
        std::cout << "Error: unhandled ptr type " << ptr.get_type().get_name().to_string() << std::endl;
    }
    return "unknown";
}

void Logger::log_res(rttr::variant result)
{
    std::unique_lock<std::mutex> lk(mut);
    Q_ASSERT(result_awaiting < invoks.size());
    invoks[result_awaiting].res = std::move(result);
}

void Logger::log_create_producer(const std::string &type, std::vector<rttr::variant> args)
{
    std::unique_lock<std::mutex> lk(mut);
    for (auto &a : args) {
        // this will rewove shared/weak/unique ptrs
        if (a.get_type().is_wrapper()) {
            a = a.extract_wrapped_value();
        }
        const std::string class_name = a.get_type().get_name().to_string();
    }
    constr[type].push_back({type, std::move(args)});
    operations.emplace_back(ConstrId{type, constr[type].size() - 1});
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
std::string quoted(const std::string &input)
{
#if __cpp_lib_quoted_string_io
    std::stringstream ss;
    ss << std::quoted(input);
    return ss.str();
#else
    // very incomplete implem
    return "\"" + input + "\"";
#endif
}
} // namespace

void Logger::print_trace()
{
    dump_count++;
    auto process_args = [&](const std::vector<rttr::variant> &args, const std::unordered_set<size_t> &refs = {}) {
        std::stringstream ss;
        bool deb = true;
        size_t i = 0;
        for (const auto &a : args) {
            if (deb) {
                deb = false;
                i = 0;
            } else {
                ss << ", ";
                ++i;
            }
            if (refs.count(i) > 0) {
                ss << "dummy_" << i;
            } else if (a.get_type() == rttr::type::get<int>()) {
                ss << a.convert<int>();
            } else if (a.get_type() == rttr::type::get<double>()) {
                ss << a.convert<double>();
            } else if (a.get_type() == rttr::type::get<float>()) {
                ss << a.convert<float>();
            } else if (a.get_type() == rttr::type::get<size_t>()) {
                ss << a.convert<size_t>();
            } else if (a.get_type() == rttr::type::get<bool>()) {
                ss << (a.convert<bool>() ? "true" : "false");
            } else if (a.get_type().is_enumeration()) {
                auto e = a.get_type().get_enumeration();
                ss << e.get_name().to_string() << "::" << a.convert<std::string>();
            } else if (a.can_convert<QString>()) {
                ss << quoted(a.convert<QString>().toStdString());
            } else if (a.can_convert<std::string>()) {
                ss << quoted(a.convert<std::string>());
            } else if (a.can_convert<std::unordered_set<int>>()) {
                auto set = a.convert<std::unordered_set<int>>();
                ss << "{";
                bool beg = true;
                for (int s : set) {
                    if (beg)
                        beg = false;
                    else
                        ss << ", ";
                    ss << s;
                }
                ss << "}";
            } else if (a.get_type().is_pointer()) {
                ss << get_ptr_name(a);
            } else {
                std::cout << "Error: unhandled arg type " << a.get_type().get_name().to_string() << std::endl;
            }
        }
        return ss.str();
    };
    auto process_args_fuzz = [&](const std::vector<rttr::variant> &args, const std::unordered_set<size_t> &refs = {}) {
        std::stringstream ss;
        bool deb = true;
        size_t i = 0;
        for (const auto &a : args) {
            if (deb) {
                deb = false;
                i = 0;
            } else {
                ss << " ";
                ++i;
            }
            if (refs.count(i) > 0) {
                continue;
            } else if (a.get_type() == rttr::type::get<int>()) {
                ss << a.convert<int>();
            } else if (a.get_type() == rttr::type::get<double>()) {
                ss << a.convert<double>();
            } else if (a.get_type() == rttr::type::get<float>()) {
                ss << a.convert<float>();
            } else if (a.get_type() == rttr::type::get<size_t>()) {
                ss << a.convert<size_t>();
            } else if (a.get_type() == rttr::type::get<bool>()) {
                ss << (a.convert<bool>() ? "1" : "0");
            } else if (a.get_type().is_enumeration()) {
                ss << a.convert<int>();
            } else if (a.can_convert<QString>()) {
                std::string out = a.convert<QString>().toStdString();
                if (out.empty()) {
                    out = "$$";
                }
                ss << out;
            } else if (a.can_convert<std::string>()) {
                std::string out = a.convert<std::string>();
                if (out.empty()) {
                    out = "$$";
                }
                ss << out;
            } else if (a.can_convert<std::unordered_set<int>>()) {
                auto set = a.convert<std::unordered_set<int>>();
                ss << set.size() << " ";
                bool beg = true;
                for (int s : set) {
                    if (beg)
                        beg = false;
                    else
                        ss << " ";
                    ss << s;
                }
            } else if (a.get_type().is_pointer()) {
                if (a.can_convert<TimelineModel *>()) {
                    ss << get_id_from_ptr(a.convert<TimelineModel *>());
                } else if (a.can_convert<TimelineItemModel *>()) {
                    ss << get_id_from_ptr(static_cast<TimelineModel *>(a.convert<TimelineItemModel *>()));
                } else if (a.can_convert<ProjectItemModel *>()) {
                    // only one binModel, we skip the parameter since it's unambiguous
                } else {
                    std::cout << "Error: unhandled ptr type " << a.get_type().get_name().to_string() << std::endl;
                }
            } else {
                std::cout << "Error: unhandled arg type " << a.get_type().get_name().to_string() << std::endl;
            }
        }
        return ss.str();
    };
    std::ofstream fuzz_file;
    fuzz_file.open("fuzz_case_" + std::to_string(dump_count) + ".txt");
    std::ofstream test_file;
    test_file.open("test_case_" + std::to_string(dump_count) + ".cpp");
    test_file << "TEST_CASE(\"Regression\") {" << std::endl;
    test_file << "auto binModel = pCore->projectItemModel();" << std::endl;
    test_file << "binModel->clean();" << std::endl;
    test_file << "std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);" << std::endl;
    test_file << "std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);" << std::endl;
    test_file << "TimelineModel::next_id = 0;" << std::endl;
    test_file << "{" << std::endl;
    test_file << "Mock<ProjectManager> pmMock;" << std::endl;
    test_file << "When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);" << std::endl;
    test_file << "ProjectManager &mocked = pmMock.get();" << std::endl;
    test_file << "pCore->m_projectManager = &mocked;" << std::endl;

    size_t nbrConstructedTimelines = 0;
    auto check_consistancy = [&]() {
        for (size_t i = 0; i < nbrConstructedTimelines; ++i) {
            test_file << "REQUIRE(timeline_" << i << "->checkConsistency());" << std::endl;
        }
    };
    for (const auto &o : operations) {
        bool isUndo = false;
        if (o.can_convert<Logger::Undo>()) {
            isUndo = true;
            Undo undo = o.convert<Logger::Undo>();
            if (undo.undo) {
                test_file << "undoStack->undo();" << std::endl;
                fuzz_file << "u" << std::endl;
            } else {
                test_file << "undoStack->redo();" << std::endl;
                fuzz_file << "r" << std::endl;
            }
        } else if (o.can_convert<Logger::InvokId>()) {
            InvokId id = o.convert<Logger::InvokId>();
            Invok &invok = invoks[id.id];
            std::unordered_set<size_t> refs;
            bool is_static = false;
            rttr::method m = invok.ptr.get_type().get_method(invok.method);
            if (!m.is_valid()) {
                is_static = true;
                m = rttr::type::get_by_name("TimelineFunctions").get_method(invok.method);
            }
            if (!m.is_valid()) {
                std::cout << "ERROR: unknown method " << invok.method << std::endl;
                continue;
            }
            test_file << "{" << std::endl;
            for (const auto &a : m.get_parameter_infos()) {
                if (isIthParamARef(m, a.get_index())) {
                    refs.insert(a.get_index());
                    test_file << a.get_type().get_name().to_string() << " dummy_" << std::to_string(a.get_index()) << ";" << std::endl;
                }
            }

            if (m.get_return_type() != rttr::type::get<void>()) {
                test_file << m.get_return_type().get_name().to_string() << " res = ";
            }
            if (is_static) {
                test_file << "TimelineFunctions::" << invok.method << "(" << get_ptr_name(invok.ptr) << ", " << process_args(invok.args, refs) << ");"
                          << std::endl;
            } else {
                test_file << get_ptr_name(invok.ptr) << "->" << invok.method << "(" << process_args(invok.args, refs) << ");" << std::endl;
            }
            if (m.get_return_type() != rttr::type::get<void>() && invok.res.is_valid()) {
                test_file << "REQUIRE( res == " << invok.res.to_string() << ");" << std::endl;
            }
            test_file << "}" << std::endl;

            std::string invok_name = invok.method;
            if (translation_table.count(invok_name) > 0) {
                auto args = invok.args;
                if (rttr::type::get<TimelineModel>().get_method(invok_name).is_valid() ||
                    rttr::type::get<TimelineFunctions>().get_method(invok_name).is_valid()) {
                    args.insert(args.begin(), invok.ptr);
                    // adding an arg just messed up the references
                    std::unordered_set<size_t> new_refs;
                    for (const size_t &r : refs) {
                        new_refs.insert(r + 1);
                    }
                    std::swap(refs, new_refs);
                }
                fuzz_file << translation_table[invok_name] << " " << process_args_fuzz(args, refs) << std::endl;
            } else {
                std::cout << "ERROR: unknown method " << invok_name << std::endl;
            }

        } else if (o.can_convert<Logger::ConstrId>()) {
            ConstrId id = o.convert<Logger::ConstrId>();
            std::string constr_name = std::string("constr_") + id.type;
            if (translation_table.count(constr_name) > 0) {
                fuzz_file << translation_table[constr_name] << " " << process_args_fuzz(constr[id.type][id.id].second) << std::endl;
            } else {
                std::cout << "ERROR: unknown constructor " << constr_name << std::endl;
            }
            if (id.type == "TimelineModel") {
                test_file << "TimelineItemModel tim_" << id.id << "(&reg_profile, undoStack);" << std::endl;
                test_file << "Mock<TimelineItemModel> timMock_" << id.id << "(tim_" << id.id << ");" << std::endl;
                test_file << "auto timeline_" << id.id << " = std::shared_ptr<TimelineItemModel>(&timMock_" << id.id << ".get(), [](...) {});" << std::endl;
                test_file << "TimelineItemModel::finishConstruct(timeline_" << id.id << ", guideModel);" << std::endl;
                test_file << "Fake(Method(timMock_" << id.id << ", adjustAssetRange));" << std::endl;
                nbrConstructedTimelines++;
            } else if (id.type == "TrackModel") {
                std::string params = process_args(constr[id.type][id.id].second);
                test_file << "TrackModel::construct(" << params << ");" << std::endl;
            } else if (id.type == "ClipModel") {
                std::string params = process_args(constr[id.type][id.id].second);
                test_file << "ClipModel::construct(" << params << ");" << std::endl;
            } else if (id.type == "test_producer") {
                std::string params = process_args(constr[id.type][id.id].second);
                test_file << "createProducer(reg_profile, " << params << ");" << std::endl;
            } else if (id.type == "test_producer_sound") {
                std::string params = process_args(constr[id.type][id.id].second);
                test_file << "createProducerWithSound(reg_profile, " << params << ");" << std::endl;
            } else {
                std::cout << "Error: unknown constructor " << id.type << std::endl;
            }
        } else {
            std::cout << "Error: unknown operation" << std::endl;
        }
        check_consistancy();
        if (!isUndo) {
            test_file << "undoStack->undo();" << std::endl;
            check_consistancy();
            test_file << "undoStack->redo();" << std::endl;
            check_consistancy();
        }
    }
    test_file << "}" << std::endl;
    test_file << "pCore->m_projectManager = nullptr;" << std::endl;
    test_file << "}" << std::endl;
}
void Logger::clear()
{
    is_executing = false;
    invoks.clear();
    operations.clear();
    constr.clear();
}

LogGuard::LogGuard()
{
    m_hasGuard = Logger::start_logging();
}
LogGuard::~LogGuard()
{
    if (m_hasGuard) {
        Logger::stop_logging();
    }
}
bool LogGuard::hasGuard() const
{
    return m_hasGuard;
}

void Logger::log_undo(bool undo)
{
    Logger::Undo u;
    u.undo = undo;
    operations.push_back(u);
}
