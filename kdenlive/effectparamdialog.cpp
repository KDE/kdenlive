/***************************************************************************
                          effectparamdialog.cpp  -  description
                             -------------------
    begin                : Sun Feb 9 2003
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

#include "effectparamdialog.h"

#include <qhbox.h>
#include <qvbox.h>
#include <qcombobox.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kpushbutton.h>

#include <assert.h>

#include <kfixedruler.h>
#include <kmmtimeline.h>
#include <kmmtrackpanel.h>

#include "effectparamdesc.h"

#include "docclipref.h"
#include "effect.h"

EffectParamDialog::EffectParamDialog(KdenliveApp *app, KdenliveDoc *document, QWidget *parent, const char *name ) :
									QVBox(parent,name),
									m_desc(0),
									m_app(app),
									m_document(document)
{
	KIconLoader loader;

	m_presetLayout = new QHBox(this);
	m_presets = new QComboBox(m_presetLayout);
	m_presets->insertItem(i18n("Custom"));

	m_presetAdd = new KPushButton(i18n("add"), m_presetLayout, "preset_add");
	m_presetDelete = new KPushButton(i18n("delete"), m_presetLayout, "preset_delete");

	m_presetAdd->setPixmap( loader.loadIcon( "edit_add", KIcon::Toolbar ) );
	m_presetDelete->setPixmap( loader.loadIcon( "edit_remove", KIcon::Toolbar ) );

	m_editLayout = new QVBox(this);
	m_timeline = new KTimeLine(0, 0, m_editLayout, name);
	m_timeline->setPanelWidth(50);

	m_presets->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
}

EffectParamDialog::~EffectParamDialog()
{
}

void EffectParamDialog::slotSetEffectDescription(const EffectDesc &desc)
{
	clearEffect();
	m_desc = &desc;
	m_clip = NULL;
	m_effect = NULL;
	generateLayout();
}

void EffectParamDialog::generateLayout()
{
	m_timeline->clearTrackList();

	assert(m_desc);

	for(uint count=0; count<m_desc->numParameters(); ++count) {
		EffectParamDesc *paramDesc = m_desc->parameter(count);
		assert(paramDesc);
		KTrackPanel *panel = paramDesc->createClipPanel(m_app, m_timeline, m_document, m_clip);
		assert(panel);
		m_timeline->appendTrack(panel);
	}

	if(m_clip) {
		m_timeline->slotSetProjectLength(m_clip->cropDuration());
	} else {
		m_timeline->slotSetProjectLength(GenTime(100.0));
	}
}

void EffectParamDialog::slotSetEffect(DocClipRef *clip, Effect *effect)
{
	kdWarning() << "EffectParamDialog::slotSetEffect" << endl;
	assert(effect);
	clearEffect();

	m_clip = clip;
	m_effect = effect;
	if(m_effect) {
		m_desc = &m_effect->effectDescription();
	} else {
		m_desc = 0;
	}
	generateLayout();
}

void EffectParamDialog::clearEffect()
{
	m_effect = 0;
	m_clip = 0;
	m_desc = 0;
}
