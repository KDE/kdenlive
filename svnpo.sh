#!/bin/bash
getlocale() {
    local locale=$1
	wget -O $locale.po "http://websvn.kde.org/*checkout*/trunk/l10n-kde4/$locale/messages/extragear-multimedia/kdenlive.po"
    if [ $? -eq 0 ] ; then
        mkdir $locale
        mv $locale.po $locale/kdenlive.po
        echo "GETTEXT_PROCESS_PO_FILES($locale ALL INSTALL_DESTINATION \${LOCALE_INSTALL_DIR} kdenlive.po)" >> $locale/CMakeLists.txt
        echo "ADD_SUBDIRECTORY($locale)" >> CMakeLists.txt
    else
        rm $locale.po
    fi
}
[ -d po ] || mkdir po
cd po
locales=$(wget -O - http://websvn.kde.org/*checkout*/trunk/l10n-kde4/subdirs | grep -v x-test)
echo "find_package(Gettext REQUIRED)" > CMakeLists.txt
for l in $locales; do
    getlocale $l &
done

