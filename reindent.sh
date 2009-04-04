#! /bin/sh

SRCDIRS="src plugins renderer thumbnailer"

if [ $# -gt 0 ]
then
	FILES=$@
else
	FILES=$(find $SRCDIRS -type f -name \*.cpp -o -name \*.h)
fi

astyle --indent=spaces=4 --brackets=attach \
       --indent-labels --unpad=paren \
       --pad=oper --convert-tabs \
       --indent-preprocessor \
       ${FILES}

echo -n "Delete .orig files (y/N) ? "
read del

if [ "x${del}" = "xy" ]
then
   echo "removing .orig files"
   find src -iname "*.orig" -exec rm {} ";"
else
   echo "Not deleting .orig files"
fi
