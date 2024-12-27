# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

add_subdirectory(automask)
add_subdirectory(vosk)
add_subdirectory(whisper)

INSTALL(FILES 
    checkpackages.py
    otiointerface.py
    checkgpu.py

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts)
