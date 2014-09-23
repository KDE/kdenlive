#!/bin/bash
locales=$(wget -O - http://websvn.kde.org/*checkout*/trunk/l10n-kde4/subdirs)
echo "find_package(Gettext REQUIRED)" > CMakeLists.txt
for locale in $locales; do
	wget "http://websvn.kde.org/*checkout*/trunk/l10n-kde4/$locale/messages/extragear-multimedia/kdenlive.po"
    if [ $? -eq 0 ] ; then
        mkdir $locale
        mv kdenlive.po $locale
        echo "GETTEXT_PROCESS_PO_FILES($locale ALL INSTALL_DESTINATION \${LOCALE_INSTALL_DIR} kdenlive.po)" >> $locale/CMakeLists.txt
        echo "ADD_SUBDIRECTORY($locale)" >> CMakeLists.txt
    fi
done

