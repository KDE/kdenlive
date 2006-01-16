/***************************************************************************
                       exportconfig  -  description
                          -------------------
 begin                : Sun Nov 16 2003
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
#include "exportconfig.h"

#include <qvbox.h>
#include <qlayout.h>

#include <avfileformatwidget.h>

ExportConfig::ExportConfig(QPtrList < AVFileFormatDesc > &formatList, QWidget * parent, const char *name, WFlags f):
KJanusWidget(parent, name, TreeList), m_formatList(formatList)
{
    m_pageList.setAutoDelete(true);
    generateLayout();
}


ExportConfig::~ExportConfig()
{
}

void ExportConfig::generateLayout()
{
    m_pageList.clear();

    QPtrListIterator < AVFileFormatDesc > itt(m_formatList);

    while (itt.current()) {
	QVBox *frame = addVBoxPage(QString(itt.current()->name()));
	QHBoxLayout *layout = new QHBoxLayout(frame, 0, 6);
	AVFormatWidgetBase *page = itt.current()->createWidget(frame);
	m_pageList.append(page);
	layout->addWidget(page->widget());
	++itt;
    }
}

KURL ExportConfig::url()
{
    AVFileFormatWidget *fileFormatWidget =
	m_pageList.at(activePageIndex())->fileFormatWidget();
    if (fileFormatWidget) {
	return fileFormatWidget->fileUrl();
    }
    return KURL();
}

void ExportConfig::setFormatList(const QPtrList < AVFileFormatDesc > &list)
{
    m_formatList = list;
    generateLayout();
}

#include "exportconfig.moc"
