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

INSTALL (FILES
previews/affine.webp
previews/bi-linear_x.webp
previews/bi-linear_y.webp
previews/burst.webp
previews/checkerboard_small.webp
previews/clock.webp
previews/cloud.webp
previews/composite.webp
previews/curtain.webp
previews/dissolve.webp
previews/frei0r.addition_alpha.webp
previews/frei0r.addition.webp
previews/frei0r.alphaatop.webp
previews/frei0r.alphain.webp
previews/frei0r.alphaout.webp
previews/frei0r.alphaover.webp
previews/frei0r.alphaxor.webp
previews/frei0r.burn.webp
previews/frei0r.cairoaffineblend.webp
previews/frei0r.cairoblend.webp
previews/frei0r.color_only.webp
previews/frei0r.darken.webp
previews/frei0r.difference.webp
previews/frei0r.divide.webp
previews/frei0r.dodge.webp
previews/frei0r.euclid_eraser.webp
previews/frei0r.grain_extract.webp
previews/frei0r.grain_merge.webp
previews/frei0r.hardlight.webp
previews/frei0r.hue.webp
previews/frei0r.lighten.webp
previews/frei0r.multiply.webp
previews/frei0r.overlay.webp
previews/frei0r.saturation.webp
previews/frei0r.screen.webp
previews/frei0r.sleid0r_push-down.webp
previews/frei0r.sleid0r_push-left.webp
previews/frei0r.sleid0r_push-right.webp
previews/frei0r.sleid0r_push-up.webp
previews/frei0r.sleid0r_slide-down.webp
previews/frei0r.sleid0r_slide-left.webp
previews/frei0r.sleid0r_slide-right.webp
previews/frei0r.sleid0r_slide-up.webp
previews/frei0r.sleid0r_wipe-barn-door-h.webp
previews/frei0r.sleid0r_wipe-barn-door-v.webp
previews/frei0r.sleid0r_wipe-circle.webp
previews/frei0r.sleid0r_wipe-down.webp
previews/frei0r.sleid0r_wipe-left.webp
previews/frei0r.sleid0r_wipe-rect.webp
previews/frei0r.sleid0r_wipe-right.webp
previews/frei0r.sleid0r_wipe-up.webp
previews/frei0r.softlight.webp
previews/frei0r.subtract.webp
previews/frei0r.uvmap.webp
previews/frei0r.value.webp
previews/horizontal_blinds.webp
previews/linear_x.webp
previews/linear_y.webp
previews/luma.webp
previews/luma01.webp
previews/luma02.webp
previews/luma03.webp
previews/luma04.webp
previews/luma05.webp
previews/luma06.webp
previews/luma07.webp
previews/luma08.webp
previews/luma09.webp
previews/luma10.webp
previews/luma11.webp
previews/luma12.webp
previews/luma13.webp
previews/luma14.webp
previews/luma15.webp
previews/luma16.webp
previews/luma17.webp
previews/luma18.webp
previews/luma19.webp
previews/luma20.webp
previews/luma21.webp
previews/luma22.webp
previews/matte.webp
previews/mix.webp
previews/qtblend.webp
previews/radial.webp
previews/radial-bars.webp
previews/region.webp
previews/slide.webp
previews/spiral.webp
previews/spiral2.webp
previews/square.webp
previews/square2.webp
previews/square2-bars.webp
previews/symmetric_clock.webp
previews/vqm.webp
previews/wipe.webp


DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/transitions/previews)
