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

#include <qlayout.h>
#include <qlabel.h>


#include <kdialog.h>

#include "printproperties.h"

PrintSettings::PrintSettings( QWidget *parent, const char *name )
 : KPrintDialogPage( parent, name )
{
    setTitle( i18n( "Layout" ) );
    QGridLayout *lo = new QGridLayout ( this, 4, 2 );
    lo->setSpacing( KDialog::spacingHint() );

    QLabel *lab = new QLabel(i18n("Thumbnails: "), this);
    m_images = new QComboBox(this);
    m_images->insertItem(i18n("None"));
    m_images->insertItem(i18n("Small"));
    m_images->insertItem(i18n("Large"));


    lo->addWidget(lab, 0, 0);
    lo->addWidget(m_images, 0, 1);

    m_printFullPath = new QCheckBox(i18n("Print full path for clip names"), this);
    lo->addMultiCellWidget( m_printFullPath, 1, 1, 0, 1 );

    m_printGray = new QCheckBox(i18n("Grayscale images"), this);
    lo->addMultiCellWidget( m_printGray, 2, 2, 0, 1);

    m_printFilter = new QCheckBox(i18n("Print filtered clips only"), this);
    lo->addMultiCellWidget( m_printFilter, 3, 3, 0, 1);
}

void PrintSettings::setOptions(const QMap<QString,QString>& opts)
{
    QString v;
    v = opts["kde-kdenlive-images"];
    if ( ! v.isEmpty() ) {
	m_images->setCurrentItem( v.toInt());
    }
    else m_images->setCurrentItem( 1 );

    v = opts["kde-kdenlive-fullpath"];
    if ( ! v.isEmpty() )
	m_printFullPath->setChecked( v == "true" );

    v = opts["kde-kdenlive-filter"];
    if ( ! v.isEmpty() )
	m_printFilter->setChecked( v == "true" );

    v = opts["kde-kdenlive-gray"];
    if ( ! v.isEmpty() )
	m_printGray->setChecked( v == "true" );
    else m_printGray->setChecked( true );

}

void PrintSettings::getOptions(QMap<QString,QString>& opts, bool )
{
    opts["kde-kdenlive-images"] = QString::number(m_images->currentItem());
    opts["kde-kdenlive-fullpath"] = m_printFullPath->isChecked() ? "true" : "false";
    opts["kde-kdenlive-filter"] = m_printFilter->isChecked() ? "true" : "false";
    opts["kde-kdenlive-gray"] = m_printGray->isChecked() ? "true" : "false";
}

bool PrintSettings::isValid(QString& msg)
{
        // let's imagine that autofit is not allowed for whatever reason
        /*if (m_printImages->isChecked())
        {
                msg = i18n("You cannot print with AutoFit mode on.");
                return false;
        }*/
        return true;
}