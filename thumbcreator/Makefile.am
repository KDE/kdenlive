# set the include path found by configure
INCLUDES = $(all_includes) $(LIBMLT_CFLAGS)


# these are the headers for your project
noinst_HEADERS = westleythumbcreator.h


kde_module_LTLIBRARIES = westleythumbcreator.la


westleythumbcreator_la_SOURCES = westleythumbcreator.cpp
westleythumbcreator_la_LIBADD = $(LIB_KIO)
westleythumbcreator_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN) $(LIBMLT_LIBS) -lmlt++

# let automoc handle all of the meta source files (moc)
METASOURCES = AUTO


services_DATA = westleythumbcreator.desktop
servicesdir = $(kde_servicesdir)

