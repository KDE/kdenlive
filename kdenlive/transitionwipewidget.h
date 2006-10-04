/***************************************************************************
                          transitionWipeWidget  -  description
                             -------------------
    begin                :  2006
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

#ifndef TRANSITIONWIPEWIDGET_H
#define TRANSITIONWIPEWIDGET_H

#include <qdom.h>
#include <qmap.h>

#include "transitionwipe_ui.h"

namespace Gui {

class transitionWipeWidget : public transitionWipe_UI
{
        Q_OBJECT
public:
        transitionWipeWidget( QWidget* parent=0, const char* name=0, WFlags fl=0);
        ~transitionWipeWidget();

    enum TRANSITIONWIPETYPE {
	TOPLEFT_TRANSITION = 0,
	TOP_TRANSITION = 1,
	TOPRIGHT_TRANSITION= 2,
	LEFT_TRANSITION = 3,
	CENTER_TRANSITION = 4,
	RIGHT_TRANSITION = 5,
	BOTTOMLEFT_TRANSITION = 6,
	BOTTOM_TRANSITION = 7,
	BOTTOMRIGHT_TRANSITION = 8
    };

private:

	TRANSITIONWIPETYPE startTransition, endTransition;
	int m_startTransparency;
	int m_endTransparency;

private slots:
	void updateButtons();
	void updateTransition();
	void updateTransparency(int transp);

public slots:
        QMap < QString, QString > parameters();
        void setParameters(QString geom);

signals:
        void applyChanges();
};

}  //  end GUI namespace

#endif
