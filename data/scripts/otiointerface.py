# SPDX-FileCopyrightText: 2019 Vincent Pinon <vpinon@kde.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

from opentimelineio import *

for a in plugins.ActiveManifest().adapters:
    if a.has_feature('read') and a.has_feature('write'):
        print('*.'+' *.'.join(a.suffixes), end=' ')
