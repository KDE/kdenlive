#!bin/sh

kdenlive_subdirs="plugins renderer src src/widgets"

$EXTRACTRC --tag=name --tag=description --tag=label --tag=comment --tag=paramlistdisplay data/*.rc effects/*.xml >> rc.cpp
$EXTRACTRC `find $kdenlive_subdirs -name \*.rc -o -name \*.ui` >> rc.cpp

$XGETTEXT `find $kdenlive_subdirs -name \*.cpp -o -name \*.h` -o $podir/kdenlive.pot
