/*
    SPDX-FileCopyrightText: 2019 Nicolas Carion
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "core.h"
#include "fuzzing.hpp"
#include "logger.hpp"
#include <QApplication>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>

void signalHandler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";

    Logger::print_trace();

    exit(signum);
}

int main(int argc, char **argv)
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGSEGV, signalHandler);
    QApplication app(argc, argv);
    qputenv("MLT_TESTS", QByteArray("1"));
    Core::build(false);
    std::stringstream ss;
    std::string str;
    while (getline(std::cin, str)) {
        ss << str << std::endl;
    }
    std::cout << "executing " << ss.str() << std::endl;
    fuzz(ss.str());
    Logger::print_trace();
    return 0;
}
