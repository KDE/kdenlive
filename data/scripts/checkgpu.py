# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import sys
import subprocess
import torch

if torch.cuda.is_available() and torch.cuda.device_count() > 0:
    for i in torch.cuda.device_count():
        print (torch.cuda.get_device_name(i))

print ('cpu')


