
EXTRA_DIST = index.docbook mainwindow.png monitor_panel.png project_panel.png timeline_panel.png project_with_clips.png debug_tab.png setup_renderer.png rendering_project.png

KDE_LANG = de
KDE_DOCS = kdenlive
DOCDIR = $(DESTDIR)${kde_htmldir}

docbook_DATA = index.docbook
docbookdir = $(kde_htmldir)/de/kdenlive


index_de.po: ../en/index_en.pot
	msgmerge -o index_de.po index_de.po ../en/index_en.pot

index.docbook: ../en/index.docbook ../en/index_en.pot index_de.po
	${MAKE} index_de.po ../en/index-shifted.docbook
	@echo "converting German help"
	@echo "(this takes very very long, please be patient...)"
	po2xml ../en/index-shifted.docbook index_de.po | \
	sed s/^\<\!ENTITY\ %\ English\ /\<\!ENTITY\ %\ German\ /g > $@

noinst_HEADERS =  index_de.po

# --- Translated html help, not needed in distribution. Just for the Web site ---

index.html: index.docbook
	@MEINPROC@ --check index.docbook
	mkdir -p ./html
	mv *.html ./html/
	cp -R *.png ./html/
	rm -Rf ./common
	mkdir -p ./html/common
	cp -R ${DOCDIR}/de/common/* ./html/common
	for file in ./html/*.html; do { \
		echo $$file; \
		cat $$file | sed s+help:/common+./common+g > $$file.new; \
		mv $$file.new $$file; \
	} done