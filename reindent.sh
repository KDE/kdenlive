#! /bin/sh

WRKDIR=$(dirname $0)
SRCDIRS="$WRKDIR/src $WRKDIR/plugins $WRKDIR/renderer $WRKDIR/thumbnailer"

if [ $# -gt 0 ]
then
	FILES=$@
else
	FILES=$(find $SRCDIRS -type f -name \*.cpp -o -name \*.h)
fi

astyle --indent=spaces=4 --brackets=linux \
       --indent-labels --unpad=paren \
       --pad=oper --convert-tabs \
       --indent-preprocessor \
       ${FILES}

echo -n "Delete .orig files (y/N) ? "
read del

if [ "x${del}" = "xy" ]
then
   echo "removing .orig files"
   find $SRCDIRS -iname "*.orig" -exec rm {} ";"
else
   echo "Not deleting .orig files"
fi
