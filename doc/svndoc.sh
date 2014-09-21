#!/bin/bash
locales=$(wget -O - http://websvn.kde.org/*checkout*/trunk/l10n-kde4/subdirs)
echo "KDE4_CREATE_HANDBOOK(index.docbook INSTALL_DESTINATION \${HTML_INSTALL_DIR}/en SUBDIR kdenlive)" > CMakeLists.txt
for locale in $locales; do
    wget "http://websvn.kde.org/*checkout*/trunk/l10n-kde4/$locale/docs/extragear-multimedia/kdenlive/index.docbook"
    if [ $? -eq 0 ] ; then
        mkdir $locale
        mv index.docbook $locale
        echo "KDE4_CREATE_HANDBOOK(index.docbook INSTALL_DESTINATION \${HTML_INSTALL_DIR}/$locale SUBDIR kdenlive)" > $locale/CMakeLists.txt
        echo "ADD_SUBDIRECTORY($locale)" >> CMakeLists.txt
    fi
done

