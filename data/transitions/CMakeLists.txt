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
