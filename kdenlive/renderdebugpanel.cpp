/***************************************************************************
                         renderdebugpanel.cpp  -  description
                            -------------------
   begin                : Sun Jan 5 2003
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

#include "renderdebugpanel.h"

#include <qtextedit.h>
#include <klocale.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <qtooltip.h>

RenderDebugPanel::RenderDebugPanel( QWidget *parent, const char *name ) :
		QVBox( parent, name ),
		m_mainLayout( this, "mainLayout" ),
		m_rendererList( &m_mainLayout, "list" ),
		m_textLayout( QSplitter::Vertical, &m_mainLayout, "textLayout" ),
		m_widgetStack( &m_textLayout, "stack" ),
		m_vemlEdit( &m_textLayout, "vemlEdit" ),
		m_buttonLayout( this, "button layout" ),
		m_spacer( &m_buttonLayout, "spacer" ),
		m_ignoreMessages( i18n( "Ignore Incoming Messages " ), &m_buttonLayout, "ignore messages" ),
		m_sendVemlButton( i18n( "Send Veml Command" ), &m_buttonLayout, "ignore messages" ),
		m_saveMessages( i18n( "Save Messages" ), &m_buttonLayout, "save button" ),
		m_nextId( 0 )
{
	m_spacer.setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum ) );
	m_rendererList.setSizePolicy( QSizePolicy( QSizePolicy::Maximum, QSizePolicy::Expanding ) );
	m_widgetStack.setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );
	m_vemlEdit.setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );
	connect( &m_rendererList, SIGNAL( highlighted( int ) ), &m_widgetStack, SLOT( raiseWidget( int ) ) );
	connect( &m_saveMessages, SIGNAL( clicked() ), this, SLOT( saveMessages() ) );
	connect( &m_sendVemlButton, SIGNAL( clicked() ), this, SLOT( sendDebugVeml() ) );
	m_ignoreMessages.setChecked( true );
	
	//Debug Panel Tooltips
	QToolTip::add( &m_saveMessages, i18n( "Save debug messages from renderer and Kdenlive" ) );
	QToolTip::add( &m_sendVemlButton, i18n( "Send VEML messages to the renderer" ) );
	if( m_ignoreMessages.isChecked() ){
		QToolTip::add( &m_ignoreMessages, i18n( "Unselect to ignore debug messages" ) );
	}else{
		QToolTip::add( &m_ignoreMessages, i18n( "Select to ignore debug messages" ) );
	}
}

RenderDebugPanel::~RenderDebugPanel()
{}

void RenderDebugPanel::slotPrintRenderDebug( const char * &name, const QString &message )
{
	slotPrintDebug( name, message );
}

void RenderDebugPanel::slotPrintRenderWarning( const char * &name, const QString &message )
{
	slotPrintWarning( name, message );
}

void RenderDebugPanel::slotPrintRenderError( const char * &name, const QString &message )
{
	slotPrintError( name, message );
}

/** Prints a warning message to the debug area. */
void RenderDebugPanel::slotPrintWarning( const char * &name, const QString &message )
{
	if ( m_ignoreMessages.isChecked() ) return ;

	QTextEdit *edit = getTextEdit( name );
	QString newmess = message;

	/*newmess.replace("<", "&lt;");
	  newmess.replace(">", "&gt;");
	  edit->append("<font color=red>" + newmess + "</font>");*/
	if ( edit ) {
		edit->setColor( QColor( 0, 0, 0 ) );
		edit->append( newmess );
	} else {
		kdError() << "Render Debug Edit box does not exist" << endl;
	}
}

/** Prints an error message to the debug window. */
void RenderDebugPanel::slotPrintError( const char * &name, const QString &message )
{
	if ( m_ignoreMessages.isChecked() ) return ;

	QTextEdit *edit = getTextEdit( name );
	QString newmess = message;
	/*newmess.replace("<", "&lt;");
	  newmess.replace(">", "&gt;");
	  edit->append("<font color=red>" + newmess + "</font>");*/
	if ( edit ) {
		edit->setColor( QColor( 255, 0, 0 ) );
		edit->append( newmess );
	} else {
		kdError() << "Render Debug Edit box does not exist" << endl;
	}
}

/** Prints a debug (informational) message to the debug */
void RenderDebugPanel::slotPrintDebug( const char * &name, const QString &message )
{
	if ( m_ignoreMessages.isChecked() ) return ;

	QTextEdit *edit = getTextEdit( name );
	QString newmess = message;
	/*newmess.replace("<", "&lt;");
	  newmess.replace(">", "&gt;");
	  edit->append("<font color=black>" + newmess + "</font>");*/
	if ( edit ) {
		edit->setColor( QColor( 0, 128, 0 ) );
		edit->append( newmess );
	} else {
		kdError() << "Render Debug Edit box does not exist" << endl;
	}
}

/** Returns the text edit widget with the given name, creating one if it doesn't exist. */
QTextEdit * RenderDebugPanel::getTextEdit( const char * &name )
{
	QTextEdit * edit;

	if ( !m_boxNames.contains( name ) ) {
		edit = new QTextEdit( &m_widgetStack, name );
		//edit->setTextFormat(LogText);
		edit->setReadOnly( true );
		m_widgetStack.addWidget( edit, m_nextId );
		m_rendererList.insertItem( name, m_nextId );
		m_boxNames[ name ] = m_nextId;
		++m_nextId;
	} else {
		edit = ( QTextEdit * ) m_widgetStack.widget( m_boxNames[ name ] );
	}

	return edit;
}

/** Requests a filename from the user and saves all messages into that file. */
void RenderDebugPanel::saveMessages()
{
	QString save;
	KURL url = KFileDialog::getSaveURL( 0, 0, this, i18n( "Save Debug Messages..." ) );

	if ( !url.isEmpty() ) {
		QFile file( url.path() );
		if ( file.open( IO_WriteOnly ) ) {
			for ( int count = 0; count < m_boxNames.count(); count++ ) {
				QTextEdit *edit = ( QTextEdit * ) m_widgetStack.widget( count );
				save = "*******************************************\n";
				save = save + "* " + edit->name() + "\n" +
				       "*******************************************\n\n" +
				       edit->text() +
				       "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

				file.writeBlock( save.ascii(), save.length() );
			}
			file.close();
		}
	}
}

void RenderDebugPanel::setIgnoreMessages(bool ignore)
{
	m_ignoreMessages.setChecked( ignore );
}

bool RenderDebugPanel::ignoreMessages() const
{
	return m_ignoreMessages.isChecked();
}

void RenderDebugPanel::sendDebugVeml()
{
	emit debugVemlSendRequest(m_rendererList.currentText(), m_vemlEdit.text());
}
