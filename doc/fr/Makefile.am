
EXTRA_DIST = index.docbook

KDE_LANG = fr

KDE_DOCS = kdenlive

docbook_DATA = index.docbook
docbookdir = $(kde_htmldir)/fr/kdenkive



index_fr.po: ../en/index_en.pot
	msgmerge -o index_fr.po index_fr.po ../en/index_en.pot

index.docbook: ../en/index.docbook ../en/index_en.pot index_fr.po
	${MAKE} index_fr.po ../en/index-shifted.docbook
	@echo "converting french help"
	@echo "(this takes very very long, please be patient...)"
	po2xml ../en/index-shifted.docbook index_fr.po | \
	sed s/^\<\!ENTITY\ %\ English\ /\<\!ENTITY\ %\ French\ /g > $@

noinst_HEADERS =  index_fr.po
