FILE(GLOB pofiles RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.po)

FOREACH(_current_PO_FILE ${pofiles})
    GET_FILENAME_COMPONENT(_basename ${_current_PO_FILE} NAME_WE)
    GETTEXT_PROCESS_PO_FILES(${_basename} ALL INSTALL_DESTINATION share/locale/ ${_current_PO_FILE})
ENDFOREACH(_current_PO_FILE)
