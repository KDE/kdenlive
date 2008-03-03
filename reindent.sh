#! /bin/sh

astyle --indent=spaces=4 --brackets=attach \
       --indent-labels --unpad=paren \
       --pad=oper --convert-tabs \
       --indent-preprocessor \
       `find -type f -wholename './src/*.cpp'` `find -type f -wholename './src/*.h'`

echo -e "Delete .orig files (y/N) ?"
read del

if [ "x${del}" = "xy" ]
then
   echo "removing .orig files"
   find src -iname "*.orig" -exec rm {} ";"
else
   echo "Not deleting .orig files"
fi
