/***************************************************************************
                          transitiondialog.h  -  description
                             -------------------
    begin                :  Mar 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TransitionDialog_H
#define TransitionDialog_H

#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qmap.h>
#include <qtabwidget.h>

#include <kjanuswidget.h>
#include <kcombobox.h>

#include "transition.h"
#include "transitioncrossfade_ui.h"
#include "transitionaudiofade_ui.h"
#include "transitionlumafile_ui.h"
#include "transitionwipewidget.h"
#include "transitionpipwidget.h"


class QVBox;

class DocClipRef;
class DocClipAVFile;
class KdenliveDoc;

namespace Gui {
    class KdenliveApp;

/**Displays the properties of a sepcified clip.
  *@author Jason Wood
  */

    class TransitionDialog:public QWidget { //QTabWidget {
      Q_OBJECT public:
              TransitionDialog(KdenliveApp * app, QWidget * parent = 0, const char *name = 0);
	 virtual ~ TransitionDialog();

    transitionWipeWidget *transitWipe;
    Transition::TRANSITIONTYPE selectedTransition();
    bool transitionDirection();
    const QMap < QString, QString > transitionParameters();
    void setActivePage(const Transition::TRANSITIONTYPE &pageName);
    void setTransitionDirection(bool direc);
    void setTransitionParameters(const QMap < QString, QString > parameters);
    void setTransition(Transition *transition);
    bool isActiveTransition(Transition *transition);
    bool belongsToClip(DocClipRef *clip);
    bool isOnTrack(int ix);
    QString getLumaFilePath(QString fileName);
    void refreshLumas();

    private slots:
	void applyChanges();
        void connectTransition();
        void disconnectTransition();
	void resetTransitionDialog();
	void initLumaFiles();

    private:
	int m_videoFormat;
	transitionCrossfade_UI *transitCrossfade;
	transitionLumaFile_UI *transitLumaFile;
	transitionAudiofade_UI *transitAudiofade;
	transitionPipWidget *transitPip;
	Transition *m_transition;
	KJanusWidget *propertiesDialog;
	KComboBox *trackPolicy;
	QString m_lumaType;

    signals:
	void transitionChanged(bool);
    };

}				// namespace Gui
#endif
