/***************************************************************************
                          AVFormatWidgetCodecList.cpp  -  description
                             -------------------
    begin                : Tue Feb 4 2003
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

#include "qhbox.h"
#include "qlabel.h"
#include "qcombobox.h"
#include "qwidgetstack.h"
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <kdebug.h>

#include "avformatwidgetcodeclist.h"
#include "avformatdesccodeclist.h"
#include "avformatdesccodec.h"
#include "krender.h"

AVFormatWidgetCodecList::AVFormatWidgetCodecList(AVFormatDescCodecList *desc, QWidget *parent, const char *name ) :
                                    QGroupBox(1, Horizontal, desc->name(), parent, name),
                                    AVFormatWidgetBase(),
                                    m_codecLayout(new QHBox(this, "codec_layout")),
                                    m_codecLabel(new QLabel(i18n("Codec:"), m_codecLayout, "codec_label")),
                                    m_codecSelect(new QComboBox(m_codecLayout, "codec_select")),
                                    m_widgetStack(new QWidgetStack(this, "widget_stack"))
{
  QToolTip::add( m_codecSelect, i18n( "Select the codec for your output file" ) );

  QValueListConstIterator<QString> itt = desc->codecList().begin();
  int count = 0;

  while(itt != desc->codecList().end()) {
    AVFormatDescCodec *codecDesc = desc->renderer()->findCodec(*itt);
    if(codecDesc != 0) {
      m_codecSelect->insertItem(codecDesc->name(), count);
      m_widgetStack->addWidget(codecDesc->createWidget(m_widgetStack)->widget(), count);
      ++count;
    }
    ++itt;
  }
}

AVFormatWidgetCodecList::~AVFormatWidgetCodecList()
{
}

QWidget *AVFormatWidgetCodecList::widget()
{
  return this;
}
