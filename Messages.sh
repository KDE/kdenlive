#!bin/sh

kdenlive_subdirs="plugins renderer data src src/ui"

$EXTRACTRC --tag=name --tag=description --tag=label --tag=comment --tag=paramlistdisplay data/effects/*.xml data/kdenliveeffectscategory.rc >> rc.cpp
$EXTRACTRC `find $kdenlive_subdirs -name \*.rc -a ! -name encodingprofiles.rc -a ! -name camcorderfilters.rc -o -name \*.ui` >> rc.cpp

$XGETTEXT `find $kdenlive_subdirs -name \*.cpp -o -name \*.h` *.cpp -o $podir/kdenlive.pot
rm -f rc.cpp
