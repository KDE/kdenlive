#! /bin/sh

for i in `ls src/*.{cpp,h}`
do
	indent -kr -nut -pmt -ss -bad -bap -prs -bap -nbc -nce -cdb -fca $i
	sed -e "s/} const const/}const/" -i $i
done