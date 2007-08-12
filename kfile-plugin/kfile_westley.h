/***************************************************************************
                          kfile_westley  -  description
                             -------------------
    begin                : Sat Aug 4 2007
    copyright            : (C) 2007 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

    #ifndef __KFILE_WESTLEY_H__
    #define __KFILE_WESTLEY_H__
   

    /**
     * Note: For further information look into <$KDEDIR/include/kfilemetainfo.h>
     */
    #include <kfilemetainfo.h>
   

    class QStringList;
   

    class westleyPlugin: public KFilePlugin
    {
        Q_OBJECT
        
    public:
        westleyPlugin( QObject *parent, const char *name, const QStringList& args );
        
        virtual bool readInfo( KFileMetaInfo& info, uint what);
    };
   

    #endif // __KFILE_WESTLEY_H__