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

#include <kdialogbase.h>

#include "transition.h"
#include "transitioncrossfade_ui.h"
#include "transitionwipe_ui.h"
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

    class TransitionDialog:public QTabWidget {
      Q_OBJECT public:
              TransitionDialog(QWidget * parent = 0, const char *name = 0);
	 virtual ~ TransitionDialog();

    QString selectedTransition();
    bool transitionDirection();
    const QMap < QString, QString > transitionParameters();
    void setActivePage(const QString &pageName);
    void setTransitionDirection(bool direc);
    void setTransitionParameters(const QMap < QString, QString > parameters);
    void setTransition(Transition *transition);
    bool isActiveTransition(Transition *transition);

    private slots:
	void applyChanges();
        void connectTransition();
        void disconnectTransition();

      private:
	transitionCrossfade_UI *transitCrossfade;
        transitionWipe_UI *transitWipe;
	transitionPipWidget *transitPip;
	Transition *m_transition;

    signals:
	void transitionChanged(bool);

    };

}				// namespace Gui
#endif
