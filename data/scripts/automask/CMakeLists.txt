# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

INSTALL(FILES
    sam-objectmask.py
    kdenlive_sam2_video_predictor.py
    kdenlive_build_sam.py
    requirements-sam.txt
    requirements-cuda118.txt
    requirements-cuda124.txt
    requirements-cuda126.txt
    requirements-cuda128.txt
DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts/automask)

install(FILES
  sammodelsinfo.rc
  DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX})
