# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

INSTALL(FILES 
    checkpackages.py
    otiointerface.py
    speech.py
    speechtotext.py
    whispertotext.py
    whispertosrt.py
    checkgpu.py
    requirements-whisper.txt
    requirements-whisper-windows.txt
    requirements-seamless.txt

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts)
