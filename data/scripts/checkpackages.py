#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import sys
import subprocess
import importlib.metadata
import importlib.util

def print_help():
    print("""
    THIS SCRIPT IS PART OF KDENLIVE (www.kdenlive.org)

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
missing = set()

for arg in sys.argv[1:]:
    if not arg.startswith("--"):
        required.add(arg)

if len(required) == 0:
    print_help()
    sys.exit("Error: You need to provide at least one package name")

installed = {pkg.name for pkg in importlib.metadata.distributions()}
missing = required - installed

if '--check' in sys.argv:
    for m in missing:
        print("Missing: ", m)
elif '--install' in sys.argv and len(sys.argv) > 1:
    # install missing modules
    python = sys.executable
    if len(missing) > 0:
        print("Installing missing packages: ", missing)
        for m in missing:
            try:
                subprocess.check_call([python, '-m', 'pip', 'install', m])
            except:
                print("failed installing ", m)
elif '--upgrade' in sys.argv:
    # update modules
    print("Updating packages: ", required)
    python = sys.executable
    for r in required:
        try:
            subprocess.check_call([python, '-m', 'pip', 'install', '--upgrade', r])
        except:
                print("failed installing ", r)
elif '--details' in sys.argv:
    # check modules version
    python = sys.executable
    for m in missing:
        print("Name: ", m, " Version: missing ")
    subprocess.check_call([python, '-m', 'pip', 'show', *required])
else:
    print_help()
    sys.exit("Error: You need to provide a mode")
