/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "devices.hpp"

#include <QStorageInfo>
#include <solid/block.h>
#include <solid/device.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>

bool isOnRemovableDevice(const QUrl &file)
{
    return isOnRemovableDevice(file.path());
}

bool isOnRemovableDevice(const QString &path)
{
#ifdef Q_OS_MAC
    // Parsing Solid devices seems to crash on Mac
    return false;
#endif
    const QString mountPath = QStorageInfo(path).rootPath();
    // We list volumes to find the one with matching mount path

    for (const auto &d : Solid::Device::allDevices()) {
        if (d.is<Solid::StorageAccess>()) {
            auto iface = d.as<Solid::StorageAccess>();
            if (iface->filePath() == mountPath) {
                auto parent = d.parent();

                // try to cope with encrypted devices
                if (!parent.isValid() || !parent.is<Solid::StorageDrive>()) {
                    if (d.is<Solid::StorageVolume>()) {
                        auto volume_iface = d.as<Solid::StorageVolume>();
                        auto enc = volume_iface->encryptedContainer();
                        if (enc.isValid()) {
                            parent = enc.parent();
                        }
                    }
                }

                if (parent.isValid() && parent.is<Solid::StorageDrive>()) {
                    auto parent_iface = parent.as<Solid::StorageDrive>();
                    return parent_iface->isRemovable();
                }
            }
        }
    }
    // not found, defaulting to false
    return false;
}
