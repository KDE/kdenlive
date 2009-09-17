#! /bin/sh
EXTRACTRC=`which extractrc`
# XGETTEXT="`which xgettext` --kde -ki18n --no-location -s "
XGETTEXT="`which xgettext` -s "
podir=`pwd`/po
kdenlive_subdirs="src src/widgets renderer"


kde_inc=${includedir:-${KDEDIR:-@CMAKE_INSTALL_PREFIX@}/include/kde}/kde.pot

if [ -f ${kde_inc} ]; then
   kde_inc=" -x ${kde_inc} "
else
   kde_inc=""
fi

$EXTRACTRC --tag=name --tag=description --tag=label effects/*.xml >> rc.cpp || exit 11
$EXTRACTRC `find $kdenlive_subdirs -name \*.ui` >> rc.cpp || exit 11
$EXTRACTRC `find $kdenlive_subdirs -name \*.rc` >> rc.cpp || exit 11

$XGETTEXT -C -kde -ci18n -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ktr2i18n:1 \
        -kI18N_NOOP:1 -kI18N_NOOP2:1c,2 -ktranslate -kaliasLocale -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 --add-comments=\"TRANSLATORS:\" ${kde_inc} `find $kdenlive_subdirs -name \*.cpp -o -name \*.h` *.cpp -o $podir/kdenlive.pot

#$XGETTEXT -C -kki18n -ki18n -ktr2i18n -kI18N_NOOP -ktr `find $kdenlive_subdirs -name \*.cpp -o -name \*.h` *.cpp -o $podir/kdenlive.pot
rm -f rc.cpp
