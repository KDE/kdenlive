/*
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "doc/documentchecker.h"

TEST_CASE("Basic tests of the document checker parts", "[DocumentChecker]")
{
    QString path = sourcesPath + "/dataset/test-mix.kdenlive";

    SECTION("Asset functions")
    {
        QDomDocument doc;
        Xml::docContentFromFile(doc, path, false);

        QStringList filters = DocumentChecker::getAssetsServiceIds(doc, QStringLiteral("filter"));
        CHECK(filters == QStringList({"volume", "panner", "audiolevel", "avfilter.fieldorder"}));
        qDebug() << filters;

        DocumentChecker::removeAssetsById(doc, QStringLiteral("filter"), QStringList({"volume"}));
        filters = DocumentChecker::getAssetsServiceIds(doc, QStringLiteral("filter"));

        CHECK(filters == QStringList({"panner", "audiolevel", "avfilter.fieldorder"}));

        QStringList transitions = DocumentChecker::getAssetsServiceIds(doc, QStringLiteral("transition"));
        CHECK(transitions == QStringList{"luma", "mix", "frei0r.cairoblend"});

        qDebug() << filters;
        qDebug() << transitions;
    }

    SECTION("Check build-in luma detection")
    {
        CHECK(DocumentChecker::isMltBuildInLuma(QStringLiteral("luma05.pgm")));
        CHECK_FALSE(DocumentChecker::isMltBuildInLuma(QStringLiteral("luma05.png")));
        CHECK_FALSE(DocumentChecker::isMltBuildInLuma(QStringLiteral("my-luma05.pgm")));
        CHECK_FALSE(DocumentChecker::isMltBuildInLuma(QStringLiteral("luma87.pgm")));
    }
}
