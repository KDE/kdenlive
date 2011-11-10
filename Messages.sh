#!bin/sh

kdenlive_subdirs="plugins renderer src src/widgets"

$EXTRACTRC --tag=name --tag=description --tag=label --tag=comment --tag=paramlistdisplay effects/*.xml data/kdenliveeffectscategory.rc >> rc.cpp
$EXTRACTRC `find $kdenlive_subdirs -name \*.rc -a ! -name encodingprofiles.rc -o -name \*.ui` >> rc.cpp

$XGETTEXT `find $kdenlive_subdirs -name \*.cpp -o -name \*.h` *.cpp -o $podir/kdenlive.pot
rm -f rc.cpp
