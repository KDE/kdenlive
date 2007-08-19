/***************************************************************************
                          printproperties  -  description
                             -------------------
    begin                : Sat Jul 28 2007
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



#ifndef PRINTPROPS_H
#define PRINTPROPS_H

// include files for Qt
#include <qcheckbox.h>
#include <qcombobox.h>

#include <klocale.h>
#include <kdeprint/kprintdialogpage.h>

class PrintSettings : public KPrintDialogPage
{
public:
	PrintSettings( QWidget *parent = 0, const char *name = 0 );

        void setOptions(const QMap<QString,QString>& opts);
        void getOptions(QMap<QString,QString>& opts, bool include_def = false);
        bool isValid(QString& msg);
private:
        QCheckBox       *m_printFullPath;
        QCheckBox       *m_printFilter;
        QCheckBox       *m_printGray;
	QComboBox	*m_images;

};

#endif
