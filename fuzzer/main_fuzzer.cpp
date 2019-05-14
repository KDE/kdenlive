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
#include <QApplication>
#include <cstring>
#include <iostream>
#include <mlt++/MltFactory.h>
#include <mlt++/MltRepository.h>

int argc = 1;
char *argv[1] = {"fuzz"};
QApplication app(argc, argv);
std::unique_ptr<Mlt::Repository> repo(Mlt::Factory::init(nullptr));

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    qputenv("MLT_TESTS", QByteArray("1"));
    Core::build(false);
    const char *input = reinterpret_cast<const char *>(data);
    char *target = new char[size + 1];
    strncpy(target, input, size);
    target[size] = '\0';
    std::string str(target);
    // std::cout<<"Testcase "<<str<<std::endl;
    fuzz(std::string(str));
    delete[] target;
    return 0;
}
