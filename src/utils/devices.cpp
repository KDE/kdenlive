/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

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
    QString mountPath = QStorageInfo(path).rootPath();

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
