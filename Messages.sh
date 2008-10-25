#! /bin/sh
EXTRACTRC=`which extractrc`
XGETTEXT="`which xgettext` --kde -ki18n --no-location -s "
podir=`pwd`/po
kdenlive_subdirs="src src/widgets renderer `pwd`"


$EXTRACTRC --tag=name --tag=description --tag=label effects/*.xml >> rc.cpp || exit 11
$EXTRACTRC `find $kdenlive_subdirs -name \*.ui` >> rc.cpp || exit 11
$EXTRACTRC `find $kdenlive_subdirs -name \*.rc` >> rc.cpp || exit 11
$XGETTEXT -C -kki18n -ki18n -ktr2i18n -kI18N_NOOP -ktr `find $kdenlive_subdirs -name \*.cpp -o -name \*.h` *.cpp -o $podir/kdenlive.pot
rm -f rc.cpp
