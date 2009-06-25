#! /bin/sh

MAX=1106
echo "document.getElementById(\"lastupdated\").innerHTML = \""`date`"\";" > poprogress.js

for f in *; do
  if test -d "$f"; then
    for pot in *.pot; do
      thing=`msgfmt --statistics "$f/kdenlive.po" 2>&1`
      set -- $thing
      foo=$1
      ans=$(($foo * 100 / $MAX))
      echo "document.getElementById(\"$f\").style.width = $ans + \"%\";" >> poprogress.js
    done
  fi
done


