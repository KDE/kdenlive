/*
    SPDX-FileCopyrightText: 2026 Erich Seifert <dev@erichseifert.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"

#include "render/renderserver.h"

#include <QJsonDocument>

namespace {
// A message as sent by kdenlive_render (renderer/renderjob.cpp)
QByteArray finishedMessage(const QString &log)
{
    QJsonObject args{{QStringLiteral("url"), QStringLiteral("/tmp/out.mp4")}, {QStringLiteral("status"), -2}, {QStringLiteral("error"), log}};
    QJsonObject method{{QStringLiteral("setRenderingFinished"), args}};
    return QJsonDocument(method).toJson();
}
} // namespace

TEST_CASE("Messages from render jobs", "[RenderServer]")
{
    SECTION("A message split over several reads is only returned once complete")
    {
        const QByteArray message = finishedMessage(QStringLiteral("line 1\nline 2\n}\n"));
        QByteArray buffer = message.left(message.size() / 2);
        CHECK(RenderServer::takeMessages(buffer).isEmpty());
        buffer.append(message.mid(message.size() / 2));
        const QList<QByteArray> messages = RenderServer::takeMessages(buffer);
        REQUIRE(messages.size() == 1);
        CHECK(messages.first() == message);
        CHECK(buffer.isEmpty());
        QJsonParseError error;
        const QJsonObject json = QJsonDocument::fromJson(messages.first(), &error).object();
        CHECK(error.error == QJsonParseError::NoError);
        CHECK(json.value(QStringLiteral("setRenderingFinished")).toObject().value(QStringLiteral("status")).toInt() == -2);
    }

    SECTION("Several messages in one read")
    {
        const QByteArray first = finishedMessage(QStringLiteral("a"));
        const QByteArray second = finishedMessage(QStringLiteral("b"));
        QByteArray buffer = first + second + second.left(10);
        const QList<QByteArray> messages = RenderServer::takeMessages(buffer);
        REQUIRE(messages.size() == 2);
        CHECK(messages.at(0) == first);
        CHECK(messages.at(1) == second);
        CHECK(buffer == second.left(10));
    }
}
