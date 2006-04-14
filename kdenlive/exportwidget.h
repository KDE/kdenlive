/***************************************************************************
                          exportwidget  -  description
                             -------------------
    begin                : Tue Nov 15 2005
    copyright            : (C) 2005 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EXPORTW_H
#define EXPORTW_H

#include <stdint.h>

#include <qlayout.h>

#ifdef ENABLE_FIREWIRE
#include <libiec61883/iec61883.h>
#endif


#include "gentime.h"
#include "exportbasewidget_ui.h"

class exportWidget : public exportBaseWidget_UI
{
        Q_OBJECT
public:
    exportWidget( const GenTime &duration, QWidget* parent=0, const char* name=0, WFlags fl=0);
        virtual ~exportWidget();

private:
        QHBoxLayout* flayout;
        GenTime m_duration;
        bool m_isRunning;
        
        /** AVC stuff 
        int m_port;
        int m_node;
        uint64_t m_guid;
        AVC *m_avc;*/

private slots:
	void startExport();
        void stopExport();
	void exportFileToTheora(QString dstFileName, int width = 320, int height = 240, int audio =2, int video =4);
	void slotAdjustWidgets(int pos);
        void initEncoders();
        void initDvConnection();

public slots:
	void endExport();
	void reportProgress(GenTime progress);

signals:
    void exportTimeLine(QString, QString, QString, GenTime, GenTime);
    void stopTimeLineExport();
    void exportToFirewire(QString, int);
	
};
#endif
