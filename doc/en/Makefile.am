####### kdevelop will overwrite this part!!! (begin)##########


EXTRA_DIST = index.docbook mainwindow.png monitor_panel.png project_panel.png timeline_panel.png project_with_clips.png debug_tab.png setup_renderer.png rendering_project.png

####### kdevelop will overwrite this part!!! (end)############

endocbook_DATA = index.docbook
endocbookdir = $(kde_htmldir)/en/kdenlive

KDE_LANG = en
KDE_DOCS = kdenlive
DOCDIR = $(DESTDIR)${kde_htmldir}

index_en.pot: index.docbook index-shifted.docbook
	xml2pot index.docbook > index_en.pot

index-shifted.docbook: index.docbook
	cat index.docbook | expand | sed s/^[\ ]*// > index-shifted.docbook
noinst_HEADERS =  index_en.pot index-shifted.docbook

# --- Translated html help, not needed in distribution. Just for the Web site ---

index.html: index.docbook
	@MEINPROC@ --check index.docbook
	mkdir -p ./html
	mv *.html ./html/
	cp -R *.png ./html/
	rm -Rf ./common
	mkdir -p ./html/common
	cp -R ${DOCDIR}/en/common/* ./html/common
	for file in ./html/*.html; do { \
		echo $$file; \
		cat $$file | sed s+help:/common+./common+g > $$file.new; \
		mv $$file.new $$file; \
	} done
