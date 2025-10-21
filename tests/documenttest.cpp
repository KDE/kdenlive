/*
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"
// test specific headers
#include "doc/documentchecker.h"

TEST_CASE("Basic tests of the document checker parts", "[DocumentChecker]")
{
    QString path = sourcesPath + "/dataset/test-mix.kdenlive";

    SECTION("Asset functions")
    {
        QDomDocument doc;
        Xml::docContentFromFile(doc, path, false);

        QStringList filters = KdenliveTests::getAssetsServiceIds(doc, QStringLiteral("filter"));
        CHECK(filters == QStringList({"audiolevel", "avfilter.fieldorder", "panner", "volume"}));
        qDebug() << filters;

        KdenliveTests::removeAssetsById(doc, QStringLiteral("filter"), QStringList({"volume"}));
        filters = KdenliveTests::getAssetsServiceIds(doc, QStringLiteral("filter"));

        CHECK(filters == QStringList({"audiolevel", "avfilter.fieldorder", "panner"}));

        QStringList transitions = KdenliveTests::getAssetsServiceIds(doc, QStringLiteral("transition"));
        CHECK(transitions == QStringList{"frei0r.cairoblend", "luma", "mix"});

        qDebug() << filters;
        qDebug() << transitions;
    }

    SECTION("Check built-in luma detection")
    {
        CHECK(KdenliveTests::isMltBuildInLuma(QStringLiteral("luma05.pgm")));
        CHECK_FALSE(KdenliveTests::isMltBuildInLuma(QStringLiteral("luma05.png")));
        CHECK_FALSE(KdenliveTests::isMltBuildInLuma(QStringLiteral("my-luma05.pgm")));
        CHECK_FALSE(KdenliveTests::isMltBuildInLuma(QStringLiteral("luma87.pgm")));
    }
}

TEST_CASE("Missing project items", "[DocumentChecker]")
{
    QString path = sourcesPath + "/dataset/missing-proxy.kdenlive";

    SECTION("Missing proxy detection")
    {
        QDomDocument doc;
        Xml::docContentFromFile(doc, path, false);
        DocumentChecker d(QUrl::fromLocalFile(path), doc);
        d.hasErrorInProject();
        QMap<DocumentChecker::MissingType, int> results = d.getCheckResults();
        if (results.contains(DocumentChecker::MissingType::Clip)) {
            qDebug() << "::: MISSING CLIP: " << results.value(DocumentChecker::MissingType::Clip);
        }
        CHECK_FALSE(results.contains(DocumentChecker::MissingType::Clip));
        CHECK(results.contains(DocumentChecker::MissingType::Proxy));
        CHECK(results.value(DocumentChecker::MissingType::Proxy) == 1);
    }
}
