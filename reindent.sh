#! /bin/sh

astyle --indent=spaces=4 --brackets=attach \
       --indent-labels --unpad=paren \
       --pad=oper --convert-tabs \
       --indent-preprocessor \
       `find -type f -wholename './src/*.cpp'` `find -type f -wholename './src/*.h'`