####### kdevelop will overwrite this part!!! (begin)##########


EXTRA_DIST = index.docbook 

####### kdevelop will overwrite this part!!! (end)############

endocbook_DATA = index.docbook
endocbookdir = $(kde_htmldir)/en/kdenlive

KDE_LANG = en
KDE_DOCS = kdenlive

index_en.pot: index.docbook index-shifted.docbook
	xml2pot index.docbook > index_en.pot

index-shifted.docbook: index.docbook
	cat index.docbook | expand | sed s/^[\ ]*// > index-shifted.docbook
noinst_HEADERS =  index_en.pot index-shifted.docbook
