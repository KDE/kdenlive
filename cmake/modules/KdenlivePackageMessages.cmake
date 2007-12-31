 
 FIND_PROGRAM(XGETTEXT_EXECUTABLE xgettext)
 FIND_PROGRAM(FIND_EXECUTABLE find)
 FIND_PROGRAM(RM_EXECUTABLE rm)
 FIND_PROGRAM(XTRACTRC_EXECUTABLE extractrc)

 SET(KDE_POT_FILE ${KDE3_INCLUDE_DIR}/kde.pot)
 
 ADD_CUSTOM_TARGET(package-messages
     COMMAND $(MAKE) all # first make sure all generated source exists
        COMMAND ${RM_EXECUTABLE} -f po/*.gmo
        #COMMAND ${XTRACTRC_EXECUTABLE} ${CMAKE_SOURCE_DIR}/kdenlive/*.rc >> ${CMAKE_SOURCE_DIR}/rc.cpp
        COMMAND ${XTRACTRC_EXECUTABLE} --tag=name --tag=description --tag=label --context=effectNames ${CMAKE_SOURCE_DIR}/effects/*.xml >> ${CMAKE_SOURCE_DIR}/rc.cpp
        COMMAND ${XGETTEXT_EXECUTABLE} -C
            -ki18n -ktr2i18n -kI18N_NOOP
            -x ${KDE_POT_FILE}
            `${FIND_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR} -name \\*.ui -o -name \\*.cpp`
            `${FIND_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR} -name \\*.ui -o -name \\*.cpp`
            -o ${CMAKE_SOURCE_DIR}/po/kdenlive.pot
        COMMAND $(MAKE) translations
        # DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kwave/menus.config
    )
