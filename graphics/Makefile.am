####### kdevelop will overwrite this part!!! (begin)##########


EXTRA_DIST = norenderer.png kdenlive-splash.xcf kdenlive-splash.png 

install-data-local:
	$(mkinstalldirs) $(kde_datadir)/kdenlive/graphics/
	$(INSTALL_DATA) $(srcdir)/norenderer.png $(kde_datadir)/kdenlive/graphics/norenderer.png
	$(mkinstalldirs) $(kde_datadir)/kdenlive/
	$(INSTALL_DATA) $(srcdir)/kdenlive-splash.png $(kde_datadir)/kdenlive/kdenlive-splash.png

uninstall-local:
	-rm -f $(kde_datadir)/kdenlive/graphics/norenderer.png
	-rm -f $(kde_datadir)/kdenlive/kdenlive-splash.png

####### kdevelop will overwrite this part!!! (end)############
