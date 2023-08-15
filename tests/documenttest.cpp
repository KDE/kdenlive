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
        QDomNodeList filterNodes = doc.elementsByTagName(QStringLiteral("filter"));
        QStringList filters = DocumentChecker::getAssetsServiceIds(filterNodes);
        CHECK(filters == QStringList({"volume", "panner", "audiolevel", "avfilter.fieldorder"}));
        qDebug() << filters;

        DocumentChecker::removeAssetsById(filterNodes, QStringList({"volume"}));
        filters = DocumentChecker::getAssetsServiceIds(filterNodes);

        CHECK(filters == QStringList({"panner", "audiolevel", "avfilter.fieldorder"}));

        QDomNodeList transitionNodes = doc.elementsByTagName(QStringLiteral("transition"));
        QStringList transitions = DocumentChecker::getAssetsServiceIds(transitionNodes);
        CHECK(transitions == QStringList{"luma", "mix", "frei0r.cairoblend"});

        qDebug() << filters;
        qDebug() << transitions;
    }

    path = sourcesPath + "/dataset/test-missing.kdenlive";
    SECTION("Missing assets replacement")
    {
        // Ensure we detect a missing lut file
        QDomDocument doc;
        Xml::docContentFromFile(doc, path, false);
        QDomNodeList filterNodes = doc.elementsByTagName(QStringLiteral("filter"));
        QStringList filters = DocumentChecker::getAssetsServiceIds(filterNodes);
        CHECK(filters == QStringList({"volume", "panner", "audiolevel", "brightness", "avfilter.lut3d"}));
        const QMap<QString, QString> filterPairs = DocumentChecker::getFilterPairs();
        QStringList assetsToCheck = DocumentChecker::getAssetsFiles(filterNodes, filterPairs);
        qDebug() << assetsToCheck;
        REQUIRE(!assetsToCheck.isEmpty());
    }

    SECTION("Check build-in luma detection")
    {
        CHECK(DocumentChecker::isMltBuildInLuma(QStringLiteral("luma05.pgm")));
        CHECK_FALSE(DocumentChecker::isMltBuildInLuma(QStringLiteral("luma05.png")));
        CHECK_FALSE(DocumentChecker::isMltBuildInLuma(QStringLiteral("my-luma05.pgm")));
        CHECK_FALSE(DocumentChecker::isMltBuildInLuma(QStringLiteral("luma87.pgm")));
    }
}
