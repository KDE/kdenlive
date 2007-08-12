# set the include path found by configure
INCLUDES = $(all_includes)

# these are the headers for your project
noinst_HEADERS   = kfile_westley.h

kde_module_LTLIBRARIES = kfile_westley.la

kfile_westley_la_SOURCES = kfile_westley.cpp
kfile_westley_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN)
kfile_westley_la_LIBADD = $(LIB_KIO)


# let automoc handle all of the meta source files (moc)


services_DATA = kfile_westley.desktop
servicesdir = $(kde_servicesdir)


METASOURCES = AUTO
