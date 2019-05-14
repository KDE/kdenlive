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
