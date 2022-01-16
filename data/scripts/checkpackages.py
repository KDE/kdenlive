# SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import sys
import subprocess
import pkg_resources


def print_help():
    print("""
    Usage: python3 checkpackages.py [mode] [packages]

    Where [packages] is a list of python package names separated by blank space

    And [mode] one of the following:

    --help     print this help
    --install  install missing packages
    --upgrade  upgrade the packages
    --details  show details about the packages like eg. version
    --check    show which of the given packages are not yet installed
    """)


if '--help' in sys.argv:
    print_help()
    sys.exit()

required = set()

for arg in sys.argv[1:]:
    if not arg.startswith("--"):
        required.add(arg)

if len(required) == 0:
    print("Error: You need to provide at least one package name")
    print_help()
    sys.exit()

installed = {pkg.key for pkg in pkg_resources.working_set}
missing = required - installed
if '--check' in sys.argv:
    print("Missing pachages: ", missing)
elif '--install' in sys.argv and missing and len(sys.argv) > 1:
    # install missing modules
    print("Installing missing packages: ", missing)
    python = sys.executable
    subprocess.check_call([python, '-m', 'pip', 'install', *missing], stdout=subprocess.DEVNULL)
elif '--upgrade' in sys.argv:
    # update modules
    print("Updating packages: ", required)
    python = sys.executable
    subprocess.check_call([python, '-m', 'pip', 'install', '--upgrade', *required], stdout=subprocess.DEVNULL)
elif '--details' in sys.argv:
    # check modules version
    python = sys.executable
    subprocess.check_call([python, '-m', 'pip', 'show', *required])
else:
    print("Error: You need to provide a mode")
    print_help()
