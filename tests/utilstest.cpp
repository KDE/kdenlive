/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "catch.hpp"
#include "test_utils.hpp"

#include "utils/qstringutils.h"

TEST_CASE("Testing for different utils", "[Utils]")
{

    SECTION("Append to filename")
    {
        QString filename("myfile.mp4");
        QString appendix("-hello");
        QString newName = QStringUtils::appendToFilename(filename, appendix);
        qDebug() << newName;
        REQUIRE(newName == QStringLiteral("myfile-hello.mp4"));
    }

    SECTION("Unique names should really be unique")
    {
        QStringList names;
        for (int i = 0; i < 1000; i++) {
            names << QStringUtils::getUniqueName(names, QStringLiteral("my923test.string-1"));
        }

        REQUIRE(names.removeDuplicates() == 0);
    }
}
