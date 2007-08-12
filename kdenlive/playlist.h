/***************************************************************************
                          playlist  -  description
                             -------------------
    begin                : Fri Jul 27 2007
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


#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <kdialogbase.h>
#include <klistview.h>
#include <kiconview.h>

#include "docclipref.h"
#include "kdenlive.h"
#include "fixplaylist_ui.h"

namespace Gui {

/**
  * PlayList is the dialog which helps to manage playlist clips.
  *@author Jean-Baptiste Mardelle
  */

    class PlayList:public KDialogBase {
      Q_OBJECT public:
	PlayList(QListView *m_listView, QIconView *m_iconView, bool iconView = false, QWidget *parent = 0, const char *name = 0);
	virtual ~PlayList();
	static void saveDescription(QString path, QString description);
	static void explodePlaylist(DocClipRef *clip, KdenliveApp *doc);

    private:
	KListView *m_broken_list;
	QString m_oldPath;
	QString m_newPath;
	FixPlayList_UI *replaceDialog;
	

    private slots:
	void slotFixLists();
	void fixPlayListClip(QString fileUrl, QString oldPath, QString newPath, bool partialReplace = false);
	void slotFixAllLists();
	void slotPreviewFix();
	void slotDoFix(QString oldPath, QString newPath);

    signals:
	void signalFixed();
    };

} // namespace

#endif

