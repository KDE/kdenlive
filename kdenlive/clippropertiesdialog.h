/***************************************************************************
                          clippropertiesdialog.h  -  description
                             -------------------
    begin                : Mon Mar 17 2003
    copyright            : (C) 2003 by Jason Wood
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

#ifndef CLIPPROPERTIESDIALOG_H
#define CLIPPROPERTIESDIALOG_H

#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>

class QVBox;

class DocClipRef;
class KdenliveDoc;
class KdenliveApp;
class DocClipAVFile;

/**Displays the properties of a sepcified clip.
  *@author Jason Wood
  */

class ClipPropertiesDialog : public QVBox  {
   Q_OBJECT
public: 
	ClipPropertiesDialog(QWidget *parent=0, const char *name=0);
	virtual ~ClipPropertiesDialog();
	/** Specifies the clip that we wish to display the properties of. */
	void setClip(DocClipRef *clip);
private:
	//video properties
	QLabel *filenameLabel;
	QLabel *frameSizeLabel;
	QLabel *videoLength;
	QLabel *systemLabel;
	QLabel *decompressorLabel;
	//audio properties
	QLabel *samplingRateLabel;
	QLabel *channelsLabel;
	QLabel *formatLabel;
	QLabel *audiobitLabel;
	
  	DocClipRef *m_clip;
};

#endif
