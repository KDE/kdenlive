/*
    SPDX-FileCopyrightText: 2022 Eric Jiang <erjiang@alumni.iu.edu>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"

#include "utils/sysinfo.hpp"

TEST_CASE("Get system memory stats", "[Utils]")
{
    SysMemInfo memInfo = SysMemInfo::getMemoryInfo();
// this is only supported on some OSes right now
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC) || defined(Q_OS_WIN) || defined(Q_OS_FREEBSD)
    qDebug() << "sysinfotest:" << memInfo.freeMemory() << "MB free of" << memInfo.totalMemory() << "MB total";
    CHECK(memInfo.isSuccessful());
    CHECK(memInfo.freeMemory() > 0);
    CHECK(memInfo.totalMemory() > memInfo.freeMemory());
#endif
}
