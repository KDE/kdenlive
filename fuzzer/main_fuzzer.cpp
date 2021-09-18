/*
    SPDX-FileCopyrightText: 2019 Nicolas Carion
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
