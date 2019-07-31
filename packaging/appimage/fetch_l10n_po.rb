
#!/usr/bin/env ruby
#
# Ruby script for pulling l10n application translations for kdenlive
# Requires ruby version >= 1.9
#
# Copyright (c)      2005, Mark Kretschmann, <kretschmann at kde dot org>
# Copyright (c)      2014, Nicolas Lécureuil, <kde at nicolaslecureuil dot fr>
# Copyright (c) 2010-2018, Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

require 'rbconfig'
isWindows = RbConfig::CONFIG['host_os'] =~ /mswin|mingw|cygwin/i
STDOUT.sync = true

branch = "trunk"
tag = ""

enable_kdenlive = false

for opt in $*

    case opt
        when "--enable-kdenlive"
            enable_kdenlive = true
        else
            puts("Unknown option '#{opt}'.\n")
            puts("Possible arguments to customize i18n extraction:\n")
            puts("--enable-kdenlive\n")
            exit
    end
end

print ("extract kdenlive i18n      : #{enable_kdenlive}\n")

i18nlangs = []

if isWindows
    i18nlangs = `type .\\project\\release\\subdirs`
else
    i18nlangs = `cat subdirs`
end

##########################################################################################
# EXTRACT TRANSLATED APPLICATION FILES

Dir.chdir( "../.." )

if !(File.exists?("po") && File.directory?("po"))
    Dir.mkdir( "po" )
end

Dir.chdir( "po" )
topmakefile = File.new( "CMakeLists.txt", File::CREAT | File::RDWR | File::TRUNC )

i18nlangs.each_line do |lang|

    lang.chomp!()

    if (lang != nil && lang != "")

        print ("#{lang} ")

        if !(File.exists?(lang) && File.directory?(lang))
            Dir.mkdir(lang)
        end

        Dir.chdir(lang)

        makefile = File.new( "CMakeLists.txt", File::CREAT | File::RDWR | File::TRUNC )
        makefile << "file(GLOB _po_files *.po)\n"
        makefile << "GETTEXT_PROCESS_PO_FILES( #{lang} ALL INSTALL_DESTINATION ${LOCALE_INSTALL_DIR} PO_FILES ${_po_files} )\n"
        makefile.close()

        if (enable_kdenlive == true)

            # kdenlive

            for part in ['kdenlive']

                if isWindows
                    `svn cat svn://anonsvn.kde.org/home/kde/#{branch}/l10n-kf5/#{lang}/messages/kdemultimedia/#{part}.po > #{part}.po`
                else
                    `svn cat svn://anonsvn.kde.org/home/kde/#{branch}/l10n-kf5/#{lang}/messages/kdemultimedia/#{part}.po 2> /dev/null | tee #{part}.po `
                end

                if FileTest.size( "#{part}.po" ) == 0
                    File.delete( "#{part}.po" )
                end
            end
        end

        Dir.chdir("..")
        topmakefile << "add_subdirectory( #{lang} )\n"

    end
end

topmakefile.close()
puts ("\n")
