# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

INSTALL(FILES 
    whispertotext.py
    whispertosrt.py
    whisperquery.py
    seamlessquery.py
    requirements-whisper.txt
    requirements-whisper-windows.txt
    requirements-seamless.txt

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts/whisper)
