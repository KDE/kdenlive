####### kdevelop will overwrite this part!!! (begin)##########

SUBDIRS = 32x32 

EXTRA_DIST = hi16-app-kdenlive.png hi32-app-kdenlive.png hi48-app-kdenlive.png hi64-app-kdenlive.png 

install-data-local:
	$(mkinstalldirs) $(kde_icondir)/hicolor/16x16/apps/
	$(INSTALL_DATA) $(srcdir)/hi16-app-kdenlive.png $(kde_icondir)/hicolor/16x16/apps/kdenlive.png
	$(mkinstalldirs) $(kde_icondir)/hicolor/32x32/apps/
	$(INSTALL_DATA) $(srcdir)/hi32-app-kdenlive.png $(kde_icondir)/hicolor/32x32/apps/kdenlive.png
	$(mkinstalldirs) $(kde_icondir)/hicolor/48x48/apps/
	$(INSTALL_DATA) $(srcdir)/hi48-app-kdenlive.png $(kde_icondir)/hicolor/48x48/apps/kdenlive.png
	$(mkinstalldirs) $(kde_icondir)/hicolor/64x64/apps/
	$(INSTALL_DATA) $(srcdir)/hi64-app-kdenlive.png $(kde_icondir)/hicolor/64x64/apps/kdenlive.png

uninstall-local:
	-rm -f $(kde_icondir)/hicolor/16x16/apps/kdenlive.png
	-rm -f $(kde_icondir)/hicolor/32x32/apps/kdenlive.png
	-rm -f $(kde_icondir)/hicolor/48x48/apps/kdenlive.png
	-rm -f $(kde_icondir)/hicolor/64x64/apps/kdenlive.png

####### kdevelop will overwrite this part!!! (end)############
