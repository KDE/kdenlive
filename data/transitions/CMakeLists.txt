# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>, Julius Künzel <julius.kuenzel@kde.org>

INSTALL (FILES 
affine.xml
composite.xml
dissolve.xml
luma.xml
matte.xml
mix.xml
qtblend.xml
region.xml
slide.xml
vqm.xml
wipe.xml

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/transitions)

add_subdirectory(frei0r)

install(FILES
  parameters.txt
  DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/transitions)

# Install subdirectories
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lumas)
  add_subdirectory(lumas)
endif()

# Create previews directory
install(DIRECTORY DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/transitions/previews)
