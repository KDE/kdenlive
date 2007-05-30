#FIND_PACKAGE(Gettext)
#GETTEXT_CREATE_TRANSLATIONS(kdenlive.pot de.po)
FILE (GLOB _lang *.po)
FOREACH( _current_LANG ${_lang} )
    STRING(REGEX REPLACE "^.*\\/([a-z\\_A-Z]*)\\.po$" "\\1" _lang_name  "${_current_LANG}")
    STRING(REGEX REPLACE "^(.*)\\/([a-z\\_A-Z]*)\\.po$" "\\1" _path  "${_current_LANG}")

    EXECUTE_PROCESS(COMMAND msgfmt ${_path}/${_lang_name}.po -o ${_path}/${_lang_name}.gmo)
    INSTALL(FILES ${_path}/${_lang_name}.gmo DESTINATION share/locale/${_lang_name}/LC_MESSAGES/ RENAME kdenlive.mo)
ENDFOREACH(_current_LANG)

MESSAGE(STATUS ${_target})

#ADD_CUSTOM_TARGET(messages COMMAND generate)
#MACRO(generate)
#    EXECUTE_PROCESS(COMMAND msgmerge ${_path}/${_lang_name}.po ${_path}/kdenlive.pot)
#ENDMACRO(generate)