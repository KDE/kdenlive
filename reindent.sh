#! /bin/sh

FILES="`find -type f -wholename './src/*.cpp'` `find -type f -wholename './src/*.h'` `find -type f -wholename './renderer/*.cpp'` `find -type f -wholename './renderer/*.h'` `find -type f -wholename './plugins/*.cpp'` `find -type f -wholename './plugins/*.h'` `find -type f -wholename './thumbnailer/*.cpp'` `find -type f -wholename './thumbnailer/*.h'`"

if [ $# -gt 0 ]
then
	FILES=$@
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
