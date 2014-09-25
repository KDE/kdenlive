#!/bin/bash
getlocale() {
    local locale=$1
    wget -O $locale.docbook "http://websvn.kde.org/*checkout*/trunk/l10n-kde4/$locale/docs/extragear-multimedia/kdenlive/index.docbook"
    if [ $? -eq 0 ] ; then
        mkdir $locale
        mv $locale.docbook $locale/index.docbook
        echo "KDE4_CREATE_HANDBOOK(index.docbook INSTALL_DESTINATION \${HTML_INSTALL_DIR}/$locale SUBDIR kdenlive)" > $locale/CMakeLists.txt
        echo "ADD_SUBDIRECTORY($locale)" >> CMakeLists.txt
    else
        rm $locale.docbook
    fi
}
[ -d doc ] || mkdir doc
cd doc
locales=$(wget -O - http://websvn.kde.org/*checkout*/trunk/l10n-kde4/subdirs | grep -v x-test)
echo "KDE4_CREATE_HANDBOOK(index.docbook INSTALL_DESTINATION \${HTML_INSTALL_DIR}/en SUBDIR kdenlive)" > CMakeLists.txt
for l in $locales; do
    getlocale $l &
done

