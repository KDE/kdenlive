#! /bin/sh

LANGS="de it fr"
case $1 in

    translate)
	xml2pot en/index.docbook >docbook.pot
	
	for i in ${LANGS}
	do
	    msgmerge -U ${i}/index_${i}.po docbook.pot
	done
    ;;
    build)
	for i in ${LANGS}
	do
	    po2xml en/index.docbook ${i}/index_${i}.po > ${i}/index.docbook
	done
    
    ;;
    
esac
