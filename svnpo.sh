#!/bin/bash
[ -d po ] || mkdir po || exit
cd po
echo 'FIND_PACKAGE(Gettext)
FIND_PROGRAM(GETTEXT_MSGFMT_EXECUTABLE msgfmt)
IF(GETTEXT_MSGFMT_EXECUTABLE)
  FILE(GLOB PO_FILES *.po)
  SET(GMO_FILES)
  FOREACH(_po ${PO_FILES})
    GET_FILENAME_COMPONENT(_lang ${_po} NAME_WE)
    SET(_gmo ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)
    ADD_CUSTOM_COMMAND(OUTPUT ${_gmo}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${_gmo} ${_po}
        DEPENDS ${_po})
    INSTALL(FILES ${_gmo}
        DESTINATION ${LOCALE_INSTALL_DIR}/${_lang}/LC_MESSAGES/
        RENAME kdenlive.mo)
    LIST(APPEND GMO_FILES ${_gmo})
    #GETTEXT_PROCESS_PO_FILES(${_lang} ALL INSTALL_DESTINATION \${LOCALEDIR} PO_FILES {_lang}.po)
  ENDFOREACH(_po ${PO_FILES})
  ADD_CUSTOM_TARGET(translations ALL DEPENDS ${GMO_FILES})
ENDIF(GETTEXT_MSGFMT_EXECUTABLE)' > CMakeLists.txt
locales=$(wget -O - -U Mozilla/5.0 https://websvn.kde.org/*checkout*/trunk/l10n-kf5/subdirs | grep -v x-test)
for locale in $locales; do
	wget -O $locale.po -U Mozilla/5.0 "https://websvn.kde.org/*checkout*/trunk/l10n-kf5/$locale/messages/kdemultimedia/kdenlive.po" || rm $locale.po
done

