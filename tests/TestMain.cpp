/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <QApplication>
#include <mlt++/MltFactory.h>
#include <mlt++/MltRepository.h>
#define private public
#define protected public
#include "bin/projectitemmodel.h"
#include "core.h"
#include "mltconnection.h"
#include "src/effects/effectsrepository.hpp"
#include "src/mltcontroller/clipcontroller.h"

/* This file is intended to remain empty.
Write your tests in a file with a name corresponding to what you're testing */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kdenlive"));
    std::unique_ptr<Mlt::Repository> repo(Mlt::Factory::init(nullptr));
    qputenv("MLT_TESTS", QByteArray("1"));
    qSetMessagePattern(QStringLiteral("%{file}:%{line} – %{message}"));
    Core::build(QString(), true);
    MltConnection::construct(QString());
    pCore->projectItemModel()->buildPlaylist();
    // if Kdenlive is not installed, ensure we have one keyframable effect
    EffectsRepository::get()->reloadCustom(QFileInfo("../data/effects/audiobalance.xml").absoluteFilePath());

    int result = Catch::Session().run(argc, argv);
    pCore->cleanup();
    ClipController::mediaUnavailable.reset();

    // global clean-up...
    // delete repo;
    Core::m_self.reset();
    Mlt::Factory::close();
    return (result < 0xff ? result : 0xff);
}
