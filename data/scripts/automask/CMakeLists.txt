# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

INSTALL(FILES
    sam-objectmask.py
    requirements-sam.txt
    requirements-cuda118.txt
    requirements-cuda124.txt
    requirements-cuda126.txt
DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts/automask)

install(FILES
  sammodelsinfo.rc
  DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX})
