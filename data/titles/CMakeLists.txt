# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Marco Gittler <marco@gitma.de>

FILE(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.kdenlivetitle")
INSTALL (FILES ${files} DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/titles)
