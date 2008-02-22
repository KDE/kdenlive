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
#include <QDomElement>
#include "ui_keyframewidget_ui.h"

class PlotWrapper : public KPlotWidget {
	Q_OBJECT
	public:
		PlotWrapper (QWidget *parent=0):KPlotWidget(parent){}
		QList<KPlotPoint*> pointsUnderPoint(const QPoint& p){
			return KPlotWidget::pointsUnderPoint( p );
		}
};


class ParameterPlotter : public QWidget , public Ui::KeyframeWidget_UI {
	Q_OBJECT
	public:
		ParameterPlotter (QWidget *parent=0);
		virtual ~ParameterPlotter(){}
		void replot(const QString& name="");
	private:
		KPlotPoint* movepoint;
		PlotWrapper* kplotwidget;
		int activeIndexPlot;
		bool m_moveX,m_moveY,m_moveTimeline,m_newPoints;
		QPoint oldmousepoint;
		int maxx,maxy;
		QStringList parameterNameList;
		void createParametersNew();
		QList<KPlotObject*> plotobjects;
		QList<QColor> colors;
		QDomElement itemParameter;
		void updateButtonStatus();
	protected:
		void mouseMoveEvent ( QMouseEvent * event );
		void mousePressEvent ( QMouseEvent * event );
	public slots:
		void setPointLists(const QDomElement&,int ,int);
		void slotSetMoveX();
		void slotSetMoveY();
		void slotSetNew();
		void slotSetHelp();
		void slotShowInTimeline();
		void slotParameterChanged(const QString&);
	signals:
		void parameterChanged(QDomElement );
		void updateFrame(int);
	
};

