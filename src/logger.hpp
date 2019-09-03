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
 *   This program is distributed in the hope that stdd::it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#pragma once
#include <climits>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/variant.h>
#pragma GCC diagnostic pop

/** @brief This class is meant to provide an easy way to reproduce bugs involving the model.
 * The idea is to log any modifier function involving a model class, and trace the parameters that were passed, to be able to generate a test-case producing the
 * same behaviour. Note that many modifier functions of the models are nested. We are only interested in the top-most call, and we must ignore bottom calls.
 */
class Logger
{
public:
    /// @brief Inits the logger. Must be called at startup
    static void init();

    /** @brief Notify the logger that the current thread wants to start logging.
     * This function returns true if this is a top-level call, meaning that we indeed want to log it. If the function returns false, the  caller must not log.
     */
    static bool start_logging();

    /** @brief This logs the construction of an object of type T, whose new instance is passed. The instance will be kept around in case future calls refer to
     * it. The arguments should more or less match the constructor arguments. In general, it's better to call the corresponding macro TRACE_CONSTR */
    template <typename T> static void log_constr(T *inst, std::vector<rttr::variant> args);

    /** @brief Logs the call to a member function on a given instance of class T. The string contains the method name, and then the vector contains all the
     * parameters. In general, the method should be registered in RTTR. It's better to call the corresponding macro TRACE() if appropriate */
    template <typename T> static void log(T *inst, std::string str, std::vector<rttr::variant> args);
    static void log_create_producer(const std::string &type, std::vector<rttr::variant> args);

    /** @brief When the last function logged has a return value, you can log it through this function, by passing the corresponding value. In general, it's
     * better to call the macro TRACE_RES */
    static void log_res(rttr::variant result);

    // log whenever an undo/redo occurred
    static void log_undo(bool undo);

    /// @brief Notify that we are done with our function. Must not be called if start_logging returned false.
    static void stop_logging();
    static void print_trace();

    /// @brief Resets the current log
    static void clear();

    static std::unordered_map<std::string, std::string> translation_table;
    static std::unordered_map<std::string, std::string> back_translation_table;

protected:
    /** @brief Look amongst the known instances to get the name of a given pointer */
    static std::string get_ptr_name(const rttr::variant &ptr);
    template <typename T> static size_t get_id_from_ptr(T *ptr);
    struct InvokId
    {
        size_t id;
    };
    struct ConstrId
    {
        std::string type;
        size_t id;
    };
    struct Undo
    {
        bool undo;
    };
    // a construction log contains the pointer as first parameter, and the vector of parameters
    using Constr = std::pair<rttr::variant, std::vector<rttr::variant>>;
    struct Invok
    {
        rttr::variant ptr;
        std::string method;
        std::vector<rttr::variant> args;
        rttr::variant res;
    };
    thread_local static bool is_executing;
    thread_local static size_t result_awaiting;
    static std::mutex mut;
    static std::vector<rttr::variant> operations;
    static std::unordered_map<std::string, std::vector<Constr>> constr;
    static std::vector<Invok> invoks;
    static int dump_count;
};

/** @brief This class provides a RAII mechanism to log the execution of a function */
class LogGuard
{
public:
    LogGuard();
    ~LogGuard();
    // @brief Returns true if we are the top-level caller.
    bool hasGuard() const;

protected:
    bool m_hasGuard = false;
};

/// See Logger::log_constr. Note that the macro fills in the ptr instance for you.
#define TRACE_CONSTR(ptr, ...)                                                                                                                                 \
    LogGuard __guard;                                                                                                                                          \
    if (__guard.hasGuard()) {                                                                                                                                  \
        Logger::log_constr((ptr), {__VA_ARGS__});                                                                                                              \
    }

/// See Logger::log. Note that the macro fills the ptr instance and the method name for you.
#define TRACE(...)                                                                                                                                             \
    LogGuard __guard;                                                                                                                                          \
    if (__guard.hasGuard()) {                                                                                                                                  \
        Logger::log(this, __FUNCTION__, {__VA_ARGS__});                                                                                                        \
    }

/// Same as TRACE, but called from a static function
#define TRACE_STATIC(ptr, ...)                                                                                                                                 \
    LogGuard __guard;                                                                                                                                          \
    if (__guard.hasGuard()) {                                                                                                                                  \
        Logger::log(ptr.get(), __FUNCTION__, {__VA_ARGS__});                                                                                                   \
    }

/// See Logger::log_res
#define TRACE_RES(res)                                                                                                                                         \
    if (__guard.hasGuard()) {                                                                                                                                  \
        Logger::log_res(res);                                                                                                                                  \
    }

/******* Implementations ***********/
template <typename T> void Logger::log_constr(T *inst, std::vector<rttr::variant> args)
{
    std::unique_lock<std::mutex> lk(mut);
    for (auto &a : args) {
        // this will rewove shared/weak/unique ptrs
        if (a.get_type().is_wrapper()) {
            a = a.extract_wrapped_value();
        }
    }
    std::string class_name = rttr::type::get<T>().get_name().to_string();
    constr[class_name].push_back({inst, std::move(args)});
    operations.emplace_back(ConstrId{class_name, constr[class_name].size() - 1});
}

template <typename T> void Logger::log(T *inst, std::string fctName, std::vector<rttr::variant> args)
{
    std::unique_lock<std::mutex> lk(mut);
    for (auto &a : args) {
        // this will rewove shared/weak/unique ptrs
        if (a.get_type().is_wrapper()) {
            a = a.extract_wrapped_value();
        }
    }
    std::string class_name = rttr::type::get<T>().get_name().to_string();
    invoks.push_back({inst, std::move(fctName), std::move(args), rttr::variant()});
    operations.emplace_back(InvokId{invoks.size() - 1});
    result_awaiting = invoks.size() - 1;
}

template <typename T> size_t Logger::get_id_from_ptr(T *ptr)
{
    const std::string class_name = rttr::type::get<T>().get_name().to_string();
    for (size_t i = 0; i < constr.at(class_name).size(); ++i) {
        if (constr.at(class_name)[i].first.convert<T *>() == ptr) {
            return i;
        }
    }
    std::cerr << "Error: ptr of type " << class_name << " not found" << std::endl;
    return INT_MAX;
}
