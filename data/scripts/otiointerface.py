# SPDX-FileCopyrightText: 2019 Vincent Pinon <vpinon@kde.org>
# SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import sys
import opentimelineio as otio


def print_help():
    print("""
    THIS SCRIPT IS PART OF KDENLIVE (www.kdenlive.org)

    Usage: python3 otiointerface.py [options]

    Where [options] is at least one of the following:

    --export-suffixes  print out file suffixes of all otio adapters that can be used for export (support write)
    --import-suffixes  print out file suffixes of all otio adapters that can be used for import (support read)
    """)


if '--help' in sys.argv:
    print_help()
    sys.exit()

use_read = False
use_write = False
if '--export-suffixes' in sys.argv:
    use_write = True
if '--import-suffixes' in sys.argv:
    use_read = True
if not use_read and not use_write:
    print_help()
    sys.exit("Error: You need to provide at least one valid option")

suffixes = otio.adapters.suffixes_with_defined_adapters(
    read=use_read, write=use_write
)

print('*.'+' *.'.join(sorted(suffixes)), end=' ')
