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
#include "timeline2/model/timelinemodel.hpp"
#include <QString>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

thread_local bool Logger::is_executing = false;
std::mutex Logger::mut;
std::vector<rttr::variant> Logger::operations;
std::vector<Logger::Invok> Logger::invoks;
std::unordered_map<std::string, std::vector<Logger::Constr>> Logger::constr;
thread_local size_t Logger::result_awaiting = INT_MAX;

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
std::string Logger::get_ptr_name(rttr::variant ptr)
{
    if (ptr.can_convert<TimelineModel *>()) {
        return "timeline_" + std::to_string(get_id_from_ptr(ptr.convert<TimelineModel *>()));
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

void Logger::print_trace()
{
    auto process_args = [&](const std::vector<rttr::variant> &args, const std::unordered_set<size_t> &refs = {}) {
        std::stringstream ss;
        bool deb = true;
        size_t i = 0;
        for (const auto &a : args) {
            if (deb) {
                deb = false;
            } else {
                ss << ", ";
            }
            if (refs.count(i) > 0) {
                ss << "dummy_" << i;
            } else if (a.get_type() == rttr::type::get<int>()) {
                ss << a.convert<int>();
            } else if (a.can_convert<bool>()) {
                ss << (a.convert<bool>() ? "true" : "false");
            } else if (a.can_convert<QString>()) {
                  // Not supported in c++ < 14
                  // ss << std::quoted(a.convert<QString>().toStdString());
                  ss << "\" "<<a.convert<QString>().toStdString()<<" \"";
            } else if (a.get_type().is_pointer()) {
                ss << get_ptr_name(a);
            } else {
                std::cout << "Error: unhandled arg type " << a.get_type().get_name().to_string() << std::endl;
            }
            ++i;
        }
        return ss.str();
    };
    std::ofstream test_file;
    test_file.open("test_case.cpp");
    // Not supported on GCC < 5.1
    // auto test_file = std::ofstream("test_case.cpp");
    test_file << "TEST_CASE(\"Regression\") {" << std::endl;
    test_file << "auto binModel = pCore->projectItemModel();" << std::endl;
    test_file << "std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);" << std::endl;
    test_file << "std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);" << std::endl;
    test_file << "Mock<ProjectManager> pmMock;" << std::endl;
    test_file << "When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);" << std::endl;
    test_file << "ProjectManager &mocked = pmMock.get();" << std::endl;
    test_file << "pCore->m_projectManager = &mocked;" << std::endl;

    auto check_consistancy = [&]() {
        if (constr.count("TimelineModel") > 0) {
            for (size_t i = 0; i < constr["TimelineModel"].size(); ++i) {
                test_file << "REQUIRE(timeline_" << i << "->checkConsistency());" << std::endl;
            }
        }
    };
    for (const auto &o : operations) {
        if (o.can_convert<Logger::InvokId>()) {
            InvokId id = o.convert<Logger::InvokId>();
            Invok &invok = invoks[id.id];
            std::unordered_set<size_t> refs;
            rttr::method m = invok.ptr.get_type().get_method(invok.method);
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
            test_file << get_ptr_name(invok.ptr) << "->" << invok.method << "(" << process_args(invok.args, refs) << ");" << std::endl;
            if (m.get_return_type() != rttr::type::get<void>() && invok.res.is_valid()) {
                test_file << "REQUIRE( res == " << invok.res.to_string() << ");" << std::endl;
            }
            test_file << "}" << std::endl;

        } else if (o.can_convert<Logger::ConstrId>()) {
            ConstrId id = o.convert<Logger::ConstrId>();
            if (id.type == "TimelineModel") {
                test_file << "TimelineItemModel tim_" << id.id << "(new Mlt::Profile(), undoStack);" << std::endl;
                test_file << "Mock<TimelineItemModel> timMock_" << id.id << "(tim_" << id.id << ");" << std::endl;
                test_file << "auto timeline_" << id.id << " = std::shared_ptr<TimelineItemModel>(&timMock_" << id.id << ".get(), [](...) {});" << std::endl;
                test_file << "TimelineItemModel::finishConstruct(timeline_" << id.id << ", guideModel);" << std::endl;
                test_file << "Fake(Method(timMock_" << id.id << ", adjustAssetRange));" << std::endl;
            } else if (id.type == "TrackModel") {
                std::string params = process_args(constr[id.type][id.id].second);
                test_file << "TrackModel::construct(" << params << ");" << std::endl;

            } else {
                std::cout << "Error: unknown constructor " << id.type << std::endl;
            }
        } else {
            std::cout << "Error: unknown operation" << std::endl;
        }
        check_consistancy();
        test_file << "undoStack->undo();" << std::endl;
        check_consistancy();
        test_file << "undoStack->redo();" << std::endl;
        check_consistancy();
    }
    test_file << "}" << std::endl;
}
void Logger::clear()
{
    is_executing = false;
    invoks.clear();
    operations.clear();
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
