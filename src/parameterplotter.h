/***************************************************************************
                          parameterplotter.h  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <KPlotWidget>

class ParameterPlotter : public KPlotWidget {
	Q_OBJECT
	public:
		ParameterPlotter (QWidget *parent=0);
	void setPointLists(const QList< QPair<QString, QMap<int,QVariant> > >&,int,int);
		QList< QPair<QString, QMap<int,QVariant> > > getPointLists();
	private:
		QList< QPair<QString, QMap<int,QVariant> > > pointlists;
		KPlotPoint* movepoint;
		KPlotObject *plot;
		QPoint oldmousepoint;
	protected:
		void mouseMoveEvent ( QMouseEvent * event );
		void mousePressEvent ( QMouseEvent * event );	
	
};
