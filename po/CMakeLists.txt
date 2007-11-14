#EXECUTE_PROCESS(COMMAND kde-config --install locale --expandvars
#                OUTPUT_VARIABLE KDE3_LOCALEDIR)
SET(KDE3_LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
GETTEXT_CREATE_TRANSLATIONS(kdenlive.pot ALL
    ca.po
    cs.po
    de.po
    fr.po
    hu.po
    it.po
    ja.po
    nl.po
    pt_BR.po
    tr.po
)

# ADD_POFILES("kdenlive")

