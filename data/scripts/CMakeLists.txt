# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

add_subdirectory(vosk)
add_subdirectory(whisper)
INSTALL(FILES 
    checkpackages.py
    otiointerface.py
    vosk/speech.py
    vosk/speechtotext.py
    whisper/whispertotext.py
    whisper/whispertosrt.py
    checkgpu.py
    whisper/requirements-whisper.txt
    whisper/requirements-whisper-windows.txt
    whisper/requirements-seamless.txt

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts)
