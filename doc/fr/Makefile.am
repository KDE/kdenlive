
EXTRA_DIST = index.docbook mainwindow.png monitor_panel.png project_panel.png timeline_panel.png project_with_clips.png debug_tab.png setup_renderer.png rendering_project.png

KDE_LANG = fr
KDE_DOCS = kdenlive
DOCDIR = $(DESTDIR)${kde_htmldir}

docbook_DATA = index.docbook
docbookdir = $(kde_htmldir)/fr/kdenlive


index_fr.po: ../en/index_en.pot
	msgmerge -o index_fr.po index_fr.po ../en/index_en.pot

index.docbook: ../en/index.docbook ../en/index_en.pot index_fr.po
	${MAKE} index_fr.po ../en/index-shifted.docbook
	@echo "converting french help"
	@echo "(this takes very very long, please be patient...)"
	po2xml ../en/index-shifted.docbook index_fr.po | \
	sed s/^\<\!ENTITY\ %\ English\ /\<\!ENTITY\ %\ French\ /g > $@

noinst_HEADERS =  index_fr.po

# --- Translated html help, not needed in distribution. Just for the Web site ---

index.html: index.docbook
	@MEINPROC@ --check index.docbook
	mkdir -p ./html
	mv *.html ./html/
	cp -R *.png ./html/
	rm -Rf ./common
	mkdir -p ./html/common
	cp -R ${DOCDIR}/fr/common/* ./html/common
	for file in ./html/*.html; do { \
		echo $$file; \
		cat $$file | sed s+help:/common+./common+g > $$file.new; \
		mv $$file.new $$file; \
	} done