/*
    SPDX-FileCopyrightText: 2026 Yash Bavadiya <krbavadiya11@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "assets/view/widgets/gradienteditwidget.hpp"

static constexpr qreal Eps = 1e-4;

TEST_CASE("GradientData default state", "[GradientData]")
{
    GradientData g;
    g.enforceMinStops();
    g.sortStops();

    REQUIRE(g.stops.size() == 2);
    REQUIRE(qAbs(g.stops.at(0).first - 0.0) < Eps);
    REQUIRE(qAbs(g.stops.at(1).first - 1.0) < Eps);
    REQUIRE(g.stops.at(0).second == QColor(Qt::black));
    REQUIRE(g.stops.at(1).second == QColor(Qt::white));
}

TEST_CASE("GradientData toString/fromString round-trip", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(Qt::black)});
    g.stops.append({0.5, QColor(255, 0, 0, 255)});
    g.stops.append({1.0, QColor(Qt::white)});

    const QString serial = g.toString();

    GradientData g2;
    g2.fromString(serial);

    REQUIRE(g2.stops.size() == 3);
    for (int i = 0; i < 3; i++) {
        REQUIRE(qAbs(g2.stops.at(i).first - g.stops.at(i).first) < Eps);
        REQUIRE(g2.stops.at(i).second == g.stops.at(i).second);
    }
}

TEST_CASE("GradientData add stop", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(Qt::black)});
    g.stops.append({1.0, QColor(Qt::white)});

    const int idx = g.addStop(0.5);

    REQUIRE(idx >= 0);
    REQUIRE(g.stops.size() == 3);

    // Find the stop at ~0.5
    bool found = false;
    for (const auto &stop : g.stops) {
        if (qAbs(stop.first - 0.5) < Eps) {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("GradientData remove stop", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(Qt::black)});
    g.stops.append({0.5, QColor(255, 0, 0, 255)});
    g.stops.append({1.0, QColor(Qt::white)});

    // Remove middle stop
    const bool removed = g.removeStop(1);

    REQUIRE(removed);
    REQUIRE(g.stops.size() == 2);
}

TEST_CASE("GradientData minimum stops enforced", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(Qt::black)});
    g.stops.append({1.0, QColor(Qt::white)});

    // Cannot remove when only 2 stops remain
    const bool removed = g.removeStop(0);

    REQUIRE_FALSE(removed);
    REQUIRE(g.stops.size() == 2);
}

TEST_CASE("GradientData maximum stops enforced", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(Qt::black)});
    g.stops.append({1.0, QColor(Qt::white)});

    // Fill up to max
    for (int i = 2; i < GradientMaxStops; i++) {
        const qreal pos = static_cast<qreal>(i) / GradientMaxStops;
        g.addStop(pos);
    }
    REQUIRE(g.stops.size() == GradientMaxStops);

    // One more should be rejected
    const int idx = g.addStop(0.999);
    REQUIRE(idx == -1);
    REQUIRE(g.stops.size() == GradientMaxStops);
}

TEST_CASE("GradientData position snapping", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(Qt::black)});
    g.stops.append({1.0, QColor(Qt::white)});

    // Add a stop at 0.5
    g.addStop(0.5);
    REQUIRE(g.stops.size() == 3);

    // Try to add another within MinDistance of 0.5
    g.addStop(0.504); // within 0.01 of 0.5
    REQUIRE(g.stops.size() == 4);

    // Verify all stops maintain minimum distance from each other
    g.sortStops();
    for (int i = 0; i < g.stops.size() - 1; i++) {
        REQUIRE(g.stops.at(i + 1).first - g.stops.at(i).first >= GradientMinDistance - Eps);
    }
}

TEST_CASE("GradientData project load round-trip", "[GradientData]")
{
    GradientData g;
    g.stops.append({0.0, QColor(10, 20, 30, 255)});
    g.stops.append({0.33, QColor(100, 150, 200, 128)});
    g.stops.append({1.0, QColor(255, 240, 220, 255)});

    const QString serial = g.toString();

    GradientData g2;
    g2.fromString(serial);

    REQUIRE(g2.stops.size() == 3);
    for (int i = 0; i < 3; i++) {
        REQUIRE(qAbs(g2.stops.at(i).first - g.stops.at(i).first) < Eps);
        REQUIRE(g2.stops.at(i).second.red() == g.stops.at(i).second.red());
        REQUIRE(g2.stops.at(i).second.green() == g.stops.at(i).second.green());
        REQUIRE(g2.stops.at(i).second.blue() == g.stops.at(i).second.blue());
        REQUIRE(g2.stops.at(i).second.alpha() == g.stops.at(i).second.alpha());
    }
}
