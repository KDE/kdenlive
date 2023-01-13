/*
SPDX-FileCopyrightText: 2022 Eric Jiang <erjiang@alumni.iu.edu>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sysinfo.hpp"

#include <QDebug>

#include <kcoreaddons_version.h>

/*
 * TODO: kmemoryinfo was introduced in KF 5.95 but currently we still support
 * some systems with KF 5.92. Once we bump the minimum requirement to KF 5.95+
 * we can get rid of this class completely and replace it with KMemoryInfo.
 */
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 95, 0)
    #include <kmemoryinfo.h>

#elif defined(Q_OS_WIN)
    #include <windows.h>

#elif defined(Q_OS_MAC)
    #include <mach/mach.h>
    #include <mach/vm_statistics.h>
    #include <sys/sysctl.h>

#elif defined(Q_OS_LINUX)
    #include <QFile>

#elif defined(Q_OS_FREEBSD)
    #include <fcntl.h>
    #include <kvm.h>
    #include <sys/sysctl.h>

    template<class T>
    bool sysctlread(const char *name, T &var)
    {
        auto sz = sizeof(var);
        return (sysctlbyname(name, &var, &sz, NULL, 0) == 0);
    }

#endif

// returns system free memory in MB
SysMemInfo SysMemInfo::getMemoryInfo()
{
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 95, 0)
    KMemoryInfo memInfo;
if (!memInfo.isNull()) {
    return {true,
        static_cast<int>(memInfo.availablePhysical() / 1024 / 1024),
        static_cast<int>(memInfo.totalPhysical() / 1024 / 1024)};
}

#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX status;
    ZeroMemory(&status, sizeof(MEMORYSTATUSEX));
    status.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&status);
    return {true,
        status.ullAvailPhys / 1024 / 1024,
        status.ullTotalPhys / 1024 / 1024};

#elif defined(Q_OS_MAC)
    // get total memory
    int mib [] = { CTL_HW, HW_MEMSIZE };
    int64_t value = 0;
    size_t length = sizeof(value);

    if(-1 == sysctl(mib, 2, &value, &length, NULL, 0)) {
        qDebug() << "SysMemInfo: sysctl failed";
        return {false, -1, -1};
    }
    int total = value / 1024 / 1024;

    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmstat;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)&vmstat, &count) != KERN_SUCCESS) {
        qDebug() << "SysMemInfo: Failed to get host_statistics64()";
        return meminfo;
    }
    int available = (
        vmstat.free_count +
        // purgeable_count is memory that can be reclaimed by the system
        vmstat.purgeable_count +
        // external_page_count includes caches
        vmstat.external_page_count
        ) * vm_page_size / 1024 / 1024;

    return {true, available, total};

#elif defined(Q_OS_LINUX)
    // use /proc/meminfo to get available and total memory with Qt
    QFile meminfo("/proc/meminfo");
    if (!meminfo.open(QIODevice::ReadOnly)) {
        qDebug() << "SysMemInfo: Failed to open /proc/meminfo";
        return {false, -1, -1};
    }
    QTextStream stream(&meminfo);
    QString line;
    int memTotal = -1;
    int memAvailable = -1;
    while (true) {
        line = stream.readLine();
        // can't use atEnd() because /proc/meminfo's apparent size is 0 bytes
        if (line.isNull()) {
            break;
        }
        if (line.startsWith("MemTotal:")) {
            memTotal = line.split(" ", Qt::SkipEmptyParts)[1].toInt();
        } else if (line.startsWith("MemAvailable:")) {
            // Use MemAvailable because OS caches in memory are evictable
            memAvailable = line.split(" ", Qt::SkipEmptyParts)[1].toInt();
        }
    }
    if (memAvailable != -1 && memTotal != -1) {
        return {true, memAvailable / 1024, memTotal / 1024};
    }
    return {false, -1, -1};

#elif defined(Q_OS_FREEBSD)
    // FreeBSD code from KMemoryInfo
    quint64 memSize = 0;
    quint64 pageSize = 0;

    int mib[4];
    size_t sz = 0;

    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;
    sz = sizeof(memSize);
    if (sysctl(mib, 2, &memSize, &sz, NULL, 0) != 0) {
        return {false, -1, -1};
    }

    mib[0] = CTL_HW;
    mib[1] = HW_PAGESIZE;
    sz = sizeof(pageSize);
    if (sysctl(mib, 2, &pageSize, &sz, NULL, 0) != 0) {
        return {false, -1, -1};
    }

    quint32 v_pageSize = 0;
    if (sysctlread("vm.stats.vm.v_page_size", v_pageSize)) {
        pageSize = v_pageSize;
    }
    quint64 zfs_arcstats_size = 0;
    if (!sysctlread("kstat.zfs.misc.arcstats.size", zfs_arcstats_size)) {
        zfs_arcstats_size = 0; // no ZFS used
    }
    quint32 v_cache_count = 0;
    if (!sysctlread("vm.stats.vm.v_cache_count", v_cache_count)) {
        return {false, -1, -1};
    }
    quint32 v_inactive_count = 0;
    if (!sysctlread("vm.stats.vm.v_inactive_count", v_inactive_count)) {
        return {false, -1, -1};
    }
    quint32 v_free_count = 0;
    if (!sysctlread("vm.stats.vm.v_free_count", v_free_count)) {
        return {false, -1, -1};
    }
    quint64 vfs_bufspace = 0;
    if (!sysctlread("vfs.bufspace", vfs_bufspace)) {
        return {false, -1, -1};
    }

    return {
        true,
        static_cast<int>((pageSize * (v_cache_count + v_free_count + v_inactive_count)
            + vfs_bufspace + zfs_arcstats_size) / 1024 / 1024),
        static_cast<int>(memSize / 1024 / 1024)
    };

#else
    qDebug() << "WARNING: SysMemInfo() not implemented for this OS";

#endif
    return {false, -1, -1};
}
